/* SPDX-License-Identifier: MIT */
/* All native LAB storage roles through the existing owner; CPU provider only. */
#define REMESA_VA_PLACEMENT_FIXTURE
#include "test_large_depth_resources.c"
#include "../../../tools/remesa/lab_scene_storage.h"
static unsigned fail_create_at;
static int native_create(void *data,size_t n,uint32_t *id)
{
 struct fixture *f=data;
 CHECK(n&&!(n&4095)&&n<=0x2000000&&f->creates<64);
 if(fail_create_at&&f->creates+1==fail_create_at)return ENOMEM;
 struct backing *b=&f->backing[f->creates];b->bytes=malloc(n);CHECK(b->bytes);
 memset(b->bytes,0xa5,n);b->length=n;b->live=true;*id=++f->creates;return 0;
}
static struct remesa_depth_scene_image_request request(struct remesa_shader_image **images)
{
 return (struct remesa_depth_scene_image_request){.programs={images[0],images[1],images[2]},
 .width=1280,.height=800,.vertex_bank=4,.pixel_bank=3,.state_bank=4,
 .triangle={{160,100},{1120,100},{160,675}},.draw_depth={.75,.25,.75},.clear_depth=1,
 .compare=REMESA_DEPTH_LESS,.depth_write=true};
}
static struct remesa_persistent_config config(struct fixture *f,struct remesa_persistent_ops *p)
{
 struct remesa_persistent_config c={.ops=p,.priv=f,.context=1900,.va_start=0x20000000,
 .va_end=0x48000000,.byte_budget=128u*1024*1024,.resource_only_backend=true};
 c.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20000000,0x20080000,32768};
 c.placement[REMESA_VA_RASTER]=(struct remesa_persistent_va_range){0x30000000,0x40000000,1048576};
 c.placement[REMESA_VA_SURFACE]=(struct remesa_persistent_va_range){0x40000000,0x48000000,0};
 c.placement[REMESA_VA_DEFAULT]=c.placement[REMESA_VA_SURFACE];return c;
}
static int lab_scene_storage_checks(void)
{
 struct remesa_shader_image *images[]={image_for(0xff00ff00),image_for(0xffff00ff),image_for(0xffffff00)};
 struct remesa_depth_scene_image_request r=request(images);
 for(unsigned mode=0;mode<7;mode++) {
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  struct remesa_persistent_ops o=ops;o.create=native_create;
  struct remesa_persistent_config c=config(&f,&o);
  if(mode==5) {
   c.placement[REMESA_VA_PROGRAM].end=0x20100000;
   c.reserved_count=1;c.reserved[0].start=0x20028000;c.reserved[0].end=0x20080000;
  }
  if(mode==6)c.placement[REMESA_VA_RASTER]=(struct remesa_persistent_va_range){0x40000000,0x48000000,1048576};
  CHECK(!remesa_persistent_open(&d,&c));
  struct remesa_lab_scene_storage s={0},prior=s;
  fail_create_at=mode==1?1:mode==2?6:mode==3?11:0;
  if(mode==4)r.gpu_va=0x20000000;
  int e=remesa_lab_scene_storage_prepare(&d,&r,&s);
  if(mode) {
   CHECK(e==(mode==4?EINVAL:mode>=5?ENOTSUP:ENOMEM)&&!memcmp(&s,&prior,sizeof(s)));
   CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);
   CHECK(!remesa_persistent_close(&d));r.gpu_va=0;continue;
  }
  CHECK(!e&&f.creates==11&&s.device==&d);
  for(unsigned i=0;i<REMESA_LAB_SCENE_ROLES;i++)
   printf("CPU_BINDING role=%u owner=%llu handle=%u va=0x%08llx bytes=%zu\n",i,
    (unsigned long long)s.bindings[i].owner,s.bindings[i].handle,
    (unsigned long long)s.bindings[i].gpu_va,s.bindings[i].allocation_bytes);
  CHECK(s.bindings[REMESA_LAB_AUX_PDS].gpu_va==0x20000000);
  CHECK(s.bindings[REMESA_LAB_AUX_USE].gpu_va==0x20020000);
  CHECK(s.generated.gpu_va==0x20028000&&s.generated.ta_bytes==124);
  /* Independently recorded original Xpsb scene_info -> store-prefix replay
   * at1280x800, not encoder/decoder roundtrip. Base comes from live ownership. */
  const uint32_t expected_ta[14]={0x220,0x407d1000,0x208,0x0a028028,
   0x20c,0x0701c01c,0x214,0x460,0x210,0x3104f,0x248,0x31f4ff,0x21c,0x407e1000};
  CHECK(!memcmp(s.ta_scene,expected_ta,sizeof(expected_ta)));
  CHECK(s.raster_scene[0]==0x408&&s.raster_scene[1]==0x407e1000);
  CHECK(s.raster_scene[1]!=s.region.gpu_va+0x1000);
  for(unsigned i=0;i<7;i++)printf("CPU_SCENE_TA reg=0x%x value=0x%08x\n",s.ta_scene[i*2],s.ta_scene[i*2+1]);
  printf("CPU_SCENE_RASTER reg=0x%x value=0x%08x\n",s.raster_scene[0],s.raster_scene[1]);
  CHECK(s.color.allocation_bytes==4096000&&s.depth.allocation_bytes==4096000&&s.region.allocation_bytes==0x1e000);
  CHECK(s.bindings[REMESA_LAB_TA_TABLE].allocation_bytes==0x2000000);
  CHECK(s.bindings[REMESA_LAB_TA_PARAMETER].gpu_va==0x30100000);
  CHECK(s.bindings[REMESA_LAB_TA_PARAMETER].allocation_bytes==0x2000000);
  CHECK(!remesa_persistent_depth_scene_verify(&d,s.objects[REMESA_LAB_GENERATED]));
  /* Allocated auxiliary resources contain only the test provider pattern,
   * not historical instruction templates or an executable zero-fill claim. */
  CHECK(get(&f,s.bindings[REMESA_LAB_AUX_USE].handle)->bytes[0]==0xa5);
  prior=s;CHECK(remesa_lab_scene_storage_prepare(&d,&r,&s)==EBUSY&&!memcmp(&s,&prior,sizeof(s)));
  struct remesa_persistent_memory_backing receipt=s.bindings[REMESA_LAB_CONTROL],saved=receipt;
  void *mapped=NULL;CHECK(!d.ops->map(d.priv,s.objects[REMESA_LAB_CONTROL],true,&mapped));
  CHECK(remesa_persistent_memory_snapshot(&d,s.objects[REMESA_LAB_AUX_USE],&receipt)==EBUSY&&!memcmp(&receipt,&saved,sizeof(saved)));
  d.ops->unmap(d.priv,s.objects[REMESA_LAB_CONTROL],true);
  CHECK(remesa_persistent_memory_snapshot(&d,&f,&receipt)==EINVAL&&!memcmp(&receipt,&saved,sizeof(saved)));
  void *released=s.objects[REMESA_LAB_COLOR];
  remesa_lab_scene_storage_destroy(&s);CHECK(!s.device&&f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);
  CHECK(remesa_persistent_memory_snapshot(&d,released,&receipt)==EINVAL&&!memcmp(&receipt,&saved,sizeof(saved)));
  CHECK(!remesa_persistent_close(&d));
 }
 for(unsigned i=0;i<3;i++)remesa_shader_image_destroy(images[i]);
 printf("PASS native LAB storage11 owned roles1280x800: compatible USE window, color/Z/region, TA32MiB domains, rollback early/middle/late, wrong-window/domain refusal, metadata lease/lifetime refusal; NO GPU (%u checks)\n",checks);
 return 0;
}

#ifndef REMESA_LAB_STORAGE_FIXTURE
int main(void) {return lab_scene_storage_checks();}
#endif
