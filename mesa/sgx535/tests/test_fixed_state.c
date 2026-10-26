/* SPDX-License-Identifier: MIT */
#ifdef NDEBUG
#error This qualification requires assertions
#endif
#include <assert.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "pipe/p_state.h"
#include "remesa_fixed_state.h"

int main(void)
{
   unsigned cases = 0;
#define CHECK(condition) do { ++cases; assert(condition); } while (0)
   struct pipe_blend_state b = {0};
   struct pipe_depth_stencil_alpha_state d = {0};
   struct pipe_rasterizer_state r = {0};
   b.rt[0].colormask = PIPE_MASK_RGBA;
   CHECK(remesa_validate_blend(&b) == 0);
   CHECK(remesa_validate_dsa(&d) == 0);
   CHECK(remesa_validate_rasterizer(&r) == 0);
   CHECK(remesa_validate_blend(NULL) == EINVAL);
   CHECK(remesa_validate_dsa(NULL) == EINVAL);
   CHECK(remesa_validate_rasterizer(NULL) == EINVAL);
   struct pipe_rasterizer_state window={.half_pixel_center=1,.bottom_edge_rule=1,.clamp_vertex_color=1};
   CHECK(remesa_validate_recording_window_rasterizer(&window)==0);
   CHECK(remesa_validate_rasterizer(&window)==ENOTSUP);
   window.multisample=1;CHECK(remesa_validate_recording_window_rasterizer(&window)==ENOTSUP);
   window.multisample=0;window.bottom_edge_rule=0;
   CHECK(remesa_validate_recording_window_rasterizer(&window)==ENOTSUP);
   CHECK(remesa_validate_recording_window_rasterizer(NULL)==EINVAL);
   const float quarter[3][4]={{8.25f,8.25f,.5f,1},{24.25f,8.25f,.5f,1},{8.25f,24.25f,.5f,1}};
   const float reflected[3][4]={{8.25f,23.75f,.5f,1},{24.25f,23.75f,.5f,1},{8.25f,7.75f,.5f,1}};
   const float tied[3][4]={{8,8,.5f,1},{24,8,.5f,1},{8,24,.5f,1}};
   CHECK(!remesa_prove_half_pixel_edge_independence(quarter,32,32));
   CHECK(!remesa_prove_half_pixel_edge_independence(reflected,32,32));
   CHECK(remesa_prove_half_pixel_edge_independence(tied,32,32)==ENOTSUP);
   float malformed[3][4];memcpy(malformed,quarter,sizeof(malformed));malformed[0][0]=nextafterf(8.25f,9);
   CHECK(remesa_prove_half_pixel_edge_independence((const float (*)[4])malformed,32,32)==ERANGE);
   malformed[0][0]=INFINITY;
   CHECK(remesa_prove_half_pixel_edge_independence((const float (*)[4])malformed,32,32)==EINVAL);
   CHECK(remesa_prove_half_pixel_edge_independence(quarter,64,32)==ENOTSUP);
   CHECK(remesa_prove_half_pixel_edge_independence(NULL,32,32)==EINVAL);
#define BAD_B(field, value) do { struct pipe_blend_state t=b; t.field=value; CHECK(remesa_validate_blend(&t)==ENOTSUP); } while (0)
   BAD_B(rt[0].colormask, PIPE_MASK_R);
   BAD_B(rt[0].blend_enable, 1);
   BAD_B(logicop_enable, 1);
   BAD_B(dither, 1);
   BAD_B(alpha_to_coverage, 1);
   BAD_B(alpha_to_coverage_dither, 1);
   BAD_B(alpha_to_one, 1);
   BAD_B(advanced_blend_func, 1);
   BAD_B(independent_blend_enable, 1);
   BAD_B(max_rt, 1);
   /* Disabled blend/logic-op operands and unused RT slots cannot affect RT0. */
   b.rt[0].rgb_func=7; b.rt[0].alpha_src_factor=31;
   b.logicop_func=15; b.rt[7].blend_enable=1;
   CHECK(remesa_validate_blend(&b)==0);
#define BAD_D(field, value) do { struct pipe_depth_stencil_alpha_state t=d; t.field=value; CHECK(remesa_validate_dsa(&t)==ENOTSUP); } while (0)
   BAD_D(depth_enabled, 1);
   BAD_D(stencil[0].enabled, 1);
   BAD_D(stencil[1].enabled, 1);
   BAD_D(alpha_enabled, 1);
   BAD_D(depth_bounds_test, 1);
   d.depth_writemask=1; d.depth_func=7; d.alpha_ref_value=0.7f; d.stencil[0].writemask=255;
   CHECK(remesa_validate_dsa(&d)==0);
   for (unsigned function = PIPE_FUNC_NEVER; function <= PIPE_FUNC_ALWAYS; ++function) {
      d.depth_func=function;
      CHECK(remesa_validate_dsa(&d)==0);
      struct pipe_depth_stencil_alpha_state enabled=d;
      enabled.depth_enabled=1;
      CHECK(remesa_validate_dsa(&enabled)==ENOTSUP);
   }
#define BAD_R(field, value) do { struct pipe_rasterizer_state t=r; t.field=value; CHECK(remesa_validate_rasterizer(&t)==ENOTSUP); } while (0)
   BAD_R(cull_face, PIPE_FACE_BACK);
   BAD_R(fill_front, PIPE_POLYGON_MODE_LINE);
   BAD_R(fill_back, PIPE_POLYGON_MODE_POINT);
   BAD_R(offset_tri, 1);
   BAD_R(poly_smooth, 1);
   BAD_R(poly_stipple_enable, 1);
   BAD_R(multisample, 1);
   BAD_R(force_persample_interp, 1);
   BAD_R(conservative_raster_mode, 1);
   BAD_R(rasterizer_discard, 1);
   BAD_R(tile_raster_order_fixed, 1);
   BAD_R(clip_plane_enable, 1);
   BAD_R(unclamped_fragment_depth_values, 1);
   BAD_R(depth_clamp, 1);
   BAD_R(clip_halfz, 1);
   BAD_R(half_pixel_center, 1);
   BAD_R(bottom_edge_rule, 1);
   BAD_R(subpixel_precision_x, 1);
   BAD_R(subpixel_precision_y, 1);
   BAD_R(flatshade, 1);
   BAD_R(light_twoside, 1);
   BAD_R(clamp_vertex_color, 1);
   BAD_R(clamp_fragment_color, 1);
   struct pipe_rasterizer_state lab=r;lab.half_pixel_center=1;
   CHECK(remesa_validate_rasterizer(&lab)==ENOTSUP);
   CHECK(remesa_validate_lab_rasterizer(&lab,REMESA_LAB_RASTER_QUARTER_FBO)==0);
   CHECK(remesa_validate_lab_rasterizer(NULL,REMESA_LAB_RASTER_QUARTER_FBO)==EINVAL);
   CHECK(remesa_validate_lab_rasterizer(&lab,(enum remesa_lab_raster_profile)0)==ENOTSUP);
   CHECK(remesa_validate_lab_rasterizer(&r,REMESA_LAB_RASTER_QUARTER_FBO)==ENOTSUP);
   lab.clamp_vertex_color=1;
   CHECK(remesa_validate_lab_rasterizer(&lab,REMESA_LAB_RASTER_QUARTER_FBO)==0);
   CHECK(remesa_validate_rasterizer(&lab)==ENOTSUP);
   lab.clamp_vertex_color=0;
#define BAD_L(field,value) do {struct pipe_rasterizer_state t=lab;t.field=value;CHECK(remesa_validate_lab_rasterizer(&t,REMESA_LAB_RASTER_QUARTER_FBO)==ENOTSUP);}while(0)
   BAD_L(bottom_edge_rule,1);BAD_L(cull_face,PIPE_FACE_BACK);
   BAD_L(multisample,1);BAD_L(poly_stipple_enable,1);BAD_L(depth_clamp,1);
   BAD_L(clip_halfz,1);BAD_L(rasterizer_discard,1);BAD_L(half_pixel_center,0);
   BAD_L(fill_front,PIPE_POLYGON_MODE_LINE);BAD_L(conservative_raster_mode,1);
   BAD_L(clamp_fragment_color,1);BAD_L(light_twoside,1);
#undef BAD_L
   /* Triangle-only draws have no point/line primitives or varying color. */
   r.depth_clip_near=1; r.depth_clip_far=1;
   r.front_ccw=1; r.flatshade_first=1; r.scissor=1;
   r.point_size=19; r.line_width=3; r.line_stipple_enable=1;
   r.offset_units=99; r.offset_scale=99;
   CHECK(remesa_validate_rasterizer(&r)==0);
   /* Gallium front_ccw refers to window-space orientation. These are host
    * primitive decisions only; no SGX cull or edge rule is established. */
   const float positive[3][4]={{1,2,.5f,1},{17,2,.5f,1},{1,10,.5f,1}};
   float negative[3][4];
   memcpy(negative,positive,sizeof(negative));
   for (unsigned vertex=0; vertex<3; ++vertex) negative[vertex][1]=-negative[vertex][1];
   bool discard=false;
   for (unsigned winding=0; winding<2; ++winding) {
      const float (*triangle)[4] = winding ? positive : (const float (*)[4])negative;
      for (unsigned front_ccw=0; front_ccw<2; ++front_ccw) {
         for (unsigned cull=PIPE_FACE_NONE; cull<=PIPE_FACE_FRONT_AND_BACK; ++cull) {
            bool front=(winding!=0)==(front_ccw!=0);
            bool expected=cull==PIPE_FACE_FRONT_AND_BACK ||
                          (front ? cull==PIPE_FACE_FRONT : cull==PIPE_FACE_BACK);
            CHECK(!remesa_cull_window_triangle(triangle,cull,front_ccw,&discard));
            CHECK(discard==expected);
         }
      }
   }
   struct pipe_viewport_state viewport={0};
   viewport.scale[0]=viewport.translate[0]=16;
   viewport.scale[1]=viewport.translate[1]=16;
   viewport.scale[2]=viewport.translate[2]=.5f;
   const float clip[3][4]={{-2,0,0,1},{0,-1,0,1},{0,1,0,1}};
   struct remesa_clipped_polygon polygon;
   CHECK(!remesa_prepare_constant_triangle(clip,&viewport,32,32,&polygon));
   CHECK(polygon.count==4);
   for (unsigned i=2; i<polygon.count; ++i) {
      float triangle[3][4];
      memcpy(triangle[0],polygon.window[0],sizeof(triangle[0]));
      memcpy(triangle[1],polygon.window[i-1],sizeof(triangle[0]));
      memcpy(triangle[2],polygon.window[i],sizeof(triangle[0]));
      CHECK(!remesa_cull_window_triangle((const float (*)[4])triangle,PIPE_FACE_BACK,true,&discard));
      CHECK(!discard);
   }
   viewport.scale[1]=-16;
   CHECK(!remesa_prepare_constant_triangle(clip,&viewport,32,32,&polygon));
   for (unsigned i=2; i<polygon.count; ++i) {
      float triangle[3][4];
      memcpy(triangle[0],polygon.window[0],sizeof(triangle[0]));
      memcpy(triangle[1],polygon.window[i-1],sizeof(triangle[0]));
      memcpy(triangle[2],polygon.window[i],sizeof(triangle[0]));
      CHECK(!remesa_cull_window_triangle((const float (*)[4])triangle,PIPE_FACE_BACK,true,&discard));
      CHECK(discard);
   }
   const float line[3][4]={{1,2,0,1},{17,2,0,1},{25,2,0,1}};
   CHECK(!remesa_cull_window_triangle(line,PIPE_FACE_BACK,true,&discard));
   CHECK(discard);
   const float diagonal[3][4]={{0,0,0,1},{1,1,0,1},{2,2,0,1}};
   discard=true;
   CHECK(remesa_cull_window_triangle(diagonal,PIPE_FACE_FRONT,true,&discard)==ERANGE);
   CHECK(discard); /* uncertain orientation does not mutate output */
   CHECK(!remesa_cull_window_triangle(diagonal,PIPE_FACE_NONE,true,&discard));
   CHECK(!discard);
   CHECK(!remesa_cull_window_triangle(diagonal,PIPE_FACE_FRONT_AND_BACK,true,&discard));
   CHECK(discard);
   float invalid[3][4];
   memcpy(invalid,positive,sizeof(invalid)); invalid[2][0]=NAN;
   discard=true;
   CHECK(remesa_cull_window_triangle((const float (*)[4])invalid,PIPE_FACE_BACK,true,&discard)==EINVAL);
   CHECK(discard);
   memcpy(invalid,positive,sizeof(invalid)); invalid[0][3]=2;
   CHECK(remesa_cull_window_triangle((const float (*)[4])invalid,PIPE_FACE_BACK,true,&discard)==ENOTSUP);
   CHECK(discard);
   CHECK(remesa_cull_window_triangle(positive,4,true,&discard)==EINVAL);
   CHECK(discard);
   CHECK(remesa_cull_window_triangle(NULL,PIPE_FACE_BACK,true,&discard)==EINVAL);
   CHECK(remesa_cull_window_triangle(positive,PIPE_FACE_BACK,true,NULL)==EINVAL);
   /* Binary32 extremes cannot overflow/underflow the binary64 determinant;
    * this also checks the actual i386 host rather than a size_t-only model. */
   const float large[3][4]={{0,0,0,1},{FLT_MAX,0,0,1},{0,FLT_MAX,0,1}};
   const float tiny[3][4]={{0,0,0,1},{FLT_TRUE_MIN,0,0,1},{0,FLT_TRUE_MIN,0,1}};
   CHECK(!remesa_cull_window_triangle(large,PIPE_FACE_BACK,true,&discard));
   CHECK(!discard);
   CHECK(!remesa_cull_window_triangle(tiny,PIPE_FACE_BACK,true,&discard));
   CHECK(!discard);
   printf("fixed state: %u admission cases PASS; no GPU execution\n", cases);
   return 0;
}
