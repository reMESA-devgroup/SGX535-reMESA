/* SPDX-License-Identifier: MIT */
#ifndef REMESA_OPENGL_LAB_CONTEXT_H
#define REMESA_OPENGL_LAB_CONTEXT_H
#include <stdint.h>
#include <stddef.h>
struct pipe_screen;
struct pipe_resource;
/* Explicit diagnostic state_tracker frontend; no DRI admission or cap override.
 * Caller retains both objects. The COLOR resource must be owned32x32 BGRA8.
 * This clears that resource through real Mesa GL, then verifies its CPU map.
 * Any underlying device callbacks belong to the caller's selected backend.
 * It does not qualify GL compliance, shader acceleration or GPU completion. */
struct remesa_opengl_lab_report {
 unsigned context_created,computed_version,glsl_version;
 unsigned gl_error,pipe_error,green_pixels;
 unsigned gl21_created,gl21_error,gl21_bad_version;
 unsigned shader_object,shader_compiled,shader_error;
 unsigned triangle_called,triangle_gl_error,triangle_pipe_error;
 unsigned vs_attempts,vs_error,fs_attempts,fs_error;
 unsigned es_fs_compiled,es_vs_compiled,program_linked,program_gl_error;
 unsigned program_draw_called,program_draw_gl_error,program_draw_pipe_error,program_draw_indexed;
 unsigned program_vs_attempts,program_vs_error,program_fs_attempts,program_fs_error;
 unsigned program_vs_bytes,program_fs_bytes;
 unsigned readback_called,readback_error,readback_bytes,readback_green,readback_zero,readback_other;
 unsigned fbo_created,fbo_status,fbo_gl_error;
 unsigned internal_target,fbo_width,fbo_height,fbo_internal_format,fbo_samples;
 unsigned fbo_samples_query_supported;
 char version_string[128],shader_log[512],program_log[512];
};
int remesa_opengl_lab_clear(struct pipe_screen *screen,struct pipe_resource *color,
 struct remesa_opengl_lab_report *report);
/* Same real frontend and shaders, without a preceding clear. Caller supplies
 * a fresh resource whose backend initialization is its own responsibility.
 * This does not change framebuffer load/preservation policy. The caller must
 * separately inspect draw status, completion and readback; success here only
 * means context/clear-probe setup did not fail. */
int remesa_opengl_lab_triangle(struct pipe_screen *screen,struct pipe_resource *color,
 struct remesa_opengl_lab_report *report);
/* Real GL renderbuffer/FBO creation on the supplied screen, with no clear
 * preceding its indexed draw. This probes the offscreen coordinate contract;
 * it does not relax raster-state validation or qualify a GPU result. */
int remesa_opengl_lab_fbo_triangle(struct pipe_screen *,struct pipe_resource *,
 struct remesa_opengl_lab_report *);
/* The same fresh FBO path using actual glDrawArrays, with no index binding.
 * Raster admission and the caller's backend remain authoritative. */
int remesa_opengl_lab_fbo_arrays(struct pipe_screen *,struct pipe_resource *,
 struct remesa_opengl_lab_report *);
/* Dedicated physical-lab candidate frontend: exactly one glDrawArrays into
 * a fresh complete FBO, quarter-pixel-shifted canonical triangle. No clear or
 * second draw occurs. Driver raster/backend admission remains authoritative.
 * On successful draw AND glReadPixels, commits exactly4096 BGRA bytes. A
 * rejected/failed draw leaves readback unchanged; inspect report errors.
 * report/readback must be disjoint. No submission qualification is implied. */
int remesa_opengl_lab_draw_arrays_quarter(struct pipe_screen *,struct pipe_resource *,
 struct remesa_opengl_lab_report *,void *readback,size_t capacity);
#endif
