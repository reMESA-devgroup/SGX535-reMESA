/* SPDX-License-Identifier: MIT
 * Actual Mesa shader callbacks and independently authored host lowering.
 * No shader bytes, rendering, completion fixtures or hardware submissions.
 */
#include "remesa_shader_program.h"
#include "remesa_fragment_body.h"
#include "vertex_nir_fixture.h"
#include "uniform_nir_fixture.h"
/* Included test fixture has external allocation interception and an inherited
 * entry point. Declare them for the real Mesa -Wmissing-prototypes contract. */
void *__wrap_calloc(size_t, size_t);
int inherited_nir_main(void);
#define main inherited_nir_main
#include "test_nir.c"
#undef main
#define CHECK(condition) do { assert(condition); ++cases; } while(0)
/* Original constant-channel fixtures. These are NIR operations, not SGX
 * instructions; normalized output still must equal an admitted color profile. */
static nir_shader *channel_fixture(unsigned mode)
{
 nir_shader *s=fixture(0xff00ff00);
 nir_function *fn=exec_node_data(nir_function,exec_list_get_head(&s->functions),node);
 nir_block *b=exec_node_data(nir_block,exec_list_get_head(&fn->impl->body),cf_node.node);
 nir_load_const_instr *load=exec_node_data(nir_load_const_instr,exec_list_get_head(&b->instr_list),instr.node);
 nir_intrinsic_instr *store=exec_node_data(nir_intrinsic_instr,exec_list_get_tail(&b->instr_list),instr.node);
 nir_def *src=&load->def;
 if(mode==3||mode==4)load->def.num_components=mode==3?2:3;
 if(mode<3) {
  nir_alu_instr *mov=rzalloc_size(s,sizeof(*mov)+sizeof(nir_alu_src));
  mov->instr.type=nir_instr_type_alu;mov->instr.block=b;mov->op=nir_op_mov;
  mov->def.parent_instr=&mov->instr;mov->def.bit_size=32;mov->def.num_components=mode==0?4:mode+1;
  mov->src[0].src.ssa=src;
  const unsigned channels[4]={1,0,mode==2?3:1,3};
  for(unsigned c=0;c<mov->def.num_components;c++)mov->src[0].swizzle[c]=channels[c];
  exec_node_insert_node_before(&store->instr.node,&mov->instr.node);src=&mov->def;
 }
 if(mode) {
  nir_alu_instr *vec=rzalloc_size(s,sizeof(*vec)+4*sizeof(nir_alu_src));
  vec->instr.type=nir_instr_type_alu;vec->instr.block=b;vec->op=nir_op_vec4;
  vec->def.parent_instr=&vec->instr;vec->def.bit_size=32;vec->def.num_components=4;
  const unsigned channels[4]={mode==3?1:0,mode==3?0:1,mode==3?1:(mode==4?2:0),mode==2?2:1};
  for(unsigned c=0;c<4;c++) {vec->src[c].src.ssa=(mode==1&&c==3)?&load->def:src;
   vec->src[c].swizzle[0]=(mode==1&&c==3)?3:channels[c];}
  exec_node_insert_node_before(&store->instr.node,&vec->instr.node);src=&vec->def;
 }
 store->src[0].ssa=src;return s;
}
static nir_alu_instr *first_channel_alu(nir_shader *s)
{
 nir_function *fn=exec_node_data(nir_function,exec_list_get_head(&s->functions),node);
 nir_block *b=exec_node_data(nir_block,exec_list_get_head(&fn->impl->body),cf_node.node);
 nir_foreach_instr(i,b)if(i->type==nir_instr_type_alu)return nir_instr_as_alu(i);
 assert(!"missing channel operation");return NULL;
}
static nir_shader *scalar_color_fixture(void)
{
 nir_shader *s=fixture(UINT32_C(0xff00ff00));
 nir_function *fn=exec_node_data(nir_function,exec_list_get_head(&s->functions),node);
 nir_block *b=exec_node_data(nir_block,exec_list_get_head(&fn->impl->body),cf_node.node);
 nir_load_const_instr *value=exec_node_data(nir_load_const_instr,exec_list_get_head(&b->instr_list),instr.node);
 nir_intrinsic_instr *old=exec_node_data(nir_intrinsic_instr,exec_list_get_tail(&b->instr_list),instr.node);
 for(unsigned c=0;c<4;c++) {
  nir_alu_instr *channel=rzalloc_size(s,sizeof(*channel)+sizeof(nir_alu_src));
  channel->instr.type=nir_instr_type_alu;channel->instr.block=b;channel->op=nir_op_mov;
  channel->def.parent_instr=&channel->instr;channel->def.num_components=1;channel->def.bit_size=32;
  channel->src[0].src.ssa=&value->def;channel->src[0].swizzle[0]=c;
  exec_list_push_tail(&b->instr_list,&channel->instr.node);
  nir_intrinsic_instr *store=rzalloc_size(s,sizeof(*store)+2*sizeof(nir_src));
  store->instr.type=nir_instr_type_intrinsic;store->instr.block=b;store->intrinsic=nir_intrinsic_store_output;
  store->num_components=1;store->src[0].ssa=&channel->def;store->src[1].ssa=old->src[1].ssa;
  nir_intrinsic_set_write_mask(store,1);nir_intrinsic_set_component(store,c);
  nir_intrinsic_set_src_type(store,nir_type_float32);
  nir_intrinsic_set_io_semantics(store,(nir_io_semantics){.location=FRAG_RESULT_DATA0,.num_slots=1});
  exec_list_push_tail(&b->instr_list,&store->instr.node);
 }
 exec_node_remove(&old->instr.node);return s;
}
static nir_shader *vertex_group_fixture(unsigned components,bool duplicate_x)
{
 nir_shader *s=vertex_fixture();
 ralloc_set_destructor(s,destroy_marker);
 nir_function *fn=exec_node_data(nir_function,exec_list_get_head(&s->functions),node);
 nir_block *b=exec_node_data(nir_block,exec_list_get_head(&fn->impl->body),cf_node.node);
 nir_intrinsic_instr *store=exec_node_data(nir_intrinsic_instr,exec_list_get_tail(&b->instr_list),instr.node);
 nir_def *input=store->src[0].ssa;
 nir_alu_instr *group=rzalloc_size(s,sizeof(*group)+components*sizeof(nir_alu_src));
 group->instr.type=nir_instr_type_alu;group->instr.block=b;group->op=components==2?nir_op_vec2:nir_op_vec3;
 group->def.parent_instr=&group->instr;group->def.bit_size=32;group->def.num_components=components;
 for(unsigned k=0;k<components;k++){group->src[k].src.ssa=input;group->src[k].swizzle[0]=k;}
 exec_node_insert_node_before(&store->instr.node,&group->instr.node);
 nir_alu_instr *join=rzalloc_size(s,sizeof(*join)+4*sizeof(nir_alu_src));
 join->instr.type=nir_instr_type_alu;join->instr.block=b;join->op=nir_op_vec4;
 join->def.parent_instr=&join->instr;join->def.bit_size=32;join->def.num_components=4;
 for(unsigned k=0;k<4;k++){join->src[k].src.ssa=k<components?&group->def:input;join->src[k].swizzle[0]=k;}
 if(duplicate_x)join->src[0].swizzle[0]=1;
 exec_node_insert_node_before(&store->instr.node,&join->instr.node);store->src[0].ssa=&join->def;
 return s;
}
int main(void)
{
 unsigned cases=0;
 /* Candidate packing exercises Gama's supplied partition only. A word here
  * is a research preview, never a program accepted by the submit path. */
 struct remesa_pds_candidate_request candidate={.policy=REMESA_PDS_CANDIDATE_ANALYSIS_ONLY,
  .selector={0,1,2,2}};
 struct remesa_pds_candidate_preview preview={0},preview_saved;
 CHECK(!remesa_pds_candidate_preview(&candidate,&preview));
 uint32_t base_word=(7u<<24)|(1u<<9)|(2u<<7)|(2u<<5)|5u;
 CHECK(preview.word==base_word);
 CHECK(!preview.output_arity_known&&!preview.fetch_verified&&!preview.semantics_verified);
 candidate.source[0]=1;candidate.source[1]=1;
 CHECK(!remesa_pds_candidate_preview(&candidate,&preview));
 CHECK((preview.word^base_word)==((1u<<18)|(1u<<13)));
 for(unsigned n=0;n<4;n++) {
  unsigned bank=candidate.selector[n]>>1,half=candidate.selector[n]&1;
  CHECK(preview.pair[n].bank==bank&&preview.pair[n].index==2+half);
  CHECK(preview.scalar[n].bank==bank&&preview.scalar[n].index==1+half);
 }
 candidate.source[0]=1;candidate.source[1]=24;candidate.selector[3]=3;
 CHECK(!remesa_pds_candidate_preview(&candidate,&preview));
 CHECK(preview.pair[2].index==48&&preview.pair[3].index==49);
 CHECK(preview.scalar[2].index==24&&preview.scalar[3].index==25);
 CHECK(!memcmp(&candidate,&preview.decoded,sizeof(candidate)));
 preview_saved=preview;
 candidate.policy=0;
 CHECK(remesa_pds_candidate_preview(&candidate,&preview)==ENOTSUP);
 CHECK(!memcmp(&preview,&preview_saved,sizeof(preview)));
 candidate.policy=REMESA_PDS_CANDIDATE_ANALYSIS_ONLY;
 candidate.source[0]=64;
 CHECK(remesa_pds_candidate_preview(&candidate,&preview)==ERANGE);
 candidate.source[0]=1;candidate.source[1]=32;
 CHECK(remesa_pds_candidate_preview(&candidate,&preview)==ERANGE);
 candidate.source[1]=24;candidate.selector[0]=4;
 CHECK(remesa_pds_candidate_preview(&candidate,&preview)==ERANGE);
 CHECK(!memcmp(&preview,&preview_saved,sizeof(preview)));
 candidate.selector[0]=0;
 struct remesa_pds_candidate_request decoded=candidate;
 CHECK(remesa_pds_candidate_decode(preview.word|(1u<<4),&decoded)==EPROTO);
 CHECK(!memcmp(&decoded,&candidate,sizeof(decoded)));
 CHECK(remesa_pds_candidate_decode(preview.word|(1u<<27),&decoded)==ENOTSUP);
 CHECK(remesa_pds_candidate_decode(preview.word^(1u<<24),&decoded)==ENOTSUP);
 CHECK(remesa_pds_candidate_decode(preview.word^1u,&decoded)==ENOTSUP);
 CHECK(!memcmp(&decoded,&candidate,sizeof(decoded)));
 CHECK(remesa_pds_candidate_decode(preview.word,NULL)==EINVAL);
 union candidate_storage {struct remesa_pds_candidate_request request;
  struct remesa_pds_candidate_preview preview;} candidate_box,candidate_snapshot;
 memset(&candidate_box,0,sizeof(candidate_box));candidate_box.request=candidate;candidate_snapshot=candidate_box;
 CHECK(remesa_pds_candidate_preview(&candidate_box.request,&candidate_box.preview)==EINVAL);
 CHECK(!memcmp(&candidate_box,&candidate_snapshot,sizeof(candidate_box)));
 candidate.source[0]=63;candidate.source[1]=31;
 CHECK(!remesa_pds_candidate_preview(&candidate,&preview));
 CHECK(preview.pair[1].index==127&&preview.pair[3].index==63);
 candidate.source[0]=1;candidate.source[1]=24;
 /* Explicit condition3 analysis of the compiled-VS CPU suffix. This does not
  * turn a preview into CODE or identify the physical predicate/DS mapping. */
 {
 struct remesa_pds_candidate_preview preview={0},preview_saved;
 struct remesa_pds_candidate_request decoded;
 struct remesa_pds_candidate_request vertex_candidate={
  .policy=REMESA_PDS_CANDIDATE_ANALYSIS_ONLY,.source={2,0},.selector={0,1,3,3}};
 CHECK(!remesa_pds_candidate_preview_condition(&vertex_candidate,3,&preview));
 CHECK(preview.word==((3u<<24)|(2u<<18)|(1u<<9)|(3u<<7)|(3u<<5)|5u));
 CHECK(!memcmp(&preview.decoded,&vertex_candidate,sizeof(vertex_candidate)));
 CHECK(preview.pair[0].bank==0&&preview.pair[0].index==4&&
  preview.pair[1].bank==0&&preview.pair[1].index==5&&
  preview.pair[2].bank==1&&preview.pair[2].index==1&&
  preview.pair[3].bank==1&&preview.pair[3].index==1);
 CHECK(preview.scalar[0].index==2&&preview.scalar[1].index==3);
 CHECK(!preview.output_arity_known&&!preview.fetch_verified&&!preview.semantics_verified);
 preview_saved=preview;decoded=candidate;
 CHECK(remesa_pds_candidate_decode(preview.word,&decoded)==ENOTSUP&&
  !memcmp(&decoded,&candidate,sizeof(decoded)));
 CHECK(!remesa_pds_candidate_decode_condition(preview.word,3,&decoded)&&
  !memcmp(&decoded,&vertex_candidate,sizeof(decoded)));
 struct remesa_pds_candidate_request saved_decoded=decoded;
 CHECK(remesa_pds_candidate_decode_condition(preview.word,7,&decoded)==ENOTSUP&&
  !memcmp(&decoded,&saved_decoded,sizeof(decoded)));
 CHECK(remesa_pds_candidate_decode_condition(preview.word|(1u<<4),3,&decoded)==EPROTO&&
  !memcmp(&decoded,&saved_decoded,sizeof(decoded)));
 for(unsigned condition=0;condition<9;condition++)if(condition!=3&&condition!=7) {
  CHECK(remesa_pds_candidate_preview_condition(&vertex_candidate,condition,&preview)==ENOTSUP&&
   !memcmp(&preview,&preview_saved,sizeof(preview)));
  CHECK(remesa_pds_candidate_decode_condition(preview.word,condition,&decoded)==ENOTSUP&&
   !memcmp(&decoded,&saved_decoded,sizeof(decoded)));
 }
 CHECK(remesa_pds_candidate_preview_condition(&candidate_box.request,3,&candidate_box.preview)==EINVAL);
 CHECK(!memcmp(&candidate_box,&candidate_snapshot,sizeof(candidate_box)));
 /* Supported numerical boundaries roundtrip; they are not hardware limits. */
 vertex_candidate.source[0]=63;vertex_candidate.source[1]=31;
 for(unsigned condition=3;condition<=7;condition+=4) {
  CHECK(!remesa_pds_candidate_preview_condition(&vertex_candidate,condition,&preview));
  CHECK(!remesa_pds_candidate_decode_condition(preview.word,condition,&decoded)&&
   !memcmp(&decoded,&vertex_candidate,sizeof(decoded)));
 }
 }
 struct remesa_shader_ir ir={0};
 nir_shader *nir=fixture(0xffff00ff);
 CHECK(!remesa_shader_ir_lower(nir,&ir));ralloc_free(nir);
 struct remesa_shader_program program={0},copy={0};
 CHECK(!remesa_shader_program_prepare(&ir,&program));copy=program;
 CHECK(!remesa_shader_program_validate(&program));
 CHECK(!remesa_shader_program_prepare(&ir,&copy)&&!memcmp(&copy,&program,sizeof(copy)));
 /* Compilation keeps the original immutable semantic IR, while pure copies
  * do not consume extra scalar values in its independently authored plan. */
 struct remesa_shader_ir copied=ir;
 copied.instructions[2]=copied.instructions[1];
 copied.instructions[2].src[0].reg=1;
 copied.instructions[1]=(struct remesa_ir_instruction){
  .op=REMESA_IR_COPY,.dst=1,.components=4,.type=REMESA_IR_FLOAT32};
 for(unsigned c=0;c<4;c++)copied.instructions[1].src[c].channel=c;
 copied.count=3;
 CHECK(!remesa_shader_program_prepare(&copied,&copy));
 CHECK(!memcmp(&copy.usse,&program.usse,sizeof(program.usse)));
 CHECK(!memcmp(&copy.semantic,&copied,sizeof(copied)));
 CHECK(!remesa_shader_program_validate(&copy));
 copy=program;
 memset(&ir,0,sizeof(ir));CHECK(!remesa_shader_program_validate(&program));
 struct remesa_shader_program_report report={0};
 CHECK(!remesa_shader_program_report(&program,&report));
 CHECK(report.stage==REMESA_IR_FRAGMENT&&report.status==REMESA_PROGRAM_SEMANTIC_ONLY&&
       report.virtual_instructions==6&&!report.binary_error&&report.binary_bytes==8&&
       report.launch_error==ENOTSUP);
 CHECK(report.register_evidence.use_banks==16&&
       report.register_evidence.use_address_bits==25&&
       report.register_evidence.declared_pds_alignment_shift==20&&
       !report.register_evidence.use_va_packing_verified&&
       !report.register_evidence.rev121_verified&&
       !report.register_evidence.instruction_fields_known);
 unsigned char binary[128],before[128];memset(binary,0xa5,sizeof(binary));memcpy(before,binary,sizeof(before));
 size_t written=987;
 CHECK(!remesa_shader_program_encode(&program,binary,sizeof(binary),&written)&&written==8);
 struct remesa_fragment_constant encoded;
 CHECK(!remesa_fragment_constant_decode(binary,written,&encoded)&&encoded.argb==0xffff00ff);
 CHECK(!memcmp(binary+8,before+8,sizeof(binary)-8));
 memcpy(binary,before,sizeof(binary));written=987;
 CHECK(remesa_shader_program_encode(&program,NULL,1,&written)==EINVAL);
 CHECK(remesa_shader_program_encode(&program,NULL,0,&written)==ENOSPC&&written==987);
 /* A different independently valid lowering must not become this program. */
 nir=fixture(0xff00ff00);CHECK(!remesa_shader_ir_lower(nir,&ir));ralloc_free(nir);
 CHECK(!remesa_usse_lower(&ir,&copy.usse));
 CHECK(remesa_shader_program_validate(&copy)==EPROTO);
 struct remesa_shader_program original=copy;
 CHECK(remesa_shader_program_report(&copy,&report)==EPROTO&&!memcmp(&copy,&original,sizeof(copy)));
 CHECK(remesa_shader_program_encode(&copy,binary,sizeof(binary),&written)==EPROTO&&written==987);
 CHECK(!memcmp(binary,before,sizeof(binary)));
 copy=program;copy.status=0;CHECK(remesa_shader_program_validate(&copy)==ENOTSUP);
 copy=program;copy.schema=2;CHECK(remesa_shader_program_validate(&copy)==ENOTSUP);
 /* Output aliases must not mutate an immutable compilation snapshot. */
 union program_storage {struct remesa_shader_program program;struct remesa_shader_ir ir;
                        struct remesa_shader_program_report report;struct remesa_pds_plan plan;} box,snapshot;
 memset(&box,0,sizeof(box));box.program=program;snapshot=box;
 CHECK(remesa_shader_program_prepare(&box.program.semantic,&box.program)==EINVAL);
 CHECK(!memcmp(&box,&snapshot,sizeof(box)));
 CHECK(remesa_shader_program_report(&box.program,&box.report)==EINVAL);
 CHECK(!memcmp(&box,&snapshot,sizeof(box)));
 /* Symbolic storage metadata only, never authority or a GPU launch image. */
 struct remesa_pds_request request={.operation=2,.generation=3,.owner_cookie=4,
  .code={.allocation_cookie=5,.owner_cookie=4,.mapping_generation=6,
         .address=0x1000,.offset=16,.bytes=32,.allocation_bytes=64},
  .data={.allocation_cookie=7,.owner_cookie=4,.mapping_generation=8,
         .address=0x2000,.offset=32,.bytes=32,.allocation_bytes=128}};
 struct remesa_pds_plan plan={0};
 CHECK(remesa_shader_program_prepare_launch(&box.program,&request,&box.plan)==EINVAL);
 CHECK(!memcmp(&box,&snapshot,sizeof(box)));
 CHECK(!remesa_shader_program_prepare_launch(&program,&request,&plan));
 CHECK(remesa_shader_program_encode_launch(&program,&plan,&request,binary,sizeof(binary),&written)==ENOTSUP);
 CHECK(written==987&&!memcmp(binary,before,sizeof(binary)));
 struct remesa_pds_request current=request;current.generation++;
 CHECK(remesa_shader_program_encode_launch(&program,&plan,&current,binary,sizeof(binary),&written)==ESTALE);
 CHECK(written==987&&!memcmp(binary,before,sizeof(binary)));
 current=request;current.code.mapping_generation++;
 CHECK(remesa_shader_program_encode_launch(&program,&plan,&current,binary,sizeof(binary),&written)==ESTALE);
 CHECK(remesa_shader_program_encode_launch(&program,&plan,NULL,binary,sizeof(binary),&written)==EINVAL);
 /* Link actual independently lowered stages; the result carries no bytes,
  * resource handles or execution authority. Equivalent copy routing links
  * identically; a different color requires a fresh pair. */
 struct remesa_shader_program vertex_program={0},green_program={0};
 nir=vertex_fixture();CHECK(!remesa_shader_ir_lower(nir,&ir));ralloc_free(nir);
 CHECK(!remesa_shader_program_prepare(&ir,&vertex_program));
 nir=fixture(0xff00ff00);CHECK(!remesa_shader_ir_lower(nir,&ir));ralloc_free(nir);
 CHECK(!remesa_shader_program_prepare(&ir,&green_program));
 /* A synchronized uniform snapshot is specialized into the same immutable
  * constant fragment program as actual NIR, without a historical color blob. */
 const uint32_t colors[]={0xffff00ff,0xff00ff00,0xffffff00};
 for(unsigned n=0;n<3;n++) {
  struct remesa_shader_program specialized,expected;
  CHECK(!remesa_shader_program_prepare_color(colors[n],&specialized));
  CHECK(!remesa_shader_program_validate(&specialized));
  nir=fixture(colors[n]);CHECK(!remesa_shader_ir_lower(nir,&ir));ralloc_free(nir);
  CHECK(!remesa_shader_program_prepare(&ir,&expected));
  CHECK(!memcmp(&specialized,&expected,sizeof(expected)));
  uint32_t resolved=0;
  CHECK(!remesa_shader_ir_fragment_color(&specialized.semantic,&resolved)&&resolved==colors[n]);
 }
 const uint32_t unsupported_colors[]={0,0xff000000,0xffffffff,0xff0000ff,
  0xffff0000,0xff00fe00,0x7f00ff00};
 struct remesa_shader_program specialized_sentinel;
 memset(&specialized_sentinel,0xa5,sizeof(specialized_sentinel));
 for(unsigned n=0;n<sizeof(unsupported_colors)/sizeof(unsupported_colors[0]);n++) {
  copy=specialized_sentinel;
  CHECK(remesa_shader_program_prepare_color(unsupported_colors[n],&copy)==ENOTSUP);
  CHECK(!memcmp(&copy,&specialized_sentinel,sizeof(copy)));
 }
 CHECK(remesa_shader_program_prepare_color(0xff00ff00,NULL)==EINVAL);
 struct remesa_shader_pipeline linked={0},linked_sentinel,linked_copy;
 memset(&linked_sentinel,0xa5,sizeof(linked_sentinel));linked=linked_sentinel;
 CHECK(!remesa_shader_pipeline_prepare(&vertex_program,&program,&linked));
 CHECK(linked.schema==REMESA_SHADER_PIPELINE_SCHEMA&&linked.status==REMESA_PROGRAM_SEMANTIC_ONLY&&
  linked.position_input_slot==0&&linked.fragment_argb==0xffff00ff&&linked.binary_error==ENOTSUP);
 CHECK(linked.virtual_instructions[0]==vertex_program.usse.count&&linked.virtual_instructions[1]==program.usse.count);
 CHECK(!remesa_shader_pipeline_revalidate(&linked,&vertex_program,&program));
 CHECK(remesa_shader_pipeline_revalidate(&linked,&vertex_program,&green_program)==ESTALE);
 CHECK(remesa_shader_pipeline_revalidate(NULL,&vertex_program,&program)==EINVAL);
 for(unsigned field=0;field<9;field++) {
  linked_copy=linked;
  switch(field) {
   case 0:linked_copy.schema++;break;
   case 1:linked_copy.status=0;break;
   case 2:linked_copy.identity[0]^=1;break;
   case 3:linked_copy.identity[1]^=1;break;
   case 4:linked_copy.virtual_instructions[0]++;break;
   case 5:linked_copy.virtual_instructions[1]++;break;
   case 6:linked_copy.position_input_slot++;break;
   case 7:linked_copy.fragment_argb^=1;break;
   case 8:linked_copy.binary_error=0;break;
  }
  CHECK(remesa_shader_pipeline_revalidate(&linked_copy,&vertex_program,&program)==ESTALE);
 }
 linked_copy=linked_sentinel;
 CHECK(remesa_shader_pipeline_prepare(&program,&vertex_program,&linked_copy)==ENOTSUP);
 CHECK(!memcmp(&linked_copy,&linked_sentinel,sizeof(linked_copy)));
 CHECK(remesa_shader_pipeline_prepare(NULL,&program,&linked_copy)==EINVAL);
 CHECK(remesa_shader_pipeline_prepare(&vertex_program,NULL,&linked_copy)==EINVAL);
 CHECK(remesa_shader_pipeline_prepare(&vertex_program,&program,NULL)==EINVAL);
 CHECK(!memcmp(&linked_copy,&linked_sentinel,sizeof(linked_copy)));
 copy=program;copy.usse.instructions[0].bits^=1;
 CHECK(remesa_shader_pipeline_prepare(&vertex_program,&copy,&linked_copy)!=0);
 CHECK(!memcmp(&linked_copy,&linked_sentinel,sizeof(linked_copy)));
 CHECK(remesa_shader_pipeline_revalidate(&linked,&vertex_program,&copy)!=0);
 union linked_storage {struct remesa_shader_program program;struct remesa_shader_pipeline pair;} linked_box,linked_before;
 linked_box.program=vertex_program;linked_before=linked_box;
 CHECK(remesa_shader_pipeline_prepare(&linked_box.program,&program,&linked_box.pair)==EINVAL);
 CHECK(!memcmp(&linked_box,&linked_before,sizeof(linked_box)));
 /* Backend handoff owns both complete programs; semantic keys alone cannot
  * authenticate a later rebind or replace independently compiled programs. */
 struct remesa_shader_draw_programs draw_programs,draw_sentinel,draw_copy;
 memset(&draw_sentinel,0xa5,sizeof(draw_sentinel));draw_programs=draw_sentinel;
 CHECK(!remesa_shader_draw_programs_prepare(&vertex_program,&program,&draw_programs));
 CHECK(!remesa_shader_draw_programs_validate(&draw_programs));
 CHECK(!remesa_shader_draw_programs_revalidate(&draw_programs,&vertex_program,&program));
 CHECK(!memcmp(&draw_programs.vertex,&vertex_program,sizeof(vertex_program)));
 CHECK(!memcmp(&draw_programs.fragment,&program,sizeof(program)));
 CHECK(draw_programs.pipeline.fragment_argb==0xffff00ff&&
       draw_programs.pipeline.binary_error==ENOTSUP);
 CHECK(remesa_shader_draw_programs_revalidate(&draw_programs,&vertex_program,&green_program)==ESTALE);
 CHECK(!remesa_shader_program_prepare(&copied,&copy));
 CHECK(!memcmp(&copy.usse,&program.usse,sizeof(copy.usse)));
 CHECK(remesa_shader_draw_programs_revalidate(&draw_programs,&vertex_program,&copy)==ESTALE);
 struct remesa_shader_program transient_vertex=vertex_program,transient_fragment=program;
 CHECK(!remesa_shader_draw_programs_prepare(&transient_vertex,&transient_fragment,&draw_copy));
 remesa_shader_program_finish(&transient_vertex);remesa_shader_program_finish(&transient_fragment);
 CHECK(!remesa_shader_draw_programs_validate(&draw_copy));
 CHECK(!memcmp(&draw_copy.fragment,&program,sizeof(program)));
 copy=program;remesa_shader_program_finish(&copy);
 CHECK(!remesa_shader_draw_programs_validate(&draw_programs));
 CHECK(remesa_shader_draw_programs_revalidate(&draw_programs,&vertex_program,&copy)==ENOTSUP);
 draw_copy=draw_programs;draw_copy.fragment=green_program;
 CHECK(remesa_shader_draw_programs_validate(&draw_copy)==ESTALE);
 draw_copy=draw_programs;draw_copy.vertex.usse.instructions[0].src^=1;
 CHECK(remesa_shader_draw_programs_validate(&draw_copy)!=0);
 draw_copy=draw_programs;draw_copy.pipeline.binary_error=0;
 CHECK(remesa_shader_draw_programs_validate(&draw_copy)==ESTALE);
 draw_copy=draw_sentinel;
 CHECK(remesa_shader_draw_programs_prepare(&program,&vertex_program,&draw_copy)==ENOTSUP);
 CHECK(!memcmp(&draw_copy,&draw_sentinel,sizeof(draw_copy)));
 CHECK(remesa_shader_draw_programs_prepare(NULL,&program,&draw_copy)==EINVAL);
 CHECK(remesa_shader_draw_programs_prepare(&vertex_program,NULL,&draw_copy)==EINVAL);
 CHECK(remesa_shader_draw_programs_prepare(&vertex_program,&program,NULL)==EINVAL);
 CHECK(!memcmp(&draw_copy,&draw_sentinel,sizeof(draw_copy)));
 CHECK(remesa_shader_draw_programs_validate(NULL)==EINVAL);
 CHECK(remesa_shader_draw_programs_revalidate(NULL,&vertex_program,&program)==EINVAL);
 CHECK(remesa_shader_draw_programs_revalidate(&draw_programs,NULL,&program)==EINVAL);
 CHECK(remesa_shader_draw_programs_revalidate(&draw_programs,&vertex_program,NULL)==EINVAL);
 union draw_program_storage {struct remesa_shader_program program;
  struct remesa_shader_draw_programs draw;} draw_box,draw_before;
 memset(&draw_box,0,sizeof(draw_box));draw_box.program=vertex_program;draw_before=draw_box;
 CHECK(remesa_shader_draw_programs_prepare(&draw_box.program,&program,&draw_box.draw)==EINVAL);
 CHECK(!memcmp(&draw_box,&draw_before,sizeof(draw_box)));
 draw_box.draw.fragment=program;draw_before=draw_box;
 CHECK(remesa_shader_draw_programs_prepare(&vertex_program,&draw_box.draw.fragment,&draw_box.draw)==EINVAL);
 CHECK(!memcmp(&draw_box,&draw_before,sizeof(draw_box)));
 /* Both symbolic program layouts share one retained allocation, with four
  * disjoint windows. This is an explicit host packing policy, not proof of
  * the SGX data/code boundary or receiver fetch rule. */
 struct remesa_pds_block_request block={.operation=2,.generation=3,
  .allocation={.allocation_cookie=11,.owner_cookie=4,.mapping_generation=6,
   .address=0x1000,.offset=0,.bytes=64,.allocation_bytes=128},
  .data_bytes=16,.code_bytes=16,.code_alignment=16};
 struct remesa_pds_request stage_requests[2];
 CHECK(!remesa_pds_request_from_block(&block,&stage_requests[0]));
 block.allocation.offset=64;
 CHECK(!remesa_pds_request_from_block(&block,&stage_requests[1]));
 struct remesa_shader_pipeline_launch launch,launch_sentinel,launch_copy;
 memset(&launch_sentinel,0xa5,sizeof(launch_sentinel));launch=launch_sentinel;
 CHECK(!remesa_shader_pipeline_prepare_launch(&vertex_program,&program,
  &stage_requests[0],&stage_requests[1],&launch));
 CHECK(launch.stage[0].stage==REMESA_IR_VERTEX&&launch.stage[1].stage==REMESA_IR_FRAGMENT);
 CHECK(launch.stage[0].relocations[0].address==0x1010&&launch.stage[1].relocations[0].address==0x1050);
 CHECK(launch.semantic.fragment_argb==0xffff00ff&&launch.semantic.binary_error==ENOTSUP);
 CHECK(!remesa_shader_pipeline_revalidate_launch(&launch,&vertex_program,&program,
  &stage_requests[0],&stage_requests[1]));
 CHECK(remesa_shader_pipeline_revalidate_launch(&launch,&vertex_program,&green_program,
  &stage_requests[0],&stage_requests[1])==ESTALE);
 for(unsigned failure=0;failure<8;failure++) {
  struct remesa_pds_request bad=stage_requests[1];launch_copy=launch_sentinel;
  switch(failure) {
   case 0:bad.operation++;break;
   case 1:bad.generation++;break;
   case 2:bad.owner_cookie++;bad.code.owner_cookie++;bad.data.owner_cookie++;break;
   case 3:bad.code.mapping_generation++;bad.data.mapping_generation++;break;
   case 4:bad.code.offset=stage_requests[0].code.offset;break;
   case 5:bad.data.offset=stage_requests[0].data.offset;break;
   case 6:bad.code.allocation_cookie++;bad.data.allocation_cookie++;break;
   case 7:bad.code.bytes=UINT64_MAX;break;
  }
  CHECK(remesa_shader_pipeline_prepare_launch(&vertex_program,&program,&stage_requests[0],&bad,&launch_copy)!=0);
  CHECK(!memcmp(&launch_copy,&launch_sentinel,sizeof(launch_copy)));
  CHECK(remesa_shader_pipeline_revalidate_launch(&launch,&vertex_program,&program,&stage_requests[0],&bad)!=0);
 }
 CHECK(remesa_shader_pipeline_prepare_launch(&vertex_program,&program,NULL,&stage_requests[1],&launch_copy)==EINVAL);
 CHECK(remesa_shader_pipeline_prepare_launch(&vertex_program,&program,&stage_requests[0],NULL,&launch_copy)==EINVAL);
 CHECK(remesa_shader_pipeline_prepare_launch(&vertex_program,&program,&stage_requests[0],&stage_requests[1],NULL)==EINVAL);
 CHECK(!memcmp(&launch_copy,&launch_sentinel,sizeof(launch_copy)));
 launch_copy=launch;launch_copy.stage[0]=launch.stage[1];
 CHECK(remesa_shader_pipeline_revalidate_launch(&launch_copy,&vertex_program,&program,
  &stage_requests[0],&stage_requests[1])==ESTALE);
 launch_copy=launch;launch_copy.stage[1].relocations[0].address++;
 CHECK(remesa_shader_pipeline_revalidate_launch(&launch_copy,&vertex_program,&program,
  &stage_requests[0],&stage_requests[1])==EINVAL);
 CHECK(remesa_shader_pipeline_revalidate_launch(NULL,&vertex_program,&program,
  &stage_requests[0],&stage_requests[1])==EINVAL);
 union launch_storage {struct remesa_shader_program program;struct remesa_pds_request request;
  struct remesa_shader_pipeline_launch launch;} launch_box,launch_before;
 memset(&launch_box,0,sizeof(launch_box));launch_box.program=vertex_program;launch_before=launch_box;
 CHECK(remesa_shader_pipeline_prepare_launch(&launch_box.program,&program,&stage_requests[0],
  &stage_requests[1],&launch_box.launch)==EINVAL);
 CHECK(!memcmp(&launch_box,&launch_before,sizeof(launch_box)));
 launch_box.request=stage_requests[1];launch_before=launch_box;
 CHECK(remesa_shader_pipeline_prepare_launch(&vertex_program,&program,&stage_requests[0],
  &launch_box.request,&launch_box.launch)==EINVAL);
 CHECK(!memcmp(&launch_box,&launch_before,sizeof(launch_box)));
 linked_box.program=program;linked_before=linked_box;
 CHECK(remesa_shader_pipeline_prepare(&vertex_program,&linked_box.program,&linked_box.pair)==EINVAL);
 CHECK(!memcmp(&linked_box,&linked_before,sizeof(linked_box)));
 remesa_shader_program_finish(&program);remesa_shader_program_finish(&program);
 CHECK(remesa_shader_program_validate(&program)==ENOTSUP);

 static const struct remesa_device_ops device_ops={.allocate=alloc,.destroy_bo=release,.map=map,.unmap=unmap,.submit=submit,.wait=wait,.destroy_fence=release};
 struct remesa_device dev={.ops=&device_ops,.offline_test_backend=true};
 struct pipe_screen *screen=remesa_screen_create(&dev);CHECK(screen);
 struct pipe_context *ctx=screen->context_create(screen,NULL,0),*other=screen->context_create(screen,NULL,0);
 CHECK(ctx&&other);
 linked_copy=linked_sentinel;
 CHECK(remesa_context_shader_pipeline_report(ctx,&linked_copy)==ENODATA);
 CHECK(!memcmp(&linked_copy,&linked_sentinel,sizeof(linked_copy)));
 CHECK(remesa_context_shader_pipeline_report(NULL,&linked_copy)==EINVAL);
 CHECK(remesa_context_shader_pipeline_report(ctx,NULL)==EINVAL);
 launch_copy=launch_sentinel;
 CHECK(remesa_context_prepare_shader_launch(ctx,&stage_requests[0],&stage_requests[1],&launch_copy)==ENODATA);
 CHECK(!memcmp(&launch_copy,&launch_sentinel,sizeof(launch_copy)));
 struct pipe_shader_state state={.type=PIPE_SHADER_IR_NIR};
 for(unsigned mode=0;mode<5;mode++) {
  nir=channel_fixture(mode);uint32_t color=0;
  CHECK(!remesa_nir_constant_fragment(nir,&color)&&color==(mode==4?0xff00ff00:0xffff00ff));
  CHECK(!remesa_shader_ir_lower(nir,&ir));
  struct remesa_shader_program channels={0};CHECK(!remesa_shader_program_prepare(&ir,&channels));
  CHECK(!remesa_shader_program_validate(&channels));
  state.ir.nir=nir;unsigned freed_before=destroyed;void *handle=ctx->create_fs_state(ctx,&state);
  CHECK(handle&&destroyed==freed_before+1);ctx->bind_fs_state(ctx,handle);CHECK(!remesa_context_error(ctx));
  CHECK(!remesa_context_shader_program_report(ctx,PIPE_SHADER_FRAGMENT,handle,&report));
  CHECK(!report.binary_error&&report.binary_bytes==8&&report.launch_error==ENOTSUP);
  CHECK(!remesa_context_shader_program_encode(ctx,PIPE_SHADER_FRAGMENT,handle,binary,sizeof(binary),&written)&&written==8);
  CHECK(!remesa_fragment_constant_decode(binary,written,&encoded)&&encoded.argb==color);
  CHECK(!memcmp(binary+8,before+8,sizeof(binary)-8));
  memcpy(binary,before,sizeof(binary));written=987;
  ctx->delete_fs_state(ctx,handle);
  CHECK(remesa_context_shader_program_report(ctx,PIPE_SHADER_FRAGMENT,handle,&report)==EINVAL);
 }
 /* Component-store lowering reaches the real shader callbacks, and the
  * owned program remains valid after Gallium consumes/frees its NIR. */
 state.ir.nir=scalar_color_fixture();unsigned scalar_freed=destroyed;
 void *scalar_fs=ctx->create_fs_state(ctx,&state);
 CHECK(scalar_fs&&destroyed==scalar_freed+1);
 ctx->bind_fs_state(ctx,scalar_fs);CHECK(!remesa_context_error(ctx));
 CHECK(!remesa_context_shader_program_report(ctx,PIPE_SHADER_FRAGMENT,scalar_fs,&report));
 CHECK(report.stage==REMESA_IR_FRAGMENT&&!report.binary_error&&report.binary_bytes==8&&report.launch_error==ENOTSUP);
 CHECK(!remesa_context_shader_program_encode(ctx,PIPE_SHADER_FRAGMENT,scalar_fs,binary,sizeof(binary),&written)&&written==8);
 CHECK(!remesa_fragment_constant_decode(binary,written,&encoded)&&encoded.argb==0xff00ff00);
 CHECK(!memcmp(binary+8,before+8,sizeof(binary)-8));
 memcpy(binary,before,sizeof(binary));written=987;
 ctx->delete_fs_state(ctx,scalar_fs);
 CHECK(remesa_context_shader_program_report(ctx,PIPE_SHADER_FRAGMENT,scalar_fs,&report)==EINVAL);
 for(unsigned mode=0;mode<7;mode++) {
  nir=channel_fixture(0);nir_alu_instr *alu=first_channel_alu(nir);uint32_t color=123;
  if(mode==0)alu->src[0].swizzle[0]=4;
  if(mode==1)alu->def.bit_size=16;
  if(mode==2)alu->def.num_components=5;
  if(mode==3)alu->exact=true;
  if(mode==4)alu->op=nir_op_fadd;
  if(mode==5)alu->src[0].src.ssa=NULL;
  if(mode==6)alu->src[0].src.ssa=&alu->def;
  CHECK(remesa_nir_constant_fragment(nir,&color)==ENOTSUP&&color==123);
  state.ir.nir=nir;unsigned before=destroyed;
  CHECK(!ctx->create_fs_state(ctx,&state)&&destroyed==before+1);
 }
 for(unsigned width=2;width<=3;width++) {
  state.ir.nir=vertex_group_fixture(width,false);unsigned freed=destroyed;
  void *handle=ctx->create_vs_state(ctx,&state);
  CHECK(handle&&destroyed==freed+1);ctx->bind_vs_state(ctx,handle);CHECK(!remesa_context_error(ctx));
  CHECK(!remesa_context_shader_program_report(ctx,PIPE_SHADER_VERTEX,handle,&report));
  CHECK(report.stage==REMESA_IR_VERTEX&&report.binary_error==ENOTSUP);
  CHECK(remesa_context_shader_program_encode(ctx,PIPE_SHADER_VERTEX,handle,binary,sizeof(binary),&written)==ENOTSUP);
  CHECK(written==987&&!memcmp(binary,before,sizeof(binary)));
  ctx->delete_vs_state(ctx,handle);
  state.ir.nir=vertex_group_fixture(width,true);freed=destroyed;
  CHECK(!ctx->create_vs_state(ctx,&state)&&destroyed==freed+1);
 }
 state.ir.nir=vertex_fixture();unsigned freed=destroyed;
 void *vs=ctx->create_vs_state(ctx,&state);CHECK(vs);
 state.ir.nir=fixture(0xffff00ff);void *fs=ctx->create_fs_state(ctx,&state);
 CHECK(fs&&destroyed==freed+1);
 CHECK(!remesa_context_shader_program_report(ctx,PIPE_SHADER_VERTEX,vs,&report));
 CHECK(report.stage==REMESA_IR_VERTEX&&report.binary_error==ENOTSUP);
 CHECK(!remesa_context_shader_program_report(ctx,PIPE_SHADER_FRAGMENT,fs,&report));
 CHECK(report.stage==REMESA_IR_FRAGMENT&&!report.binary_error&&report.binary_bytes==8&&report.launch_error==ENOTSUP);
 CHECK(report.register_evidence.use_banks==16&&
       !report.register_evidence.use_va_packing_verified&&
       !report.register_evidence.rev121_verified&&
       !report.register_evidence.instruction_fields_known);
 ctx->bind_vs_state(ctx,vs);ctx->bind_fs_state(ctx,fs);CHECK(!remesa_context_error(ctx));
 CHECK(!remesa_context_shader_pipeline_report(ctx,&linked_copy));
 CHECK(!memcmp(&linked_copy,&linked,sizeof(linked)));
 CHECK(!remesa_context_error(ctx));
 CHECK(!remesa_context_prepare_shader_launch(ctx,&stage_requests[0],&stage_requests[1],&launch_copy));
 CHECK(!remesa_context_revalidate_shader_launch(ctx,&launch_copy,&stage_requests[0],&stage_requests[1]));
 CHECK(!remesa_context_error(ctx));
 struct remesa_pds_request changed_request=stage_requests[1];changed_request.generation++;
 CHECK(remesa_context_revalidate_shader_launch(ctx,&launch_copy,&stage_requests[0],&changed_request)==ESTALE);
 CHECK(remesa_context_prepare_shader_launch(ctx,NULL,&stage_requests[1],&launch_copy)==EINVAL);
 CHECK(remesa_context_revalidate_shader_launch(ctx,NULL,&stage_requests[0],&stage_requests[1])==EINVAL);
 state.ir.nir=fixture(0xff00ff00);void *green_fs=ctx->create_fs_state(ctx,&state);CHECK(green_fs);
 ctx->bind_fs_state(ctx,green_fs);
 CHECK(remesa_context_revalidate_shader_launch(ctx,&launch_copy,&stage_requests[0],&stage_requests[1])==ESTALE);
 CHECK(!remesa_context_shader_pipeline_report(ctx,&linked_copy)&&linked_copy.fragment_argb==0xff00ff00);
 ctx->delete_fs_state(ctx,green_fs);ctx->bind_fs_state(ctx,fs);
 ctx->bind_fs_state(ctx,NULL);linked_copy=linked_sentinel;
 CHECK(remesa_context_shader_pipeline_report(ctx,&linked_copy)==ENODATA);
 CHECK(!memcmp(&linked_copy,&linked_sentinel,sizeof(linked_copy)));
 launch_copy=launch_sentinel;
 CHECK(remesa_context_prepare_shader_launch(ctx,&stage_requests[0],&stage_requests[1],&launch_copy)==ENODATA);
 CHECK(!memcmp(&launch_copy,&launch_sentinel,sizeof(launch_copy)));
 ctx->bind_fs_state(ctx,fs);
 other->bind_vs_state(other,vs);
 CHECK(remesa_context_error(other)==EINVAL);
 CHECK(remesa_context_shader_pipeline_report(other,&linked_copy)==ENODATA);
 CHECK(!memcmp(&linked_copy,&linked_sentinel,sizeof(linked_copy)));
 struct fixture uniform=make(0);state.ir.nir=uniform.s;
 void *uniform_fs=ctx->create_fs_state(ctx,&state);CHECK(uniform_fs);
 ctx->bind_fs_state(ctx,uniform_fs);
 CHECK(remesa_context_shader_pipeline_report(ctx,&linked_copy)==ENOTSUP);
 CHECK(remesa_context_prepare_shader_launch(ctx,&stage_requests[0],&stage_requests[1],&launch_copy)==ENOTSUP);
 CHECK(!memcmp(&launch_copy,&launch_sentinel,sizeof(launch_copy)));
 CHECK(!memcmp(&linked_copy,&linked_sentinel,sizeof(linked_copy)));
 ctx->delete_fs_state(ctx,uniform_fs);ctx->bind_fs_state(ctx,fs);
 struct remesa_shader_pds_preview shader_preview,shader_sentinel;
 memset(&shader_preview,0xa5,sizeof(shader_preview));shader_sentinel=shader_preview;
 CHECK(!remesa_context_shader_pds_preview(ctx,PIPE_SHADER_VERTEX,vs,&candidate,&shader_preview));
 CHECK(shader_preview.stage==REMESA_IR_VERTEX&&shader_preview.candidate.word==preview_saved.word);
 CHECK(!shader_preview.candidate.fetch_verified&&!shader_preview.candidate.semantics_verified);
 CHECK(!remesa_context_error(ctx));
 uint64_t vertex_identity=shader_preview.semantic_identity;
 CHECK(!remesa_context_shader_pds_preview(ctx,PIPE_SHADER_FRAGMENT,fs,&candidate,&shader_preview));
 CHECK(shader_preview.stage==REMESA_IR_FRAGMENT&&shader_preview.semantic_identity!=vertex_identity);
 shader_preview=shader_sentinel;
 CHECK(remesa_context_shader_pds_preview(other,PIPE_SHADER_VERTEX,vs,&candidate,&shader_preview)==EINVAL);
 CHECK(!memcmp(&shader_preview,&shader_sentinel,sizeof(shader_preview)));
 CHECK(remesa_context_shader_pds_preview(ctx,PIPE_SHADER_VERTEX,fs,&candidate,&shader_preview)==EINVAL);
 CHECK(remesa_context_shader_pds_preview(ctx,PIPE_SHADER_COMPUTE,vs,&candidate,&shader_preview)==ENOTSUP);
 CHECK(remesa_context_shader_pds_preview(ctx,PIPE_SHADER_VERTEX,vs,NULL,&shader_preview)==EINVAL);
 candidate.policy=0;
 CHECK(remesa_context_shader_pds_preview(ctx,PIPE_SHADER_FRAGMENT,fs,&candidate,&shader_preview)==ENOTSUP);
 CHECK(!memcmp(&shader_preview,&shader_sentinel,sizeof(shader_preview)));
 candidate.policy=REMESA_PDS_CANDIDATE_ANALYSIS_ONLY;
 CHECK(remesa_context_shader_program_encode(ctx,PIPE_SHADER_VERTEX,vs,binary,sizeof(binary),&written)==ENOTSUP);
 CHECK(!remesa_context_shader_program_encode(ctx,PIPE_SHADER_FRAGMENT,fs,binary,sizeof(binary),&written)&&written==8);
 CHECK(!remesa_fragment_constant_decode(binary,written,&encoded)&&encoded.argb==0xffff00ff);
 CHECK(!memcmp(binary+8,before+8,sizeof(binary)-8));
 memcpy(binary,before,sizeof(binary));written=987;
 struct remesa_shader_program_report sentinel;memset(&sentinel,0xa5,sizeof(sentinel));report=sentinel;
 CHECK(remesa_context_shader_program_report(other,PIPE_SHADER_VERTEX,vs,&report)==EINVAL);
 CHECK(!memcmp(&report,&sentinel,sizeof(report)));
 CHECK(remesa_context_shader_program_report(ctx,PIPE_SHADER_VERTEX,fs,&report)==EINVAL);
 CHECK(remesa_context_shader_program_report(ctx,PIPE_SHADER_COMPUTE,vs,&report)==ENOTSUP);
 CHECK(remesa_context_shader_program_report(ctx,PIPE_SHADER_VERTEX,NULL,&report)==EINVAL);
 CHECK(remesa_context_shader_program_report(NULL,PIPE_SHADER_VERTEX,vs,&report)==EINVAL);
 CHECK(remesa_context_shader_program_encode(ctx,PIPE_SHADER_VERTEX,vs,NULL,1,&written)==EINVAL);
 ctx->delete_vs_state(ctx,vs);ctx->delete_fs_state(ctx,fs);
 CHECK(remesa_context_shader_pipeline_report(ctx,&linked_copy)==ENODATA);
 CHECK(!memcmp(&linked_copy,&linked_sentinel,sizeof(linked_copy)));
 CHECK(remesa_context_revalidate_shader_launch(ctx,&launch,&stage_requests[0],&stage_requests[1])==ENODATA);
 CHECK(remesa_context_shader_pds_preview(ctx,PIPE_SHADER_VERTEX,vs,&candidate,&shader_preview)==EINVAL);
 CHECK(!memcmp(&shader_preview,&shader_sentinel,sizeof(shader_preview)));
 CHECK(remesa_context_shader_program_report(ctx,PIPE_SHADER_VERTEX,vs,&report)==EINVAL);
 CHECK(remesa_context_shader_program_encode(ctx,PIPE_SHADER_FRAGMENT,fs,binary,sizeof(binary),&written)==EINVAL);
 CHECK(written==987&&!memcmp(binary,before,sizeof(binary)));
 state.ir.nir=fixture(0xffff00ff);freed=destroyed;fail_next_calloc=true;
 CHECK(!ctx->create_fs_state(ctx,&state)&&remesa_context_error(ctx)==ENOMEM&&destroyed==freed+1);
 state.ir.nir=fixture(0x00ff00ff);freed=destroyed;
 CHECK(!ctx->create_fs_state(ctx,&state)&&destroyed==freed+1);
 /* Leave valid unbound plans for ordinary context teardown to own. */
 state.ir.nir=vertex_fixture();CHECK(ctx->create_vs_state(ctx,&state));
 state.ir.nir=fixture(0xff00ff00);CHECK(ctx->create_fs_state(ctx,&state));
 screen->destroy(screen);linked_copy=linked_sentinel;launch_copy=launch_sentinel;
 CHECK(remesa_context_shader_pipeline_report(ctx,&linked_copy)==ESHUTDOWN);
 CHECK(!memcmp(&linked_copy,&linked_sentinel,sizeof(linked_copy)));
 CHECK(remesa_context_prepare_shader_launch(ctx,&stage_requests[0],&stage_requests[1],&launch_copy)==ESHUTDOWN);
 CHECK(!memcmp(&launch_copy,&launch_sentinel,sizeof(launch_copy)));
 CHECK(remesa_context_revalidate_shader_launch(ctx,&launch,&stage_requests[0],&stage_requests[1])==ESHUTDOWN);
 ctx->destroy(ctx);other->destroy(other);
 printf("PASS %u semantic program/actual shader callback cases; PDS candidate preview only, no GPU submissions\n",cases);
 return 0;
}
