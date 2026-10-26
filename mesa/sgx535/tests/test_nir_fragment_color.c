/* SPDX-License-Identifier: MIT */
#include "remesa_nir.h"
#include "remesa_usse_fragment_color.h"
#include "nir_builder.h"
#include "util/ralloc.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef NDEBUG
#error Assertions required
#endif
uint32_t nir_debug=NIR_DEBUG_EXTENDED_VALIDATION;
int main(void)
{
 static const nir_shader_compiler_options options={0};unsigned checks=0;
 glsl_type_singleton_init_or_ref();
 for(unsigned color=0;color<2;color++)for(unsigned mode=0;mode<13;mode++){
  nir_builder b=nir_builder_init_simple_shader(MESA_SHADER_FRAGMENT,&options,"restricted color passthrough");
  nir_variable *in=nir_variable_create(b.shader,nir_var_shader_in,glsl_vec4_type(),"color_in");
  nir_variable *out=nir_variable_create(b.shader,nir_var_shader_out,glsl_vec4_type(),"color_out");
  in->data.location=color?VARYING_SLOT_COL1:VARYING_SLOT_COL0;out->data.location=FRAG_RESULT_DATA0;
  nir_def *value=nir_load_deref(&b,nir_build_deref_var(&b,in));
  if(mode==1)value=nir_mov(&b,value);
  if(mode==2)value=nir_fadd(&b,value,nir_imm_vec4(&b,0,0,0,0));
  if(mode==3)value=nir_swizzle(&b,value,(unsigned[]){2,1,0,3},4);
  nir_store_deref(&b,nir_build_deref_var(&b,out),value,mode==4?7:15);
  /* Validate canonical fixtures before intentionally decorating metadata. */
  nir_validate_shader(b.shader,"color passthrough fixture");
  if(mode==5)in->data.interpolation=INTERP_MODE_FLAT;
  if(mode==6)in->data.location=VARYING_SLOT_TEX0;
  if(mode==7)b.shader->info.fs.uses_discard=true;
  if(mode==8)b.shader->info.num_textures=1;
  if(mode==9)in->data.sample=1;
  if(mode==10)b.shader->info.io_lowered=true;
  if(mode==11)out->data.location=FRAG_RESULT_DEPTH;
  if(mode==12)in->data.precision=GLSL_PRECISION_MEDIUM;
  unsigned slot=999;unsigned char bytes[16],saved[16];memset(bytes,0xa5,16);memcpy(saved,bytes,16);
  struct remesa_usse_fragment_color_extent extent,saved_extent;memset(&extent,0xa5,sizeof(extent));saved_extent=extent;
  int e=remesa_nir_fragment_color_input(b.shader,&slot);
  int encoded=remesa_usse_fragment_color_encode_nir(b.shader,bytes,16,&extent);
  if(!mode){
   assert(!e&&!encoded&&slot==color+1);checks++;
   unsigned char oracle[8];struct remesa_usse_fragment_color_extent direct;
   const struct remesa_usse_fragment_color_request r={color+1,0};
   assert(!remesa_usse_fragment_color_encode(&r,oracle,8,&direct));checks++;
   assert(!memcmp(bytes,oracle,8)&&!memcmp(&direct,&extent,sizeof(extent)));checks++;
   assert(extent.r_id==color+10&&extent.logical_bytes==8);checks++;
   for(unsigned j=8;j<16;j++){assert(bytes[j]==0xa5);checks++;}
   unsigned unused=0;assert(remesa_nir_constant_fragment(b.shader,&unused)==ENOTSUP);checks++;
  }else{
   assert(e==ENOTSUP&&encoded==ENOTSUP&&slot==999);checks++;
   assert(!memcmp(bytes,saved,16)&&!memcmp(&extent,&saved_extent,sizeof(extent)));checks++;
  }
  ralloc_free(b.shader);
 }
 glsl_type_singleton_decref();
 printf("PASS real NIR color passthrough 26fixtures, constant-color matcher unchanged (%u checks)\n",checks);
 return 0;
}
