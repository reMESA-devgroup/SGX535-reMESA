/* SPDX-License-Identifier: MIT. Actual ownership/encoders; fake GEM, GPU0. */
int parameter_owner_fixture(void);
#define main parameter_owner_fixture
#include "test_persistent_shader.c"
#undef main
#include "remesa_usse.h"
#ifdef REMESA_WITH_NIR
uint32_t nir_debug=0;
#endif
static void parameter_endpoint(struct fixture *f,struct remesa_device *d,unsigned context)
{
 struct remesa_persistent_config cfg={.ops=&ops,.priv=f,.context=context,
  .va_start=0x20000000,.va_end=0x20100000,.byte_budget=4*4096,.resource_only_backend=true};
 cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};
 CHECK(!remesa_persistent_open(d,&cfg));
}
int main(void)
{
 struct remesa_vertex_parameter_values values={0};
 for(unsigned i=0;i<16;i++)values.coefficient_bits[i]=0x80000000u+i*0x010203u;
 for(unsigned pass=0;pass<2;pass++){
  if(pass){memset(&values,0,sizeof values);for(unsigned i=0;i<4;i++)values.coefficient_bits[i]=0x3f800000;values.coefficient_bits[4]=0x40080000;}
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  parameter_endpoint(&f,&d,9600+pass);void *bo=NULL;
  CHECK(!remesa_persistent_vertex_parameters_prepare(&d,&values,4,&bo)&&bo);
  struct remesa_persistent_vertex_parameters_backing b;
  CHECK(!remesa_persistent_vertex_parameters_snapshot(&d,bo,&b));
  CHECK(b.image.gpu_va==0x20080000&&b.image.allocation_bytes==4096&&b.image.state_offset==0&&
   b.image.state_bytes==80&&b.image.use_offset==80&&b.image.use_bytes==8&&b.image.pds_offset==96&&
   b.image.pds_bytes==60&&b.image.ta_offset==160&&b.image.ta_bytes==8);
  CHECK(b.coefficient_dwords==16&&b.effective_source_dwords==17&&b.transfer_dwords==20);
  unsigned char *bytes=get(&f,b.image.handle)->bytes;
  CHECK(shader_pds_le32(bytes)==b.image.gpu_va);
  for(unsigned i=0;i<16;i++)CHECK(shader_pds_le32(bytes+4+4*i)==values.coefficient_bits[i]);
  for(unsigned i=68;i<80;i++)CHECK(!bytes[i]);
  const struct remesa_usse_cpu_control_ee ee={.record_last=1};unsigned char expected[80],mask[80];size_t n;
  CHECK(!remesa_usse_cpu_control_ee_encode(&ee,expected,sizeof expected,&n)&&n==8&&!memcmp(bytes+80,expected,n));
  uint32_t fixup;CHECK(!remesa_use_entry_preview_fixup(&b.image.use_entry,&fixup));
  const struct remesa_pds_state_image_request pds={b.image.gpu_va,4096,0,20,fixup,REMESA_PDS_STATE_TASK_PARAMETERS};
  CHECK(!remesa_pds_structure_encode_state_image(&pds,expected,mask,sizeof expected,&n)&&n==60);
  CHECK(!memcmp(bytes+96,expected,60)&&!memcmp(b.image.pds_known_mask,mask,60));
  CHECK(!remesa_ta_parameter_reference_encode(b.image.gpu_va+96,20,expected)&&!memcmp(bytes+160,expected,8));
  for(unsigned i=88;i<4096;i++)if((i<96||i>=156)&&(i<160||i>=168))CHECK(!bytes[i]);
  struct remesa_persistent_isp_update_backing legacy;
  CHECK(remesa_persistent_isp_update_snapshot(&d,bo,&legacy)==EINVAL);
  struct remesa_persistent_scene_close_backing close;
  CHECK(remesa_persistent_scene_close_snapshot(&d,bo,&close)==EINVAL);
  struct remesa_persistent_scene_state_backing state;
  CHECK(remesa_persistent_scene_state_snapshot(&d,bo,&state)==EINVAL);
  unsigned maps=f.cpu_maps,reads=f.reads,writes=f.writes;
  for(unsigned i=0;i<200;i++)CHECK(!remesa_persistent_vertex_parameters_snapshot(&d,bo,&b));
  CHECK(f.cpu_maps==maps&&f.reads==reads&&f.writes==writes);
  void *cpu=NULL;CHECK(d.ops->map(d.priv,bo,true,&cpu)==ENOTSUP&&!cpu);
  CHECK(!d.ops->map(d.priv,bo,false,&cpu)&&cpu);
  d.ops->destroy_bo(d.priv,bo);CHECK(!f.destroys&&remesa_persistent_close(&d)==EBUSY);
  d.ops->unmap(d.priv,bo,false);CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);
  CHECK(!remesa_persistent_close(&d));
 }
 {
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};parameter_endpoint(&f,&d,9610);
  void *bo=NULL;CHECK(remesa_persistent_vertex_parameters_prepare(&d,&values,16,&bo)!=0&&!bo&&!f.creates);
  CHECK(remesa_persistent_vertex_parameters_prepare(&d,NULL,4,&bo)==EINVAL&&!bo&&!f.creates);
  CHECK(remesa_persistent_vertex_parameters_prepare(&d,&values,4,(void *)&values)==EINVAL&&!f.creates);
  bo=(void *)1;CHECK(remesa_persistent_vertex_parameters_prepare(&d,&values,4,&bo)==EBUSY&&bo==(void *)1&&!f.creates);
  CHECK(!remesa_persistent_close(&d));
 }
 /* Keep complete shader blocks, including the actual gears9/10 vec4 state.
  * Every admitted source policy comes from the existing DMA/source encoder. */
 const unsigned counts[]={1,8,16,36,40,127};
 for(unsigned k=0;k<sizeof counts/sizeof counts[0];k++){
  uint32_t coefficients[127];unsigned count=counts[k];
  for(unsigned i=0;i<127;i++)coefficients[i]=0x3f000000u+i*31u;
  for(unsigned aligned=0;aligned<2;aligned++){
   unsigned transfer=aligned?(count+4u)&~3u:count+1u;
   struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};parameter_endpoint(&f,&d,9650+k*2+aligned);
   struct remesa_pds_dma_partition partition;CHECK(!remesa_pds_structure_dma_partition(transfer,&partition));void *bo=NULL;
   int e=remesa_persistent_vertex_parameter_block_prepare(&d,coefficients,count,transfer,4,&bo);
   if(partition.count!=1){CHECK(e==ENOTSUP&&!bo&&!f.creates);}
   else{
    CHECK(!e&&bo);struct remesa_persistent_vertex_parameters_backing b;
    CHECK(!remesa_persistent_vertex_parameters_snapshot(&d,bo,&b));
    CHECK(b.coefficient_dwords==count&&b.effective_source_dwords==count+1&&b.transfer_dwords==transfer);
    unsigned char *bytes=get(&f,b.image.handle)->bytes;
    CHECK(b.image.state_bytes==transfer*4&&shader_pds_le32(bytes)==b.image.gpu_va);
    uint32_t fixup;CHECK(!remesa_use_entry_preview_fixup(&b.image.use_entry,&fixup));
    CHECK(shader_pds_le32(bytes+b.image.pds_offset)==b.image.gpu_va);
    CHECK(shader_pds_le32(bytes+b.image.pds_offset+4)==partition.control[0]);
    CHECK(shader_pds_le32(bytes+b.image.pds_offset+8)==(0x200000u|fixup));
    CHECK(shader_pds_le32(bytes+b.image.pds_offset+12)==0x04000000u);
    for(unsigned i=0;i<count;i++)CHECK(shader_pds_le32(bytes+4+4*i)==coefficients[i]);
    for(unsigned i=(count+1)*4;i<transfer*4;i++)CHECK(!bytes[i]);
    unsigned char ta[8];CHECK(!remesa_ta_parameter_reference_encode(b.image.gpu_va+b.image.pds_offset,transfer,ta));
    CHECK(!memcmp(ta,bytes+b.image.ta_offset,8));d.ops->destroy_bo(d.priv,bo);
   }
   CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);CHECK(!remesa_persistent_close(&d));
  }
 }
 {
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};parameter_endpoint(&f,&d,9680);
  uint32_t coefficients[127]={0};void *bo=NULL;
  const unsigned bad[][2]={{0,4},{128,128},{16,0},{16,129},{16,17},{16,24}};
  for(unsigned k=0;k<sizeof bad/sizeof bad[0];k++)CHECK(remesa_persistent_vertex_parameter_block_prepare(&d,coefficients,bad[k][0],bad[k][1],4,&bo)!=0&&!bo&&!f.creates);
  CHECK(remesa_persistent_vertex_parameter_block_prepare(&d,(void *)(UINTPTR_MAX-3),16,20,4,&bo)==EINVAL&&!f.creates);
  CHECK(remesa_persistent_vertex_parameter_block_prepare(&d,(void *)((unsigned char *)coefficients+1),16,20,4,&bo)==EINVAL&&!f.creates);
  CHECK(!remesa_persistent_close(&d));
 }
 const enum failure ordinary[]={FAIL_CREATE,FAIL_CREATE_WITH_HANDLE,FAIL_PIN,FAIL_MAP,FAIL_MAP_NULL,FAIL_READ_MAP,FAIL_READ_BODY,FAIL_READ_TAIL,FAIL_READ_GAP};
 for(unsigned i=0;i<sizeof ordinary/sizeof ordinary[0];i++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true,.fail=ordinary[i],.corrupt_offset=92};
  struct remesa_device d={0};parameter_endpoint(&f,&d,9620+i);void *bo=NULL;
  CHECK(remesa_persistent_vertex_parameters_prepare(&d,&values,4,&bo)!=0&&!bo);
  CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);CHECK(!remesa_persistent_close(&d));
 }
 const enum failure uncertain[]={FAIL_CREATE_UNCERTAIN,FAIL_PIN_UNCERTAIN,FAIL_MAP_UNCERTAIN,FAIL_MAP_NONNULL_ERROR,FAIL_UNMAP_UNCERTAIN};
 for(unsigned i=0;i<sizeof uncertain/sizeof uncertain[0];i++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true,.fail=uncertain[i]};struct remesa_device d={0};parameter_endpoint(&f,&d,9640+i);void *bo=NULL;
  CHECK(remesa_persistent_vertex_parameters_prepare(&d,&values,4,&bo)==EOWNERDEAD);
  CHECK(f.creates&&!f.destroys&&!f.forbidden&&remesa_persistent_close(&d)==EBUSY);
 }
 puts("PASS owned vertex parameter source17/transfer20/allocation4096, EE8/PDS60/TA8, bit-exact state, immutable publication, metadata without IO, rollback and uncertain retention; GPU0");return 0;
}
