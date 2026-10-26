/* SPDX-License-Identifier: MIT
 * Real Gallium transfers over a CPU-backed test device; zero GPU submissions.
 */
#include "remesa_screen.h"
#include "remesa_device.h"
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef REMESA_TEST_MESA_UPLOAD
#include "util/u_transfer.h"
#endif
#ifdef NDEBUG
#error "Transfer qualification requires assertions"
#endif

static unsigned fail_calloc;
void *__real_calloc(size_t, size_t);
void *__wrap_calloc(size_t, size_t);
void *__wrap_calloc(size_t count,size_t size)
{
 if(fail_calloc && !--fail_calloc)return NULL;
 return __real_calloc(count,size);
}
struct fixture {
 unsigned allocations,frees,maps,unmaps,writes,waits,fences,submits;
 int wait_error,map_error;bool null_mapping;
};
static int allocate(void *v,size_t bytes,void **out)
{
 struct fixture *f=v;*out=calloc(1,bytes);if(!*out)return ENOMEM;
 ++f->allocations;return 0;
}
static void drop(void *v,void *bo){++((struct fixture *)v)->frees;free(bo);}
static int map_bo(void *v,void *bo,bool write,void **out)
{
 struct fixture *f=v;(void)write;++f->maps;*out=f->null_mapping?NULL:bo;return f->map_error;
}
static void unmap_bo(void *v,void *bo,bool write)
{
 struct fixture *f=v;(void)bo;++f->unmaps;f->writes+=write;
}
static int submit(void *v,void *bo,const struct remesa_draw_packet *packet,void **fence)
{
 (void)bo;(void)packet;(void)fence;++((struct fixture *)v)->submits;return ENOTSUP;
}
static int wait(void *v,void *fence,uint64_t ns,bool visible)
{
 struct fixture *f=v;(void)fence;(void)ns;assert(visible);++f->waits;return f->wait_error;
}
static void destroy_fence(void *v,void *fence){(void)fence;++((struct fixture *)v)->fences;}
static const struct remesa_device_ops ops={.allocate=allocate,.destroy_bo=drop,.map=map_bo,.unmap=unmap_bo,.submit=submit,.wait=wait,.destroy_fence=destroy_fence};
static void release(struct pipe_resource *r)
{if(!--r->reference.count)r->screen->resource_destroy(r->screen,r);}
static struct pipe_resource *create(struct pipe_screen *s,bool texture)
{
 struct pipe_resource t={.target=texture?PIPE_TEXTURE_2D:PIPE_BUFFER,
  .format=texture?PIPE_FORMAT_B8G8R8A8_UNORM:PIPE_FORMAT_R8_UNORM,
  .width0=texture?11:120,.height0=texture?5:1,.depth0=1,.array_size=1};
 return s->resource_create(s,&t);
}
static void check_buffer(struct pipe_context *c,struct pipe_resource *r,unsigned first,unsigned end,unsigned char value)
{
 struct pipe_box all={.width=120,.height=1,.depth=1};struct pipe_transfer *t=NULL;
 unsigned char *p=c->buffer_map(c,r,0,PIPE_MAP_READ,&all,&t);assert(p);
 for(unsigned i=0;i<120;i++)assert(p[i]==(i>=first&&i<end?value:0x31));
 c->buffer_unmap(c,t);
}
int main(void)
{
 struct fixture f={0};struct remesa_device dev={.ops=&ops,.priv=&f,.resource_only_backend=true,
  .resource_limits={.max_width=4096,.max_height=4096,.row_alignment=64,.max_resource_bytes=64*1024*1024}};
 struct pipe_screen *s=remesa_screen_create(&dev);assert(s);
 struct pipe_context *c=s->context_create(s,NULL,0),*other=s->context_create(s,NULL,0);assert(c&&other);
 struct pipe_resource *r=create(s,false);assert(r);
 unsigned char seed[120];memset(seed,0x31,sizeof(seed));
 c->buffer_subdata(c,r,PIPE_MAP_DISCARD_WHOLE_RESOURCE,0,sizeof(seed),seed);assert(!remesa_context_error(c));
#ifdef REMESA_TEST_MESA_UPLOAD
 /* These are the linked Mesa 25.2 utilities, not a rewritten upload mock. */
 u_default_buffer_subdata(c,r,0,0,sizeof(seed),seed);
 u_default_buffer_subdata(c,r,0,3,1,seed);check_buffer(c,r,0,0,0);
#endif
 struct pipe_box box={.x=8,.width=32,.height=1,.depth=1};struct pipe_transfer *t=NULL,*extra=NULL;
 unsigned char *p=c->buffer_map(c,r,0,PIPE_MAP_WRITE|PIPE_MAP_FLUSH_EXPLICIT,&box,&t);assert(p);
 memset(p,0xa5,32);assert(!other->buffer_map(other,r,0,PIPE_MAP_READ,&box,&extra)&&!extra);
 unsigned writes=f.writes,unmaps=f.unmaps;
 struct pipe_box part={.x=4,.width=8,.height=1,.depth=1};
 other->transfer_flush_region(other,t,&part);assert(remesa_context_error(other)==EINVAL);
 other->buffer_unmap(other,t);assert(remesa_context_error(other)==EINVAL&&f.unmaps==unmaps);
 c->transfer_flush_region(c,t,&part);assert(!remesa_context_error(c));
 memset(p+4,0x99,8); /* Changes after flush must not be committed by unmap. */
 part.x=31;part.width=2;c->transfer_flush_region(c,t,&part);assert(remesa_context_error(c)==EINVAL);
 part.x=-1;c->transfer_flush_region(c,t,&part);assert(remesa_context_error(c)==EINVAL);
 c->buffer_unmap(c,t);assert(f.writes==writes+1);check_buffer(c,r,12,20,0xa5);
 p=c->buffer_map(c,r,0,PIPE_MAP_WRITE|PIPE_MAP_FLUSH_EXPLICIT,&box,&t);assert(p);
 memset(p,0x77,32);writes=f.writes;c->buffer_unmap(c,t);assert(f.writes==writes);check_buffer(c,r,12,20,0xa5);
 /* Invalid usage must not map backing or bypass pending writer retirement. */
 const unsigned rejected[]={PIPE_MAP_READ|PIPE_MAP_FLUSH_EXPLICIT,
  PIPE_MAP_READ|PIPE_MAP_WRITE|PIPE_MAP_DISCARD_RANGE,PIPE_MAP_READ|PIPE_MAP_UNSYNCHRONIZED,
  PIPE_MAP_WRITE|PIPE_MAP_PERSISTENT,PIPE_MAP_WRITE|PIPE_MAP_COHERENT,
  PIPE_MAP_WRITE|PIPE_MAP_FLUSH_EXPLICIT|PIPE_MAP_DIRECTLY};
 unsigned maps=f.maps;
 for(unsigned i=0;i<sizeof(rejected)/sizeof(rejected[0]);i++)
  assert(!c->buffer_map(c,r,0,rejected[i],&box,&t)&&!t&&f.maps==maps);
 /* Unsynchronized uploads may map idle backing without probing a writer.
  * No flush means no changed bytes; the CPU lease still excludes aliases. */
 unsigned waits=f.waits;
 p=c->buffer_map(c,r,0,PIPE_MAP_WRITE|PIPE_MAP_UNSYNCHRONIZED|PIPE_MAP_FLUSH_EXPLICIT,&box,&t);
 assert(p&&t&&f.waits==waits);c->buffer_unmap(c,t);maps=f.maps;
 for(unsigned failure=1;failure<=2;failure++) {
  fail_calloc=failure;assert(!c->buffer_map(c,r,0,PIPE_MAP_WRITE|PIPE_MAP_FLUSH_EXPLICIT,&box,&t)&&!t);
  assert(!fail_calloc&&f.maps==maps&&r->reference.count==1);
 }
 f.null_mapping=true;assert(!c->buffer_map(c,r,0,PIPE_MAP_WRITE|PIPE_MAP_FLUSH_EXPLICIT,&box,&t)&&!t);
 f.null_mapping=false;f.map_error=EIO;assert(!c->buffer_map(c,r,0,PIPE_MAP_WRITE|PIPE_MAP_FLUSH_EXPLICIT,&box,&t)&&!t);
 f.map_error=0;assert(f.maps==f.unmaps&&r->reference.count==1);
 /* A discard map waits for the same independently imported writer fence. */
 struct pipe_fence_handle *fence=NULL;assert(!remesa_resource_attach_writer(c,r,&f,&fence));
 maps=f.maps;f.wait_error=EAGAIN;
 waits=f.waits;
 assert(!c->buffer_map(c,r,0,PIPE_MAP_WRITE|PIPE_MAP_UNSYNCHRONIZED,&box,&t));
 assert(!t&&f.maps==maps&&f.waits==waits); /* not even a wait/poll */
 assert(!c->buffer_map(c,r,0,PIPE_MAP_WRITE|PIPE_MAP_DISCARD_WHOLE_RESOURCE|PIPE_MAP_DONTBLOCK,&box,&t));
 c->buffer_subdata(c,r,PIPE_MAP_DISCARD_RANGE|PIPE_MAP_DONTBLOCK,8,1,seed);
 assert(remesa_context_error(c)==EAGAIN&&f.maps==maps);
 f.wait_error=0;p=c->buffer_map(c,r,0,PIPE_MAP_WRITE|PIPE_MAP_DISCARD_RANGE|PIPE_MAP_DIRECTLY,&box,&t);assert(p);
 assert(p[4]==0xa5);c->buffer_unmap(c,t);s->fence_reference(s,&fence,NULL);assert(f.fences==1);
 release(r);
 /* Explicit texture flush: nonzero map origin, padded stride, partial rows. */
 r=create(s,true);assert(r);box=(struct pipe_box){.width=11,.height=5,.depth=1};
 p=c->texture_map(c,r,0,PIPE_MAP_WRITE,&box,&t);assert(p&&t->stride==64);
 memset(p,0xcd,320);c->texture_unmap(c,t);
#ifdef REMESA_TEST_MESA_UPLOAD
 unsigned char texel[4]={1,3,5,7};
 struct pipe_box pixel={.width=1,.height=1,.depth=1};
 unsigned upload_maps=f.maps;
 u_default_texture_subdata(c,r,0,0,&pixel,texel,4,4);
 assert(f.maps==upload_maps+1);
 p=c->texture_map(c,r,0,PIPE_MAP_READ,&pixel,&t);assert(p&&!memcmp(p,texel,4));
 c->texture_unmap(c,t);memset(texel,0xcd,4);
 c->texture_subdata(c,r,0,PIPE_MAP_DISCARD_RANGE,&pixel,texel,4,4);
 assert(!remesa_context_error(c));
#endif
 box=(struct pipe_box){.x=2,.y=1,.width=5,.height=3,.depth=1};
 p=c->texture_map(c,r,0,PIPE_MAP_WRITE|PIPE_MAP_FLUSH_EXPLICIT|PIPE_MAP_DISCARD_RANGE,&box,&t);
 assert(p&&t->stride==64&&t->layer_stride==148);
 for(unsigned y=0;y<3;y++)memset(p+y*64,0x7a,20);
 part=(struct pipe_box){.x=1,.y=1,.width=2,.height=2,.depth=1};
 c->transfer_flush_region(c,t,&part);assert(!remesa_context_error(c));
 part.y=2;part.height=2;c->transfer_flush_region(c,t,&part);assert(remesa_context_error(c)==EINVAL);
 c->texture_unmap(c,t);box=(struct pipe_box){.width=11,.height=5,.depth=1};
 p=c->texture_map(c,r,0,PIPE_MAP_READ,&box,&t);assert(p);
 for(unsigned y=0;y<5;y++)for(unsigned x=0;x<64;x++)
  assert(p[y*64+x]==(y>=2&&y<4&&x>=12&&x<20?0x7a:0xcd));
 c->texture_unmap(c,t);
 /* Destroying a context drains every lease, committing only flushed bytes.
  * Resource and screen lifetimes remain valid while mappings own references. */
 box=(struct pipe_box){.width=11,.height=5,.depth=1};
 p=c->texture_map(c,r,0,PIPE_MAP_WRITE|PIPE_MAP_FLUSH_EXPLICIT,&box,&t);assert(p);memset(p,0x44,44);
 part=(struct pipe_box){.width=1,.height=1,.depth=1};c->transfer_flush_region(c,t,&part);
 struct pipe_resource *b=create(s,false);assert(b);
 struct pipe_box one={.width=1,.height=1,.depth=1};
 unsigned char *bp=c->buffer_map(c,b,0,PIPE_MAP_WRITE,&one,&extra);assert(bp);*bp=0x88;
 release(b);c->destroy(c);assert(r->reference.count==1);
 p=other->texture_map(other,r,0,PIPE_MAP_READ,&box,&t);assert(p&&p[0]==0x44&&p[4]==0xcd);
 other->texture_unmap(other,t);release(r);
 /* Closing a screen rejects new maps but still permits orderly lease drain. */
 r=create(s,false);assert(r);bp=other->buffer_map(other,r,0,PIPE_MAP_WRITE,&one,&t);assert(bp);
 s->destroy(s);assert(!other->buffer_map(other,r,0,PIPE_MAP_READ,&one,&extra)&&!extra);
 release(r);other->destroy(other);
 assert(f.allocations==f.frees&&f.maps==f.unmaps&&!f.submits);
 puts("PASS explicit buffer/texture flush, relative bounds, padding, discard synchronization, allocation/map failures, ownership and context lease teardown; CPU backing, zero GPU submissions");
 return 0;
}
