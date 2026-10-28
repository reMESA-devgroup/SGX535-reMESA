/* SPDX-License-Identifier: MIT. Real owner/Gallium callbacks, injected GPU0. */
#define REMESA_LINKED_BACKGROUND_FIXTURE
#define REMESA_LINKED_TEST_BYTES (32u*1024u*1024u)
#include "test_linked_background.c"
#include "remesa_screen.h"
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"
static unsigned copy_calls;static int copy_error;static struct remesa_gpu_copy last;
static int gpu_copy(void *priv,const struct remesa_gpu_copy *r)
{
 struct fixture *f=priv;CHECK(get(f,r->source.id)->pinned&&get(f,r->destination.id)->pinned);
 CHECK(!get(f,r->source.id)->lease&&!get(f,r->destination.id)->lease);
 copy_calls++;last=*r;return copy_error; /* No pixels produced by this fixture. */
}
static struct pipe_resource *image(struct pipe_screen *s,unsigned w,unsigned h)
{struct pipe_resource t={.target=PIPE_TEXTURE_2D,.format=PIPE_FORMAT_B8G8R8A8_UNORM,.width0=w,.height0=h,.depth0=1,.array_size=1};return s->resource_create(s,&t);}
static void release_resource(struct pipe_resource *r)
{if(!--r->reference.count)r->screen->resource_destroy(r->screen,r);}
int main(void)
{
 const unsigned sizes[][2]={{1280,800},{640,640},{641,479}};
 for(unsigned v=0;v<4;v++){
  unsigned w=sizes[v%3][0],h=sizes[v%3][1];struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  target_ops=ops;target_ops.create=target_create;
  struct remesa_persistent_config cfg={.ops=&target_ops,.priv=&f,.context=5100+v,
   .va_start=0x40001000,.va_end=0x42000000,.byte_budget=32u*1024u*1024u,
   .resource_only_backend=true,.resource_limits={4096,4096,4,16u*1024u*1024u}};
  CHECK(!remesa_persistent_open(&d,&cfg));CHECK(!remesa_persistent_set_native_copy(&d,gpu_copy));
  CHECK(remesa_persistent_set_native_copy(&d,gpu_copy)==EBUSY);
  struct pipe_screen *s=remesa_screen_create(&d);CHECK(s);
  CHECK(!remesa_screen_enable_native_copy(s,remesa_persistent_copy_region));
  struct pipe_context *c=s->context_create(s,NULL,0);CHECK(c);
  CHECK(remesa_screen_enable_native_copy(s,remesa_persistent_copy_region)==EBUSY);
  struct pipe_resource *a=image(s,w+2,h+4),*b=image(s,w+2,h+4);CHECK(a&&b);
  struct pipe_box all={.width=w+2,.height=h+4,.depth=1};struct pipe_transfer *t=NULL;
  unsigned char *p=c->texture_map(c,a,0,PIPE_MAP_WRITE,&all,&t);CHECK(p);memset(p,0x5a,t->layer_stride);c->texture_unmap(c,t);
  p=c->texture_map(c,b,0,PIPE_MAP_WRITE,&all,&t);CHECK(p);memset(p,0xa5,t->layer_stride);c->texture_unmap(c,t);
  struct pipe_box box={.x=1,.y=1,.width=w,.height=h,.depth=1};unsigned maps=f.cpu_maps;copy_calls=0;copy_error=0;
  c->resource_copy_region(c,b,0,1,1,0,a,0,&box);
  CHECK(!remesa_context_error(c)&&copy_calls==1&&!remesa_context_cpu_copy_count(c)&&remesa_context_native_copy_count(c)==1);
  CHECK(f.cpu_maps==maps&&last.width==w&&last.height==h&&last.source_x==1&&last.destination_y==1&&last.source.stride==(w+2)*4);
  CHECK(get(&f,last.destination.id)->bytes[0]==0xa5); /* No mocked rendering. */
  c->resource_copy_region(c,a,0,1,1,0,a,0,&box);CHECK(remesa_context_error(c)==EOPNOTSUPP&&copy_calls==1&&f.cpu_maps==maps);
  p=c->texture_map(c,a,0,PIPE_MAP_READ,&all,&t);CHECK(p);maps=f.cpu_maps;
  c->resource_copy_region(c,b,0,1,1,0,a,0,&box);CHECK(remesa_context_error(c)==EBUSY&&copy_calls==1&&f.cpu_maps==maps);c->texture_unmap(c,t);
  copy_error=EINVAL;c->resource_copy_region(c,b,0,1,1,0,a,0,&box);CHECK(remesa_context_error(c)==EINVAL&&copy_calls==2&&f.cpu_maps==maps&&remesa_context_native_copy_count(c)==1);
  copy_error=0;c->resource_copy_region(c,b,0,1,1,0,a,0,&box);CHECK(!remesa_context_error(c)&&copy_calls==3&&remesa_context_native_copy_count(c)==2);
  if(v==3){
   copy_error=EOWNERDEAD;c->resource_copy_region(c,b,0,1,1,0,a,0,&box);CHECK(remesa_context_error(c)==EOWNERDEAD&&copy_calls==4);
   CHECK(!c->texture_map(c,a,0,PIPE_MAP_READ,&all,&t)&&remesa_context_error(c)==EOWNERDEAD);
   CHECK(!c->texture_map(c,b,0,PIPE_MAP_WRITE,&all,&t)&&remesa_context_error(c)==EOWNERDEAD);
  }
  c->destroy(c);release_resource(a);release_resource(b);s->destroy(s);
  if(v==3)CHECK(f.destroys==0&&f.unpins==0&&remesa_persistent_close(&d)==EBUSY);
  else CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!remesa_persistent_close(&d));
  CHECK(!f.forbidden);
 }
 puts("PASS native-copy owner + Gallium: three storage sizes, no CPU fallback/maps, owned IDs/stride, self/lease refusal, known-unissued cancellation/retry, uncertain two-BO quarantine; injected GPU0/API0");return 0;
}
