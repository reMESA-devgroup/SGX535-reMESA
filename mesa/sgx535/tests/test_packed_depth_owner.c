/* SPDX-License-Identifier: MIT */
/* Injected provider only: ownership of generated1280x800 scene bytes. */
int persistent_fixture_regressions(void);
#define main persistent_fixture_regressions
#include "test_persistent_shader.c"
#undef main
#include "remesa_depth_scene_image.h"
static struct remesa_shader_image *image_for(uint32_t color)
{
 struct remesa_shader_draw_programs pair=programs(color);
 struct remesa_usse_vertex_mapping mapping={0};
 for(unsigned c=0;c<4;c++){mapping.input[c]=(struct remesa_usse_cpu_register){2,c};mapping.output[c]=(struct remesa_usse_cpu_register){1,c};}
 struct remesa_shader_image *image=NULL;CHECK(!remesa_shader_image_prepare_vertex_candidate(&pair,&mapping,&image));return image;
}
int main(void)
{
 struct remesa_shader_image *images[]={image_for(0xff00ff00),image_for(0xffff00ff),image_for(0xffffff00)};
 struct remesa_depth_scene_image_request r={.programs={images[0],images[1],images[2]},
  .width=1280,.height=800,.vertex_bank=2,.pixel_bank=3,.state_bank=4,
  .triangle={{160,100},{1120,100},{160,675}},.draw_depth={.75,.25,.75},.clear_depth=1,
  .compare=REMESA_DEPTH_LESS,.depth_write=true};
 struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
 open_device(&f,&d,1600);void *scene=NULL;
 CHECK(!remesa_persistent_depth_scene_prepare(&d,&r,&scene)&&scene);
 CHECK(f.creates==1&&f.pins==1);
 struct remesa_persistent_depth_scene_backing view;
 CHECK(!remesa_persistent_depth_scene_snapshot(&d,scene,&view));
 CHECK(view.vertex_profile==REMESA_DEPTH_VERTEX_COPY);
 CHECK(view.owner==1600&&view.gpu_va==0x100000&&view.handle==1);
 CHECK(view.width==1280&&view.height==800&&view.allocation_bytes==4096&&view.used_bytes<4096&&view.ta_bytes==124);
 struct remesa_depth_scene_image expected;
 r.gpu_va=view.gpu_va;r.allocation_bytes=view.allocation_bytes;
 CHECK(!remesa_depth_scene_image_encode(&r,&expected));
 CHECK(!memcmp(f.backing[0].bytes,expected.bytes,4096));
 CHECK(!memcmp(view.use_entries,expected.use_entries,sizeof(view.use_entries)));
 r.gpu_va=r.allocation_bytes=0;
 /* The explicit programmable profile survives allocation, VA derivation,
  * immutable write/readback and lease release without changing the copy BO. */
 void *compiled=NULL;r.vertex_profile=REMESA_DEPTH_VERTEX_COMPILED_POSITION4;
 CHECK(!remesa_persistent_depth_scene_prepare(&d,&r,&compiled)&&compiled);
 struct remesa_persistent_depth_scene_backing compiled_view;
 CHECK(!remesa_persistent_depth_scene_snapshot(&d,compiled,&compiled_view));
 CHECK(compiled_view.vertex_profile==REMESA_DEPTH_VERTEX_COMPILED_POSITION4);
 CHECK(compiled_view.handle!=view.handle&&compiled_view.gpu_va!=view.gpu_va);
 r.gpu_va=compiled_view.gpu_va;r.allocation_bytes=compiled_view.allocation_bytes;
 struct remesa_depth_scene_image compiled_expected;
 CHECK(!remesa_depth_scene_image_encode(&r,&compiled_expected));
 const uint8_t *compiled_bytes=get(&f,compiled_view.handle)->bytes;
 CHECK(!memcmp(compiled_bytes,compiled_expected.bytes,4096));
 for(unsigned i=0;i<4;i++) {
  CHECK(shader_pds_le32(compiled_bytes+compiled_expected.vertex_pds[i]+20)==0xe0000000u);
  CHECK(shader_pds_le32(compiled_bytes+compiled_expected.vertex_pds[i]+36)==12);
  CHECK(((shader_pds_le32(compiled_bytes+compiled_expected.ta_offset+28*i+24)>>25)&31)==4);
 }
 CHECK(!remesa_persistent_depth_scene_verify(&d,compiled));
 d.ops->destroy_bo(d.priv,compiled);
 CHECK(!remesa_persistent_depth_scene_verify(&d,scene));
 CHECK(!memcmp(f.backing[0].bytes,expected.bytes,4096));
 r.gpu_va=r.allocation_bytes=0;r.vertex_profile=REMESA_DEPTH_VERTEX_COPY;
 struct remesa_shader_image_view source;CHECK(!remesa_shader_image_inspect(images[0],&source));
 uint8_t preserved[16];memcpy(preserved,source.vertex.bytes,16);
 CHECK(remesa_persistent_depth_scene_prepare(&d,&r,(void *)source.vertex.bytes)==EINVAL);
 CHECK(!memcmp(preserved,source.vertex.bytes,16));
 void *failed=NULL;r.state_bank=3;
 CHECK(remesa_persistent_depth_scene_prepare(&d,&r,&failed)==ENOTSUP&&!failed);
 CHECK(f.creates==f.destroys+1);r.state_bank=4;
 f.fail=FAIL_READ_BODY;
 CHECK(remesa_persistent_depth_scene_prepare(&d,&r,&failed)==EPROTO&&!failed);
 CHECK(f.creates==f.destroys+1);f.fail=FAIL_NONE;
 for(unsigned i=0;i<3;i++)remesa_shader_image_destroy(images[i]);
 CHECK(!remesa_persistent_depth_scene_verify(&d,scene)); /* Host images are gone. */
 void *mapped=(void *)1;CHECK(d.ops->map(d.priv,scene,true,&mapped)==ENOTSUP&&!mapped);
 CHECK(!d.ops->map(d.priv,scene,false,&mapped));
 struct remesa_persistent_depth_scene_backing saved=view;
 CHECK(remesa_persistent_depth_scene_snapshot(&d,scene,&view)==EBUSY&&!memcmp(&saved,&view,sizeof(view)));
 unsigned destroyed_before=f.destroys;
 d.ops->destroy_bo(d.priv,scene);CHECK(f.destroys==destroyed_before&&remesa_persistent_close(&d)==EBUSY);
 d.ops->unmap(d.priv,scene,false);CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);
 CHECK(!remesa_persistent_close(&d));
 /* Rejected caller VA and failed encoding leave the output empty and release
  * only tentative allocation; they do not alter admission or resource limits. */
 struct fixture bad={.allow_gpu_map=true,.allow_sync=true};struct remesa_device other={0};open_device(&bad,&other,1601);
 scene=NULL;r.gpu_va=0x40000000;CHECK(remesa_persistent_depth_scene_prepare(&other,&r,&scene)==EINVAL&&!scene&&!bad.creates);
 r.gpu_va=r.allocation_bytes=0;r.programs[0]=r.programs[1]=r.programs[2]=NULL;
 CHECK(remesa_persistent_depth_scene_prepare(&other,&r,&scene)!=0&&!scene);
 CHECK(bad.creates==bad.destroys);CHECK(!remesa_persistent_close(&other));
 printf("PASS: packed1280x800 scene owner: one mapped allocation, derived VA, exact readback, host-independent lifetime, deferred close, write/foreignVA refusal; NO GPU (%u checks)\n",checks);
 return 0;
}
