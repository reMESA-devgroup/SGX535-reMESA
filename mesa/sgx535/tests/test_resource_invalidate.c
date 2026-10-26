/* SPDX-License-Identifier: MIT */
/* Real Gallium callbacks with explicit asynchronous CPU oracle; no GPU. */
int inherited_resource_sync_tests(void);
#define main inherited_resource_sync_tests
#include "test_resource_sync.c"
#undef main
int main(void)
{
 struct fixture f={0};
 const struct remesa_device_ops ops={.allocate=allocate,.destroy_bo=drop,
  .map=map_bo,.unmap=unmap_bo,.submit=submit,.wait=wait_job,.destroy_fence=destroy_fence};
 struct remesa_device dev={.ops=&ops,.priv=&f,.offline_test_backend=true};
 struct pipe_screen *s=remesa_screen_create(&dev);assert(s&&s->caps.invalidate_buffer);
 struct pipe_context *a=s->context_create(s,NULL,0),*b=s->context_create(s,NULL,0);
 assert(a&&b&&a->invalidate_resource);
 struct pipe_resource *v=buffer(s,48,PIPE_BIND_VERTEX_BUFFER),*i=buffer(s,6,PIPE_BIND_INDEX_BUFFER),*t=target(s);
 assert(v&&i&&t);
 const float pos[3][4]={{8,8,.5,1},{24,8,.5,1},{8,24,.5,1}};
 const uint16_t ix[3]={0,1,2};
 a->buffer_subdata(a,v,0,0,sizeof(pos),pos);assert(!remesa_context_error(a));
 a->buffer_subdata(a,i,0,0,sizeof(ix),ix);assert(!remesa_context_error(a));
 bind(a,v,t);assert(!remesa_context_set_framebuffer_load(a,true));
 draw(a,i);assert(!remesa_context_error(a)&&f.count==1);
 unsigned maps=f.maps;
 b->invalidate_resource(b,t);assert(remesa_context_error(b)==EAGAIN);
 assert(f.timeout==UINT64_MAX&&f.visible&&f.maps==maps);
 f.jobs[0].state=1; /* command retired without verified CPU visibility */
 b->invalidate_resource(b,t);assert(remesa_context_error(b)==EAGAIN&&f.maps==maps);
 draw(a,i);assert(remesa_context_error(a)==ENOTSUP&&f.count==1);
 published(&f,&f.jobs[0]);
 b->invalidate_resource(b,t);assert(!remesa_context_error(b)&&f.maps==maps);
 /* Explicit discard through another context permits REPLACE without the
  * private diagnostic-discard switch or a preserving backend. */
 draw(a,i);assert(!remesa_context_error(a)&&f.count==2);
 published(&f,&f.jobs[1]);
 struct pipe_box box={.width=32,.height=32,.depth=1};struct pipe_transfer *tr=NULL;
 void *bytes=b->texture_map(b,t,0,PIPE_MAP_WRITE,&box,&tr);assert(bytes);
 memset(bytes,0xa5,4096);
 b->invalidate_resource(b,t);assert(remesa_context_error(b)==EBUSY);
 b->texture_unmap(b,tr);
 draw(a,i);assert(remesa_context_error(a)==ENOTSUP&&f.count==2);
 maps=f.maps;b->invalidate_resource(b,t);assert(!remesa_context_error(b)&&f.maps==maps);
 bytes=b->texture_map(b,t,0,PIPE_MAP_READ,&box,&tr);assert(bytes);
 for(unsigned j=0;j<4096;j++)assert(((unsigned char *)bytes)[j]==0xa5);
 b->texture_unmap(b,tr); /* invalidation never fabricates a clear */
 b->invalidate_resource(b,v);assert(!remesa_context_error(b));
 box=(struct pipe_box){.width=48,.height=1,.depth=1};
 bytes=b->buffer_map(b,v,0,PIPE_MAP_READ,&box,&tr);assert(bytes&&!memcmp(bytes,pos,sizeof(pos)));b->buffer_unmap(b,tr);
 b->invalidate_resource(b,NULL);assert(remesa_context_error(b)==EINVAL);
 struct remesa_device otherdev={.ops=&ops,.priv=&f,.offline_test_backend=true};
 struct pipe_screen *other=remesa_screen_create(&otherdev);assert(other);
 struct pipe_resource *foreign=target(other);assert(foreign);
 b->invalidate_resource(b,foreign);assert(remesa_context_error(b)==EINVAL);
 resource_drop(foreign);other->destroy(other);
 draw(a,i);assert(!remesa_context_error(a)&&f.count==3);
 f.jobs[2].state=3;b->invalidate_resource(b,t);assert(remesa_context_error(b)==EIO);
 published(&f,&f.jobs[2]); /* Later oracle success cannot heal failure. */
 b->invalidate_resource(b,t);assert(remesa_context_error(b)==EIO);
 draw(a,i);assert(f.count==3&&remesa_context_error(a)==ENOTSUP);
 a->destroy(a);resource_drop(v);resource_drop(i);resource_drop(t);
 s->destroy(s);b->invalidate_resource(b,NULL);assert(remesa_context_error(b)==ESHUTDOWN);
 b->destroy(b);assert(f.allocated==f.freed);
 puts("PASS: cross-context discard/reuse, pending/visibility/lease/error guards, unchanged backing and no implicit clear; CPU oracle only, NO SGX");
 return 0;
}
