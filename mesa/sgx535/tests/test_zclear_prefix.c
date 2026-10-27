/* SPDX-License-Identifier: MIT. Generated owned Z-clear prefix, GPU0. */
int clear_persistent_fixture(void);
#define main clear_persistent_fixture
#include "test_persistent_shader.c"
#undef main
#include "remesa_ta_zclear.h"
static void clear_open(struct fixture *f,struct remesa_device *d,unsigned context)
{
 struct remesa_persistent_config cfg={.ops=&ops,.priv=f,.context=context,
  .va_start=0x20000000,.va_end=0x20100000,.byte_budget=8*4096,.resource_only_backend=true};
 cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};
 CHECK(!remesa_persistent_open(d,&cfg));
}
static float clear_float(const uint8_t *p)
{uint32_t v=shader_pds_le32(p);float f;memcpy(&f,&v,4);return f;}
int main(void)
{
 CHECK(!clear_persistent_fixture());
 const unsigned sizes[3][2]={{1280,800},{640,640},{641,479}};
 for(unsigned size=0;size<3;size++){
  unsigned w=sizes[size][0],h=sizes[size][1];
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  clear_open(&f,&d,2400+size);void *prefix=NULL;
  CHECK(!remesa_persistent_zclear_prefix_prepare(&d,w,h,1,4,3,4,&prefix));
  struct remesa_persistent_zclear_prefix_backing b;
  CHECK(!remesa_persistent_zclear_prefix_snapshot(&d,prefix,&b));
  CHECK(b.width==w&&b.height==h&&b.used_bytes==444&&b.allocation_bytes==4096&&b.index_count==3);
  CHECK(b.vertex_capacity_bytes==64&&b.vertex_referenced_bytes==48&&b.index_capacity_bytes==12&&b.index_referenced_bytes==6);
  const uint8_t *bytes=get(&f,b.handle)->bytes;
  CHECK(clear_float(bytes+64)==0&&clear_float(bytes+68)==0&&clear_float(bytes+72)==1&&clear_float(bytes+76)==1);
  CHECK(clear_float(bytes+80)==2*w&&clear_float(bytes+100)==2*h);
  CHECK(clear_float(bytes+112)==w&&clear_float(bytes+116)==h); /* Unreferenced fourth record. */
  const uint8_t indices[12]={0,0,1,0,2,0,3,0,2,0,1,0};CHECK(!memcmp(bytes+128,indices,12));
  CHECK(shader_pds_le32(bytes+304)==0x54c5&&shader_pds_le32(bytes+308)==0x01e00100);
  CHECK(shader_pds_le32(bytes+312)==0x0e000000);
  CHECK(shader_pds_le32(bytes+416)==(0x40000000|((b.gpu_va+352)>>4))&&shader_pds_le32(bytes+420)==0x0c02a203);
  CHECK(shader_pds_le32(bytes+424)==0x81400003&&shader_pds_le32(bytes+428)==b.gpu_va+128);
  CHECK(shader_pds_le32(bytes+436)==(b.gpu_va+160)/16&&shader_pds_le32(bytes+440)==0x02000103);
  /* Same retained copy-PDS producer and relocated sources; exact committed
+   * lengths are checked independently, masks retain unknown DATA cells. */
  uint8_t pds[64],mask[64];size_t n=0;uint32_t fixup=0;
  CHECK(!remesa_use_entry_preview_fixup(&b.entries[0],&fixup));
  const struct remesa_pds_vertex_image_request vr={b.gpu_va,4096,64,4,fixup};
  CHECK(!remesa_pds_structure_encode_vertex_image(&vr,pds,mask,64,&n)&&n==64&&!memcmp(pds,bytes+160,64));
  for(unsigned i=444;i<4096;i++)CHECK(!bytes[i]);
  /* All pixel centres strictly lie inside the single covering triangle. */
  for(uint64_t y=0;y<h;y++)for(uint64_t x=0;x<w;x++)CHECK((2*x+1)*h+(2*y+1)*w<4*(uint64_t)w*h);
  struct remesa_ta_zclear_request q={w,h,w,h,1};struct remesa_ta_zclear_geometry quad,single;
  CHECK(!remesa_ta_zclear_geometry_encode(&q,&quad));CHECK(!remesa_ta_zclear_single_geometry_encode(&q,&single));
  CHECK(!memcmp(single.positions,bytes+64,64)&&!memcmp(single.indices,bytes+128,12));
  CHECK(!memcmp(single.positions+48,quad.positions+48,16)&&!memcmp(single.indices,quad.indices,12));
  unsigned maps=f.cpu_maps;for(unsigned i=0;i<100;i++)CHECK(!remesa_persistent_zclear_prefix_snapshot(&d,prefix,&b));
  CHECK(f.cpu_maps==maps);void *cpu=NULL;CHECK(d.ops->map(d.priv,prefix,true,&cpu)==ENOTSUP&&!cpu);
  CHECK(!d.ops->map(d.priv,prefix,false,&cpu)&&cpu);d.ops->destroy_bo(d.priv,prefix);
  CHECK(!f.destroys&&remesa_persistent_close(&d)==EBUSY);d.ops->unmap(d.priv,prefix,false);
  CHECK(f.creates==1&&f.destroys==1&&f.pins==f.unpins&&!f.forbidden&&!remesa_persistent_close(&d));
 }
 const enum failure fail[]={FAIL_CREATE,FAIL_PIN,FAIL_MAP,FAIL_READ_BODY,FAIL_READ_TAIL,
  FAIL_MAP_UNCERTAIN,FAIL_UNMAP_UNCERTAIN};
 for(unsigned i=0;i<sizeof(fail)/sizeof(fail[0]);i++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  clear_open(&f,&d,2420+i);void *prefix=NULL;f.fail=fail[i];
  CHECK(remesa_persistent_zclear_prefix_prepare(&d,1280,800,1,4,3,4,&prefix)!=0);
  if(fail[i]==FAIL_MAP_UNCERTAIN||fail[i]==FAIL_UNMAP_UNCERTAIN){
   CHECK(prefix&&f.creates==1&&!f.destroys&&!f.unpins&&remesa_persistent_close(&d)==EBUSY);
  }else{f.fail=FAIL_NONE;CHECK(!prefix&&f.creates==f.destroys&&f.pins==f.unpins&&!remesa_persistent_close(&d));}
  CHECK(!f.forbidden);
 }
 struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};clear_open(&f,&d,2440);void *prefix=NULL;
 CHECK(remesa_persistent_zclear_prefix_prepare(&d,0,800,1,4,3,4,&prefix)==EINVAL&&!f.creates);
 CHECK(remesa_persistent_zclear_prefix_prepare(&d,1280,800,-1,4,3,4,&prefix)==EINVAL&&!f.creates);
 CHECK(remesa_persistent_zclear_prefix_prepare(&d,1280,800,1,4,4,4,&prefix)==EBUSY&&!f.creates);
 CHECK(!remesa_persistent_close(&d));
 puts("PASS owned Z-clear prefix: generated VS16/FS8/stateUSE16/PDS/state54c5/TA28, single full-target triangle, exact capacity vs referenced ranges, paired+odd size pixel-centre coverage, immutable publication/lease lifetime and rollback/quarantine; GPU0/API0/COLORclear0");return 0;
}
