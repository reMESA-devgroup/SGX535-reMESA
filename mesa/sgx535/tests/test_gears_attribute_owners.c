/* SPDX-License-Identifier: MIT. Existing physical-profile CODE/PDS ownership;
 * fake provider, no device, pixel generation or execution claim. */
int inherited_parameter_persistent_tests(void);
#define main inherited_parameter_persistent_tests
#include "test_persistent_shader.c"
#undef main
int inherited_parameter_linkage_tests(void);
#define main inherited_parameter_linkage_tests
#include "test_shader_linkage.c"
#undef main
#include "remesa_screen.h"
#if defined(REMESA_WITH_NIR) && defined(REMESA_TEST_REAL_NIR_BUILDER)
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"
int inherited_gears_nir_tests(void);
#define main inherited_gears_nir_tests
#include "test_nir_gears_vertex.c"
#undef main
#elif defined(REMESA_WITH_NIR)
uint32_t nir_debug=0;
#endif
static void open_link_owner(struct fixture *f,struct remesa_device *d,uint64_t context)
{
 struct remesa_persistent_config cfg={.ops=&ops,.priv=f,.context=context,
  .va_start=0x20000000,.va_end=0x20100000,.byte_budget=16*4096,.resource_only_backend=true};
 cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};
 CHECK(!remesa_persistent_open(d,&cfg));
}
static struct remesa_shader_linkage gears_pair(void)
{
 struct remesa_shader_program vs=vertex(1),fs=fragment(1);
#if defined(REMESA_WITH_NIR) && defined(REMESA_TEST_REAL_NIR_BUILDER)
 nir_shader *nir=make(true,0);struct remesa_shader_ir semantic;
 CHECK(!remesa_shader_ir_lower(nir,&semantic));struct remesa_shader_program actual;
 CHECK(!remesa_shader_program_prepare(&semantic,&actual));
 CHECK(!memcmp(&actual,&vs,sizeof vs));vs=actual;ralloc_free(nir);
#endif
 struct remesa_shader_linkage link;CHECK(!remesa_shader_linkage_prepare(&vs,&fs,&link));return link;
}
static void setup_gears(struct fixture *f,struct remesa_device *d,unsigned context,
 void **program,void **geometry,void **parameters,unsigned parameter_bank,unsigned count)
{
 open_link_owner(f,d,context);struct remesa_shader_linkage link=gears_pair();const uint32_t banks[]={4,3};
 CHECK(remesa_persistent_linked_program_prepare(d,&link,banks,program)==ENOTSUP&&!*program);
 CHECK(!remesa_persistent_gears_attribute_program_prepare(d,&link,banks,program));
 unsigned char raw[144];for(unsigned i=0;i<sizeof raw;i++)raw[i]=(unsigned char)(i*13+3);
 const struct remesa_linked_geometry_input in={.vertices=raw,.vertex_bytes=sizeof raw,.vertex_count=3};
 CHECK(!remesa_persistent_linked_geometry_prepare(d,&in,geometry));
 uint32_t coefficients[40]={0};for(unsigned i=0;i<count;i++)coefficients[i]=0x3f800000u+i*29u;
#if defined(REMESA_WITH_NIR) && defined(REMESA_TEST_REAL_NIR_BUILDER)
 if(count==36||count==40){
  struct pipe_screen *screen=remesa_screen_create(d);CHECK(screen);
  struct pipe_context *ctx=screen->context_create(screen,NULL,0);CHECK(ctx);
  CHECK(remesa_context_prepare_vertex_parameters(ctx,parameter_bank,parameters)==ENOTSUP&&!*parameters);
  struct pipe_shader_state state={.type=PIPE_SHADER_IR_NIR,.ir.nir=make(count==36,0)};
  void *vs=ctx->create_vs_state(ctx,&state);CHECK(vs);ctx->bind_vs_state(ctx,vs);
  CHECK(remesa_context_prepare_vertex_parameters(ctx,parameter_bank,parameters)==ENODATA&&!*parameters);
  const struct pipe_constant_buffer binding={.user_buffer=coefficients,.buffer_size=count*4u};
  ctx->set_constant_buffer(ctx,PIPE_SHADER_VERTEX,0,false,&binding);CHECK(!remesa_context_error(ctx));
  CHECK(remesa_context_prepare_vertex_parameters(ctx,parameter_bank,(void *)ctx)==EINVAL);
  CHECK(!remesa_context_prepare_vertex_parameters(ctx,parameter_bank,parameters));
  ctx->delete_vs_state(ctx,vs);ctx->destroy(ctx);screen->destroy(screen);
 }else
#endif
 CHECK(!remesa_persistent_vertex_parameter_block_prepare(d,coefficients,count,(count+4u)&~3u,parameter_bank,parameters));
}
#ifndef REMESA_GEARS_ATTRIBUTE_OWNER_FIXTURE
int main(void)
{
#if defined(REMESA_WITH_NIR) && defined(REMESA_TEST_REAL_NIR_BUILDER)
 glsl_type_singleton_init_or_ref();
#endif
 CHECK(!inherited_parameter_persistent_tests());CHECK(!inherited_parameter_linkage_tests());
 struct remesa_shader_linkage link=gears_pair();struct remesa_shader_selected_resources resource;
 CHECK(remesa_shader_program_selected_resources(&link.vertex,&resource)==ENOTSUP);
 CHECK(!remesa_shader_program_gears_attribute_resources(&link.vertex,&resource));
 CHECK(resource.profile==REMESA_PDS_SELECTED_VS_GEARS_ATTRIBUTE_PADDED3&&resource.logical_use_bytes==320&&resource.reserved_use_bytes==320);
 CHECK(resource.vertex.input_count==2&&resource.vertex.stride_dwords==8&&resource.vertex.output_dwords==8);
 struct remesa_shader_program state=vertex(0);CHECK(remesa_shader_program_gears_attribute_resources(&state,&resource)==ENOTSUP);
 for(unsigned scenario=0;scenario<3;scenario++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  void *program=NULL,*geometry=NULL,*parameters=NULL,*pds=NULL;
  setup_gears(&f,&d,9800+scenario,&program,&geometry,&parameters,scenario==1?3:4,scenario==2?40:36);
  unsigned creates=f.creates;CHECK(remesa_persistent_linked_pds_prepare(&d,program,geometry,&pds)==ENOTSUP&&!pds&&f.creates==creates);
  int e=remesa_persistent_gears_attribute_pds_prepare(&d,program,geometry,parameters,&pds);
  if(scenario){CHECK(e==(scenario==1?EBUSY:ENOTSUP)&&!pds&&f.creates==creates);}
  else{
   CHECK(!e&&pds);struct remesa_persistent_gears_attribute_pds_backing b;
   CHECK(!remesa_persistent_gears_attribute_pds_snapshot(&d,pds,&b));
   CHECK(b.input_dwords==8&&b.temporary_dwords==11&&b.output_dwords==8);
   CHECK(b.parameters.coefficient_dwords==36&&b.parameters.effective_source_dwords==37&&b.parameters.transfer_dwords==40);
   CHECK(b.pds.geometry.stride_bytes==48&&b.pds.geometry.vertex_count==3);
   CHECK(b.pds.program.program_bytes[0]==320&&b.pds.program.reserved_bytes[0]==320&&b.pds.program.program_offset[1]==320);
   unsigned char expected[320];size_t n;
   CHECK(!remesa_shader_program_encode(&link.vertex,expected,sizeof expected,&n)&&n==320);
   CHECK(!memcmp(expected,get(&f,b.pds.program.handle)->bytes,320));
   unsigned char encoded[64],mask[64];CHECK(!remesa_pds_selected_encode(b.pds.plans,encoded,mask,sizeof encoded,&n)&&n==64);
   CHECK(!memcmp(encoded,get(&f,b.pds.handle)->bytes,64));
   CHECK(shader_pds_le32(encoded+20)==0xe0000000&&shader_pds_le32(encoded+36)==12);
   struct remesa_pds_selected_plan rebuilt;
   CHECK(!remesa_shader_program_prepare_selected_pds(&link.vertex,&b.pds.plans[0].request,&rebuilt));
   struct remesa_pds_selected_request stale=b.pds.plans[0].request;stale.semantic_identity++;
   CHECK(remesa_shader_program_prepare_selected_pds(&link.vertex,&stale,&b.pds.plans[0])==ESTALE);
   uint8_t index[20];CHECK(remesa_persistent_linked_index_encode(&d,pds,0,3,index)==ENOTSUP);
   struct remesa_persistent_linked_state_request request={0};void *draw_state=NULL;
   CHECK(remesa_persistent_linked_state_prepare(&d,pds,&request,4,&draw_state)==ENOTSUP&&!draw_state);
   unsigned maps=f.cpu_maps,reads=f.reads,writes=f.writes;
   for(unsigned i=0;i<200;i++)CHECK(!remesa_persistent_gears_attribute_pds_snapshot(&d,pds,&b));
   CHECK(f.cpu_maps==maps&&f.reads==reads&&f.writes==writes);
   d.ops->destroy_bo(d.priv,program);program=NULL;d.ops->destroy_bo(d.priv,geometry);geometry=NULL;
   d.ops->destroy_bo(d.priv,parameters);parameters=NULL;CHECK(!f.destroys);
   CHECK(!remesa_persistent_gears_attribute_pds_snapshot(&d,pds,&b));
   void *cpu=NULL;CHECK(!d.ops->map(d.priv,pds,false,&cpu)&&cpu);
   d.ops->destroy_bo(d.priv,pds);CHECK(!f.destroys&&remesa_persistent_close(&d)==EBUSY);
   d.ops->unmap(d.priv,pds,false);pds=NULL;
  }
  if(program)d.ops->destroy_bo(d.priv,program);
  if(geometry)d.ops->destroy_bo(d.priv,geometry);
  if(parameters)d.ops->destroy_bo(d.priv,parameters);
  CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);CHECK(!remesa_persistent_close(&d));
 }
 const enum failure failures[]={FAIL_MAP,FAIL_READ_BODY,FAIL_MAP_UNCERTAIN,FAIL_UNMAP_UNCERTAIN};
 for(unsigned i=0;i<sizeof failures/sizeof failures[0];i++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};void *program=NULL,*geometry=NULL,*parameters=NULL,*pds=NULL;
  setup_gears(&f,&d,9820+i,&program,&geometry,&parameters,4,36);f.fail=failures[i];f.fail_map_handle=f.creates+1;
  int e=remesa_persistent_gears_attribute_pds_prepare(&d,program,geometry,parameters,&pds);CHECK(e);
  if(failures[i]==FAIL_MAP||failures[i]==FAIL_READ_BODY){
   CHECK(!pds);d.ops->destroy_bo(d.priv,program);d.ops->destroy_bo(d.priv,geometry);d.ops->destroy_bo(d.priv,parameters);
   CHECK(f.creates==f.destroys&&f.pins==f.unpins);CHECK(!remesa_persistent_close(&d));
  }else{
   CHECK(e==EOWNERDEAD&&pds);d.ops->destroy_bo(d.priv,program);d.ops->destroy_bo(d.priv,geometry);d.ops->destroy_bo(d.priv,parameters);
   CHECK(!f.destroys&&remesa_persistent_close(&d)==EBUSY);
  }
 }
#if defined(REMESA_WITH_NIR) && defined(REMESA_TEST_REAL_NIR_BUILDER)
 glsl_type_singleton_decref();
#endif
 puts("PASS existing gears attribute CODE320/PDS with owned source37/transfer40, semantic admission, 8input/11temp/8output, transitive lifetime/bank conflicts and quarantine; legacy scene/index refusal retained; GPU0");return 0;
}
#endif
