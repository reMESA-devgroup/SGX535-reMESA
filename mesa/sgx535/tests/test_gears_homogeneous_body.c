/* SPDX-License-Identifier: MIT. Mode-consistent TA ownership, GPU0. */
#define REMESA_GEARS_ATTRIBUTE_OWNER_FIXTURE
#include "test_gears_attribute_owners.c"
int main(void)
{
#if defined(REMESA_WITH_NIR) && defined(REMESA_TEST_REAL_NIR_BUILDER)
 glsl_type_singleton_init_or_ref();
#endif
 const unsigned sizes[][2]={{1280,800},{640,640},{641,479},{1600,768}};
 for(unsigned k=0;k<4;k++)for(unsigned many=0;many<2;many++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  void *program=NULL,*geometry=NULL,*parameters=NULL,*pds=NULL,*state=NULL,*legacy=NULL,*close=NULL,*body=NULL,*prefix=NULL,*ta=NULL;
  setup_gears(&f,&d,11200+k*2+many,&program,&geometry,&parameters,4,36);
  CHECK(!remesa_persistent_gears_attribute_pds_prepare(&d,program,geometry,parameters,&pds));
  struct remesa_persistent_linked_state_request q={
   .isp={.depth_bits_present=true,.depth_test=true,.depth_write=true,.compare=REMESA_DEPTH_LESS,.rgba_mask=15},
   .viewport={.framebuffer_width=sizes[k][0],.framebuffer_height=sizes[k][1],.width=sizes[k][0],.height=sizes[k][1],.depth_bits=24,.far_z=1,.path=REMESA_TA_VIEWPORT_MATRIX_WINDOW}};
  CHECK(!remesa_persistent_gears_attribute_state_prepare(&d,pds,&q,4,&legacy));
  CHECK(!remesa_persistent_gears_homogeneous_state_prepare(&d,pds,&q,4,&state));
  CHECK(!remesa_persistent_scene_close_prepare(&d,sizes[k][0],sizes[k][1],4,&close));
  const struct remesa_linked_body_draw mixed[2]={{state,0,3},{legacy,0,3}};
  unsigned creates=f.creates;
  CHECK(remesa_persistent_gears_homogeneous_body_prepare(&d,mixed,2,close,&body)==EINVAL&&!body&&f.creates==creates);
  CHECK(remesa_persistent_gears_attribute_body_prepare(&d,mixed,2,close,&body)==EINVAL&&!body&&f.creates==creates);
  struct remesa_linked_body_draw draws[16];unsigned count=many?16:1;
  for(unsigned i=0;i<count;i++)draws[i]=(struct remesa_linked_body_draw){state,0,3};
  CHECK(!remesa_persistent_gears_homogeneous_body_prepare(&d,draws,count,close,&body));
  CHECK(!remesa_persistent_initialized_prefix_prepare(&d,sizes[k][0],sizes[k][1],1,4,3,4,&prefix));
  CHECK(!remesa_persistent_linked_ta_prepare(&d,prefix,body,&ta));
  struct remesa_persistent_linked_ta_backing tb;
  CHECK(!remesa_persistent_linked_ta_snapshot(&d,ta,&tb));
  CHECK(tb.command_bytes==68+36*count&&tb.read_allocation_count==8);
  struct remesa_persistent_linked_state_backing sb;
  CHECK(!remesa_persistent_gears_homogeneous_state_snapshot(&d,state,&sb));
  unsigned char expected[8];CHECK(!remesa_ta_state_reference_encode(sb.image.gpu_va+sb.image.pds_offset,15,false,expected));
  const unsigned char *commands=get(&f,tb.handle)->bytes;
  for(unsigned i=0;i<count;i++)CHECK(!memcmp(commands+56+36*i,expected,8));
  struct remesa_persistent_zclear_prefix_backing pb;CHECK(!remesa_persistent_zclear_prefix_snapshot(&d,prefix,&pb));
  const unsigned char *clear=get(&f,pb.handle)->bytes;
  CHECK(shader_pds_le32(clear+pb.state_offset+36)==0x10000);
  unsigned maps=f.cpu_maps,reads=f.reads,writes=f.writes;
  for(unsigned i=0;i<100;i++)CHECK(!remesa_persistent_linked_ta_snapshot(&d,ta,&tb));
  CHECK(f.cpu_maps==maps&&f.reads==reads&&f.writes==writes);
  void *parents[]={program,geometry,parameters,pds,state,legacy,close,body,prefix};
  for(unsigned i=0;i<9;i++)d.ops->destroy_bo(d.priv,parents[i]);
  CHECK(!remesa_persistent_linked_ta_snapshot(&d,ta,&tb));d.ops->destroy_bo(d.priv,ta);
  CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);CHECK(!remesa_persistent_close(&d));
 }
#if defined(REMESA_WITH_NIR) && defined(REMESA_TEST_REAL_NIR_BUILDER)
 glsl_type_singleton_decref();
#endif
 puts("PASS homogeneous-only state/PARAM/index bodies, 1/16 draws, four sizes, reset prefix and transitive lifetime; GPU0");return 0;
}
