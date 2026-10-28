/* SPDX-License-Identifier: MIT. Actual owner/Gallium, injected GPU0. */
#define REMESA_LINKED_BACKGROUND_FIXTURE
#define REMESA_LINKED_TEST_BYTES (32u*1024u*1024u)
#include "test_linked_background.c"
#include "remesa_screen.h"
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"
static unsigned calls;static int fill_error;static struct remesa_gpu_fill last;
static int gpu_fill(void *priv,const struct remesa_gpu_fill *f)
{
 struct fixture *test=priv;CHECK(get(test,f->destination.id)->pinned&&!get(test,f->destination.id)->lease);
 calls++;last=*f;return fill_error; /* No pixel rendering in this fixture. */
}
static struct pipe_resource *image(struct pipe_screen *s,unsigned w,unsigned h)
{struct pipe_resource t={.target=PIPE_TEXTURE_2D,.format=PIPE_FORMAT_B8G8R8A8_UNORM,.width0=w,.height0=h,.depth0=1,.array_size=1,.bind=PIPE_BIND_RENDER_TARGET};return s->resource_create(s,&t);}
static void drop_resource(struct pipe_resource *r)
{if(!--r->reference.count)r->screen->resource_destroy(r->screen,r);}
int main(void)
{
 const unsigned sizes[][2]={{1280,800},{640,640},{641,479}};
 for(unsigned v=0;v<4;v++){
  unsigned w=sizes[v%3][0],h=sizes[v%3][1];struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  target_ops=ops;target_ops.create=target_create;
  struct remesa_persistent_config cfg={.ops=&target_ops,.priv=&f,.context=5200+v,
   .va_start=0x40001000,.va_end=0x42000000,.byte_budget=32u*1024u*1024u,
   .resource_only_backend=true,.resource_limits={4096,4096,4,16u*1024u*1024u}};
  CHECK(!remesa_persistent_open(&d,&cfg));CHECK(!remesa_persistent_set_native_fill(&d,gpu_fill));
  CHECK(remesa_persistent_set_native_fill(&d,gpu_fill)==EBUSY);
  struct pipe_screen *s=remesa_screen_create(&d);CHECK(s);CHECK(!remesa_screen_enable_native_fill(s,remesa_persistent_fill_region));
  struct pipe_context *c=s->context_create(s,NULL,0);CHECK(c);CHECK(remesa_screen_enable_native_fill(s,remesa_persistent_fill_region)==EBUSY);
  struct pipe_resource *a=image(s,w+2,h+4);CHECK(a);
  struct pipe_box all={.width=w+2,.height=h+4,.depth=1};struct pipe_transfer *t=NULL;
  unsigned char *p=c->texture_map(c,a,0,PIPE_MAP_WRITE,&all,&t);CHECK(p);memset(p,0xa5,t->layer_stride);c->texture_unmap(c,t);
  struct pipe_box box={.x=1,.y=1,.width=w,.height=h,.depth=1};unsigned maps=f.cpu_maps;calls=0;fill_error=0;
  const unsigned char pixel[4]={0x56,0x34,0x12,0x7f};
  c->clear_texture(c,a,0,&box,pixel);
  CHECK(!remesa_context_error(c)&&calls==1&&!remesa_context_cpu_clear_count(c)&&remesa_context_native_fill_count(c)==1);
  CHECK(f.cpu_maps==maps&&last.width==w&&last.height==h&&last.destination_x==1&&last.destination_y==1&&last.destination.stride==(w+2)*4&&last.color==0x7f123456);
  CHECK(get(&f,last.destination.id)->bytes[0]==0xa5); /* No fake rendering claim. */
  struct pipe_surface tmpl={.format=a->format};struct pipe_surface *surface=c->create_surface(c,a,&tmpl);CHECK(surface);
  const union pipe_color_union color={.f={1,0,0,1}};
  c->clear_render_target(c,surface,&color,1,1,w,h,false);
  CHECK(!remesa_context_error(c)&&calls==2&&last.color==0xffff0000u&&f.cpu_maps==maps);
  struct pipe_framebuffer_state fb={.width=w+2,.height=h+4,.nr_cbufs=1,.cbufs={{.texture=a,.format=a->format}}};
  c->set_framebuffer_state(c,&fb);struct pipe_scissor_state scissor={.minx=1,.miny=1,.maxx=w+1,.maxy=h+1};
  c->clear(c,PIPE_CLEAR_COLOR0,&scissor,&color,0,0);
  CHECK(!remesa_context_error(c)&&calls==3&&remesa_context_native_fill_count(c)==3&&f.cpu_maps==maps);
  c->clear(c,PIPE_CLEAR_COLOR0|PIPE_CLEAR_DEPTH,NULL,&color,1,0);CHECK(remesa_context_error(c)==ENOTSUP&&calls==3);
  p=c->texture_map(c,a,0,PIPE_MAP_READ,&all,&t);CHECK(p);maps=f.cpu_maps;
  c->clear_texture(c,a,0,&box,pixel);CHECK(remesa_context_error(c)==EBUSY&&calls==3&&f.cpu_maps==maps);c->texture_unmap(c,t);
  struct pipe_box bad=box;bad.width=8;c->clear_texture(c,a,0,&bad,pixel);CHECK(remesa_context_error(c)==ENOTSUP&&calls==3);
  fill_error=EINVAL;c->clear_texture(c,a,0,&box,pixel);CHECK(remesa_context_error(c)==EINVAL&&calls==4&&f.cpu_maps==maps&&remesa_context_native_fill_count(c)==3);
  fill_error=0;c->clear_texture(c,a,0,&box,pixel);CHECK(!remesa_context_error(c)&&calls==5&&remesa_context_native_fill_count(c)==4);
  if(v==3){fill_error=EOWNERDEAD;c->clear_texture(c,a,0,&box,pixel);CHECK(remesa_context_error(c)==EOWNERDEAD&&calls==6);
   CHECK(!c->texture_map(c,a,0,PIPE_MAP_READ,&all,&t)&&remesa_context_error(c)==EOWNERDEAD);}
  struct pipe_framebuffer_state empty={0};c->set_framebuffer_state(c,&empty);if(!--surface->reference.count)c->surface_destroy(c,surface);
  c->destroy(c);drop_resource(a);s->destroy(s);
  if(v==3)CHECK(!f.destroys&&!f.unpins&&remesa_persistent_close(&d)==EBUSY);
  else CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!remesa_persistent_close(&d));
  CHECK(!f.forbidden);
 }
 puts("PASS native fill owner + Gallium texture/surface/framebuffer clears: three sizes, packed color, no CPU maps/fallback, cancellation and terminal quarantine; injected GPU0/API0");return 0;
}
