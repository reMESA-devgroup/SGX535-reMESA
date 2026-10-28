/* SPDX-License-Identifier: MIT. Frontend writer gates, injected callbacks only. */
int legacy_copy_clear_fixture(void);
#define main legacy_copy_clear_fixture
#include "test_copy_clear.c"
#undef main
static unsigned native_calls;static int native_error;
static int native_copy(void *priv,const struct remesa_copy_region *r)
{(void)priv;assert(r->source.bo&&r->destination.bo&&r->width==16&&r->height==16);native_calls++;return native_error;}
int main(void)
{
 struct fixture f={0};const struct remesa_device_ops ops={.allocate=allocate,.destroy_bo=drop,.map=map_bo,.unmap=unmap_bo,.submit=submit,.wait=wait,.destroy_fence=destroy_fence};
 struct remesa_device d={.ops=&ops,.priv=&f,.resource_only_backend=true};struct pipe_screen *s=remesa_screen_create(&d);assert(s);
 assert(!remesa_screen_enable_native_copy(s,native_copy));struct pipe_context *c=s->context_create(s,NULL,0),*other=s->context_create(s,NULL,0);assert(c&&other);
 struct pipe_resource *a=image(s,32,32,PIPE_FORMAT_B8G8R8A8_UNORM),*b=image(s,32,32,PIPE_FORMAT_B8G8R8A8_UNORM);assert(a&&b);
 struct pipe_box box={.width=16,.height=16,.depth=1};struct pipe_fence_handle *fence=NULL;
 assert(!remesa_resource_attach_writer(c,a,&f,&fence));f.wait_error=EAGAIN;unsigned maps=f.maps;
 other->resource_copy_region(other,b,0,0,0,0,a,0,&box);assert(remesa_context_error(other)==EAGAIN&&!native_calls&&f.maps==maps);
 f.wait_error=0;other->resource_copy_region(other,b,0,0,0,0,a,0,&box);assert(!remesa_context_error(other)&&native_calls==1&&f.maps==maps);s->fence_reference(s,&fence,NULL);
 assert(!remesa_resource_attach_writer(c,b,&f,&fence));f.wait_error=ETIMEDOUT;
 other->resource_copy_region(other,b,0,0,0,0,a,0,&box);assert(remesa_context_error(other)==ETIMEDOUT&&native_calls==1&&f.maps==maps);
 f.wait_error=0;other->resource_copy_region(other,b,0,0,0,0,a,0,&box);assert(!remesa_context_error(other)&&native_calls==2);s->fence_reference(s,&fence,NULL);
 native_error=ENOTSUP;other->resource_copy_region(other,b,0,0,0,0,a,0,&box);assert(remesa_context_error(other)==ENOTSUP&&native_calls==3&&f.maps==maps&&!remesa_context_cpu_copy_count(other));
 native_error=EOWNERDEAD;other->resource_copy_region(other,b,0,0,0,0,a,0,&box);assert(remesa_context_error(other)==EOWNERDEAD&&native_calls==4);
 struct pipe_transfer *t=NULL;assert(!c->texture_map(c,a,0,PIPE_MAP_READ,&box,&t)&&remesa_context_error(c)==EOWNERDEAD);
 assert(!c->texture_map(c,b,0,PIPE_MAP_WRITE,&box,&t)&&remesa_context_error(c)==EOWNERDEAD&&f.maps==maps);
 assert(remesa_context_native_copy_count(other)==2&&!remesa_context_cpu_copy_count(other));
 c->destroy(c);other->destroy(other);release(a);release(b);s->destroy(s);assert(f.alloc==f.free&&!f.submitted);
 puts("PASS native-copy source/destination imported writer waits, cross-context publication, timeout, no CPU fallback and two-resource terminal frontend failure; GPU0");return 0;
}
