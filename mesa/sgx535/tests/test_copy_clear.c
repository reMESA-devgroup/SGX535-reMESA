/* SPDX-License-Identifier: MIT. Actual Gallium transfers; mocked fences; NO SGX. */
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
#ifdef NDEBUG
#error Assertions must remain enabled
#endif
struct fixture {unsigned alloc,free,maps,unmaps,submitted,waits;int wait_error;unsigned fail_map,attempt;};
static int allocate(void *v,size_t n,void **out){struct fixture *f=v;*out=calloc(1,n);if(!*out)return ENOMEM;f->alloc++;return 0;}
static void drop(void *v,void *bo){((struct fixture *)v)->free++;free(bo);}
static int map_bo(void *v,void *bo,bool w,void **out){struct fixture *f=v;(void)w;f->attempt++;if(f->fail_map==f->attempt)return EIO;f->maps++;*out=bo;return 0;}
static void unmap_bo(void *v,void *bo,bool w){(void)bo;(void)w;((struct fixture *)v)->unmaps++;}
static int submit(void *v,void *bo,const struct remesa_draw_packet *p,void **out){(void)bo;(void)p;(void)out;((struct fixture *)v)->submitted++;return ENOTSUP;}
static int wait(void *v,void *f,uint64_t n,bool visible){(void)f;(void)n;assert(visible);((struct fixture *)v)->waits++;return ((struct fixture *)v)->wait_error;}
static void destroy_fence(void *v,void *f){(void)v;(void)f;}
static struct pipe_resource *image(struct pipe_screen *s,unsigned w,unsigned h,enum pipe_format fmt){struct pipe_resource t={0};t.target=PIPE_TEXTURE_2D;t.format=fmt;t.width0=w;t.height0=h;t.depth0=t.array_size=1;return s->resource_create(s,&t);}
static void release(struct pipe_resource *r){if(!--r->reference.count)r->screen->resource_destroy(r->screen,r);}
static void dimensions(void){
 const unsigned sizes[][2]={{32,32},{320,240},{640,480},{800,600},{1024,768},{1280,720},{1280,800},{1366,768},{1600,900},{1920,1080},{17,23}};
 struct fixture f={0};const struct remesa_device_ops ops={.allocate=allocate,.destroy_bo=drop,.map=map_bo,.unmap=unmap_bo,.submit=submit,.wait=wait,.destroy_fence=destroy_fence};
 struct remesa_device dev={.ops=&ops,.priv=&f,.resource_only_backend=true,.resource_limits={4096,4096,64,64*1024*1024}};
 struct pipe_screen *s=remesa_screen_create(&dev);assert(s);struct pipe_context *c=s->context_create(s,NULL,0);assert(c&&c->resource_copy_region&&c->clear_texture);
 const unsigned char red[4]={9,8,7,6},green[4]={1,2,3,4};
 for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++)for(unsigned fmt=0;fmt<2;fmt++){
  unsigned w=sizes[i][0],h=sizes[i][1];struct pipe_resource *src=image(s,w,h,fmt?PIPE_FORMAT_R8G8B8A8_UNORM:PIPE_FORMAT_B8G8R8A8_UNORM);assert(src);
  struct pipe_resource *dst=image(s,w,h,src->format);assert(dst);
  struct pipe_box all={.width=w,.height=h,.depth=1};struct pipe_transfer *t=NULL;
  unsigned char *p=c->texture_map(c,dst,0,PIPE_MAP_WRITE,&all,&t);assert(p);memset(p,0xcd,t->layer_stride);c->texture_unmap(c,t);
  c->clear_texture(c,src,0,&all,red);assert(!remesa_context_error(c));
  struct pipe_box corner={.x=w-1,.y=h-1,.width=1,.height=1,.depth=1};
  c->clear_texture(c,src,0,&corner,green);assert(!remesa_context_error(c));
  c->resource_copy_region(c,dst,0,0,0,0,src,0,&all);assert(!remesa_context_error(c));
  p=c->texture_map(c,dst,0,PIPE_MAP_READ,&all,&t);assert(p);
  for(unsigned y=0;y<h;y++){
   for(unsigned x=0;x<w;x++)assert(!memcmp(p+(size_t)y*t->stride+x*4,(x==w-1&&y==h-1)?green:red,4));
   for(unsigned x=w*4;x<t->stride;x++)assert(p[(size_t)y*t->stride+x]==0xcd);
  }
  c->texture_unmap(c,t);
  unsigned before=f.maps;corner.width=2;c->clear_texture(c,dst,0,&corner,red);assert(remesa_context_error(c)==EINVAL&&f.maps==before);
  c->resource_copy_region(c,dst,0,0,0,0,src,0,&corner);assert(remesa_context_error(c)==EINVAL&&f.maps==before);
  corner.width=1;c->resource_copy_region(c,dst,0,w,0,0,src,0,&corner);assert(remesa_context_error(c)==EINVAL&&f.maps==before);
  release(src);release(dst);
 }
 c->destroy(c);s->destroy(s);assert(f.alloc==f.free&&f.maps==f.unmaps&&!f.submitted);
 puts("PASS 22 RGBA/BGRA CPU clear/copy sizes incl1280x800/1920x1080: exact pixels, bounds, padding; NO SGX");
}
int main(void){
 dimensions();struct fixture f={0};const struct remesa_device_ops ops={.allocate=allocate,.destroy_bo=drop,.map=map_bo,.unmap=unmap_bo,.submit=submit,.wait=wait,.destroy_fence=destroy_fence};
 struct remesa_device dev={.ops=&ops,.priv=&f,.resource_only_backend=true,.resource_limits={0,0,64,0}};
 struct pipe_screen *s=remesa_screen_create(&dev);struct pipe_context *c=s->context_create(s,NULL,0),*other=s->context_create(s,NULL,0);assert(c&&other);
 struct pipe_resource *a=image(s,9,4,PIPE_FORMAT_B8G8R8A8_UNORM),*b=image(s,9,4,PIPE_FORMAT_B8G8R8A8_UNORM),*bad=image(s,9,4,PIPE_FORMAT_R8G8B8A8_UNORM);assert(a&&b&&bad);
 struct pipe_box box={.width=9,.height=4,.depth=1};struct pipe_transfer *t=NULL;
 unsigned char source[4][36];for(unsigned y=0;y<4;y++)for(unsigned x=0;x<36;x++)source[y][x]=(unsigned char)(x+y*36);
 c->texture_subdata(c,a,0,0,&box,source,36,144);assert(!remesa_context_error(c));
 /* Overlapping self-copy snapshots all source rows before writing destination. */
 struct pipe_box partial={.width=8,.height=3,.depth=1};c->resource_copy_region(c,a,0,1,1,0,a,0,&partial);assert(!remesa_context_error(c));
 unsigned char *p=c->texture_map(c,a,0,PIPE_MAP_READ,&box,&t);assert(p);
 for(unsigned y=1;y<4;y++)assert(!memcmp(p+(size_t)y*t->stride+4,source[y-1],32));
 c->texture_unmap(c,t);
 unsigned before=f.maps;c->resource_copy_region(c,b,0,0,0,0,bad,0,&box);assert(remesa_context_error(c)==EINVAL&&f.maps==before);
 p=c->texture_map(c,a,0,PIPE_MAP_READ,&box,&t);assert(p);before=f.maps;c->resource_copy_region(other,b,0,0,0,0,a,0,&box);assert(remesa_context_error(other)==EBUSY&&f.maps==before);c->texture_unmap(c,t);
 struct pipe_fence_handle *fence=NULL;assert(!remesa_resource_attach_writer(c,a,&f,&fence));f.wait_error=EAGAIN;before=f.maps;
 c->resource_copy_region(other,b,0,0,0,0,a,0,&box);assert(remesa_context_error(other)==EAGAIN&&f.maps==before);
 f.wait_error=0;c->resource_copy_region(other,b,0,0,0,0,a,0,&box);assert(!remesa_context_error(other));s->fence_reference(s,&fence,NULL);
 unsigned unmaps=f.unmaps;f.fail_map=f.attempt+2;uint64_t copies=remesa_context_cpu_copy_count(other);
 other->resource_copy_region(other,b,0,0,0,0,a,0,&box);assert(remesa_context_error(other)==EIO&&f.unmaps==unmaps+1&&remesa_context_cpu_copy_count(other)==copies);f.fail_map=0;
 assert(!remesa_resource_attach_writer(c,b,&f,&fence));f.wait_error=EIO;before=f.maps;
 other->resource_copy_region(other,b,0,0,0,0,a,0,&box);assert(remesa_context_error(other)==EIO&&f.maps==before);
 const unsigned char value[4]={0};other->clear_texture(other,b,0,&box,value);assert(remesa_context_error(other)==EIO&&f.maps==before);s->fence_reference(s,&fence,NULL);
 struct pipe_resource bt={.target=PIPE_BUFFER,.format=PIPE_FORMAT_R8_UNORM,.width0=64,.height0=1,.depth0=1,.array_size=1,.bind=PIPE_BIND_VERTEX_BUFFER};
 struct pipe_resource *buffer=s->resource_create(s,&bt);assert(buffer);
 unsigned char bytes[64];for(unsigned i=0;i<64;i++)bytes[i]=i;
 c->buffer_subdata(c,buffer,0,0,64,bytes);assert(!remesa_context_error(c));
 struct pipe_box range={.x=4,.width=48,.height=1,.depth=1};
 c->resource_copy_region(c,buffer,0,8,0,0,buffer,0,&range);assert(!remesa_context_error(c));
 range.x=0;range.width=64;p=c->buffer_map(c,buffer,0,PIPE_MAP_READ,&range,&t);assert(p);
 assert(!memcmp(p+8,bytes+4,48));c->buffer_unmap(c,t);
 range.width=0;before=f.maps;copies=remesa_context_cpu_copy_count(c);
 c->resource_copy_region(c,buffer,0,64,0,0,buffer,0,&range);assert(!remesa_context_error(c)&&f.maps==before&&remesa_context_cpu_copy_count(c)==copies);
 range.width=1;c->resource_copy_region(c,buffer,0,64,0,0,buffer,0,&range);assert(remesa_context_error(c)==EINVAL&&f.maps==before);
 release(buffer);
 /* A fence outlives its context; CPU-only operations never redispatch draws. */
 c->destroy(c);other->destroy(other);release(a);release(b);release(bad);s->destroy(s);assert(f.alloc==f.free&&f.maps==f.unmaps&&!f.submitted);
 puts("PASS CPU copy overlap, cross-context fences, map failure, terminal quarantine, teardown; NO SGX");
 return 0;
}
