/* SPDX-License-Identifier: MIT */
/* Asynchronous ownership oracle only. No ioctl, GPU or generated pixels. */
#include "remesa_device.h"
#include "remesa_screen.h"
#include "remesa_present.h"
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct bo { unsigned refs; void *bytes; };
struct job { struct bo *bo; bool live,safe; unsigned state; struct remesa_draw_packet packet; };
struct fixture {
 struct job jobs[16]; unsigned count,allocated,freed,maps,waits;
 uint64_t timeout; bool visible; unsigned owner_releases; int fail; bool missing_fence;
 int map_error;bool null_mapping;unsigned unmaps;
};
static void drop(void *data,void *obj)
{ struct fixture *f=data;struct bo *b=obj;assert(b->refs);if(!--b->refs) {free(b->bytes);free(b);++f->freed;} }
static void release_job(struct fixture *f,struct job *j)
{ if(!j->live && j->safe && j->bo) {drop(f,j->bo);j->bo=NULL;} }
static int allocate(void *data,size_t n,void **out)
{ struct fixture *f=data;struct bo *b=calloc(1,sizeof(*b));assert(b);b->bytes=calloc(1,n);assert(b->bytes);b->refs=1;*out=b;++f->allocated;return 0; }
static int map_bo(void *data,void *obj,bool write,void **out)
{ struct fixture *f=data;(void)write;++f->maps;*out=f->null_mapping?NULL:((struct bo *)obj)->bytes;return f->map_error; }
static void unmap_bo(void *data,void *obj,bool write) { ++((struct fixture *)data)->unmaps;(void)obj;(void)write; }
static int submit(void *data,void *obj,const struct remesa_draw_packet *p,void **out)
{
 struct fixture *f=data;assert(f->count<16);struct job *j=&f->jobs[f->count++];
 assert(remesa_draw_packet_validate(p)==REMESA_PACKET_OK);j->packet=*p;
 j->bo=obj;++j->bo->refs;j->live=!f->missing_fence;j->state=f->fail ? 3 : 0;
 *out=j->live ? j : NULL;return f->fail;
}
static int wait_job(void *data,void *obj,uint64_t ns,bool visible)
{ struct fixture *f=data;struct job *j=obj;++f->waits;f->timeout=ns;f->visible=visible;
 if(j->state==3) return EIO;
 return j->state>=2 || (j->state==1 && !visible) ? 0 : EAGAIN;
}
static void destroy_fence(void *data,void *obj)
{ struct fixture *f=data;struct job *j=obj;assert(j->live);j->live=false;release_job(f,j); }
static void published(struct fixture *f,struct job *j)
{ j->state=2;j->safe=true;release_job(f,j); }
static int release_owner(struct remesa_device *d)
{
 struct fixture *f=d->priv;
 for(unsigned i=0;i<f->count;i++) if(f->jobs[i].bo) return EBUSY;
 assert(f->allocated==f->freed);++f->owner_releases;return 0;
}
static struct pipe_resource *buffer(struct pipe_screen *s,unsigned n,unsigned bind)
{ struct pipe_resource t={0};t.target=PIPE_BUFFER;t.format=PIPE_FORMAT_R8_UNORM;t.width0=n;t.height0=t.depth0=t.array_size=1;t.bind=bind;return s->resource_create(s,&t); }
static struct pipe_resource *target(struct pipe_screen *s)
{ struct pipe_resource t={0};t.target=PIPE_TEXTURE_2D;t.format=PIPE_FORMAT_B8G8R8A8_UNORM;t.width0=t.height0=32;t.depth0=t.array_size=1;t.bind=PIPE_BIND_RENDER_TARGET;return s->resource_create(s,&t); }
static void resource_drop(struct pipe_resource *p)
{ if(!--p->reference.count) p->screen->resource_destroy(p->screen,p); }
static void bind(struct pipe_context *c,struct pipe_resource *v,struct pipe_resource *t)
{
 struct pipe_vertex_buffer vb={0};vb.buffer.resource=v;++v->reference.count;c->set_vertex_buffers(c,1,&vb);
 struct pipe_vertex_element e={.src_format=PIPE_FORMAT_R32G32B32A32_FLOAT,.src_stride=16};
 void *ve=c->create_vertex_elements_state(c,1,&e);assert(ve);c->bind_vertex_elements_state(c,ve);
 /* Keep the layout alive through draws. The context-owned registry tears it
  * down with this fixture context; deleting a bound layout invalidates draw. */
 struct pipe_framebuffer_state fb={0};fb.width=fb.height=32;fb.nr_cbufs=1;fb.cbufs[0].texture=t;fb.cbufs[0].format=t->format;c->set_framebuffer_state(c,&fb);
 assert(!remesa_bind_diagnostic_pipeline(c,0xffff00ff));
}
static void draw(struct pipe_context *c,struct pipe_resource *i)
{ struct pipe_draw_info info={.mode=MESA_PRIM_TRIANGLES,.index_size=2,.instance_count=1,.index.resource=i};struct pipe_draw_start_count_bias d={.count=3};c->draw_vbo(c,&info,0,NULL,&d,1); }
int main(void)
{
 struct fixture f={0};const struct remesa_device_ops ops={.allocate=allocate,.destroy_bo=drop,.map=map_bo,.unmap=unmap_bo,.submit=submit,.wait=wait_job,.destroy_fence=destroy_fence};
 struct remesa_device dev={.ops=&ops,.priv=&f,.offline_test_backend=true};struct pipe_screen *s=remesa_screen_create_owned(&dev,release_owner);assert(s&&!dev.ops&&!dev.priv);
 struct pipe_context *a=s->context_create(s,NULL,0),*b=s->context_create(s,NULL,0);assert(a&&b);
 struct pipe_resource *v=buffer(s,48,PIPE_BIND_VERTEX_BUFFER),*i=buffer(s,6,PIPE_BIND_INDEX_BUFFER),*t=target(s);assert(v&&i&&t);
 const float vertices[3][4]={{8,8,.5f,1},{24,8,.5f,1},{8,24,.5f,1}};const uint16_t ix[3]={0,1,2};
 struct pipe_box box={.width=48,.height=1,.depth=1};struct pipe_transfer *tr=NULL;void *p=a->buffer_map(a,v,0,PIPE_MAP_WRITE,&box,&tr);assert(p);memcpy(p,vertices,48);a->buffer_unmap(a,tr);
 box.width=6;p=a->buffer_map(a,i,0,PIPE_MAP_WRITE,&box,&tr);assert(p);memcpy(p,ix,6);a->buffer_unmap(a,tr);
 bind(a,v,t);bind(b,v,t);draw(a,i);assert(!remesa_context_error(a)&&f.count==1);
 /* Rejecting a later draw before dispatch must not hide the fence for work
  * already submitted by this context. A returned fence still polls pending;
  * the error is not manufactured completion or permission to reuse backing. */
 struct pipe_fence_handle *before_error=NULL,*after_error=NULL;
 a->flush(a,&before_error,0);assert(before_error);
 struct pipe_draw_info rejected={.mode=MESA_PRIM_TRIANGLES,.instance_count=1};
 struct pipe_draw_start_count_bias rejected_draw={.count=3};
 struct pipe_draw_indirect_info indirect={0};
 a->draw_vbo(a,&rejected,0,&indirect,&rejected_draw,1);
 assert(remesa_context_error(a)==ENOTSUP&&f.count==1);
 a->flush(a,&after_error,0);assert(after_error==before_error);
 assert(!s->fence_finish(s,a,after_error,0)&&f.visible);
 s->fence_reference(s,&before_error,NULL);s->fence_reference(s,&after_error,NULL);
 a->destroy(a);
 /* Another surface has its own writer; publishing it cannot unblock t. */
 struct pipe_resource *u=target(s);assert(u);bind(b,v,u);draw(b,i);assert(f.count==2);
 published(&f,&f.jobs[1]);box.width=box.height=32;
 p=b->texture_map(b,u,0,PIPE_MAP_READ,&box,&tr);assert(p);b->texture_unmap(b,tr);
 bind(b,v,t);resource_drop(u);
 box.width=box.height=32;unsigned maps=f.maps;
 assert(!b->texture_map(b,t,0,PIPE_MAP_READ|PIPE_MAP_DONTBLOCK,&box,&tr)&&!tr);
 assert(f.timeout==0 && f.visible && f.maps==maps);
 unsigned char snapshot[32*144];memset(snapshot,0xa5,sizeof(snapshot));
 assert(remesa_present_snapshot(b,t,snapshot,144,sizeof(snapshot))==EAGAIN);
 for(unsigned x=0;x<sizeof(snapshot);x++) assert(snapshot[x]==0xa5);
 assert(f.maps==maps);
 assert(remesa_present_snapshot(b,t,snapshot,127,sizeof(snapshot))==EINVAL);
 assert(remesa_present_snapshot(b,t,snapshot,144,1)==ENOSPC);
 assert(remesa_present_snapshot(b,t,snapshot,SIZE_MAX,sizeof(snapshot))==EINVAL);
 assert(!b->texture_map(b,t,0,PIPE_MAP_WRITE|PIPE_MAP_DONTBLOCK,&box,&tr)&&f.maps==maps);
 draw(b,i);assert(remesa_context_error(b)==EAGAIN && f.count==2 && f.timeout==UINT64_MAX);
 f.jobs[0].state=1;assert(!b->texture_map(b,t,0,PIPE_MAP_READ,&box,&tr));assert(f.timeout==UINT64_MAX);
 published(&f,&f.jobs[0]);p=b->texture_map(b,t,0,PIPE_MAP_READ,&box,&tr);assert(p);b->texture_unmap(b,tr);
 assert(!remesa_present_snapshot(b,t,snapshot,144,sizeof(snapshot)));
 for(unsigned y=0;y<32;y++) {
  for(unsigned x=0;x<128;x++) assert(snapshot[y*144+x]==0);
  for(unsigned x=128;x<144;x++) assert(snapshot[y*144+x]==0xa5);
 }
 /* A backend mapping refusal is not a rendering failure. Return its precise
  * error without copying expected bytes, and release only acquired leases. */
 for(unsigned failure=0;failure<3;failure++) {
  f.map_error=failure<2?EAGAIN:0;f.null_mapping=failure!=0;
  unsigned before_unmaps=f.unmaps;memset(snapshot,0xa5,sizeof(snapshot));
  int expected=failure<2?EAGAIN:EPROTO;
  assert(remesa_present_snapshot(b,t,snapshot,144,sizeof(snapshot))==expected);
  assert(remesa_context_error(b)==expected);
  assert(f.unmaps==before_unmaps+(failure!=1));
  for(unsigned x=0;x<sizeof(snapshot);x++)assert(snapshot[x]==0xa5);
 }
 f.map_error=0;f.null_mapping=false;
 /* Persistent inputs and target: three new submissions, with explicit oracle
  * publication between them. These are not repeated SGX frame evidence. */
 for(unsigned frame=0;frame<3;frame++) {
  draw(b,i);assert(!remesa_context_error(b));struct job *j=&f.jobs[f.count-1];
  assert(j->packet.positions[0][0]==8+(float)frame);
  box.width=48;box.height=1;p=b->buffer_map(b,v,0,PIPE_MAP_WRITE,&box,&tr);assert(p);((float *)p)[0]=9+(float)frame;b->buffer_unmap(b,tr);
  assert(j->packet.positions[0][0]==8+(float)frame);published(&f,j);
 }
 assert(f.count==5);
 /* Teardown while pending must retain backend backing even after all frontend
  * refs vanish. Only the test's explicit publication releases that backing. */
 draw(b,i);assert(f.count==6);b->destroy(b);resource_drop(t);assert(f.jobs[5].bo);
 published(&f,&f.jobs[5]);assert(!f.jobs[5].bo);
 for(unsigned scenario=0;scenario<3;scenario++) {
  b=s->context_create(s,NULL,0);t=target(s);bind(b,v,t);
  struct job *prior=NULL;
  if(!scenario) {
   draw(b,i);assert(!remesa_context_error(b));prior=&f.jobs[f.count-1];
   struct pipe_resource *previous=t;t=target(s);assert(t);bind(b,v,t);
   resource_drop(previous);
  }
  f.fail=scenario==2 ? 0 : EIO;f.missing_fence=scenario!=0;
  draw(b,i);assert(remesa_context_error(b)==(scenario==2 ? EPROTO : EIO));unsigned count=f.count;
  maps=f.maps;box.width=box.height=32;
  assert(!b->texture_map(b,t,0,PIPE_MAP_WRITE,&box,&tr)&&!tr&&f.maps==maps);
  assert(!b->texture_map(b,t,0,PIPE_MAP_READ,&box,&tr)&&!tr&&f.maps==maps);
  draw(b,i);assert(f.count==count);struct pipe_fence_handle *fence=NULL;b->flush(b,&fence,0);assert(!fence);
  /* A later no-op clears the callback error, not the unknown failed work.
   * Flush must still reject the previous successful fence as its substitute. */
  struct pipe_draw_info empty_info={.mode=MESA_PRIM_TRIANGLES,.instance_count=1};
  struct pipe_draw_start_count_bias empty_draw={0};
  b->draw_vbo(b,&empty_info,0,NULL,&empty_draw,1);assert(!remesa_context_error(b));
  b->flush(b,&fence,0);assert(!fence&&f.count==count);
  b->destroy(b);resource_drop(t);assert(f.jobs[count-1].bo);
  if(prior){assert(prior->bo);published(&f,prior);assert(!prior->bo);}
  /* Explicit simulated recovery, never automatic driver rearming. */
  published(&f,&f.jobs[count-1]);assert(!f.jobs[count-1].bo);
 }
 /* A completion-time failure is sticky even if a later test event changes
  * that backend's state. The frontend never silently recovers the resource. */
 f.fail=0;f.missing_fence=false;b=s->context_create(s,NULL,0);t=target(s);bind(b,v,t);draw(b,i);
 unsigned count=f.count;struct job *failed=&f.jobs[count-1];failed->state=3;
 box.width=box.height=32;assert(!b->texture_map(b,t,0,PIPE_MAP_READ,&box,&tr));
 assert(remesa_present_snapshot(b,t,snapshot,144,sizeof(snapshot))==EIO);
 published(&f,failed);maps=f.maps;assert(!b->texture_map(b,t,0,PIPE_MAP_WRITE,&box,&tr)&&f.maps==maps);
 draw(b,i);assert(remesa_context_error(b)==EIO && f.count==count);
 b->destroy(b);resource_drop(t);assert(!failed->bo);
 /* Final pending fence retains the owning screen after owner destruction. */
 b=s->context_create(s,NULL,0);t=target(s);bind(b,v,t);draw(b,i);
 struct pipe_fence_handle *last=NULL;b->flush(b,&last,0);assert(last);
 struct job *pending=&f.jobs[f.count-1];s->destroy(s);
 assert(!s->context_create(s,NULL,0)&&!target(s)&&!f.owner_releases);
 b->destroy(b);resource_drop(t);resource_drop(v);resource_drop(i);
 assert(!s->fence_finish(s,NULL,last,0)&&pending->bo&&!f.owner_releases);
 published(&f,pending);assert(s->fence_finish(s,NULL,last,0));
 s->fence_reference(s,&last,NULL);assert(f.allocated==f.freed&&f.owner_releases==1);
 /* No external fence: backend owns detached pending work. Release backpressure
  * retains the screen/backend; collection retries teardown, never dispatch. */
 memset(&dev,0,sizeof(dev));dev.ops=&ops;dev.priv=&f;dev.offline_test_backend=true;
 s=remesa_screen_create_owned(&dev,release_owner);b=s->context_create(s,NULL,0);
 v=buffer(s,48,PIPE_BIND_VERTEX_BUFFER);i=buffer(s,6,PIPE_BIND_INDEX_BUFFER);t=target(s);
 box.width=48;box.height=1;p=b->buffer_map(b,v,0,PIPE_MAP_WRITE,&box,&tr);assert(p);memcpy(p,vertices,48);b->buffer_unmap(b,tr);
 box.width=6;p=b->buffer_map(b,i,0,PIPE_MAP_WRITE,&box,&tr);assert(p);memcpy(p,ix,6);b->buffer_unmap(b,tr);
 bind(b,v,t);draw(b,i);pending=&f.jobs[f.count-1];unsigned issued=f.count;
 s->destroy(s);b->destroy(b);resource_drop(v);resource_drop(i);resource_drop(t);
 assert(pending->bo&&f.owner_releases==1);
 assert(remesa_screen_collect(s)==EBUSY&&f.count==issued);
 published(&f,pending);assert(!remesa_screen_collect(s)&&f.count==issued&&f.owner_releases==2);
 puts("PASS: deferred owned-screen cleanup and pending-release collection without redispatch, fenced padded snapshots/error propagation; per-surface/shared-resource fences, poll/block deadlines, publication, persistent host reuse, pending teardown, 3 failed-submit cases and sticky completion failure; NO SGX");
 return 0;
}
