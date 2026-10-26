/* SPDX-License-Identifier: MIT */
#define REMESA_LAB_STORAGE_FIXTURE
#include "test_lab_scene_storage.c"
int main(void)
{
 uint8_t oracle[208],bytes[208],saved[208];CHECK(fread(oracle,1,208,stdin)==208);
 struct remesa_shader_image *images[]={image_for(0xff00ff00),image_for(0xffff00ff),image_for(0xffffff00)};
 struct remesa_depth_scene_image_request r=request(images);
 struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
 struct remesa_persistent_ops o=ops;o.create=native_create;
 struct remesa_persistent_config c=config(&f,&o);CHECK(!remesa_persistent_open(&d,&c));
 struct remesa_lab_scene_storage s={0};CHECK(!remesa_lab_scene_storage_prepare(&d,&r,&s));
 CHECK(!remesa_lab_scene_raster_encode(&s,0,0x44c0,128,16,bytes));
 CHECK(!memcmp(bytes,oracle,208)); /* Whole independent original constructor output. */
 CHECK(get(&f,s.bindings[REMESA_LAB_AUX_PDS].handle)->bytes[0x44c0]==0xa5);
 CHECK(get(&f,s.bindings[REMESA_LAB_CONTROL].handle)->bytes[0]==0xa5); /* plan only */
 uint8_t control[264];
 CHECK(!remesa_lab_scene_control_encode(&s,0,0x44c0,128,16,control));
 CHECK(!memcmp(control,oracle,208));
 const uint32_t expected_ta[14]={0x204,0,0x218,0x400000,0x23c,0x82,
  0x240,0x358637bd,0x244,0x358637bd,0x250,0x88,0x238,(uint32_t)(s.generated.gpu_va+s.generated.ta_offset)};
 for(unsigned i=0;i<14;i++)CHECK(shader_pds_le32(control+208+i*4)==expected_ta[i]);
 uint8_t unchanged[264];memcpy(unchanged,control,264);
 CHECK(remesa_lab_scene_control_encode(&s,0,UINT64_MAX,128,16,control)==EINVAL&&!memcmp(control,unchanged,264));
 s.bindings[REMESA_LAB_GENERATED].generation++;
 CHECK(remesa_lab_scene_control_encode(&s,0,0x44c0,128,16,control)==ESTALE&&!memcmp(control,unchanged,264));
 s.bindings[REMESA_LAB_GENERATED].generation--;
 memcpy(saved,bytes,sizeof(bytes));
 CHECK(remesa_lab_scene_raster_encode(&s,0,0x44c0,128,32,bytes)==ENOTSUP&&!memcmp(bytes,saved,208));
 CHECK(remesa_lab_scene_raster_encode(&s,4096,0x44c0,128,16,bytes)==EINVAL&&!memcmp(bytes,saved,208));
 CHECK(remesa_lab_scene_raster_encode(&s,0,UINT64_MAX,128,16,bytes)==EINVAL&&!memcmp(bytes,saved,208));
 s.bindings[REMESA_LAB_COLOR].generation++;
 CHECK(remesa_lab_scene_raster_encode(&s,0,0x44c0,128,16,bytes)==ESTALE&&!memcmp(bytes,saved,208));
 s.bindings[REMESA_LAB_COLOR].generation--;
 CHECK(remesa_lab_scene_raster_encode(&s,0,0x44c0,128,16,(void *)&s)==EINVAL);
 void *cpu=NULL;CHECK(!d.ops->map(d.priv,s.objects[REMESA_LAB_CONTROL],true,&cpu));
 CHECK(remesa_lab_scene_raster_encode(&s,0,0x44c0,128,16,bytes)==EBUSY&&!memcmp(bytes,saved,208));
 d.ops->unmap(d.priv,s.objects[REMESA_LAB_CONTROL],true);
 remesa_lab_scene_storage_destroy(&s);CHECK(f.creates==f.destroys&&!f.forbidden&&f.pins==f.unpins);
 CHECK(!remesa_persistent_close(&d));
 for(unsigned i=0;i<3;i++)remesa_shader_image_destroy(images[i]);
 printf("PASS owned1280 raster208 bytes match original whole constructor; stale/lease/alias/metadata/bounds refusal, auxiliary/control unchanged; CPU ONLY (%u checks)\n",checks);
 return 0;
}
