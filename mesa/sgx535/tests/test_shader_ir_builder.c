/* SPDX-License-Identifier: MIT; actual licensed Mesa NIR builder API. */
#include "remesa_shader_program.h"
#include "remesa_fragment_body.h"
#include "nir_builder.h"
#include "util/ralloc.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static void varying_linkage(void)
{
 const nir_shader_compiler_options options={0};
 nir_builder vs=nir_builder_init_simple_shader(MESA_SHADER_VERTEX,&options,"common varying producer");
 vs.shader->info.io_lowered=true;
 nir_def *zero=nir_imm_int(&vs,0),*inputs[3];
 for(unsigned n=0;n<3;n++)inputs[n]=nir_load_input(&vs,4,32,zero,.base=n,
  .dest_type=nir_type_float32,.io_semantics={.location=VERT_ATTRIB_GENERIC0+n,.num_slots=1});
 nir_def *light=nir_fsat(&vs,nir_fdot3(&vs,inputs[1],inputs[2]));
 nir_store_output(&vs,inputs[0],zero,.base=0,.write_mask=15,.src_type=nir_type_float32,
  .io_semantics={.location=VARYING_SLOT_POS,.num_slots=1});
 nir_store_output(&vs,nir_vec4(&vs,light,light,light,light),zero,.base=1,
  .write_mask=15,.src_type=nir_type_float32,.io_semantics={.location=VARYING_SLOT_COL0,.num_slots=1});
 struct remesa_shader_ir ir;
 struct remesa_shader_program vertex;
 assert(!remesa_shader_ir_lower(vs.shader,&ir));
 assert(!remesa_shader_program_prepare(&ir,&vertex));
 ralloc_free(vs.shader);
 for(unsigned flat=0;flat<2;flat++){
  nir_builder fs=nir_builder_init_simple_shader(MESA_SHADER_FRAGMENT,&options,"common varying consumer");
  nir_variable *in=nir_variable_create(fs.shader,nir_var_shader_in,glsl_vec4_type(),"color");
  nir_variable *out=nir_variable_create(fs.shader,nir_var_shader_out,glsl_vec4_type(),"result");
  in->data.location=VARYING_SLOT_COL0;
  if(flat)in->data.interpolation=INTERP_MODE_FLAT;
  out->data.location=FRAG_RESULT_DATA0;
  nir_store_deref(&fs,nir_build_deref_var(&fs,out),nir_load_deref(&fs,nir_build_deref_var(&fs,in)),15);
  struct remesa_shader_program fragment;
  assert(!remesa_shader_ir_lower(fs.shader,&ir));
  assert(!remesa_shader_program_prepare(&ir,&fragment));
  ralloc_free(fs.shader);
  struct remesa_shader_linkage link;
  assert(!remesa_shader_linkage_prepare(&vertex,&fragment,&link));
  assert(!remesa_shader_linkage_validate(&link));
  assert(link.vertex_layout.input_count==3&&link.vertex_layout.output_dwords==8);
  assert(link.fragment_input==(flat?REMESA_FRAGMENT_COLOR0_FLAT:REMESA_FRAGMENT_COLOR0_SMOOTH));
  assert(!remesa_shader_linkage_revalidate(&link,&vertex,&fragment));
  struct remesa_shader_pipeline legacy;
  assert(remesa_shader_pipeline_prepare(&vertex,&fragment,&legacy)==ENOTSUP);
 }
}
int main(void)
{
 const nir_shader_compiler_options options={0};unsigned cases=0;glsl_type_singleton_init_or_ref();
 for(unsigned stage=0;stage<2;stage++)for(unsigned lowered=0;lowered<2;lowered++)for(unsigned mode=0;mode<14;mode++) {
  nir_builder b=nir_builder_init_simple_shader(stage?MESA_SHADER_FRAGMENT:MESA_SHADER_VERTEX,&options,"original semantic IR");
  nir_def *v=NULL,*zero=NULL;nir_variable *out=NULL;
  if(lowered){b.shader->info.io_lowered=true;zero=nir_imm_int(&b,0);}
  if(stage)v=nir_imm_vec4(&b,1,0,1,1);
  else if(lowered)v=nir_load_input(&b,4,32,zero,.dest_type=nir_type_float32,.io_semantics={.location=VERT_ATTRIB_GENERIC0,.num_slots=1});
  else {nir_variable *in=nir_variable_create(b.shader,nir_var_shader_in,glsl_vec4_type(),"position");in->data.location=VERT_ATTRIB_GENERIC0;v=nir_load_deref(&b,nir_build_deref_var(&b,in));}
  if(mode==1)v=nir_mov(&b,v);
  if(mode==2)v=nir_fadd(&b,v,v);
  if(mode==3)(void)nir_mov(&b,v);
  if(mode==4)v=nir_fneg(&b,v);
  if(mode==5)b.shader->info.stage=MESA_SHADER_COMPUTE;
  if(mode==6)b.shader->info.num_textures=1;
  if(mode==7)b.shader->info.writes_memory=true;
  if(mode==8)b.shader->num_uniforms=1;
  if(mode==9)b.shader->global_mem_size=4;
  if(mode>=10) {
   nir_def *part=mode&1?nir_vec3(&b,nir_channel(&b,v,0),nir_channel(&b,v,1),nir_channel(&b,v,2)):
                         nir_vec2(&b,nir_channel(&b,v,0),nir_channel(&b,v,1));
   nir_def *x=nir_channel(&b,part,mode>=12?1:0),*y=nir_channel(&b,part,1);
   nir_def *z=nir_channel(&b,mode&1?part:v,2),*w=nir_channel(&b,v,3);
   v=nir_vec4(&b,x,y,z,w);
  }
  unsigned location=stage?FRAG_RESULT_COLOR:VARYING_SLOT_POS;
  if(lowered)nir_store_output(&b,v,zero,.write_mask=15,.src_type=nir_type_float32,.io_semantics={.location=location,.num_slots=1});
  else {out=nir_variable_create(b.shader,nir_var_shader_out,glsl_vec4_type(),"output");out->data.location=location;nir_store_deref(&b,nir_build_deref_var(&b,out),v,15);}
  struct remesa_shader_ir p,original;memset(&p,0xa5,sizeof(p));original=p;
  int error=remesa_shader_ir_lower(b.shader,&p);
  if(mode<2||(mode>=10&&mode<12)){assert(!error&&!remesa_shader_ir_validate(&p));assert(p.stage==(stage?REMESA_IR_FRAGMENT:REMESA_IR_VERTEX));
   unsigned char bytes[8]={0},encoded[8];size_t written=999;
   int encoded_error=remesa_shader_ir_encode_sgx535(&p,bytes,sizeof(bytes),&written);
   if(stage) {
    struct remesa_fragment_constant constant;
    assert(!encoded_error&&written==8);
    assert(!remesa_fragment_constant_decode(bytes,written,&constant)&&constant.argb==UINT32_C(0xffff00ff));
    memcpy(encoded,bytes,sizeof(encoded));
   } else {
    const unsigned char untouched[8]={0};
    assert(encoded_error==ENOTSUP&&written==999&&!memcmp(bytes,untouched,sizeof(bytes)));
   }
   struct remesa_shader_program program={0};struct remesa_shader_pds_preview preview={0};
   const struct remesa_pds_candidate_request request={.policy=REMESA_PDS_CANDIDATE_ANALYSIS_ONLY,
    .source={1,24},.selector={0,1,2,3}};
   assert(!remesa_shader_program_prepare(&p,&program));
   assert(!remesa_shader_program_preview_pds(&program,&request,&preview));
   assert(preview.stage==p.stage&&preview.candidate.pair[2].index==48&&preview.candidate.pair[3].index==49);
   assert(!preview.candidate.output_arity_known&&!preview.candidate.fetch_verified&&!preview.candidate.semantics_verified);
   memset(bytes,0,sizeof(bytes));written=999;
   encoded_error=remesa_shader_program_encode(&program,bytes,sizeof(bytes),&written);
   if(stage)assert(!encoded_error&&written==8&&!memcmp(bytes,encoded,sizeof(bytes)));
   else {
    const unsigned char untouched[8]={0};
    assert(encoded_error==ENOTSUP&&written==999&&!memcmp(bytes,untouched,sizeof(bytes)));
   }
   remesa_shader_program_finish(&program);
  }else assert(error&&!memcmp(&p,&original,sizeof(p)));
  cases++;ralloc_free(b.shader);
 }
 varying_linkage();
 glsl_type_singleton_decref();printf("PASS %u actual Mesa NIR lowering cases plus smooth/flat producer-consumer linking after NIR destruction; no GPU\n",cases);return 0;
}
