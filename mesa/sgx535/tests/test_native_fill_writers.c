/* SPDX-License-Identifier: MIT. Actual Gallium writer gates; GPU0. */
int legacy_copy_clear_fixture(void);
#define main legacy_copy_clear_fixture
#include "test_copy_clear.c"
#undef main
static unsigned calls;static int fill_error;
static int fill(void *priv,const struct remesa_fill_region *r)
{(void)priv;assert(r->destination.bo&&r->width==16&&r->height==16);calls++;return fill_error;}
int main(void)
{
 struct fixture f={0};const struct remesa_device_ops ops={.allocate=allocate,.destroy_bo=drop,.map=map_bo,.unmap=unmap_bo,.submit=submit,.wait=wait,.destroy_fence=destroy_fence};
 struct remesa_device d={.ops=&ops,.priv=&f,.resource_only_backend=true};struct pipe_screen *s=remesa_screen_create(&d);assert(s);
 assert(!remesa_screen_enable_native_fill(s,fill));struct pipe_context *c=s->context_create(s,NULL,0),*other=s->context_create(s,NULL,0);assert(c&&other);
 struct pipe_resource *a=image(s,32,32,PIPE_FORMAT_B8G8R8A8_UNORM);assert(a);struct pipe_box box={.width=16,.height=16,.depth=1};const unsigned char pixel[4]={1,2,3,4};
 struct pipe_fence_handle *fence=NULL;assert(!remesa_resource_attach_writer(c,a,&f,&fence));f.wait_error=EAGAIN;unsigned maps=f.maps;
 other->clear_texture(other,a,0,&box,pixel);assert(remesa_context_error(other)==EAGAIN&&!calls&&f.maps==maps);
 f.wait_error=ETIMEDOUT;other->clear_texture(other,a,0,&box,pixel);assert(remesa_context_error(other)==ETIMEDOUT&&!calls&&f.maps==maps);
 f.wait_error=0;other->clear_texture(other,a,0,&box,pixel);assert(!remesa_context_error(other)&&calls==1&&f.maps==maps);s->fence_reference(s,&fence,NULL);
 fill_error=ENOTSUP;other->clear_texture(other,a,0,&box,pixel);assert(remesa_context_error(other)==ENOTSUP&&calls==2&&f.maps==maps&&!remesa_context_cpu_clear_count(other));
 fill_error=-1;other->clear_texture(other,a,0,&box,pixel);assert(remesa_context_error(other)==EOWNERDEAD&&calls==3);
 struct pipe_transfer *t=NULL;assert(!c->texture_map(c,a,0,PIPE_MAP_READ,&box,&t)&&remesa_context_error(c)==EOWNERDEAD&&f.maps==maps);
 assert(remesa_context_native_fill_count(other)==1&&!remesa_context_cpu_clear_count(other));
 c->destroy(c);other->destroy(other);release(a);s->destroy(s);assert(f.alloc==f.free&&!f.submitted);
 puts("PASS native-fill imported writer cross-context wait/timeout, no CPU fallback and malformed callback terminal failure; GPU0");return 0;
}
