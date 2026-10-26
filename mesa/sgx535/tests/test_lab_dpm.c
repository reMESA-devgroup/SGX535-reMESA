/* SPDX-License-Identifier: MIT */
/* Original x86 info/load outputs arrive on stdin; no GPU/syscall emulator. */
#define REMESA_LAB_STORAGE_FIXTURE
#include "test_lab_scene_storage.c"
static uint32_t word(void)
{uint32_t v;CHECK(scanf("%u",&v)==1);return v;}
static void rejected(struct remesa_lab_scene_storage *s,int e)
{
 struct remesa_lab_dpm_control out,saved;
 memset(&out,0xa5,sizeof(out));saved=out;
 CHECK(remesa_lab_scene_dpm_encode(s,&out)==e);
 CHECK(!memcmp(&out,&saved,sizeof(out)));
}
int main(void)
{
 struct remesa_shader_image *images[]={image_for(0xff00ff00),image_for(0xffff00ff),image_for(0xffffff00)};
 for(unsigned variant=0;variant<2;variant++) {
  uint32_t table=word(),parameter=word(),cookie[13],pairs[44];
  for(unsigned i=0;i<13;i++)cookie[i]=word();
  for(unsigned i=0;i<44;i++)pairs[i]=word();
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device device={0};
  struct remesa_persistent_ops o=ops;o.create=native_create;
  struct remesa_persistent_config cfg=config(&f,&o);
  if(variant) {
   cfg.placement[REMESA_VA_RASTER].start=0x31000000;
   cfg.reserved_count=1;cfg.reserved[0].start=0x40000000;cfg.reserved[0].end=0x41000000;
  }
  CHECK(!remesa_persistent_open(&device,&cfg));
  struct remesa_depth_scene_image_request r=request(images);
  r.vertex_profile=REMESA_DEPTH_VERTEX_COMPILED_POSITION4;
  struct remesa_lab_scene_storage scene={0};
  CHECK(!remesa_lab_scene_storage_prepare(&device,&r,&scene));
  CHECK(scene.generated.vertex_profile==REMESA_DEPTH_VERTEX_COMPILED_POSITION4);
  CHECK(scene.bindings[REMESA_LAB_TA_TABLE].gpu_va==table);
  CHECK(scene.bindings[REMESA_LAB_TA_PARAMETER].gpu_va==parameter);
  struct remesa_lab_dpm_control out;
  CHECK(!remesa_lab_scene_dpm_encode(&scene,&out));
  CHECK(!memcmp(out.cookie,cookie,sizeof(cookie)));
  CHECK(!memcmp(out.load_pairs,pairs,sizeof(pairs)));
  CHECK(out.info_bytes==0x620000);
  CHECK(out.original_wait_mask==7&&out.retained_completion_mask==15);
  CHECK(get(&f,scene.bindings[REMESA_LAB_TA_TABLE].handle)->bytes[0]==0xa5);
  CHECK(get(&f,scene.bindings[REMESA_LAB_TA_PARAMETER].handle)->bytes[0]==0xa5);
  struct remesa_lab_scene_storage saved=scene;
  CHECK(remesa_lab_scene_dpm_encode(&scene,(void *)&scene)==EINVAL);
  CHECK(!memcmp(&scene,&saved,sizeof(scene)));
  scene.bindings[REMESA_LAB_TA_TABLE].generation++;
  rejected(&scene,ESTALE);scene=saved;
  scene.bindings[REMESA_LAB_TA_PARAMETER].gpu_va+=4096;
  rejected(&scene,ESTALE);scene=saved;
  void *mapped=NULL;
  CHECK(!device.ops->map(device.priv,scene.objects[REMESA_LAB_TA_TABLE],true,&mapped));
  rejected(&scene,EBUSY);
  device.ops->unmap(device.priv,scene.objects[REMESA_LAB_TA_TABLE],true);
  CHECK(!remesa_lab_scene_dpm_encode(&scene,&out));
  remesa_lab_scene_storage_destroy(&scene);rejected(&scene,EINVAL);
  CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);
  CHECK(!remesa_persistent_close(&device));
 }
 for(unsigned i=0;i<3;i++)remesa_shader_image_destroy(images[i]);
 printf("PASS2 original Xpsb info/load CPU oracles:13 cookies/22 store pairs from live1280 owners, relocated domains, old wait7 versus retained15, no TA content mutation, stale/alias/lease/rollback refusal; NO GPU (%u checks)\n",checks);
 return 0;
}
