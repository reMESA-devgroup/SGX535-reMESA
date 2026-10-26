/* SPDX-License-Identifier: MIT
 * Real NIR builders: exact0/1 mediump export, no intermediate arithmetic.
 */
#include "remesa_nir.h"
#include "nir_builder.h"
#include "util/ralloc.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
int main(void)
{
 static const nir_shader_compiler_options options={0};unsigned cases=0;
 glsl_type_singleton_init_or_ref();
 for(unsigned mode=0;mode<7;mode++) {
  nir_builder b=nir_builder_init_simple_shader(MESA_SHADER_FRAGMENT,&options,"mediump exact export");
  nir_variable *v=nir_variable_create(b.shader,nir_var_shader_out,glsl_vec4_type(),"gl_FragColor");
  v->data.location=FRAG_RESULT_COLOR;
  v->data.precision=mode==3?GLSL_PRECISION_LOW:GLSL_PRECISION_MEDIUM;
  nir_def *value=nir_imm_vec4(&b,mode==1||mode==2?1:0,mode==1?0:1,mode==1?1:0,1);
  if(mode==4)value=nir_mov(&b,value);
  if(mode==5)value=nir_imm_vec4(&b,0,.5f,0,1);
  if(mode==6)value=nir_fmul(&b,value,nir_imm_vec4(&b,1,1,1,1));
  nir_store_deref(&b,nir_build_deref_var(&b,v),value,15);
  uint32_t color=UINT32_C(0xdeadbeef);
  int error=remesa_nir_constant_fragment(b.shader,&color);
  if(mode<3){assert(!error);assert(color==(mode==0?0xff00ff00:mode==1?0xffff00ff:0xffffff00));}
  else {assert(error==ENOTSUP);assert(color==UINT32_C(0xdeadbeef));}
  cases++;ralloc_free(b.shader);
 }
 glsl_type_singleton_decref();
 printf("PASS actual NIR mediump exact constant cases=%u\n",cases);
 return 0;
}
