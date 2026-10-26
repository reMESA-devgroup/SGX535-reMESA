/* SPDX-License-Identifier: MIT */
#ifndef REMESA_GEARS_RUNTIME_H
#define REMESA_GEARS_RUNTIME_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
struct remesa_shader_program;
struct remesa_gears_geometry_view;
/* Borrowed pre-VS geometry and complete immutable NIR-derived programs.
 * No host projection, lighting, clipping or rasterization is permitted.
 * Coefficients are copied from the existing typed source encoder, excluding
 * its staging self-pointer. The backend must independently encode the source
 * at the actual GPU offset and compare actual programs/geometry/state with
 * its compiled, owned profile before accepting a draw. */
struct remesa_gears_runtime_draw {
 const struct remesa_shader_program *vertex,*fragment;
 const struct remesa_gears_geometry_view *geometry;
 const unsigned char *coefficients;
 size_t coefficient_bytes;
 uint32_t width,height,stride,format;
 void *depth_target;
 uint32_t depth_stride,depth_format;
 bool flat,front_ccw,clamp_vertex_color,bottom_left_storage;
 unsigned cull_face;
};
struct remesa_gears_runtime_ops {
 /* CPU preparation only. Snapshot all borrowed data before returning. */
 int (*queue)(void *device,void *target,const struct remesa_gears_runtime_draw *);
 /* The entire canonical frame is one real GPU submission. Zero requires
  * genuine retirement, full authenticated readback and a non-NULL owned
  * hardware fence. Never return a simulated fence on a hardware device.
  * CPU-only fixtures must identify themselves as offline devices explicitly.
  * Non-NULL error fences transfer ownership and retain uncertain backing. */
 int (*flush)(void *device,void *target,void **fence);
 /* Cancel only unissued CPU preparation. Ambiguous/issued work must fail
  * closed and remain retained; this is never a reset/rearm operation. */
 int (*cancel)(void *device,void *target);
};
#endif
