/* SPDX-License-Identifier: MIT */
/* Full-size allocation/VA/descriptor integration, injected CPU provider only. */
int persistent_fixture_regressions(void);
#define main persistent_fixture_regressions
#include "test_persistent_shader.c"
#undef main
#include "remesa_depth_scene_image.h"
static int large_create(void *data,size_t n,uint32_t *id)
{
 struct fixture *f=data;CHECK(n&&!(n&4095)&&n<=8u*1024*1024&&f->creates<64);
 struct backing *b=&f->backing[f->creates];b->bytes=malloc(n);CHECK(b->bytes);
 memset(b->bytes,0xa5,n);b->length=n;b->live=true;*id=++f->creates;return 0;
}
static struct remesa_shader_image *image_for(uint32_t color)
{
 struct remesa_shader_draw_programs pair=programs(color);
 struct remesa_usse_vertex_mapping mapping={0};
 for(unsigned c=0;c<4;c++){mapping.input[c]=(struct remesa_usse_cpu_register){2,c};mapping.output[c]=(struct remesa_usse_cpu_register){1,c};}
 struct remesa_shader_image *image=NULL;CHECK(!remesa_shader_image_prepare_vertex_candidate(&pair,&mapping,&image));return image;
}
static int large_depth_resources(bool placed)
{
 struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
 struct remesa_persistent_ops large_ops=ops;large_ops.create=large_create;
 struct remesa_persistent_config config={.ops=&large_ops,.priv=&f,.context=1700,
  .va_start=0x40000000,.va_end=0x42000000,.byte_budget=32u*1024*1024,.resource_only_backend=true};
 if(placed) {
  config.va_start=0x20000000;
  config.placement[REMESA_VA_DEFAULT]=(struct remesa_persistent_va_range){0x40000000,0x42000000,0};
  config.placement[REMESA_VA_SURFACE]=config.placement[REMESA_VA_DEFAULT];
  config.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20000000,0x20080000,32768};
  config.placement[REMESA_VA_RASTER]=(struct remesa_persistent_va_range){0x30000000,0x40000000,1048576};
 }
 CHECK(!remesa_persistent_open(&d,&config));
 void *color=NULL,*depth=NULL,*scene=NULL,*region=NULL;CHECK(!d.ops->allocate(d.priv,4096000,&color));
 CHECK(!remesa_persistent_depth_allocate(&d,1280,800,4,&depth));
 struct remesa_persistent_render_target_backing c;
 struct remesa_persistent_depth_backing z;
 CHECK(!remesa_persistent_render_target_snapshot(&d,color,1280,800,1280,&c));
 CHECK(!remesa_persistent_depth_snapshot(&d,depth,&z));
 CHECK(c.owner==z.owner&&c.handle!=z.handle&&c.allocation_bytes==4096000&&z.allocation_bytes==4096000);
 CHECK(c.gpu_va+c.allocation_bytes<=z.gpu_va||z.gpu_va+z.allocation_bytes<=c.gpu_va);
 CHECK(c.state.stride_bytes==5120&&c.state.image_bytes==4096000&&z.pitch_pixels==1280);
 CHECK(!remesa_persistent_scene_allocate(&d,1280,800,&region));
 struct remesa_persistent_scene_backing hw;
 CHECK(!remesa_persistent_scene_snapshot(&d,region,&hw));
 CHECK(hw.owner==c.owner&&hw.width==1280&&hw.height==800);
 CHECK(hw.layout.required_bytes==0x1d320&&hw.allocation_bytes==0x1e000&&hw.layout.allocation_pages==30);
 CHECK(hw.region_gpu_va==hw.gpu_va+0x10000&&hw.layout.clear_page_count==16);
 CHECK(hw.gpu_va>=z.gpu_va+z.allocation_bytes);
 bool all_zero=true;for(size_t i=0;i<hw.allocation_bytes;i++)all_zero&=get(&f,hw.handle)->bytes[i]==0;CHECK(all_zero);
 void *not_writable=(void *)1;CHECK(d.ops->map(d.priv,region,true,&not_writable)==ENOTSUP&&!not_writable);
 void *failed=NULL;unsigned created_before=f.creates;
 CHECK(remesa_persistent_scene_allocate(&d,0,800,&failed)==EINVAL&&!failed&&f.creates==created_before);
 f.fail=FAIL_READ_BODY;
 CHECK(remesa_persistent_scene_allocate(&d,1280,800,&failed)==EPROTO&&!failed);
 CHECK(f.creates==f.destroys+3);f.fail=FAIL_NONE;
 struct remesa_shader_image *images[]={image_for(0xff00ff00),image_for(0xffff00ff),image_for(0xffffff00)};
 const struct remesa_depth_scene_image_request request={.programs={images[0],images[1],images[2]},
 .width=1280,.height=800,.vertex_bank=placed?4:2,.pixel_bank=3,.state_bank=4,
 .triangle={{160,100},{1120,100},{160,675}},.draw_depth={.75,.25,.75},.clear_depth=1,
 .compare=REMESA_DEPTH_LESS,.depth_write=true};
 CHECK(!remesa_persistent_depth_scene_prepare(&d,&request,&scene));
 struct remesa_persistent_depth_scene_backing s;
 CHECK(!remesa_persistent_depth_scene_snapshot(&d,scene,&s));
 CHECK(s.owner==c.owner&&s.handle!=c.handle&&s.handle!=z.handle&&s.ta_bytes==124);
 CHECK((placed?(s.gpu_va==0x20000000):(s.gpu_va>=hw.gpu_va+hw.allocation_bytes))&&f.creates==f.destroys+4);
 if(placed) {
  CHECK(c.gpu_va==0x40000000&&z.gpu_va==c.gpu_va+c.allocation_bytes);
  CHECK(s.use_entries[0].register_word==s.use_entries[2].register_word);
 }
 uint8_t prefix[64];const struct remesa_depth_prefix_request pr={
 .depth={1280,800,z.pitch_pixels,z.cpp,REMESA_DEPTH_SURFACE_UNDEFINED,z.gpu_va,z.allocation_bytes},
 .color_width=1280,.color_height=800,.clear_depth=1};
 CHECK(!remesa_depth_prefix_encode(&pr,prefix));
 CHECK(shader_pds_le32(prefix)==0x480&&shader_pds_le32(prefix+56)==0x4b8);
 /* Exercise every page of the full color backing. This is CPU transport,
  * not a GPU-produced image or a qualified native response/readback. */
 void *mapped=NULL;CHECK(!d.ops->map(d.priv,color,true,&mapped));
 for(size_t i=0;i<4096000;i++)((uint8_t *)mapped)[i]=(uint8_t)((i>>12)^i);
 d.ops->unmap(d.priv,color,true);
 CHECK(!d.ops->map(d.priv,color,false,&mapped));
 bool equal=true;for(size_t i=0;i<4096000;i++)equal&=((uint8_t *)mapped)[i]==(uint8_t)((i>>12)^i);CHECK(equal);
 struct remesa_persistent_render_target_backing saved=c;
 CHECK(remesa_persistent_render_target_snapshot(&d,color,1280,800,1280,&c)==EBUSY&&!memcmp(&c,&saved,sizeof(c)));
 d.ops->unmap(d.priv,color,false);
 CHECK(remesa_persistent_render_target_snapshot(&d,color,1280,801,1280,&c)==ENOSPC&&!memcmp(&c,&saved,sizeof(c)));
 CHECK(remesa_persistent_render_target_snapshot(&d,depth,1280,800,1280,&c)==ENOTSUP);
 CHECK(remesa_persistent_render_target_snapshot(&d,scene,1280,800,1280,&c)==ENOTSUP);
 CHECK(remesa_persistent_render_target_snapshot(&d,region,1280,800,1280,&c)==ENOTSUP);
 for(unsigned i=0;i<3;i++)remesa_shader_image_destroy(images[i]);
 d.ops->destroy_bo(d.priv,color);d.ops->destroy_bo(d.priv,depth);d.ops->destroy_bo(d.priv,scene);d.ops->destroy_bo(d.priv,region);
 CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);CHECK(!remesa_persistent_close(&d));
 printf("PASS full1280x800 owned color/Z/region/compact scene: distinct live VAs, derived descriptors,1000-page CPU color transport, bounds/lease/type refusal; NO GPU (%u checks)\n",checks);
 return 0;
}

#ifndef REMESA_VA_PLACEMENT_FIXTURE
int main(void) {return large_depth_resources(false);}
#endif
