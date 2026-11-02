/* SPDX-License-Identifier: MIT */
#include <errno.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include "pipe/p_state.h"
#include "remesa_fixed_state.h"

bool remesa_viewport_swizzle_is_identity(const struct pipe_viewport_state *v)
{
   if (!v) return false;
   bool legacy = !v->swizzle_x && !v->swizzle_y && !v->swizzle_z && !v->swizzle_w;
   bool canonical = v->swizzle_x == PIPE_VIEWPORT_SWIZZLE_POSITIVE_X &&
                    v->swizzle_y == PIPE_VIEWPORT_SWIZZLE_POSITIVE_Y &&
                    v->swizzle_z == PIPE_VIEWPORT_SWIZZLE_POSITIVE_Z &&
                    v->swizzle_w == PIPE_VIEWPORT_SWIZZLE_POSITIVE_W;
   return legacy || canonical;
}

int
remesa_validate_blend(const struct pipe_blend_state *s)
{
   if (!s) return EINVAL;
   if (s->independent_blend_enable || s->max_rt || s->logicop_enable ||
       s->dither || s->alpha_to_coverage || s->alpha_to_coverage_dither ||
       s->alpha_to_one || s->advanced_blend_func || s->rt[0].blend_enable ||
       s->rt[0].colormask != PIPE_MASK_RGBA)
      return ENOTSUP;
   /* One render target, independent blending disabled. Slots 1..7 are
    * inapplicable. Disabled blend/logic-op operands and coherence have no
    * effect. Field comparisons deliberately ignore C padding. */
   return 0;
}

int
remesa_validate_dsa(const struct pipe_depth_stencil_alpha_state *s)
{
   if (!s) return EINVAL;
   if (s->depth_enabled || s->depth_bounds_test ||
       s->stencil[0].enabled || s->stencil[1].enabled || s->alpha_enabled)
      return ENOTSUP;
   /* No depth/stencil resource is admitted by the caller. A depth write mask
    * does not perform a write with the depth test disabled. Functions, masks,
    * references and bounds of disabled tests cannot change this profile. */
   return 0;
}

int
remesa_validate_rasterizer(const struct pipe_rasterizer_state *s)
{
   if (!s) return EINVAL;
   if (s->flatshade || s->light_twoside || s->clamp_vertex_color ||
       s->clamp_fragment_color || s->cull_face != PIPE_FACE_NONE ||
       s->fill_front != PIPE_POLYGON_MODE_FILL ||
       s->fill_back != PIPE_POLYGON_MODE_FILL || s->offset_tri ||
       s->poly_smooth || s->poly_stipple_enable || s->multisample ||
       s->force_persample_interp || s->conservative_raster_mode ||
       s->rasterizer_discard || s->tile_raster_order_fixed ||
       s->half_pixel_center || s->bottom_edge_rule ||
       s->subpixel_precision_x || s->subpixel_precision_y || s->depth_clamp ||
       s->clip_halfz || s->unclamped_fragment_depth_values ||
       s->clip_plane_enable)
      return ENOTSUP;
   /* The caller admits triangles only, constant fragment output, no culling,
    * and full-target scissoring only. Orientation/provoking vertex and point/
    * line fields are therefore immaterial. Offset operands are immaterial
    * with triangle offset disabled. Tile directions/conservative dilate are
    * immaterial when the corresponding feature is disabled.
    *
    * Depth-clip flags are admitted ONLY because draw preparation rejects all
    * positions outside [-1,1] clip space and requires w=1. This validator does
    * not implement clipping; the parent draw admission must enforce it. No
    * unsupported alternative edge rule is silently translated. */
   return 0;
}

int remesa_validate_lab_rasterizer(const struct pipe_rasterizer_state *s,
                                  enum remesa_lab_raster_profile profile)
{
   if (!s) return EINVAL;
   if (profile != REMESA_LAB_RASTER_QUARTER_FBO ||
       !s->half_pixel_center || s->bottom_edge_rule) return ENOTSUP;
   /* Only the finite verified quarter-window case may use this seam. Every
    * other raster restriction is delegated unchanged to the global validator.
    * Geometry, framebuffer, NIR sources and actual issue remain separate. */
   struct pipe_rasterizer_state ordinary = *s;
   ordinary.half_pixel_center = 0;
   /* This finite carrier requires a POSITION-only VS and immutable constant
    * green FS. Vertex COLOR outputs do not exist, so vertex-color clamp has
    * no semantic target. Do not generalize to varying/color-output shaders. */
   ordinary.clamp_vertex_color = 0;
   return remesa_validate_rasterizer(&ordinary);
}

int remesa_validate_recording_window_rasterizer(const struct pipe_rasterizer_state *s)
{
   if (!s) return EINVAL;
   if (!s->half_pixel_center || !s->bottom_edge_rule) return ENOTSUP;
   struct pipe_rasterizer_state ordinary = *s;
   ordinary.half_pixel_center = ordinary.bottom_edge_rule = 0;
   /* The draw path admits only POSITION and constant fragment output. There
    * is no vertex COLOR output for this clamp to act on. */
   ordinary.clamp_vertex_color = 0;
   return remesa_validate_rasterizer(&ordinary);
}

int remesa_validate_homogeneous_w2_rasterizer(const struct pipe_rasterizer_state *s)
{
   if (!s) return EINVAL;
   if (!s->half_pixel_center || !s->bottom_edge_rule || !s->front_ccw ||
       !s->depth_clip_near || !s->depth_clip_far || !s->clamp_vertex_color ||
       s->scissor) return ENOTSUP;
   struct pipe_rasterizer_state ordinary = *s;
   /* COLOR clamp is encoded by the separate CODE344 semantic variant.
    * Closed W2 inputs are strictly inside clip space and have no sample-edge
    * ties. Their proof is separate from this descriptor compatibility check. */
   ordinary.half_pixel_center = ordinary.bottom_edge_rule = 0;
   ordinary.clamp_vertex_color = 0;
   return remesa_validate_rasterizer(&ordinary);
}

int remesa_admit_homogeneous_w2_inputs(const void *pn,size_t pn_bytes,
                                      const void *coefficients,size_t coefficient_bytes,
                                      uint32_t width,uint32_t height)
{
   /* Exact actual-API inputs preserved in the separate LAB catalogue.
    * These are semantic admission data, not transformed vertices or pixels.
    * Float bit comparisons preserve normal-w/alpha, NaNs and signed zero.
    * No hashing, frame readback, shader encoding or vertex arithmetic. */
   static const uint32_t vertices[24] = {
      0x41040000,0x41040000,0x3f000000,0x3f800000,0,0,0x3f800000,0x3f800000,
      0x41c20000,0x41040000,0x3f000000,0x3f800000,0,0,0x3f800000,0x3f800000,
      0x41040000,0x41c20000,0x3f000000,0x3f800000,0,0,0x3f800000,0x3f800000
   };
   static const uint32_t constants[36] = {
      0,0,0,0x3f800000,0,0x3f800000,0,0x3f800000,
      0x3f800000,0,0,0x3f800000,0,0,0x3f800000,0x3f800000,
      0x3b800000,0,0,0,0,0xbb800000,0,0,0,0,0,0,
      0xc0000000,0x40001000,0,0x40000000,0,0,0x3f800000,0
   };
   if (!pn || !coefficients) return EINVAL;
   if (pn_bytes != sizeof(vertices) || coefficient_bytes != sizeof(constants) ||
       !((width == 1280 && height == 800) || (width == 640 && height == 640)))
      return ENOTSUP;
   if ((uintptr_t)pn > UINTPTR_MAX - pn_bytes ||
       (uintptr_t)coefficients > UINTPTR_MAX - coefficient_bytes) return EINVAL;
   for (unsigned i=0;i<24;i++) {
      uint32_t bits;memcpy(&bits,(const unsigned char *)pn+4*i,4);
      if (bits != vertices[i]) return ENOTSUP;
   }
   /* Parameter source bytes are little-endian, independently of host order. */
   const unsigned char *c = coefficients;
   for (unsigned i=0;i<36;i++) {
      uint32_t bits=(uint32_t)c[4*i] | (uint32_t)c[4*i+1]<<8 |
                    (uint32_t)c[4*i+2]<<16 | (uint32_t)c[4*i+3]<<24;
      if (bits != constants[i]) return ENOTSUP;
   }
   return 0;
}

int remesa_prove_half_pixel_edge_independence(const float p[3][4],uint32_t width,uint32_t height)
{
   int64_t xy[3][2];
   if (!p || !width || !height) return EINVAL;
   if (width > 32 || height > 32) return ENOTSUP;
   for (unsigned v=0;v<3;v++) {
      for (unsigned c=0;c<4;c++) if (!isfinite(p[v][c])) return EINVAL;
      if (p[v][3]!=1 || p[v][2]<0 || p[v][2]>1) return EINVAL;
      for (unsigned axis=0;axis<2;axis++) {
         if (p[v][axis]<0 || p[v][axis]>(axis?height:width)) return ENOTSUP;
         double quarter=(double)p[v][axis]*4;
         /* Bounds above constrain this conversion to[0,128]; no overflow or
          * libm rounding dependency on the i386 target. */
         int64_t integer=(int64_t)quarter;
         if (quarter!=(double)integer) return ERANGE;
         xy[v][axis]=integer;
      }
   }
   int64_t area=(xy[1][0]-xy[0][0])*(xy[2][1]-xy[0][1])-
                (xy[1][1]-xy[0][1])*(xy[2][0]-xy[0][0]);
   if (!area) return ENOTSUP;
   for (uint32_t y=0;y<height;y++) for (uint32_t x=0;x<width;x++)
      for (unsigned edge=0;edge<3;edge++) {
         unsigned next=(edge+1)%3;
         int64_t dx=xy[next][0]-xy[edge][0],dy=xy[next][1]-xy[edge][1];
         int64_t sx=4*(int64_t)x+2-xy[edge][0],sy=4*(int64_t)y+2-xy[edge][1];
         /* Reject collinearity even outside the finite edge: conservative,
          * and sufficient to establish that all actual edge ties are absent. */
         if (dx*sy-dy*sx==0) return ENOTSUP;
      }
   return 0;
}

static double clip_distance(const double p[4], unsigned plane)
{
   /* OpenGL canonical homogeneous volume: -w <= x,y,z <= w. */
   return p[3] + ((plane & 1) ? -p[plane / 2] : p[plane / 2]);
}

static int append_clip_vertex(double polygon[REMESA_CLIPPED_MAX_VERTICES][4],
                              unsigned *count, const double point[4])
{
   if (*count && !memcmp(polygon[*count - 1], point, sizeof(polygon[0]))) return 0;
   if (*count == REMESA_CLIPPED_MAX_VERTICES) return EOVERFLOW;
   memcpy(polygon[(*count)++], point, sizeof(polygon[0]));
   return 0;
}

int remesa_prepare_constant_triangle(const float position[3][4],
                                    const struct pipe_viewport_state *viewport,
                                    uint32_t width, uint32_t height,
                                    struct remesa_clipped_polygon *out)
{
   double polygon[2][REMESA_CLIPPED_MAX_VERTICES][4];
   struct remesa_clipped_polygon result = {0};
   unsigned current = 0, count = 3;
   if (!position || !viewport || !out || !width || !height)
      return EINVAL;
   if (!remesa_viewport_swizzle_is_identity(viewport))
      return ENOTSUP;
   for (unsigned axis = 0; axis < 3; ++axis)
      if (!isfinite(viewport->scale[axis]) ||
          !isfinite(viewport->translate[axis])) return EINVAL;
   /* Restrict this preparation contract to viewports inside the attachment.
    * Negative scale supports origin reversal. Scissoring remains separate. */
   for (unsigned axis = 0; axis < 2; ++axis) {
      double low = (double)viewport->translate[axis] - fabs(viewport->scale[axis]);
      double high = (double)viewport->translate[axis] + fabs(viewport->scale[axis]);
      if (low < 0 || high > (axis ? height : width)) return ENOTSUP;
   }
   for (unsigned vertex = 0; vertex < 3; ++vertex)
      for (unsigned axis = 0; axis < 4; ++axis) {
         if (!isfinite(position[vertex][axis])) return EINVAL;
         polygon[0][vertex][axis] = position[vertex][axis];
      }
   for (unsigned plane = 0; plane < 6 && count; ++plane) {
      unsigned next = current ^ 1, produced = 0;
      for (unsigned index = 0; index < count; ++index) {
         const double *a = polygon[current][(index + count - 1) % count];
         const double *b = polygon[current][index];
         double da = clip_distance(a, plane), db = clip_distance(b, plane);
         if ((da < 0) != (db < 0)) {
            double point[4];
            double fraction = da / (da - db);
            for (unsigned axis = 0; axis < 4; ++axis)
               point[axis] = a[axis] + fraction * (b[axis] - a[axis]);
            if (append_clip_vertex(polygon[next], &produced, point)) return EOVERFLOW;
         }
         if (db >= 0) {
            if (append_clip_vertex(polygon[next], &produced, b)) return EOVERFLOW;
         }
      }
      if (produced > 1 && !memcmp(polygon[next][0], polygon[next][produced - 1],
                                  sizeof(polygon[next][0]))) --produced;
      count = produced; current = next;
   }
   if (count < 3) count = 0;
   for (unsigned index = 0; index < count; ++index) {
      const double *p = polygon[current][index];
      /* Zero-w points at the homogeneous origin have no defined projection.
       * Fail closed rather than manufacture screen coordinates. */
      if (p[3] <= 0) return ENOTSUP;
      for (unsigned axis = 0; axis < 3; ++axis) {
         double value = p[axis] / p[3] * viewport->scale[axis] + viewport->translate[axis];
         float converted = (float)value;
         if (!isfinite(converted)) return EOVERFLOW;
         result.window[index][axis] = converted;
      }
      result.window[index][3] = 1;
   }
   result.count = count;
   *out = result;
   return 0;
}

int remesa_clip_scissor(const struct pipe_scissor_state *input,
                        uint32_t width, uint32_t height,
                        struct pipe_scissor_state *out)
{
   struct pipe_scissor_state result;
   if (!input || !out || !width || !height ||
       input->minx > input->maxx || input->miny > input->maxy) return EINVAL;
   result.minx = input->minx < width ? input->minx : width;
   result.maxx = input->maxx < width ? input->maxx : width;
   result.miny = input->miny < height ? input->miny : height;
   result.maxy = input->maxy < height ? input->maxy : height;
   *out = result;
   return 0;
}

int remesa_cull_window_triangle(const float position[3][4], unsigned cull_face,
                                bool front_ccw, bool *discard)
{
   double dx1, dy1, dx2, dy2, left, right, area, bound;
   bool result;
   if (!position || !discard) return EINVAL;
   if (cull_face & ~PIPE_FACE_FRONT_AND_BACK) return EINVAL;
   for (unsigned vertex = 0; vertex < 3; ++vertex) {
      for (unsigned axis = 0; axis < 4; ++axis)
         if (!isfinite(position[vertex][axis])) return EINVAL;
      if (position[vertex][3] != 1) return ENOTSUP;
   }
   /* A disabled cull test cannot depend on a numerically uncertain facing.
    * Likewise culling both faces needs no orientation classification. */
   if (cull_face == PIPE_FACE_NONE || cull_face == PIPE_FACE_FRONT_AND_BACK) {
      *discard = cull_face == PIPE_FACE_FRONT_AND_BACK;
      return 0;
   }
   dx1 = (double)position[1][0] - position[0][0];
   dy1 = (double)position[1][1] - position[0][1];
   dx2 = (double)position[2][0] - position[0][0];
   dy2 = (double)position[2][1] - position[0][1];
   /* Exact repeated vertices or a single axis-aligned line have zero filled
    * area. Binary32-to-binary64 conversion does not introduce underflow. */
   if ((!dx1 && !dy1) || (!dx2 && !dy2) ||
       (position[1][0] == position[2][0] && position[1][1] == position[2][1]) ||
       (!dx1 && !dx2) || (!dy1 && !dy2)) {
      *discard = true;
      return 0;
   }
   left = dx1 * dy2;
   right = dy1 * dx2;
   area = left - right;
   /* Conservatively bound roundoff from four differences, two products and
    * their subtraction. Do not decide a face by an epsilon heuristic when
    * cancellation could change its sign. This is a host error bound, not a
    * hardware subpixel or edge-coverage tolerance. */
   bound = (fabs(left) + fabs(right)) * (16 * DBL_EPSILON);
   if (fabs(area) <= bound) return ERANGE;
   result = ((area > 0) == front_ccw) ? cull_face == PIPE_FACE_FRONT :
                                               cull_face == PIPE_FACE_BACK;
   *discard = result;
   return 0;
}

int remesa_prepare_color_clear(unsigned format, uint32_t attachment_width,
                               uint32_t attachment_height, uint32_t stride,
                               size_t backing_bytes,
                               const union pipe_color_union *color,
                               uint32_t x, uint32_t y,
                               uint32_t width, uint32_t height,
                               struct remesa_color_clear *out)
{
   struct remesa_color_clear result = {0};
   uint64_t packed, allocation, offset;
   if (!color || !out || !attachment_width || !attachment_height) return EINVAL;
   if (format != PIPE_FORMAT_B8G8R8A8_UNORM &&
       format != PIPE_FORMAT_R8G8B8A8_UNORM) return ENOTSUP;
   packed = (uint64_t)attachment_width * 4;
   if (packed > UINT32_MAX) return EOVERFLOW;
   if (stride < packed || stride % 4) return EINVAL;
   allocation = (uint64_t)stride * attachment_height;
   if (allocation > SIZE_MAX) return EOVERFLOW;
   if (allocation > backing_bytes) return ENOSPC;
   /* Subtraction checks reject wrapping rectangles before offset arithmetic. */
   if (x > attachment_width || y > attachment_height ||
       width > attachment_width - x || height > attachment_height - y)
      return EINVAL;
   for (unsigned channel = 0; channel < 4; ++channel) {
      float value = color->f[channel];
      if (!isfinite(value)) return EINVAL;
      unsigned byte = value <= 0 ? 0 : value >= 1 ? 255 :
                      (unsigned)(value * 255.0f + 0.5f);
      unsigned destination = channel;
      if (format == PIPE_FORMAT_B8G8R8A8_UNORM)
         destination = channel == 0 ? 2 : channel == 2 ? 0 : channel;
      result.pixel[destination] = byte;
   }
   result.stride = stride;
   /* Empty rectangles do not have a meaningful mapping offset, especially at
    * the bottom-right boundary. Do not expose a pointer beyond allocation. */
   if (width && height) {
      offset = (uint64_t)y * stride + (uint64_t)x * 4;
      result.offset = (size_t)offset;
      result.row_bytes = (size_t)width * 4;
      result.rows = height;
   }
   *out = result;
   return 0;
}

int remesa_validate_gears_dsa(const struct pipe_depth_stencil_alpha_state *state)
{
 if(!state)return EINVAL;
 if(state->depth_bounds_test||state->stencil[0].enabled||state->stencil[1].enabled||state->alpha_enabled)return ENOTSUP;
 if(state->depth_enabled&&(state->depth_func!=PIPE_FUNC_LESS||!state->depth_writemask))return ENOTSUP;
 return 0;
}
int remesa_validate_gears_rasterizer(const struct pipe_rasterizer_state *state)
{
 if(!state)return EINVAL;
 if(!state->half_pixel_center||
    (state->flatshade&&state->flatshade_first)||
    (state->cull_face!=PIPE_FACE_NONE&&state->cull_face!=PIPE_FACE_BACK))return ENOTSUP;
 struct pipe_rasterizer_state ordinary=*state;
 ordinary.flatshade=0;ordinary.cull_face=PIPE_FACE_NONE;ordinary.clamp_vertex_color=0;
 ordinary.half_pixel_center=ordinary.bottom_edge_rule=0;
 return remesa_validate_rasterizer(&ordinary);
}
