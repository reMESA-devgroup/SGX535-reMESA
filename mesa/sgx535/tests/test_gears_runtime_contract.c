/* SPDX-License-Identifier: MIT
 * Private enablement and fixed-state rejection contracts, CPU only. */
#include "remesa_screen.h"
#include "remesa_device.h"
#include "remesa_gears_runtime.h"
#include "remesa_fixed_state.h"
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include <errno.h>
#include <assert.h>
#include <stdio.h>
static unsigned calls;
static int alloc(void*p,size_t n,void**o){(void)p;(void)n;*o=NULL;calls++;return ENOTSUP;}
static void drop(void*p,void*o){(void)p;(void)o;calls++;}
static int map(void*p,void*o,bool w,void**v){(void)p;(void)o;(void)w;*v=NULL;calls++;return ENOTSUP;}
static void unmap(void*p,void*o,bool w){(void)p;(void)o;(void)w;calls++;}
static int submit(void*p,void*o,const struct remesa_draw_packet*q,void**f){(void)p;(void)o;(void)q;*f=NULL;calls++;return ENOTSUP;}
static int wait(void*p,void*o,uint64_t n,bool v){(void)p;(void)o;(void)n;(void)v;calls++;return ENOTSUP;}
static int queue(void*p,void*o,const struct remesa_gears_runtime_draw*d){(void)p;(void)o;(void)d;calls++;return ENOTSUP;}
static int flush(void*p,void*o,void**f){(void)p;(void)o;*f=NULL;calls++;return ENOTSUP;}
static int cancel(void*p,void*o){(void)p;(void)o;calls++;return 0;}
static const struct remesa_device_ops ops={.allocate=alloc,.destroy_bo=drop,.map=map,.unmap=unmap,.submit=submit,.wait=wait,.destroy_fence=drop};
static const struct remesa_gears_runtime_ops gears={queue,flush,cancel};
int main(void){
 struct remesa_device d={.ops=&ops,.offline_test_backend=true};
 struct pipe_screen*s=remesa_screen_create(&d);assert(s);assert(remesa_screen_enable_gears_runtime(s)==ENOTSUP);s->destroy(s);
 d.gears_runtime=&gears;s=remesa_screen_create(&d);assert(s);assert(!remesa_screen_enable_gears_runtime(s));
 struct pipe_context*c=s->context_create(s,NULL,0);assert(c);assert(remesa_screen_enable_gears_runtime(s)==EBUSY);c->destroy(c);s->destroy(s);
 d.offline_test_backend=false;d.qualified_hardware_backend=true;s=remesa_screen_create(&d);assert(s);assert(!remesa_screen_enable_gears_runtime(s));s->destroy(s);
 struct pipe_depth_stencil_alpha_state z={0};assert(!remesa_validate_gears_dsa(&z));z.depth_enabled=z.depth_writemask=1;z.depth_func=PIPE_FUNC_LESS;assert(!remesa_validate_gears_dsa(&z));assert(remesa_validate_dsa(&z)==ENOTSUP);
 for(unsigned f=0;f<8;f++){z.depth_func=f;assert(remesa_validate_gears_dsa(&z)==(f==PIPE_FUNC_LESS?0:ENOTSUP));}z.depth_func=PIPE_FUNC_LESS;
 z.depth_writemask=0;assert(remesa_validate_gears_dsa(&z)==ENOTSUP);z.depth_writemask=1;
 #define ZBAD(field) do{z.field=1;assert(remesa_validate_gears_dsa(&z)==ENOTSUP);z.field=0;}while(0)
 ZBAD(depth_bounds_test);ZBAD(alpha_enabled);ZBAD(stencil[0].enabled);ZBAD(stencil[1].enabled);
 struct pipe_rasterizer_state r={0};r.fill_front=r.fill_back=PIPE_POLYGON_MODE_FILL;r.half_pixel_center=r.bottom_edge_rule=1;r.cull_face=PIPE_FACE_BACK;
 assert(!remesa_validate_gears_rasterizer(&r));assert(remesa_validate_rasterizer(&r)==ENOTSUP);
 #define RBAD(field) do{r.field=1;assert(remesa_validate_gears_rasterizer(&r)==ENOTSUP);r.field=0;}while(0)
 RBAD(light_twoside);RBAD(offset_tri);RBAD(poly_smooth);RBAD(poly_stipple_enable);RBAD(multisample);RBAD(force_persample_interp);RBAD(rasterizer_discard);RBAD(depth_clamp);RBAD(clip_halfz);RBAD(clip_plane_enable);RBAD(subpixel_precision_x);RBAD(subpixel_precision_y);
 r.cull_face=PIPE_FACE_FRONT;assert(remesa_validate_gears_rasterizer(&r)==ENOTSUP);r.cull_face=PIPE_FACE_BACK;
 r.half_pixel_center=0;assert(remesa_validate_gears_rasterizer(&r)==ENOTSUP);r.half_pixel_center=1;r.bottom_edge_rule=0;assert(!remesa_validate_gears_rasterizer(&r)); /* Pairing with viewport/front is checked at draw time. */
 assert(calls==0);puts("CPU private gears enable/fixed-state contracts PASS; legacy rejects depth/cull/window; device callbacks=0 GPU0");return 0;
}
