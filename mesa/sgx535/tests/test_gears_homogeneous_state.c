/* SPDX-License-Identifier: MIT. Existing W2 state15 witness -> owned state;
 * actual encoders, fake BO transport only. No GPU or rendering simulation. */
#define REMESA_GEARS_ATTRIBUTE_OWNER_FIXTURE
#include "test_gears_attribute_owners.c"
#include "remesa/state_upload.h"
int main(int argc,char **argv)
{
 CHECK(argc==3);unsigned char oracle_use[16],oracle_state[60];FILE *fp=fopen(argv[1],"rb");
 CHECK(fp&&fread(oracle_use,1,16,fp)==16&&fgetc(fp)==EOF&&!fclose(fp));
 fp=fopen(argv[2],"rb");CHECK(fp&&fread(oracle_state,1,60,fp)==60&&fgetc(fp)==EOF&&!fclose(fp));
#if defined(REMESA_WITH_NIR) && defined(REMESA_TEST_REAL_NIR_BUILDER)
 glsl_type_singleton_init_or_ref();
#endif
 const unsigned sizes[][2]={{1280,800},{640,640},{641,479},{1600,768}};
 for(unsigned k=0;k<4;k++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  void *program=NULL,*geometry=NULL,*parameters=NULL,*pds=NULL,*state=NULL,*legacy=NULL,*close=NULL,*body=NULL;
  setup_gears(&f,&d,10100+k,&program,&geometry,&parameters,4,36);
  CHECK(!remesa_persistent_gears_attribute_pds_prepare(&d,program,geometry,parameters,&pds));
  struct remesa_persistent_linked_state_request q={
   .isp={.depth_bits_present=true,.depth_test=true,.depth_write=true,.compare=REMESA_DEPTH_LESS,.rgba_mask=15},
   .viewport={.framebuffer_width=sizes[k][0],.framebuffer_height=sizes[k][1],
    .width=sizes[k][0],.height=sizes[k][1],.depth_bits=24,.far_z=1,.path=REMESA_TA_VIEWPORT_MATRIX_WINDOW}};
  CHECK(!remesa_persistent_gears_attribute_state_prepare(&d,pds,&q,4,&legacy));
  CHECK(!remesa_persistent_gears_homogeneous_state_prepare(&d,pds,&q,4,&state));
  struct remesa_persistent_linked_state_backing v,old;
  CHECK(!remesa_persistent_gears_homogeneous_state_snapshot(&d,state,&v));
  CHECK(!remesa_persistent_gears_attribute_state_snapshot(&d,legacy,&old));
  CHECK(v.image.state_bytes==60&&v.image.use_bytes==16&&v.image.pds_bytes==60&&v.image.ta_bytes==8);
  CHECK(v.image.use_offset==64&&v.image.pds_offset==96&&v.image.ta_offset==160);
  unsigned char *bytes=get(&f,v.image.handle)->bytes,*prior=get(&f,old.image.handle)->bytes;
  CHECK(shader_pds_le32(bytes)==0x1f41&&shader_pds_le32(prior)==0xf41);
  CHECK(!memcmp(bytes+4,prior+4,52)&&!shader_pds_le32(bytes+56)&&!shader_pds_le32(bytes+60));
  CHECK(!memcmp(bytes+v.image.use_offset,oracle_use,16));
  if(!k){CHECK(!memcmp(bytes+20,oracle_state+20,36));}
  struct remesa_state_upload upload;
  CHECK(!remesa_state_upload_pds(bytes+v.image.pds_offset,60,15,&upload));
  CHECK(upload.source_bytes==60&&upload.source_address==v.image.gpu_va);
  unsigned char reference[8];CHECK(!remesa_ta_state_reference_encode(v.image.gpu_va+v.image.pds_offset,15,false,reference));
  CHECK(!memcmp(reference,bytes+v.image.ta_offset,8));
  struct remesa_persistent_linked_state_backing sentinel;memset(&sentinel,0xa5,sizeof sentinel);old=sentinel;
  CHECK(remesa_persistent_gears_attribute_state_snapshot(&d,state,&old)==EINVAL&&!memcmp(&old,&sentinel,sizeof old));
  CHECK(remesa_persistent_gears_homogeneous_state_snapshot(&d,legacy,&old)==EINVAL&&!memcmp(&old,&sentinel,sizeof old));
  CHECK(remesa_persistent_gears_homogeneous_state_snapshot(&d,state,(void *)state)==EINVAL);
  unsigned maps=f.cpu_maps,reads=f.reads,writes=f.writes;
  for(unsigned i=0;i<100;i++)CHECK(!remesa_persistent_gears_homogeneous_state_snapshot(&d,state,&v));
  CHECK(f.cpu_maps==maps&&f.reads==reads&&f.writes==writes);
  void *rejected=NULL;unsigned creates=f.creates;q.viewport.path=REMESA_TA_VIEWPORT_INITIAL_ZERO;
  CHECK(remesa_persistent_gears_homogeneous_state_prepare(&d,pds,&q,4,&rejected)==ENOTSUP&&!rejected&&f.creates==creates);
  CHECK(!remesa_persistent_scene_close_prepare(&d,sizes[k][0],sizes[k][1],4,&close));
  const struct remesa_linked_body_draw draw={state,0,3};creates=f.creates;
  CHECK(remesa_persistent_gears_attribute_body_prepare(&d,&draw,1,close,&body)==EINVAL&&!body&&f.creates==creates);
  /* State owns the PDS and its programs, geometry and parameter block. */
  void *parents[]={program,geometry,parameters,pds};
  for(unsigned i=0;i<4;i++)d.ops->destroy_bo(d.priv,parents[i]);
  CHECK(!f.destroys&&!remesa_persistent_gears_homogeneous_state_snapshot(&d,state,&v));
  d.ops->destroy_bo(d.priv,legacy);d.ops->destroy_bo(d.priv,close);d.ops->destroy_bo(d.priv,state);
  CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);CHECK(!remesa_persistent_close(&d));
 }
#if defined(REMESA_WITH_NIR) && defined(REMESA_TEST_REAL_NIR_BUILDER)
 glsl_type_singleton_decref();
#endif
 puts("PASS owned original state15: W2 USE/viewport/scalar oracle, consumed60/capacity64, PDS15/TA4, distinct kind/lifetime, old body refusal; GPU0");return 0;
}
