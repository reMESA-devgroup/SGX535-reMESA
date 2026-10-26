/* SPDX-License-Identifier: MIT */
/* Truthful Gallium capability contract; no OpenGL or SGX execution. */
#include "remesa_screen.h"
#include "remesa_device.h"
#include "pipe/p_screen.h"
#include <errno.h>
#include <assert.h>
#include <stdio.h>
static int alloc(void *p,size_t n,void **out) { (void)p;(void)n;(void)out;return ENOTSUP; }
static void drop(void *p,void *o) { (void)p;(void)o; }
static int map_bo(void *p,void *o,bool w,void **out) { (void)p;(void)o;(void)w;(void)out;return ENOTSUP; }
static void unmap_bo(void *p,void *o,bool w) { (void)p;(void)o;(void)w; }
static int submit(void *p,void *o,const struct remesa_draw_packet *q,void **f) { (void)p;(void)o;(void)q;(void)f;return ENOTSUP; }
static int wait_fence(void *p,void *o,uint64_t n,bool v) { (void)p;(void)o;(void)n;(void)v;return ENOTSUP; }
static void destroy_fence(void *p,void *o) { (void)p;(void)o; }

int main(void)
{
   struct remesa_device device = {0};
   struct pipe_screen *screen;

   static const struct remesa_device_ops ops = {.allocate=alloc,.destroy_bo=drop,.map=map_bo,.unmap=unmap_bo,.submit=submit,.wait=wait_fence,.destroy_fence=destroy_fence};
   device.ops = &ops;
   device.offline_test_backend = true;
   screen = remesa_screen_create(&device);
   assert(screen);

   /* These are the only screen capabilities this backend can currently
    * defend: one 32x32 BGRA render target and triangle primitives. */
   assert(screen->caps.graphics);
   assert(!screen->caps.accelerated);
   assert(screen->caps.invalidate_buffer);
   assert(screen->caps.clear_scissored); /* bounded CPU clear callback */
   assert(screen->caps.max_render_targets == 1);
   assert(screen->caps.max_texture_2d_size == 32);
   /* Two owned raw input bindings; complete gears captures are CPU snapshots.
    * Legacy GPU draws still reject a second buffer or element. */
   assert(screen->caps.max_vertex_buffers == 2);
   assert(screen->caps.max_vertex_attrib_stride == 2048);
   assert(screen->caps.gl_begin_end_buffer_size == 512*1024);
   assert(screen->caps.supported_prim_modes & (1u << MESA_PRIM_TRIANGLES));
   assert(!screen->caps.npot_textures);
   assert(!screen->caps.texture_multisample);
   assert(!screen->caps.max_varyings);
   assert(!screen->caps.fs_coord_pixel_center_half_integer);
   assert(!screen->caps.fs_coord_origin_upper_left);
#ifdef REMESA_WITH_NIR
   assert(screen->shader_caps[PIPE_SHADER_VERTEX].supported_irs & (1u << PIPE_SHADER_IR_NIR));
   assert(screen->shader_caps[PIPE_SHADER_FRAGMENT].supported_irs & (1u << PIPE_SHADER_IR_NIR));
   assert(screen->shader_caps[PIPE_SHADER_VERTEX].max_instructions == 1);
   assert(screen->shader_caps[PIPE_SHADER_VERTEX].max_inputs == 1);
   assert(screen->shader_caps[PIPE_SHADER_FRAGMENT].max_inputs == 0);
#else
   assert(!screen->shader_caps[PIPE_SHADER_VERTEX].supported_irs);
   assert(!screen->shader_caps[PIPE_SHADER_FRAGMENT].supported_irs);
#endif

   screen->destroy(screen);
   device.offline_test_backend=false;device.resource_only_backend=true;
   screen=remesa_screen_create(&device);assert(screen&&!screen->caps.graphics&&!screen->caps.accelerated);
   assert(!screen->shader_caps[PIPE_SHADER_VERTEX].supported_irs&&!screen->shader_caps[PIPE_SHADER_FRAGMENT].supported_irs);
   screen->destroy(screen);
   puts("PASS: truthful restricted Gallium screen and shader capability contract");
   return 0;
}
