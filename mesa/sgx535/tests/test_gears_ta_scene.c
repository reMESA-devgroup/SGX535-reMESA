/* SPDX-License-Identifier: MIT. Real ordered TA/parameter encoder and decoder
 * contracts, fake BO transport only; no pixel or GPU simulation. */
#define REMESA_GEARS_ATTRIBUTE_OWNER_FIXTURE
#include "test_gears_attribute_owners.c"
#include "remesa/state_upload.h"
struct visited {struct remesa_ta_reference values[REMESA_TA_REFERENCE_MAX];unsigned count;};
static int visit(void *ctx,const struct remesa_ta_reference *r)
{struct visited *v=ctx;CHECK(v->count<REMESA_TA_REFERENCE_MAX);v->values[v->count++]=*r;return 0;}
static void reject_stream(const unsigned char *p,size_t bytes)
{
 struct remesa_ta_reference_counts out={777,999},saved=out;struct visited visited={0};
 CHECK(remesa_ta_gears_references_walk(p,bytes,visit,&visited,&out)!=0&&!visited.count&&!memcmp(&out,&saved,sizeof out));
}
int main(void)
{
#if defined(REMESA_WITH_NIR) && defined(REMESA_TEST_REAL_NIR_BUILDER)
 glsl_type_singleton_init_or_ref();
#endif
 const unsigned sizes[][2]={{1280,800},{640,640},{641,479}};
 for(unsigned size=0;size<3;size++)for(unsigned initial=0;initial<2;initial++)for(unsigned many=0;many<2;many++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  void *program=NULL,*geometry=NULL,*parameters=NULL,*pds=NULL,*state=NULL,*close=NULL,*body=NULL,*prefix=NULL,*ta=NULL;
  setup_gears(&f,&d,9900+2*size+initial,&program,&geometry,&parameters,4,36);
  CHECK(!remesa_persistent_gears_attribute_pds_prepare(&d,program,geometry,parameters,&pds));
  const struct remesa_persistent_linked_state_request request={
   .isp={.depth_bits_present=true,.depth_test=true,.depth_write=true,.compare=REMESA_DEPTH_LESS,.rgba_mask=15},
   .viewport={.framebuffer_width=sizes[size][0],.framebuffer_height=sizes[size][1],
    .width=sizes[size][0],.height=sizes[size][1],.depth_bits=24,.far_z=1,.path=REMESA_TA_VIEWPORT_INITIAL_ZERO}};
  CHECK(!remesa_persistent_gears_attribute_state_prepare(&d,pds,&request,4,&state));
  struct remesa_persistent_linked_state_backing sb;
  CHECK(remesa_persistent_linked_state_snapshot(&d,state,&sb)==EINVAL);
  CHECK(!remesa_persistent_gears_attribute_state_snapshot(&d,state,&sb));
  CHECK(!remesa_persistent_scene_close_prepare(&d,sizes[size][0],sizes[size][1],4,&close));
  struct remesa_linked_body_draw draws[16];unsigned draw_count=many?16:1;
  for(unsigned i=0;i<draw_count;i++)draws[i]=(struct remesa_linked_body_draw){state,0,3};unsigned creates=f.creates;
  CHECK(remesa_persistent_linked_body_prepare(&d,draws,draw_count,close,&body)!=0&&!body&&f.creates==creates);
  CHECK(!remesa_persistent_gears_attribute_body_prepare(&d,draws,draw_count,close,&body));
  CHECK(!(initial?remesa_persistent_initialized_prefix_prepare(&d,sizes[size][0],sizes[size][1],1,4,3,4,&prefix):
   remesa_persistent_zclear_prefix_prepare(&d,sizes[size][0],sizes[size][1],1,4,3,4,&prefix)));
  CHECK(!remesa_persistent_linked_ta_prepare(&d,prefix,body,&ta));
  struct remesa_persistent_linked_ta_backing tb;CHECK(!remesa_persistent_linked_ta_snapshot(&d,ta,&tb));
  CHECK(tb.command_bytes==40u+28u*initial+36u*draw_count&&tb.close_offset==tb.command_bytes-12&&tb.read_allocation_count==8);
  struct remesa_persistent_vertex_parameters_backing pb;
  /* Public PARAM refs are held by PDS now; inspect the complete PDS closure. */
  struct remesa_persistent_gears_attribute_pds_backing pd;
  CHECK(!remesa_persistent_gears_attribute_pds_snapshot(&d,pds,&pd));pb=pd.parameters;
  unsigned char *bytes=get(&f,tb.handle)->bytes;size_t at=initial?56:28;
  CHECK(shader_pds_le32(bytes+at)==(0x40000000u|((sb.image.gpu_va+sb.image.pds_offset)>>4)));
  CHECK(shader_pds_le32(bytes+at+8)==(0x40000000u|((pb.image.gpu_va+pb.image.pds_offset)>>4))&&shader_pds_le32(bytes+at+12)==0x0c01e10a);
  struct visited seen={0};struct remesa_ta_reference_counts counts;
  CHECK(!remesa_ta_gears_references_walk(bytes,tb.command_bytes,visit,&seen,&counts));
  CHECK(counts.draws==draw_count&&counts.references==4u*draw_count+4u+initial&&seen.count==counts.references);
  CHECK(seen.values[3+initial].kind==REMESA_TA_REF_DRAW_STATE&&seen.values[4+initial].kind==REMESA_TA_REF_DRAW_PARAMETERS&&seen.values[5+initial].kind==REMESA_TA_REF_DRAW_INDEX);
  CHECK(remesa_ta_references_walk(bytes,tb.command_bytes,NULL,NULL,&counts)!=0);
  unsigned char *source=get(&f,pb.image.handle)->bytes;struct remesa_state_upload decoded;
  CHECK(!remesa_parameter_upload_pds(source+pb.image.pds_offset,60,&decoded)&&decoded.source_address==pb.image.gpu_va&&decoded.source_bytes==160);
  CHECK(!remesa_parameter_upload_source(source,160,(uint32_t)pb.image.gpu_va));
  CHECK(!remesa_parameter_upload_auxiliary(source+pb.image.use_offset,8));
  unsigned char bad[160];memcpy(bad,source+pb.image.pds_offset,60);
  for(unsigned mode=0;mode<6;mode++){
   memcpy(bad,source+pb.image.pds_offset,60);const unsigned offsets[]={4,10,12,40,48,56};bad[offsets[mode]]^=0x80;
   struct remesa_state_upload out={1,2,3},saved=out;
   CHECK(remesa_parameter_upload_pds(bad,60,&out)!=0&&!memcmp(&out,&saved,sizeof out));
  }
  memcpy(bad,source,160);bad[0]^=4;CHECK(remesa_parameter_upload_source(bad,160,(uint32_t)pb.image.gpu_va)!=0);
  memcpy(bad,source,160);bad[148]=1;CHECK(remesa_parameter_upload_source(bad,160,(uint32_t)pb.image.gpu_va)!=0);
  CHECK(remesa_parameter_upload_source(source,159,(uint32_t)pb.image.gpu_va)!=0);
  memcpy(bad,source+pb.image.use_offset,8);bad[4]^=1;CHECK(remesa_parameter_upload_auxiliary(bad,8)!=0);
  unsigned char malformed[644];memcpy(malformed,bytes,tb.command_bytes);
  for(unsigned mode=0;mode<5;mode++){
   memcpy(malformed,bytes,tb.command_bytes);
   if(mode==0)malformed[at+12]^=1;
   if(mode==1)memmove(malformed+at+8,malformed+at+16,tb.command_bytes-at-16);
   if(mode==2){unsigned char temp[8];memcpy(temp,malformed+at,8);memcpy(malformed+at,malformed+at+8,8);memcpy(malformed+at+8,temp,8);}
   if(mode==3)malformed[tb.command_bytes-4]^=1;
   if(mode==4)malformed[at+24]=1;
   reject_stream(malformed,tb.command_bytes);
  }
  unsigned maps=f.cpu_maps,reads=f.reads,writes=f.writes;
  for(unsigned i=0;i<100;i++)CHECK(!remesa_persistent_linked_ta_snapshot(&d,ta,&tb));
  CHECK(f.cpu_maps==maps&&f.reads==reads&&f.writes==writes);
  void *parents[]={program,geometry,parameters,pds,state,close,body,prefix};
  for(unsigned i=0;i<sizeof parents/sizeof parents[0];i++)d.ops->destroy_bo(d.priv,parents[i]);
  CHECK(!f.destroys&&!remesa_persistent_linked_ta_snapshot(&d,ta,&tb));
  d.ops->destroy_bo(d.priv,ta);CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);CHECK(!remesa_persistent_close(&d));
 }
#if defined(REMESA_WITH_NIR) && defined(REMESA_TEST_REAL_NIR_BUILDER)
 glsl_type_singleton_decref();
#endif
 puts("PASS ordered state/PARAM/index, full READ/bank closure, independent parameter PDS/source/EE checks, strict separate grammar and malformed-input refusal, retained lifetime; GPU0");return 0;
}
