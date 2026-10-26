/* SPDX-License-Identifier: MIT: actual Mesa NIR builder, offline only. */
#include "remesa_nir.h"
#include "remesa_shader_ir.h"
#include "remesa_usse.h"
#include "nir_builder.h"
#include "util/ralloc.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#ifdef NDEBUG
#error Actual Mesa admission tests require assertions enabled
#endif
/* The release libnir omits this debug global. The test links upstream
 * nir_validate.c with assertions, using its real mask and extended checks.
 * NOVALIDATE is never set; no validator implementation is substituted. */
uint32_t nir_debug=NIR_DEBUG_EXTENDED_VALIDATION;
int main(void)
{
 static const nir_shader_compiler_options options={0};unsigned cases=0;
 glsl_type_singleton_init_or_ref();
 for(unsigned lowered=0;lowered<2;lowered++)for(unsigned color=0;color<3;color++) {
  nir_builder b=nir_builder_init_simple_shader(MESA_SHADER_FRAGMENT,&options,"reMESA constant offline");
  float r=color==1?0:1,g=color==0?0:1,bl=color==0?1:0;uint32_t out=0;
  nir_def *value=nir_imm_vec4(&b,r,g,bl,1);
  /* A component-preserving NIR mov is semantically the same constant
   * profile and must not force a new hardware program. */
  if(lowered && color==2)value=nir_mov(&b,value);
  if(lowered){b.shader->info.io_lowered=true;nir_store_output(&b,value,nir_imm_int(&b,0),.write_mask=15,.src_type=nir_type_float32,.io_semantics={.location=FRAG_RESULT_DATA0,.num_slots=1});}
  else {nir_variable *v=nir_variable_create(b.shader,nir_var_shader_out,glsl_vec4_type(),"color");v->data.location=FRAG_RESULT_DATA0;nir_store_deref(&b,nir_build_deref_var(&b,v),value,15);}
  assert(!remesa_nir_constant_fragment(b.shader,&out));assert(out==(color==0?0xffff00ff:color==1?0xff00ff00:0xffffff00));cases++;
  b.shader->info.fs.uses_discard=true;assert(remesa_nir_constant_fragment(b.shader,&out)==ENOTSUP);cases++;ralloc_free(b.shader);
 }
 /* Public builders and NIR validation exercise the newly admitted constant
  * operators with canonical Boolean representation, independently of the
  * manually constructed callback fixtures. These operations run only here
  * in the compiler; they do not request native SGX ALU instructions. */
 for(unsigned mode=0;mode<6;mode++) {
  nir_builder b=nir_builder_init_simple_shader(MESA_SHADER_FRAGMENT,&options,"reMESA constant bit expressions");
  uint32_t green[4]={0,UINT32_C(0x3f800000),0,UINT32_C(0x3f800000)};
  uint32_t first[4],second[4];
  for(unsigned c=0;c<4;c++) {
   first[c]=mode==0?green[c]|UINT32_C(0x00010000):
    mode==1?green[c]&~UINT32_C(0x00800000):mode==3?~green[c]:green[c];
   second[c]=mode==0?~UINT32_C(0x00010000):
    mode==1?green[c]&UINT32_C(0x00800000):mode==2?
     (c==3?0:UINT32_C(0x3f800000)):(c==1?0:UINT32_C(0x3f800000));
  }
  nir_def *a=nir_imm_ivec4(&b,(int32_t)first[0],(int32_t)first[1],(int32_t)first[2],(int32_t)first[3]);
  nir_def *other=mode==3?NULL:nir_imm_ivec4(&b,(int32_t)second[0],(int32_t)second[1],(int32_t)second[2],(int32_t)second[3]);
  nir_def *value=mode==0?nir_iand(&b,a,other):mode==1?nir_ior(&b,a,other):
   mode==2?nir_ixor(&b,a,other):mode==3?nir_inot(&b,a):
    nir_bcsel(&b,nir_imm_bool(&b,mode==4),a,other);
  b.shader->info.io_lowered=true;
  nir_store_output(&b,value,nir_imm_int(&b,0),.write_mask=15,.src_type=nir_type_float32,
   .io_semantics={.location=FRAG_RESULT_DATA0,.num_slots=1});
  nir_validate_shader(b.shader,"reMESA constant bit expression");
  uint32_t color=0;
  assert(!remesa_nir_constant_fragment(b.shader,&color));
  assert(color==(mode==2||mode==5?UINT32_C(0xffff00ff):UINT32_C(0xff00ff00)));
  cases++;ralloc_free(b.shader);
 }
 /* Floating arithmetic must stay rejected: no SGX ALU encoding is admitted by this
  * semantic-only profile translator. */
 nir_builder reject=nir_builder_init_simple_shader(MESA_SHADER_FRAGMENT,&options,"reMESA arithmetic reject");
 nir_def *lhs=nir_imm_vec4(&reject,1,0,1,1);
 nir_def *rhs=nir_imm_vec4(&reject,0,0,0,0);
 nir_variable *rv=nir_variable_create(reject.shader,nir_var_shader_out,glsl_vec4_type(),"color");
 rv->data.location=FRAG_RESULT_DATA0;
 nir_store_deref(&reject,nir_build_deref_var(&reject,rv),nir_fadd(&reject,lhs,rhs),15);
 uint32_t rejected=0;assert(remesa_nir_constant_fragment(reject.shader,&rejected)==ENOTSUP);cases++;
 ralloc_free(reject.shader);
 /* Unused/multiple copies are outside the deliberately finite profile. */
 nir_builder extra=nir_builder_init_simple_shader(MESA_SHADER_FRAGMENT,&options,"unused mov reject");
 nir_def *cv=nir_imm_vec4(&extra,1,0,1,1);
 (void)nir_mov(&extra,cv);
 nir_variable *ev=nir_variable_create(extra.shader,nir_var_shader_out,glsl_vec4_type(),"color");ev->data.location=FRAG_RESULT_DATA0;
 nir_store_deref(&extra,nir_build_deref_var(&extra,ev),cv,15);
 assert(remesa_nir_constant_fragment(extra.shader,&rejected)==ENOTSUP);cases++;
 ralloc_free(extra.shader);
 /* Metadata must not admit unused resource contracts or decorated outputs
  * just because their constant color happens to match the known profile. */
 for(unsigned mode=0;mode<10;mode++) {
  nir_builder b=nir_builder_init_simple_shader(MESA_SHADER_FRAGMENT,&options,"metadata rejection");
  nir_def *value=nir_imm_vec4(&b,1,0,1,1);
  nir_variable *v=nir_variable_create(b.shader,nir_var_shader_out,glsl_vec4_type(),"color");v->data.location=FRAG_RESULT_DATA0;
  nir_store_deref(&b,nir_build_deref_var(&b,v),value,15);
  if(mode==0)b.shader->info.num_textures=1;
  if(mode==1)b.shader->info.num_ubos=1;
  if(mode==2)b.shader->info.num_ssbos=1;
  if(mode==3)b.shader->info.num_images=1;
  if(mode==4)b.shader->info.outputs_read=BITFIELD64_BIT(FRAG_RESULT_DATA0);
  if(mode==5)b.shader->info.system_values_read[0]=1;
  if(mode==6)b.shader->info.xfb_stride[0]=4;
  if(mode==7)v->data.bindless=true;
  if(mode==8)v->data.explicit_binding=true;
  if(mode==9)v->data.is_xfb=true;
  uint32_t unchanged=0x12345678;
  assert(remesa_nir_constant_fragment(b.shader,&unchanged)==ENOTSUP);
  assert(unchanged==0x12345678);cases++;ralloc_free(b.shader);
 }
 /* The actual vertex builder feeds the same canonical semantic export plan
  * as the hand-written fixtures, including an identity alias. Numeric classes
  * below are explicit CPU assignments, never physical attribute/output banks. */
 for(unsigned alias=0;alias<2;alias++) {
  nir_builder b=nir_builder_init_simple_shader(MESA_SHADER_VERTEX,&options,"reMESA POSITION contract");
  b.shader->info.io_lowered=true;nir_def *zero=nir_imm_int(&b,0);
  nir_def *position=nir_load_input(&b,4,32,zero,.dest_type=nir_type_float32,
   .io_semantics={.location=VERT_ATTRIB_GENERIC0,.num_slots=1});
  if(alias)position=nir_mov(&b,position);
  nir_store_output(&b,position,zero,.write_mask=15,.src_type=nir_type_float32,
   .io_semantics={.location=VARYING_SLOT_POS,.num_slots=1});
  nir_validate_shader(b.shader,"reMESA semantic POSITION contract");
  struct remesa_shader_ir ir;struct remesa_usse_program program;
  assert(!remesa_shader_ir_lower(b.shader,&ir));assert(!remesa_usse_lower(&ir,&program));
  struct remesa_usse_vertex_mapping mapping={0};
  for(unsigned c=0;c<4;c++) {
   mapping.input[c]=(struct remesa_usse_cpu_register){0,8+c};
   mapping.output[c]=(struct remesa_usse_cpu_register){2,24+c};
  }
  struct remesa_usse_vertex_export_plan plan;
  assert(!remesa_usse_vertex_export_plan(&program,&mapping,&plan));
  assert(!remesa_usse_vertex_export_validate(&program,&mapping,&plan));
  assert(plan.route_count==4&&plan.candidate_bytes==32&&plan.used_values==15);
  assert(plan.input_bank_error==ENOTSUP&&plan.export_bank_error==ENOTSUP&&plan.termination_error==ENOTSUP);
  unsigned char bytes[32];size_t written=0;
  assert(!remesa_usse_vertex_candidate(&program,&mapping,bytes,sizeof(bytes),&written)&&written==32);
  cases++;ralloc_free(b.shader);
 }
 glsl_type_singleton_decref();printf("PASS %u actual Mesa NIR builder cases (NO GPU)\n",cases);return 0;
}
