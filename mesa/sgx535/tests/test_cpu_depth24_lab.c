/* SPDX-License-Identifier: MIT; CPU attachment/ownership tests, no rasterizer. */
#include "remesa_screen.h"
#include "remesa_device.h"
#include "remesa_resource_ref.h"
#include "remesa_depth_cpu.h"
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
static unsigned allocations,maps,submits;
static int alloc(void*p,size_t n,void**o){(void)p;*o=calloc(1,n);if(*o)allocations++;return *o?0:ENOMEM;}
static void drop(void*p,void*b){(void)p;free(b);allocations--;}
static int map(void*p,void*b,bool w,void**o){(void)p;(void)w;maps++;*o=b;return 0;}
static void unmap(void*p,void*b,bool w){(void)p;(void)b;(void)w;}
static int submit(void*p,void*b,const struct remesa_draw_packet*q,void**f){(void)p;(void)b;(void)q;*f=NULL;submits++;return ENOTSUP;}
static int wait(void*p,void*f,uint64_t t,bool v){(void)p;(void)f;(void)t;(void)v;return ENOTSUP;}
static void destroy_f(void*p,void*f){(void)p;(void)f;abort();}
static const struct remesa_device_ops ops={.allocate=alloc,.destroy_bo=drop,.map=map,.unmap=unmap,.submit=submit,.wait=wait,.destroy_fence=destroy_f};
static struct pipe_resource *resource(struct pipe_screen*s,unsigned format,unsigned bind,unsigned w,unsigned h){struct pipe_resource r={.target=PIPE_TEXTURE_RECT,.format=format,.width0=w,.height0=h,.depth0=1,.array_size=1,.bind=bind};return s->resource_create(s,&r);}
static uint32_t read_z(struct pipe_context*c,struct pipe_resource*z,unsigned x,unsigned y){struct pipe_transfer*t=NULL;struct pipe_box b={.x=x,.y=y,.width=1,.height=1,.depth=1};uint32_t out=0;void*p=c->texture_map(c,z,0,PIPE_MAP_READ,&b,&t);assert(p&&t);memcpy(&out,p,4);c->texture_unmap(c,t);return out;}
int main(void){
 struct remesa_device dev={.ops=&ops,.offline_test_backend=true};struct pipe_screen*s=remesa_screen_create(&dev);assert(s);
 assert(!s->is_format_supported(s,PIPE_FORMAT_Z24X8_UNORM,PIPE_TEXTURE_RECT,0,0,PIPE_BIND_DEPTH_STENCIL));assert(s->caps.max_texture_2d_size==32);
 assert(!resource(s,PIPE_FORMAT_Z24X8_UNORM,PIPE_BIND_DEPTH_STENCIL,32,32));
 assert(!remesa_screen_enable_cpu_depth24_lab(s));assert(s->caps.max_texture_2d_size==4096);
 assert(s->is_format_supported(s,PIPE_FORMAT_Z24X8_UNORM,PIPE_TEXTURE_RECT,0,0,PIPE_BIND_DEPTH_STENCIL));
 assert(!s->is_format_supported(s,PIPE_FORMAT_Z24_UNORM_S8_UINT,PIPE_TEXTURE_RECT,0,0,PIPE_BIND_DEPTH_STENCIL));assert(!s->is_format_supported(s,PIPE_FORMAT_Z24X8_UNORM,PIPE_TEXTURE_RECT,2,2,PIPE_BIND_DEPTH_STENCIL));assert(!s->is_format_supported(s,PIPE_FORMAT_Z24X8_UNORM,PIPE_TEXTURE_RECT,0,0,PIPE_BIND_RENDER_TARGET));
 struct pipe_context*c=s->context_create(s,NULL,0);assert(c);assert(remesa_screen_enable_cpu_depth24_lab(s)==EBUSY);
 struct pipe_resource*color=resource(s,PIPE_FORMAT_B8G8R8A8_UNORM,PIPE_BIND_RENDER_TARGET,1280,800),*z=resource(s,PIPE_FORMAT_Z24X8_UNORM,PIPE_BIND_DEPTH_STENCIL,1280,800);assert(color&&z);
 struct pipe_surface st={.format=PIPE_FORMAT_Z24X8_UNORM};struct pipe_surface*surf=c->create_surface(c,z,&st);assert(surf&&surf->texture==z);c->surface_destroy(c,surf);
 st.format=PIPE_FORMAT_B8G8R8A8_UNORM;assert(!c->create_surface(c,z,&st));
 struct pipe_framebuffer_state f={.width=1280,.height=800,.nr_cbufs=1};f.cbufs[0].format=color->format;f.cbufs[0].texture=color;f.zsbuf.format=z->format;f.zsbuf.texture=z;c->set_framebuffer_state(c,&f);assert(!remesa_context_error(c));
 union pipe_color_union rgba={.f={0,1,0,1}};c->clear(c,PIPE_CLEAR_COLOR0|PIPE_CLEAR_DEPTH,NULL,&rgba,.5,0);assert(!remesa_context_error(c));assert(read_z(c,z,0,0)==0x00800000&&read_z(c,z,1279,799)==0x00800000);
 struct pipe_scissor_state box={.minx=3,.miny=5,.maxx=10,.maxy=14};c->clear(c,PIPE_CLEAR_DEPTH,&box,NULL,1,0);assert(!remesa_context_error(c));assert(read_z(c,z,3,5)==0x00ffffff&&read_z(c,z,9,13)==0x00ffffff&&read_z(c,z,2,5)==0x00800000);
 unsigned before=maps;c->clear(c,PIPE_CLEAR_DEPTH,NULL,NULL,NAN,0);assert(remesa_context_error(c)==EINVAL&&maps==before);c->clear(c,PIPE_CLEAR_STENCIL,NULL,NULL,1,0);assert(remesa_context_error(c)==ENOTSUP&&maps==before);
 struct pipe_draw_info info={.mode=4,.instance_count=1};struct pipe_draw_start_count_bias draw={.count=3};c->draw_vbo(c,&info,0,NULL,&draw,1);assert(remesa_context_error(c)==ENOTSUP&&maps==before&&!submits);
 f.zsbuf.format=PIPE_FORMAT_Z24_UNORM_S8_UINT;c->set_framebuffer_state(c,&f);assert(remesa_context_error(c)==ENOTSUP);f.zsbuf.format=z->format;c->set_framebuffer_state(c,&f);assert(!remesa_context_error(c));
 remesa_resource_reference(&color,NULL);remesa_resource_reference(&z,NULL);struct pipe_framebuffer_state empty={0};c->set_framebuffer_state(c,&empty);assert(!remesa_context_error(c));c->destroy(c);s->destroy(s);assert(!allocations&&!submits);
 dev.offline_test_backend=false;dev.qualified_hardware_backend=true;s=remesa_screen_create(&dev);assert(s&&remesa_screen_enable_cpu_depth24_lab(s)==ENOTSUP);s->destroy(s);
 struct remesa_color_clear out,bak;memset(&out,0xa5,sizeof(out));bak=out;
 assert(remesa_prepare_depth24_clear(PIPE_FORMAT_Z24X8_UNORM,32,32,128,4096,INFINITY,0,0,32,32,&out)==EINVAL&&!memcmp(&out,&bak,sizeof(out)));
 assert(remesa_prepare_depth24_clear(PIPE_FORMAT_Z24_UNORM_S8_UINT,32,32,128,4096,1,0,0,32,32,&out)==ENOTSUP&&!memcmp(&out,&bak,sizeof(out)));
 assert(remesa_prepare_depth24_clear(PIPE_FORMAT_Z24X8_UNORM,32,32,128,4095,1,0,0,32,32,&out)==ENOSPC&&!memcmp(&out,&bak,sizeof(out)));
 puts("PASS explicit CPU-depth24 real1280attachment/clear/readback/ownership; defaults unchanged; depthdraw no maps/submits");return 0;
}
