/* SPDX-License-Identifier: MIT */
#ifndef REMESA_FIXED_STATE_H
#define REMESA_FIXED_STATE_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
struct pipe_viewport_state;
struct pipe_scissor_state;
struct pipe_blend_state;
struct pipe_depth_stencil_alpha_state;
struct pipe_rasterizer_state;
union pipe_color_union;
/* Pure compatibility admission for the unchanged restricted triangle profile.
 * No state bytes, ISA encoding or general OpenGL support are generated.
 * Return 0, EINVAL for NULL, or ENOTSUP for unsupported state. */
int remesa_validate_blend(const struct pipe_blend_state *);
int remesa_validate_dsa(const struct pipe_depth_stencil_alpha_state *);
int remesa_validate_rasterizer(const struct pipe_rasterizer_state *);
/* Opt-in finite profile verified only for the exact green quarter-window
 * triangle and unchanged retained carrier. Does not establish general edge
 * rules, expose GL capabilities or select a kernel admission profile. */
enum remesa_lab_raster_profile { REMESA_LAB_RASTER_QUARTER_FBO = 1 };
int remesa_validate_lab_rasterizer(const struct pipe_rasterizer_state *,
                                  enum remesa_lab_raster_profile);
/* Recording backend only. Requires a separate geometric proof for EVERY
 * submitted triangle; not a physical raster profile or GPU edge-rule encoder. */
int remesa_validate_recording_window_rasterizer(const struct pipe_rasterizer_state *);
/* Sufficient, conservative proof on a <=32x32 half-pixel sample grid. Exact
 * quarter-pixel coordinates permit integer edge tests without rounding bias.
 * Returns ENOTSUP for a tie/degeneracy/out-of-profile extent, ERANGE for a
 * non-quarter coordinate, EINVAL for malformed/nonfinite positions. Zero
 * means edge inclusion cannot affect any sample for this triangle. This
 * neither rasterizes pixels nor proves a GPU sample position. */
int remesa_prove_half_pixel_edge_independence(const float position[3][4],
                                             uint32_t width,uint32_t height);
/* Mesa identity is POSITIVE_X/Y/Z/W. Keep the legacy zero-initialized
 * profile accepted as before; reject every other component mapping. */
bool remesa_viewport_swizzle_is_identity(const struct pipe_viewport_state *);
/* CPU geometry preparation for constant fragment output. This performs no
 * rasterization, interpolation of varyings, command encoding or GPU admission.
 * Six homogeneous clip planes can turn a triangle into at most nine vertices.
 * Perspective division is host arithmetic, not evidence of a GPU shader.
 * Output is unchanged on failure. The caller must independently admit the
 * resulting viewport, edge rules, attachment and commands for its backend. */
#define REMESA_CLIPPED_MAX_VERTICES 9u
struct remesa_clipped_polygon {
   unsigned count;
   float window[REMESA_CLIPPED_MAX_VERTICES][4];
};
int remesa_prepare_constant_triangle(const float position[3][4],
                                    const struct pipe_viewport_state *,
                                    uint32_t width, uint32_t height,
                                    struct remesa_clipped_polygon *);
/* Intersect a well-formed Gallium scissor with attachment bounds. Empty
 * intersections are valid. This does not establish SGX scissor support. */
int remesa_clip_scissor(const struct pipe_scissor_state *, uint32_t width,
                        uint32_t height, struct pipe_scissor_state *);
/* CPU primitive rejection for filled, constant-color triangles. Facing is
 * determined AFTER viewport transformation: positive window-space area is
 * counter-clockwise, including any Y-origin reversal. This emits no SGX cull
 * state and establishes neither GPU winding nor pixel coverage rules.
 * Inputs must be finite window-space vec4 values with w=1. Numerically
 * ambiguous orientation fails closed with ERANGE; output is unchanged on
 * failure. No varying interpolation or rasterization is performed. */
int remesa_cull_window_triangle(const float position[3][4], unsigned cull_face,
                                bool front_ccw, bool *discard);
/* Checked CPU-transfer clear preparation for RGBA8/BGRA8 storage. This does
 * not map, synchronize or write a resource and is not a GPU clear. The caller
 * must authenticate the writer fence and hold the mapping lease before using
 * the returned rows. Row padding is excluded. Output is unchanged on failure. */
struct remesa_color_clear {
   size_t offset, row_bytes, stride;
   uint32_t rows;
   unsigned char pixel[4];
};
int remesa_prepare_color_clear(unsigned format, uint32_t attachment_width,
                               uint32_t attachment_height, uint32_t stride,
                               size_t backing_bytes,
                               const union pipe_color_union *,
                               uint32_t x, uint32_t y,
                               uint32_t width, uint32_t height,
                               struct remesa_color_clear *);
/* Private canonical gears state. Creation may admit disabled defaults; the
 * whole-frame draw path must separately require depth LESS/write and BACK cull. */
int remesa_validate_gears_dsa(const struct pipe_depth_stencil_alpha_state *);
int remesa_validate_gears_rasterizer(const struct pipe_rasterizer_state *);
#endif
