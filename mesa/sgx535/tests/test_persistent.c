/* SPDX-License-Identifier: MIT */
/* GEM/MMU operations and completion receipts below are offline fixtures.
 * No registers, ioctl, CPU rasterizer or GPU-output claims. */
#include "remesa_persistent.h"
#include "remesa_screen.h"
#include "remesa_resource_layout.h"
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct gem {void *bytes;size_t length;uint64_t va;bool live,pinned,mapped,cpu;};
struct event {struct remesa_gpu_submit submitted;bool retired,published,failed,released;};
struct fixture {
 struct gem gems[128];struct event events[256];
 unsigned creates,destroys,pins,unpins,maps,unmaps,syncs,dispatches,releases;
 unsigned fail,invalid,release_retries,create_failure_at;enum remesa_dispatch dispatch_result;
};
static struct gem *gem(struct fixture *f,uint32_t h) {assert(h && h<=128 && f->gems[h-1].live);return &f->gems[h-1];}
static int create(void *data,size_t n,uint32_t *h)
{struct fixture *f=data;assert(f->creates<128);struct gem *b=&f->gems[f->creates++];b->bytes=calloc(1,n);assert(b->bytes);b->length=n;b->live=true;*h=f->creates;return f->fail==1||f->creates==f->create_failure_at?ENOMEM:0;}
static int pin(void *data,uint32_t h)
{struct fixture *f=data;struct gem *b=gem(f,h);if(f->fail==2)return ENOMEM;assert(!b->pinned);b->pinned=true;++f->pins;return 0;}
static int gpu_map(void *data,uint32_t h,uint64_t va,size_t n)
{struct fixture *f=data;struct gem *b=gem(f,h);if(f->fail==3)return ENOMEM;assert(b->pinned&&!b->mapped&&n%4096==0);
 for(unsigned i=0;i<f->creates;i++) if(f->gems[i].mapped)assert(!(va<f->gems[i].va+f->gems[i].length && f->gems[i].va<va+n));
 b->mapped=true;b->va=va;++f->maps;return 0;}
static int gpu_unmap(void *data,uint32_t h,uint64_t va,size_t n)
{struct fixture *f=data;struct gem *b=gem(f,h);(void)n;if(f->fail==4)return EOWNERDEAD;assert(b->mapped&&b->va==va);b->mapped=false;++f->unmaps;return 0;}
static int unpin(void *data,uint32_t h)
{struct fixture *f=data;struct gem *b=gem(f,h);assert(b->pinned&&!b->mapped);b->pinned=false;++f->unpins;return 0;}
static int destroy(void *data,uint32_t h)
{struct fixture *f=data;struct gem *b=gem(f,h);assert(!b->mapped&&!b->pinned&&!b->cpu);free(b->bytes);memset(b,0,sizeof(*b));++f->destroys;return 0;}
static int cpu_map(void *data,uint32_t h,bool write,void **out)
{struct fixture *f=data;struct gem *b=gem(f,h);if(f->fail==5)return ENOMEM;assert(!b->cpu);b->cpu=true;*out=b->bytes;
 if(f->fail==10&&!write)((unsigned char *)b->bytes)[0]^=1;
 return f->fail==7?EIO:0;}
static int cpu_unmap(void *data,uint32_t h,bool write)
{struct fixture *f=data;struct gem *b=gem(f,h);(void)write;assert(b->cpu);if(f->fail==8)return EOWNERDEAD;b->cpu=false;return 0;}
static int sync_device(void *data,uint32_t h)
{struct fixture *f=data;gem(f,h);++f->syncs;return f->fail==9?EIO:0;}
static enum remesa_dispatch dispatch(void *data,const struct remesa_gpu_submit *s)
{struct fixture *f=data;struct gem *b=gem(f,s->gem_handle);assert(b->mapped&&b->va==s->gpu_va);
 assert(s->operation&&s->context&&s->bytes==4096&&remesa_draw_packet_validate(s->packet)==REMESA_PACKET_OK);
 assert(f->dispatches<256);f->events[f->dispatches++].submitted=*s;return f->dispatch_result;}
static struct event *event(struct fixture *f,uint64_t op)
{for(unsigned i=0;i<f->dispatches;i++)if(f->events[i].submitted.operation==op)return &f->events[i];assert(0);return NULL;}
static int poll(void *data,uint64_t ctx,uint64_t op,uint64_t ns,struct remesa_gpu_receipt *r)
{struct fixture *f=data;struct event *e=event(f,op);(void)ns;assert(ctx==e->submitted.context);
 *r=(struct remesa_gpu_receipt){ctx,op,e->retired,e->published,e->failed};if(f->invalid==1)++r->operation;if(f->invalid==2){r->published=true;r->retired=false;}if(f->invalid==3)r->published=false;return 0;}
static int release(void *data,uint64_t ctx,uint64_t op)
{struct fixture *f=data;struct event *e=event(f,op);assert(ctx==e->submitted.context&&e->retired&&e->published&&!e->released);
 if(f->fail==6)return EIO;
 if(f->release_retries){--f->release_retries;return EAGAIN;}
 e->released=true;++f->releases;return 0;}
static const struct remesa_persistent_ops ops={create,pin,gpu_map,gpu_unmap,unpin,destroy,cpu_map,cpu_unmap,sync_device,dispatch,poll,release};
static void open_device(struct fixture *f,struct remesa_device *d,uint64_t context)
{struct remesa_persistent_config c={.ops=&ops,.priv=f,.context=context,.va_start=0x100000,.va_end=0x200000,.byte_budget=65536,.offline_test_backend=true};assert(!remesa_persistent_open(d,&c));}
static void published(struct fixture *f,unsigned i) {f->events[i].retired=f->events[i].published=true;}
static void packet(struct remesa_draw_packet *p)
{const float v[3][4]={{8,8,.5f,1},{24,8,.5f,1},{8,24,.5f,1}};const uint16_t i[]={0,1,2};assert(!remesa_draw_packet_build(p,v,3,i,3,0xffff00ff));}
static void drop(struct pipe_resource *p) {if(!--p->reference.count)p->screen->resource_destroy(p->screen,p);}
static struct pipe_resource *buffer(struct pipe_screen *s,unsigned n,unsigned bind)
{struct pipe_resource t={0};t.target=PIPE_BUFFER;t.format=PIPE_FORMAT_R8_UNORM;t.width0=n;t.height0=t.depth0=t.array_size=1;t.bind=bind;return s->resource_create(s,&t);}
static void bind(struct pipe_context *c,struct pipe_resource *v,struct pipe_resource *t)
{struct pipe_vertex_buffer vb={0};vb.buffer.resource=v;++v->reference.count;c->set_vertex_buffers(c,1,&vb);
 struct pipe_vertex_element e={.src_format=PIPE_FORMAT_R32G32B32A32_FLOAT,.src_stride=16};void *ve=c->create_vertex_elements_state(c,1,&e);assert(ve);c->bind_vertex_elements_state(c,ve);
 /* Keep the layout alive through draws. The context-owned registry tears it
  * down with this fixture context; deleting a bound layout invalidates draw. */
 struct pipe_framebuffer_state fb={0};fb.width=fb.height=32;fb.nr_cbufs=1;fb.cbufs[0].texture=t;fb.cbufs[0].format=t->format;c->set_framebuffer_state(c,&fb);assert(!remesa_bind_diagnostic_pipeline(c,0xffff00ff));}
static void draw(struct pipe_context *c,struct pipe_resource *i)
{struct pipe_draw_info info={.mode=MESA_PRIM_TRIANGLES,.instance_count=1,.index_size=2,.index.resource=i};struct pipe_draw_start_count_bias dc={.count=3};c->draw_vbo(c,&info,0,NULL,&dc,1);}
static void gallium_reuse(void)
{
 struct fixture f={.dispatch_result=REMESA_ISSUED};struct remesa_device d={0};open_device(&f,&d,101);
 struct pipe_screen *s=remesa_screen_create(&d);assert(s&&strstr(s->get_name(s),"NO SGX"));
 struct pipe_context *a=s->context_create(s,NULL,0),*b=s->context_create(s,NULL,0);
 struct pipe_resource *v=buffer(s,48,PIPE_BIND_VERTEX_BUFFER),*i=buffer(s,6,PIPE_BIND_INDEX_BUFFER);
 struct pipe_resource rt={0};rt.target=PIPE_TEXTURE_2D;rt.format=PIPE_FORMAT_B8G8R8A8_UNORM;rt.width0=rt.height0=32;rt.depth0=rt.array_size=1;rt.bind=PIPE_BIND_RENDER_TARGET;
 struct pipe_resource *t=s->resource_create(s,&rt);assert(v&&i&&t&&f.creates==3&&f.maps==3);
 const float vert[3][4]={{8,8,.5f,1},{24,8,.5f,1},{8,24,.5f,1}};const uint16_t ix[]={0,1,2};struct pipe_transfer *tr=NULL;struct pipe_box box={.width=48,.height=1,.depth=1};
 void *p=a->buffer_map(a,v,0,PIPE_MAP_WRITE,&box,&tr);assert(p);memcpy(p,vert,48);a->buffer_unmap(a,tr);
 box.width=6;p=a->buffer_map(a,i,0,PIPE_MAP_WRITE,&box,&tr);assert(p);memcpy(p,ix,6);a->buffer_unmap(a,tr);bind(a,v,t);bind(b,v,t);
 draw(a,i);assert(!remesa_context_error(a));a->destroy(a);
 box.width=box.height=32;assert(!b->texture_map(b,t,0,PIPE_MAP_READ|PIPE_MAP_DONTBLOCK,&box,&tr));
 f.events[0].retired=true;assert(!b->texture_map(b,t,0,PIPE_MAP_READ,&box,&tr)&&f.releases==0);
 published(&f,0);p=b->texture_map(b,t,0,PIPE_MAP_READ,&box,&tr);assert(p);b->texture_unmap(b,tr);
 uint32_t handle=f.events[0].submitted.gem_handle;uint64_t va=f.events[0].submitted.gpu_va;
 struct pipe_fence_handle *old=NULL;
 for(unsigned frame=1;frame<=4;frame++) {
  box.width=48;box.height=1;p=b->buffer_map(b,v,0,PIPE_MAP_WRITE,&box,&tr);assert(p);((float *)p)[0]=8+(float)frame;b->buffer_unmap(b,tr);
  draw(b,i);assert(!remesa_context_error(b));assert(f.events[frame].submitted.operation==frame+1);
  assert(f.events[frame].submitted.gem_handle==handle&&f.events[frame].submitted.gpu_va==va);
  assert(f.events[frame].submitted.packet->positions[0][0]==8+(float)frame);
  if(frame==1)b->flush(b,&old,0);
  published(&f,frame);struct pipe_fence_handle *now=NULL;b->flush(b,&now,0);assert(now&&s->fence_finish(s,b,now,0));s->fence_reference(s,&now,NULL);
  if(frame>1)assert(s->fence_finish(s,b,old,0));
 }
 assert(f.creates==3&&f.pins==3&&f.maps==3&&f.releases==5);
 s->fence_reference(s,&old,NULL);b->destroy(b);drop(v);drop(i);drop(t);s->destroy(s);
 assert(f.creates==f.destroys&&f.maps==f.unmaps&&f.pins==f.unpins);assert(!remesa_persistent_close(&d));
}
static void memory_and_detach(void)
{
 struct fixture f={.dispatch_result=REMESA_ISSUED};struct remesa_device d={0};open_device(&f,&d,202);
 void *b,*p,*fence=NULL;struct remesa_draw_packet packet_data;packet(&packet_data);
 for(unsigned stage=1;stage<=3;stage++) {f.fail=stage;assert(d.ops->allocate(d.priv,4096,&b)==ENOMEM&&!b);assert(f.creates==f.destroys);}
 f.fail=0;assert(!d.ops->allocate(d.priv,4096,&b));assert(!d.ops->submit(d.priv,b,&packet_data,&fence));
 packet_data.positions[0][0]=99;assert(f.events[0].submitted.packet->positions[0][0]==8);
 d.ops->destroy_bo(d.priv,b);d.ops->destroy_fence(d.priv,fence);assert(remesa_persistent_close(&d)==EBUSY);
 assert(f.destroys==3);published(&f,0);assert(!remesa_persistent_collect(&d));assert(f.destroys==4);
 void *bos[16];for(unsigned i=0;i<16;i++)assert(!d.ops->allocate(d.priv,4096,&bos[i]));
 assert(d.ops->allocate(d.priv,1,&b)==ENOSPC&&!b);assert(d.ops->allocate(d.priv,SIZE_MAX,&b)==EINVAL);
 f.fail=5;assert(d.ops->map(d.priv,bos[0],true,&p)==ENOMEM&&!p);f.fail=0;
 assert(!d.ops->map(d.priv,bos[0],true,&p));d.ops->destroy_bo(d.priv,bos[0]);assert(remesa_persistent_close(&d)==EBUSY);d.ops->unmap(d.priv,bos[0],true);
 for(unsigned i=1;i<16;i++)d.ops->destroy_bo(d.priv,bos[i]);
 for(unsigned i=0;i<40;i++){assert(!d.ops->allocate(d.priv,3,&b));d.ops->destroy_bo(d.priv,b);}
 assert(f.creates==f.destroys&&f.maps==f.unmaps&&f.pins==f.unpins);assert(!remesa_persistent_close(&d));
}
static void ranges_and_ordering(void)
{
 struct fixture f={.dispatch_result=REMESA_ISSUED},other={.dispatch_result=REMESA_ISSUED};struct remesa_device d={0},foreign={0};
 struct remesa_persistent_config c={.ops=&ops,.priv=&f,.context=203,.va_start=0x100000,.va_end=0x102000,.byte_budget=65536,.offline_test_backend=true};
 assert(!remesa_persistent_open(&d,&c));assert(remesa_persistent_open(&d,&c)==EBUSY);
 open_device(&other,&foreign,204);void *bo,*another,*ptr=NULL;
 assert(!foreign.ops->allocate(foreign.priv,4096,&bo));assert(d.ops->map(d.priv,bo,true,&ptr)==EINVAL&&!ptr);
 assert(d.ops->wait(d.priv,(void *)1,0,true)==EINVAL);d.ops->destroy_bo(d.priv,bo);
 foreign.ops->destroy_bo(foreign.priv,bo);assert(!remesa_persistent_close(&foreign));
 assert(!d.ops->allocate(d.priv,4096,&bo)&&!d.ops->allocate(d.priv,4096,&another));
 void *third=NULL;unsigned created=f.creates;assert(d.ops->allocate(d.priv,1,&third)==ENOSPC&&!third&&f.creates==created);
 d.ops->destroy_bo(d.priv,bo);assert(d.ops->allocate(d.priv,4097,&third)==ENOSPC&&!third);d.ops->destroy_bo(d.priv,another);
 assert(!d.ops->allocate(d.priv,4097,&third));assert(f.gems[f.creates-1].va==0x100000&&f.gems[f.creates-1].length==8192);
 d.ops->destroy_bo(d.priv,third);assert(!remesa_persistent_close(&d));
 memset(&f,0,sizeof(f));f.dispatch_result=REMESA_NOT_ISSUED;open_device(&f,&d,205);
 struct remesa_draw_packet p;packet(&p);void *fence=(void *)1;
 assert(!d.ops->allocate(d.priv,4096,&bo));assert(d.ops->submit(d.priv,bo,&p,&fence)==EOPNOTSUPP&&!fence);
 assert(!d.ops->map(d.priv,bo,true,&ptr));d.ops->unmap(d.priv,bo,true);d.ops->destroy_bo(d.priv,bo);assert(!remesa_persistent_close(&d));
 memset(&f,0,sizeof(f));f.dispatch_result=REMESA_ISSUED;open_device(&f,&d,206);
 void *buffers[8],*fences[8];
 for(unsigned i=0;i<8;i++){assert(!d.ops->allocate(d.priv,4096,&buffers[i]));assert(!d.ops->submit(d.priv,buffers[i],&p,&fences[i]));}
 fence=(void *)1;assert(d.ops->submit(d.priv,buffers[0],&p,&fence)==EBUSY&&!fence&&f.dispatches==8);
 published(&f,7);assert(d.ops->wait(d.priv,fences[7],0,true)==EAGAIN&&f.releases==0);
 for(unsigned i=0;i<7;i++){published(&f,i);assert(!d.ops->wait(d.priv,fences[i],0,true));d.ops->destroy_fence(d.priv,fences[i]);}
 assert(!d.ops->wait(d.priv,fences[7],0,true));d.ops->destroy_fence(d.priv,fences[7]);
 assert(!d.ops->submit(d.priv,buffers[7],&p,&fences[7])&&f.events[8].submitted.operation==9);
 published(&f,8);assert(!d.ops->wait(d.priv,fences[7],0,true));d.ops->destroy_fence(d.priv,fences[7]);
 for(unsigned i=0;i<8;i++)d.ops->destroy_bo(d.priv,buffers[i]);
 assert(f.creates==f.destroys&&f.releases==9);assert(!remesa_persistent_close(&d));
}
/* Exercise ordered publication through real pinned Mesa callbacks. Completion
 * receipts are fixtures, never pixel output. A destroyed context's earlier
 * fence still blocks acknowledgement of a newer surface writer. */
static void gallium_ordered_surfaces(void)
{
 struct fixture f={.dispatch_result=REMESA_ISSUED};struct remesa_device d={0};open_device(&f,&d,207);
 struct pipe_screen *s=remesa_screen_create(&d);assert(s);
 struct pipe_context *a=s->context_create(s,NULL,0),*b=s->context_create(s,NULL,0);
 struct pipe_resource *v=buffer(s,48,PIPE_BIND_VERTEX_BUFFER),*i=buffer(s,6,PIPE_BIND_INDEX_BUFFER);
 struct pipe_resource rt={0};rt.target=PIPE_TEXTURE_2D;rt.format=PIPE_FORMAT_B8G8R8A8_UNORM;rt.width0=rt.height0=32;rt.depth0=rt.array_size=1;rt.bind=PIPE_BIND_RENDER_TARGET;
 struct pipe_resource *x=s->resource_create(s,&rt),*y=s->resource_create(s,&rt);assert(v&&i&&x&&y);
 const float vert[3][4]={{8,8,.5f,1},{24,8,.5f,1},{8,24,.5f,1}};const uint16_t ix[]={0,1,2};
 struct pipe_transfer *tr=NULL;struct pipe_box box={.width=48,.height=1,.depth=1};
 void *p=a->buffer_map(a,v,0,PIPE_MAP_WRITE,&box,&tr);assert(p);memcpy(p,vert,48);a->buffer_unmap(a,tr);
 box.width=6;p=a->buffer_map(a,i,0,PIPE_MAP_WRITE,&box,&tr);assert(p);memcpy(p,ix,6);a->buffer_unmap(a,tr);
 bind(a,v,x);bind(b,v,y);draw(a,i);draw(b,i);assert(!remesa_context_error(a)&&!remesa_context_error(b));
 struct pipe_fence_handle *old=NULL,*newer=NULL;a->flush(a,&old,0);b->flush(b,&newer,0);a->destroy(a);
 assert(f.dispatches==2&&f.events[0].submitted.operation==1&&f.events[1].submitted.operation==2);
 published(&f,1);assert(!s->fence_finish(s,b,newer,0)&&f.releases==0);
 box.width=box.height=32;assert(!b->texture_map(b,y,0,PIPE_MAP_READ|PIPE_MAP_DONTBLOCK,&box,&tr));
 assert(f.releases==0&&f.destroys==0);
 published(&f,0);assert(s->fence_finish(s,b,newer,0)&&f.releases==2);
 assert(s->fence_finish(s,b,old,0)&&f.releases==2);
 assert(s->fence_finish(s,b,newer,0)&&f.releases==2); /* idempotent fence, no duplicate ack */
 p=b->texture_map(b,y,0,PIPE_MAP_READ,&box,&tr);assert(p);b->texture_unmap(b,tr);
 s->fence_reference(s,&old,NULL);s->fence_reference(s,&newer,NULL);b->destroy(b);
 drop(v);drop(i);drop(x);drop(y);s->destroy(s);
 assert(f.creates==f.destroys&&f.maps==f.unmaps&&f.pins==f.unpins);assert(!remesa_persistent_close(&d));
}
static void detached_slot_reuse(void)
{
 struct fixture f={.dispatch_result=REMESA_ISSUED};struct remesa_device d={0};open_device(&f,&d,208);
 struct remesa_draw_packet p;packet(&p);void *bo[2],*fence[2];
 for(unsigned i=0;i<2;i++){assert(!d.ops->allocate(d.priv,4096,&bo[i]));assert(!d.ops->submit(d.priv,bo[i],&p,&fence[i]));}
 published(&f,0);assert(!d.ops->wait(d.priv,fence[0],0,true));d.ops->destroy_fence(d.priv,fence[0]);
 assert(!d.ops->submit(d.priv,bo[0],&p,&fence[0])&&f.events[2].submitted.operation==3);
 published(&f,2);d.ops->destroy_fence(d.priv,fence[1]);d.ops->destroy_fence(d.priv,fence[0]);
 assert(f.releases==1);published(&f,1);assert(!remesa_persistent_collect(&d)&&f.releases==3);
 for(unsigned i=0;i<2;i++)d.ops->destroy_bo(d.priv,bo[i]);
 assert(!remesa_persistent_close(&d));
 assert(f.pins==f.unpins&&f.maps==f.unmaps&&f.creates==f.destroys);
}
/* Failure storage is intentionally retained globally: no qualified reset
 * contract exists. Testing EBUSY/quarantine is not a license to free it. */
static struct remesa_device retained[7];static struct fixture failures[7];
static void failures_retained(void)
{
 for(unsigned n=0;n<5;n++) {
  struct fixture *f=&failures[n];f->dispatch_result=n==3?REMESA_ISSUANCE_AMBIGUOUS:REMESA_ISSUED;struct remesa_device *d=&retained[n];open_device(f,d,300+n);
  void *bo,*fence=NULL,*p=NULL;struct remesa_draw_packet request;packet(&request);assert(!d->ops->allocate(d->priv,4096,&bo));
  if(n==4) {f->fail=4;d->ops->destroy_bo(d->priv,bo);assert(remesa_persistent_close(d)==EBUSY&&f->unmaps==0&&f->destroys==0);continue;}
  int e=d->ops->submit(d->priv,bo,&request,&fence);assert(fence);
  if(n==3)assert(e==EIO);else {
   assert(!e);published(f,0);
   if(n==0)f->invalid=1;
   if(n==1)f->invalid=2;
   if(n==2)f->fail=6;
   assert(d->ops->wait(d->priv,fence,0,true)==EIO);
  }
  assert(d->ops->map(d->priv,bo,true,&p)==EIO&&!p);
  unsigned calls=f->dispatches;void *next=NULL;assert(d->ops->submit(d->priv,bo,&request,&next)==EIO&&!next&&f->dispatches==calls);
  d->ops->destroy_fence(d->priv,fence);d->ops->destroy_bo(d->priv,bo);
  assert(remesa_persistent_close(d)==EBUSY&&f->destroys==0&&f->unmaps==0);
 }
}
static void failed_dependency(void)
{
 struct fixture *f=&failures[5];f->dispatch_result=REMESA_ISSUED;
 struct remesa_device *d=&retained[5];open_device(f,d,305);
 struct remesa_draw_packet p;packet(&p);void *bo[2],*fence[2],*map=NULL;
 for(unsigned i=0;i<2;i++){assert(!d->ops->allocate(d->priv,4096,&bo[i]));assert(!d->ops->submit(d->priv,bo[i],&p,&fence[i]));}
 published(f,0);f->invalid=1;assert(d->ops->wait(d->priv,fence[0],0,true)==EIO);f->invalid=0;
 published(f,1);assert(d->ops->wait(d->priv,fence[1],0,true)==EIO&&f->releases==0);
 assert(d->ops->map(d->priv,bo[1],false,&map)==EIO&&!map);
 for(unsigned i=0;i<2;i++){d->ops->destroy_fence(d->priv,fence[i]);d->ops->destroy_bo(d->priv,bo[i]);}
 assert(remesa_persistent_close(d)==EBUSY&&!f->destroys&&!f->unmaps&&!f->unpins);
}

static void release_visibility_regression(void)
{
 struct fixture *f=&failures[6];struct remesa_device *d=&retained[6];
 f->dispatch_result=REMESA_ISSUED;open_device(f,d,306);
 void *bo=NULL,*fence=NULL,*map=NULL;struct remesa_draw_packet request;packet(&request);
 assert(!d->ops->allocate(d->priv,4096,&bo));assert(!d->ops->submit(d->priv,bo,&request,&fence));
 published(f,0);f->release_retries=1;assert(d->ops->wait(d->priv,fence,0,true)==EAGAIN);
 f->invalid=3;assert(d->ops->wait(d->priv,fence,0,true)==EIO);
 assert(!f->releases&&d->ops->map(d->priv,bo,false,&map)==EIO&&!map);
 d->ops->destroy_fence(d->priv,fence);d->ops->destroy_bo(d->priv,bo);
 assert(remesa_persistent_close(d)==EBUSY&&!f->destroys&&!f->unpins&&!f->unmaps);
}

static void bounded_sequences(void)
{
 const unsigned lengths[]={2,10,100};
 for(unsigned n=0;n<3;n++) {
  struct fixture f={0};struct remesa_device d={0};f.dispatch_result=REMESA_ISSUED;open_device(&f,&d,500+n);
  void *bo=NULL;assert(!d.ops->allocate(d.priv,4096,&bo));struct remesa_draw_packet request;packet(&request);
  uint64_t previous=0;
  for(unsigned i=0;i<lengths[n];i++) {
   void *fence=NULL,*map=NULL;assert(!d.ops->submit(d.priv,bo,&request,&fence));
   assert(f.events[i].submitted.operation>previous);previous=f.events[i].submitted.operation;
   assert(d.ops->wait(d.priv,fence,0,true)==EAGAIN);assert(d.ops->map(d.priv,bo,false,&map)==EBUSY&&!map);
   published(&f,i);f.release_retries=2;
   for(unsigned retry=0;retry<2;retry++) {
    assert(d.ops->wait(d.priv,fence,0,true)==EAGAIN);assert(f.releases==i);
    assert(d.ops->map(d.priv,bo,true,&map)==EBUSY&&!map);
    void *blocked=NULL;assert(d.ops->submit(d.priv,bo,&request,&blocked)==EBUSY&&!blocked&&f.dispatches==i+1);
   }
   assert(!d.ops->wait(d.priv,fence,0,true));assert(!d.ops->wait(d.priv,fence,0,true));
   assert(f.releases==i+1);d.ops->destroy_fence(d.priv,fence);
  }
  assert(f.creates==1&&f.pins==1&&f.maps==1&&f.dispatches==lengths[n]);
  d.ops->destroy_bo(d.priv,bo);assert(f.destroys==1&&f.unpins==1&&f.unmaps==1);assert(!remesa_persistent_close(&d));
 }
}

static void drawable_attachments(void)
{
 struct fixture f={0};struct remesa_device d={0};open_device(&f,&d,901);
 struct pipe_screen *screen=remesa_screen_create(&d);assert(screen);
 struct remesa_drawable_buffers b={0};
 assert(!remesa_drawable_buffers_replace(screen,32,32,true,false,&b));
 assert(b.front&&b.back&&b.front!=b.back&&b.generation==1&&!b.swaps);
 struct remesa_drawable_buffers saved=b;
 assert(remesa_drawable_buffers_replace(screen,1280,800,true,false,&b)==ENOTSUP);
 assert(remesa_drawable_buffers_replace(screen,32,32,true,true,&b)==ENOTSUP);
 assert(!memcmp(&b,&saved,sizeof(b))&&f.creates==2);
 /* Hold a consumer reference across replacement, as Mesa attachments do. */
 struct pipe_resource *reader=b.front;++reader->reference.count;
 f.create_failure_at=4;
 assert(remesa_drawable_buffers_replace(screen,32,32,true,false,&b)==ENOMEM);
 assert(!memcmp(&b,&saved,sizeof(b))&&f.destroys==2);
 assert(reader->reference.count==2&&f.gems[0].live&&f.gems[1].live);
 f.create_failure_at=0;
 assert(!remesa_drawable_buffers_replace(screen,32,32,false,false,&b));
 assert(b.front&&!b.back&&b.generation==2&&!b.swaps);
 assert(reader->reference.count==1&&f.gems[0].live&&!f.gems[1].live);
 drop(reader);remesa_drawable_buffers_release(&b);assert(!b.front&&!b.back&&!b.generation);
 remesa_drawable_buffers_release(&b);screen->destroy(screen);
 assert(f.creates==f.destroys&&f.maps==f.unmaps&&!f.dispatches);
 assert(!remesa_persistent_close(&d));
 puts("PASS: real Gallium front/back ownership, atomic replacement, second-allocation rollback, reader lifetime and explicit depth/large-target rejection; no GPU");
}
static void drawable_memory(void)
{
 struct remesa_resource_limits limits={.max_width=4096,.max_height=4096,
  .max_resource_bytes=32u*1024u*1024u,.row_alignment=64};
 struct remesa_drawable_layout plan={0},saved;
 assert(!remesa_drawable_layout(&limits,1280,800,true,4,16u*1024u*1024u,&plan));
 assert(plan.count==3&&plan.total_bytes==12288000);
 for(unsigned i=0;i<3;i++)assert(plan.planes[i].stride==5120&&
  plan.planes[i].bytes==4096000&&plan.allocation_bytes[i]==4096000);
 saved=plan;
 assert(remesa_drawable_layout(&limits,1280,800,true,4,12287999,&plan)==ENOSPC);
 assert(!memcmp(&plan,&saved,sizeof(plan)));
 struct remesa_resource_limits wide={.max_width=UINT32_MAX,.max_height=UINT32_MAX,
  .max_resource_bytes=UINT64_MAX,.row_alignment=1};
 assert(remesa_drawable_layout(&wide,UINT32_MAX/4,1,false,0,SIZE_MAX,&plan)==EOVERFLOW);
 assert(!memcmp(&plan,&saved,sizeof(plan)));
 assert(remesa_drawable_layout(&limits,0,800,true,4,16000000,&plan)==EINVAL);
 assert(remesa_drawable_layout(&limits,1280,800,true,3,16000000,&plan)==EINVAL);
 assert(!memcmp(&plan,&saved,sizeof(plan)));
 struct remesa_drawable_layout small;
 assert(!remesa_drawable_layout(&limits,1,1,false,0,4096,&small));
 assert(small.count==1&&small.planes[0].bytes==64&&small.total_bytes==4096);
 assert(!remesa_drawable_layout(&limits,1,1,false,2,8192,&small));
 assert(small.count==2&&small.total_bytes==8192);
 struct fixture f={0};struct remesa_device d={0};
 struct remesa_persistent_config c={.ops=&ops,.priv=&f,.context=900,
  .va_start=0x40000000,.va_end=0x42000000,.byte_budget=16u*1024u*1024u,
  .offline_test_backend=true,.reserved_count=2,
  .reserved={{0x40002000,0x40004000},{0x40800000,0x40808000}}};
 struct remesa_persistent_config invalid=c;
 invalid.va_end=UINT64_C(0x100001000);assert(remesa_persistent_open(&d,&invalid)==EINVAL);
 invalid=c;invalid.reserved_count=33;assert(remesa_persistent_open(&d,&invalid)==EINVAL);
 invalid=c;invalid.reserved[0].start++;assert(remesa_persistent_open(&d,&invalid)==EINVAL);
 invalid=c;invalid.reserved[0].end=invalid.va_end+4096;assert(remesa_persistent_open(&d,&invalid)==EINVAL);
 invalid=c;invalid.reserved[0].end=invalid.reserved[0].start;assert(remesa_persistent_open(&d,&invalid)==EINVAL);
 invalid=c;invalid.reserved[1]=invalid.reserved[0];assert(remesa_persistent_open(&d,&invalid)==EINVAL);
 assert(!f.creates&&!d.ops);assert(!remesa_persistent_open(&d,&c));
 /* Mutation of caller config cannot erase exclusions copied by open. */
 c.reserved_count=0;
 void *planes[3]={0};
 for(unsigned i=0;i<3;i++)assert(!d.ops->allocate(d.priv,plan.allocation_bytes[i],&planes[i]));
 assert(f.gems[0].va==0x40004000);
 assert(f.gems[1].va==0x403ec000);
 assert(f.gems[2].va==0x40808000);
 for(unsigned i=0;i<3;i++)assert(f.gems[i].length==4096000);
 /* Oversized resize fails before provider mutation and leaves old BOs live. */
 unsigned creates=f.creates;void *failed=NULL;
 assert(d.ops->allocate(d.priv,plan.total_bytes,&failed)==ENOSPC&&!failed&&f.creates==creates);
 for(unsigned i=0;i<3;i++)assert(f.gems[i].mapped&&f.gems[i].live);
 d.ops->destroy_bo(d.priv,planes[1]);
 assert(!d.ops->allocate(d.priv,4096000,&planes[1]));
 assert(f.gems[3].va==0x403ec000); /* freed hole reused, reservations retained */
 for(unsigned i=0;i<3;i++)d.ops->destroy_bo(d.priv,planes[i]);
 assert(!remesa_persistent_close(&d));assert(f.creates==f.destroys&&!f.dispatches);
 /* No fit must roll back the VA reservation/budget without a provider call. */
 memset(&f,0,sizeof(f));c.reserved_count=1;c.va_end=0x40008000;
 c.reserved[0].start=0x40002000;c.reserved[0].end=c.va_end;
 assert(!remesa_persistent_open(&d,&c));
 assert(d.ops->allocate(d.priv,12288,&failed)==ENOSPC&&!failed&&!f.creates);
 assert(!d.ops->allocate(d.priv,8192,&failed));
 d.ops->destroy_bo(d.priv,failed);assert(!remesa_persistent_close(&d));
 /* Exclusive end at 2^32 is legal; crossing it was rejected above. */
 memset(&f,0,sizeof(f));c.reserved_count=0;c.va_start=UINT64_C(0xfffff000);
 c.va_end=UINT64_C(0x100000000);assert(!remesa_persistent_open(&d,&c));
 assert(!d.ops->allocate(d.priv,4096,&failed));assert(f.gems[0].va==c.va_start);
 void *extra=NULL;assert(d.ops->allocate(d.priv,1,&extra)==ENOSPC&&!extra&&f.creates==1);
 d.ops->destroy_bo(d.priv,failed);assert(!remesa_persistent_close(&d));
 puts("PASS: drawable 1280x800 front/back/Z backing, page/budget checks, reserved VA holes, 32-bit aperture, failed resize retention and released-hole reuse; MOCK MMU, no GPU");
}
/* Preparation uses the real owner/VA/descriptor code with mock GEM services.
 * No fixture contents represent GPU depth, clear, completion or readback. */
static void typed_depth_storage(void)
{
 struct fixture f={0};struct remesa_device d={0};
 struct remesa_persistent_config cfg={.ops=&ops,.priv=&f,.context=333,
  .va_start=0x40000000,.va_end=0x40020000,.byte_budget=65536,
  .offline_test_backend=true,.reserved_count=1,
  .reserved={{0x40001000,0x40003000}}};
 assert(!remesa_persistent_open(&d,&cfg));
 void *color=NULL,*z=NULL,*mapped=NULL,*fence=(void *)1;
 assert(!d.ops->allocate(d.priv,4096,&color));
 assert(!remesa_persistent_depth_allocate(&d,33,35,4,&z));
 struct remesa_persistent_depth_backing view;
 assert(!remesa_persistent_depth_snapshot(&d,z,&view));
 assert(view.owner==333&&view.generation&&view.handle&&view.gpu_va==0x40003000);
 assert(view.width==33&&view.height==35&&view.pitch_pixels==64&&view.cpp==4);
 assert(view.allocation_bytes==16384&&view.initial_surface.minimum_backing_bytes==16384);
 assert(view.initial_surface.control_480==0x00050300);
 for(unsigned i=1;i<5;i++)assert((view.initial_surface.addresses[0].word|
  view.initial_surface.addresses[i].word)==view.gpu_va);
 /* Bind only the live owner's derived extent/address to the CPU prefix.
  * This does not hold a GPU job or establish surface visibility/content. */
 const struct remesa_depth_prefix_request binding={
  .depth={.width=view.width,.height=view.height,.pitch_pixels=view.pitch_pixels,
   .cpp=view.cpp,.status=REMESA_DEPTH_SURFACE_UNDEFINED,
   .gpu_va=view.gpu_va,.allocation_bytes=view.allocation_bytes},
  .color_width=view.width,.color_height=view.height,.clear_depth=1,
  .caller_finalize_flag=true};
 uint8_t prefix[REMESA_DEPTH_PREFIX_BYTES];
 assert(!remesa_depth_prefix_encode(&binding,prefix));
 uint32_t pair[16];for(unsigned i=0;i<16;i++)pair[i]=(uint32_t)prefix[i*4]|
  ((uint32_t)prefix[i*4+1]<<8)|((uint32_t)prefix[i*4+2]<<16)|((uint32_t)prefix[i*4+3]<<24);
 assert(pair[0]==0x480&&pair[1]==0x004503d0&&pair[2]==0xcb0);
 assert((pair[3]|pair[5])==view.gpu_va);
 assert(pair[4]==0x484&&pair[6]==0x488&&pair[8]==0x48c&&pair[10]==0x490);
 assert(pair[5]==pair[7]&&pair[7]==pair[9]&&pair[9]==pair[11]);
 assert(pair[12]==0x4bc&&pair[13]==0x200&&pair[14]==0x4b8&&pair[15]==0x3f800000);
 assert(!f.dispatches);
 assert(remesa_persistent_close(&d)==EBUSY);
 struct remesa_draw_packet draw;packet(&draw);
 assert(d.ops->submit(d.priv,z,&draw,&fence)==EINVAL&&!fence&&!f.dispatches);
 /* A 4096-byte Z object must also refuse the color diagnostic path. */
 void *small=NULL;assert(!remesa_persistent_depth_allocate(&d,32,32,4,&small));
 assert(d.ops->submit(d.priv,small,&draw,&fence)==ENOTSUP&&!fence&&!f.dispatches);
 assert(d.ops->map(d.priv,z,false,&mapped)==EAGAIN&&!mapped);
 assert(!d.ops->map(d.priv,color,true,&mapped));
 struct remesa_persistent_depth_backing before;
 memset(&before,0x5a,sizeof(before));memcpy(&view,&before,sizeof(view));
 assert(remesa_persistent_depth_snapshot(&d,z,&view)==EBUSY&&!memcmp(&view,&before,sizeof(view)));
 void *other=NULL;
 assert(remesa_persistent_depth_allocate(&d,32,32,4,&other)==EBUSY&&!other);
 d.ops->unmap(d.priv,color,true);
 assert(remesa_persistent_depth_snapshot(&d,color,&view)==EINVAL&&!memcmp(&view,&before,sizeof(view)));
 assert(remesa_persistent_depth_snapshot(&d,(void *)1,&view)==EINVAL);
 assert(remesa_persistent_depth_allocate(&d,32,32,4,&z)==EBUSY);
 assert(remesa_persistent_depth_allocate(&d,0,32,4,&other)==EINVAL&&!other);
 assert(remesa_persistent_depth_allocate(&d,32,32,3,&other)==EINVAL&&!other);
 assert(remesa_persistent_depth_allocate(&d,2049,32,4,&other)==ENOTSUP&&!other);
 unsigned created=f.creates;
 assert(remesa_persistent_depth_allocate(&d,128,128,4,&other)==ENOSPC&&!other&&f.creates==created);
 for(unsigned stage=1;stage<=3;stage++) {
  f.fail=stage;assert(remesa_persistent_depth_allocate(&d,32,32,2,&other)==ENOMEM&&!other);
 }
 f.fail=0;
 assert(!remesa_persistent_depth_allocate(&d,1,1,2,&other));
 assert(!remesa_persistent_depth_snapshot(&d,other,&view));
 assert(view.pitch_pixels==32&&view.allocation_bytes==4096&&view.initial_surface.minimum_backing_bytes==2048);
 assert(view.initial_surface.control_480==0x000a0100);
 d.ops->destroy_bo(d.priv,other);d.ops->destroy_bo(d.priv,small);
 d.ops->destroy_bo(d.priv,z);d.ops->destroy_bo(d.priv,color);
 assert(remesa_persistent_depth_snapshot(&d,z,&view)==EINVAL);
 assert(!remesa_persistent_close(&d));
 assert(f.creates==f.destroys&&f.maps==f.unmaps&&f.pins==f.unpins&&!f.dispatches);
 memset(&f,0,sizeof(f));cfg.reserved_count=0;cfg.va_start=0x100000;cfg.va_end=0x120000;
 assert(!remesa_persistent_open(&d,&cfg));other=NULL;
 assert(remesa_persistent_depth_allocate(&d,32,32,4,&other)==ENOTSUP&&!other&&!f.creates);
 assert(!remesa_persistent_close(&d));
 puts("PASS: typed depth owner, padded storage, reserved VA holes, descriptor-derived relocations, undefined content, alias/lease/foreign refusal, rollback and no dispatch; CPU MOCK ONLY");
}
static uint32_t isp_word(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void owned_isp_update(void)
{
 struct fixture f={0};struct remesa_device d={0};
 struct remesa_persistent_config cfg={.ops=&ops,.priv=&f,.context=501,
  .va_start=0x40000000,.va_end=0x40020000,.byte_budget=65536,
  .offline_test_backend=true,.reserved_count=1,.reserved={{0x40001000,0x40003000}}};
 assert(!remesa_persistent_open(&d,&cfg));
 void *color=NULL,*bo=NULL,*cpu=NULL,*fence=NULL;
 assert(!d.ops->allocate(d.priv,4096,&color));
 struct remesa_isp_opaque_triangle_request request={true,true,true,REMESA_DEPTH_LESS,15};
 assert(!remesa_persistent_isp_update_prepare(&d,&request,6,&bo));
 struct remesa_persistent_isp_update_backing view;
 assert(!remesa_persistent_isp_update_snapshot(&d,bo,&view));
 assert(view.owner==501&&view.generation&&view.handle&&view.gpu_va==0x40003000);
 assert(view.allocation_bytes==4096&&view.state_offset==0&&view.state_bytes==8);
 assert(view.use_offset==16&&view.use_bytes==16&&view.pds_offset==32&&view.pds_bytes==60);
 assert(view.ta_offset==96&&view.ta_bytes==8);
 assert(view.use_entry.entry_address==view.gpu_va+16&&view.use_entry.program_end==view.gpu_va+32);
 assert(view.use_entry.bank==6&&view.use_entry.base_address==0x40000000);
 assert(!d.ops->map(d.priv,bo,false,&cpu));
 const unsigned char *bytes=cpu;
 assert(isp_word(bytes)==1&&isp_word(bytes+4)==0x00400000);
 assert(isp_word(bytes+32)==view.gpu_va&&isp_word(bytes+36)==0x80200001);
 /* Independent inversion of the observed lower19 software address fields. */
 uint32_t fixup=isp_word(bytes+40)&0x7ffff;
 uint32_t relative=((fixup>>8)&2047u)*16u+((fixup>>4)&15u)*32768u;
 assert((fixup&15u)==6&&view.use_entry.base_address+relative==view.gpu_va+16);
 assert((isp_word(bytes+40)&~0x7ffffu)==0x200000);
 assert(isp_word(bytes+96)==(0x40000000u|((uint32_t)(view.gpu_va+32)>>4)));
 assert(isp_word(bytes+100)==0x0c02a201);
 for(unsigned i=0;i<60;i++)assert(view.pds_known_mask[i]==(i<12||i>=48?255:0));
 for(unsigned i=104;i<4096;i++)assert(bytes[i]==0);
 struct remesa_persistent_isp_update_backing old;
 memset(&old,0x5a,sizeof(old));struct remesa_persistent_isp_update_backing unchanged=old;
 assert(remesa_persistent_isp_update_snapshot(&d,bo,&unchanged)==EBUSY&&!memcmp(&old,&unchanged,sizeof(old)));
 d.ops->unmap(d.priv,bo,false);cpu=(void *)1;
 assert(d.ops->map(d.priv,bo,true,&cpu)==ENOTSUP&&!cpu);
 struct remesa_draw_packet draw;packet(&draw);
 assert(d.ops->submit(d.priv,bo,&draw,&fence)==ENOTSUP&&!fence&&!f.dispatches);
 void *closed=NULL;
 assert(!remesa_persistent_scene_close_prepare(&d,17,33,6,&closed));
 struct remesa_persistent_scene_close_backing end;
 assert(!remesa_persistent_scene_close_snapshot(&d,closed,&end));
 assert(end.image.owner==501&&end.image.generation&&end.image.handle!=view.handle);
 assert(end.image.pds_bytes==60&&end.image.ta_bytes==12&&end.image.allocation_bytes==4096);
 assert(remesa_persistent_isp_update_snapshot(&d,closed,&unchanged)==EINVAL);
 assert(remesa_persistent_scene_close_snapshot(&d,bo,&end)==EINVAL);
 assert(!d.ops->map(d.priv,closed,false,&cpu));bytes=cpu;
 assert(isp_word(bytes)==0x2000&&isp_word(bytes+4)==0x20003);
 assert(isp_word(bytes+32)==end.image.gpu_va);
 assert(isp_word(bytes+96)==(0x60000000u|((uint32_t)(end.image.gpu_va+32)>>4)));
 assert(isp_word(bytes+100)==0x0c02a201&&isp_word(bytes+104)==0xc0000000);
 for(unsigned i=108;i<4096;i++)assert(bytes[i]==0);
 d.ops->unmap(d.priv,closed,false);
 assert(d.ops->map(d.priv,closed,true,&cpu)==ENOTSUP&&!cpu);
 assert(d.ops->submit(d.priv,closed,&draw,&fence)==ENOTSUP&&!fence&&!f.dispatches);
 d.ops->destroy_bo(d.priv,closed);
 assert(remesa_persistent_scene_close_snapshot(&d,closed,&end)==EINVAL);
 assert(remesa_persistent_scene_close_prepare(&d,0,33,6,&closed)==EBUSY);
 closed=NULL;
 assert(remesa_persistent_scene_close_prepare(&d,0,33,6,&closed)==EINVAL&&!closed);
 assert(remesa_persistent_scene_close_prepare(&d,2049,33,6,&closed)==ENOTSUP&&!closed);
 assert(remesa_persistent_close(&d)==EBUSY);
 assert(remesa_persistent_isp_update_prepare(&d,&request,6,&bo)==EBUSY);
 void *next=NULL;unsigned creates=f.creates;
 assert(remesa_persistent_isp_update_prepare(&d,&request,16,&next)==ERANGE&&!next&&f.creates==creates);
 request.rgba_mask=1;
 assert(remesa_persistent_isp_update_prepare(&d,&request,6,&next)==ENOTSUP&&!next&&f.creates==creates);
 request.rgba_mask=15;
 for(unsigned fail=1;fail<=5;fail++)if(fail!=4) {
  f.fail=fail;
  assert(remesa_persistent_isp_update_prepare(&d,&request,6,&next)==ENOMEM&&!next);
 }
 f.fail=10;
 assert(remesa_persistent_isp_update_prepare(&d,&request,6,&next)==EPROTO&&!next);
 f.fail=0;
 assert(remesa_persistent_isp_update_snapshot(&d,color,&unchanged)==EINVAL);
 assert(remesa_persistent_isp_update_snapshot(&d,(void *)1,&unchanged)==EINVAL);
 d.ops->destroy_bo(d.priv,bo);
 assert(remesa_persistent_isp_update_snapshot(&d,bo,&unchanged)==EINVAL);
 d.ops->destroy_bo(d.priv,color);assert(!remesa_persistent_close(&d));
 assert(f.creates==f.destroys&&f.maps==f.unmaps&&f.pins==f.unpins&&!f.dispatches);
 /* Provider ambiguity never frees or publishes the prepared object. These
  * isolated mock endpoints intentionally remain quarantined; no hardware. */
 for(unsigned fail=7;fail<=9;fail++) {
  struct fixture *q=calloc(1,sizeof(*q));assert(q);struct remesa_device quarantined={0};
  cfg.priv=q;cfg.context=600+fail;assert(!remesa_persistent_open(&quarantined,&cfg));
  q->fail=fail;next=NULL;
  assert(remesa_persistent_isp_update_prepare(&quarantined,&request,6,&next)==EOWNERDEAD&&!next);
  assert(!q->destroys&&!q->unmaps&&!q->dispatches);
  assert(remesa_persistent_close(&quarantined)==EBUSY);
 }
 puts("PASS: owned immutable ISP STATE/USE/PDS/TA composition, reserved VAs, relocation bounds, exact readback, write/submit refusal, rollback and provider-ambiguity quarantine; CPU MOCK ONLY");
}
int main(void)
{
 struct remesa_device d={0};struct remesa_persistent_config bad={0};assert(remesa_persistent_open(&d,&bad)==EINVAL);
 typed_depth_storage();
 owned_isp_update();
 drawable_memory();
 drawable_attachments();
 bounded_sequences();release_visibility_regression();gallium_reuse();gallium_ordered_surfaces();detached_slot_reuse();memory_and_detach();ranges_and_ordering();failures_retained();failed_dependency();
 puts("PASS: bounded 2/10/100 mock operations, acknowledgement backpressure, no redispatch; Gallium persistent handle/VA/pins across 5 oracle submissions; rollback, CPU-map failure, byte/VA/job exhaustion, cross-device rejection, 40 allocation cycles, detached retirement, ordered cross-context publication of out-of-order mock receipts dependency-failure quarantine, slot-reuse drain and 6 retained failure cases; NO SGX");
}
