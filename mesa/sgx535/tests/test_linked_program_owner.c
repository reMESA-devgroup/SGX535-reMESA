/* SPDX-License-Identifier: MIT. Real owner code with injected GEM callbacks. */
int persistent_fixture_regressions(void);
#define main persistent_fixture_regressions
#include "test_persistent_shader.c"
#undef main
int linkage_fixture_regressions(void);
#define main linkage_fixture_regressions
#include "test_shader_linkage.c"
#undef main
static void open_link_owner(struct fixture *f,struct remesa_device *d,uint64_t context)
{
 struct remesa_persistent_config cfg={.ops=&ops,.priv=f,.context=context,
  .va_start=0x20000000,.va_end=0x20100000,.byte_budget=8*4096,.resource_only_backend=true};
 cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};
 CHECK(!remesa_persistent_open(d,&cfg));
}
static struct remesa_shader_linkage linked(unsigned fragment_kind)
{
 struct remesa_shader_program v=vertex(2),f=fragment(fragment_kind);
 struct remesa_shader_linkage link;CHECK(!remesa_shader_linkage_prepare(&v,&f,&link));return link;
}
int main(void)
{
 CHECK(!persistent_fixture_regressions());CHECK(!linkage_fixture_regressions());
 const uint32_t banks[2]={4,3};
 for(unsigned fk=0;fk<2;fk++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  open_link_owner(&f,&d,1900+fk);struct remesa_shader_linkage link=linked(fk);
  void *bo=NULL;CHECK(!remesa_persistent_linked_program_prepare(&d,&link,banks,&bo)&&bo);
  struct remesa_persistent_linked_program_backing b;
  CHECK(!remesa_persistent_linked_program_snapshot(&d,bo,&b));
  CHECK(b.owner==1900+fk&&b.generation&&b.handle&&b.gpu_va==0x20080000&&b.allocation_bytes==4096);
  CHECK(b.program_offset[0]==0&&b.program_bytes[0]==64&&b.reserved_bytes[0]==144);
  CHECK(b.program_offset[1]==160&&b.program_bytes[1]==8&&b.reserved_bytes[1]==(fk?16u:8u));
  unsigned char use[64];size_t n=0;
  CHECK(!remesa_shader_program_encode(&link.vertex,use,sizeof(use),&n)&&n==64);
  CHECK(!memcmp(get(&f,b.handle)->bytes,use,64));
  CHECK(!remesa_shader_program_encode(&link.fragment,use,sizeof(use),&n)&&n==8);
  CHECK(!memcmp(get(&f,b.handle)->bytes+160,use,8));
  for(unsigned i=64;i<4096;i++)if(i<160||i>=168)CHECK(!get(&f,b.handle)->bytes[i]);
  unsigned maps=f.cpu_maps,reads=f.reads,writes=f.writes;
  memset(&link,0,sizeof(link));
  for(unsigned i=0;i<200;i++)CHECK(!remesa_persistent_linked_program_snapshot(&d,bo,&b));
  CHECK(f.cpu_maps==maps&&f.reads==reads&&f.writes==writes); /* Metadata is not a readback loop. */
  void *p=(void *)1;CHECK(d.ops->map(d.priv,bo,true,&p)==ENOTSUP&&!p);
  struct remesa_draw_packet packet={0};void *fence=NULL;
  CHECK(d.ops->submit(d.priv,bo,&packet,&fence)!=0&&!fence&&!f.forbidden);
  CHECK(!d.ops->map(d.priv,bo,false,&p)&&p);
  d.ops->destroy_bo(d.priv,bo);CHECK(f.destroys==0);
  CHECK(remesa_persistent_close(&d)==EBUSY);
  d.ops->unmap(d.priv,bo,false);
  CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);
  CHECK(!remesa_persistent_close(&d));
 }
 /* Reject unsupported/invalid programs before any provider allocation. */
 {
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  open_link_owner(&f,&d,1910);struct remesa_shader_linkage link=linked(1);void *bo=NULL;
  const uint32_t same[2]={4,4};
  CHECK(remesa_persistent_linked_program_prepare(&d,&link,same,&bo)==EBUSY&&!bo&&!f.creates);
  link.vertex_layout.stride_dwords++;
  CHECK(remesa_persistent_linked_program_prepare(&d,&link,banks,&bo)==EPROTO&&!bo&&!f.creates);
  link=linked(2);CHECK(remesa_persistent_linked_program_prepare(&d,&link,banks,&bo)==ENOTSUP&&!bo&&!f.creates);
  struct remesa_shader_program v=vertex(3),fs=fragment(0);
  CHECK(!remesa_shader_linkage_prepare(&v,&fs,&link));
  CHECK(remesa_persistent_linked_program_prepare(&d,&link,banks,&bo)==ENOTSUP&&!bo&&!f.creates);
  CHECK(!remesa_persistent_close(&d));
 }
 const enum failure ordinary[]={FAIL_CREATE,FAIL_CREATE_WITH_HANDLE,FAIL_PIN,
  FAIL_MAP,FAIL_MAP_NULL,FAIL_READ_MAP,FAIL_READ_BODY,FAIL_READ_TAIL,FAIL_READ_GAP};
 for(unsigned i=0;i<sizeof(ordinary)/sizeof(ordinary[0]);i++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true,.fail=ordinary[i],.corrupt_offset=80};
  struct remesa_device d={0};open_link_owner(&f,&d,1920+i);
  struct remesa_shader_linkage link=linked(1);void *bo=NULL;
  CHECK(remesa_persistent_linked_program_prepare(&d,&link,banks,&bo)!=0&&!bo);
  f.fail=FAIL_NONE;CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);
  CHECK(!remesa_persistent_close(&d));
 }
 const enum failure uncertain[]={FAIL_CREATE_UNCERTAIN,FAIL_PIN_UNCERTAIN,
  FAIL_MAP_UNCERTAIN,FAIL_MAP_NONNULL_ERROR,FAIL_UNMAP_UNCERTAIN};
 for(unsigned i=0;i<sizeof(uncertain)/sizeof(uncertain[0]);i++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true,.fail=uncertain[i]};
  struct remesa_device d={0};open_link_owner(&f,&d,1940+i);
  struct remesa_shader_linkage link=linked(1);void *bo=NULL;
  CHECK(remesa_persistent_linked_program_prepare(&d,&link,banks,&bo)==EOWNERDEAD);
  CHECK(f.creates&&!f.destroys&&!f.forbidden);
  CHECK(remesa_persistent_close(&d)==EBUSY); /* Deliberate retention, never force-free. */
 }
 puts("PASS common linked program CODE ownership: effective64/8 vs reserved144/(8constant or16varying); allocator VA, zero padding, readonly publication,200 metadata snapshots without reads, deferred destruction, ordinary rollback and uncertain retention; fake provider GPU0");
 return 0;
}
