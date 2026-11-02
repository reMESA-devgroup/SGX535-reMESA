/* SPDX-License-Identifier: MIT. Shared authored LAB NIR fixture, not a generic
 * driver shader compiler. This is the unchanged positive/mutation builder used
 * by the existing tests and native qualification client. */
#ifndef REMESA_AUTHORED_GEARS_VERTEX_NIR_FIXTURE_H
#define REMESA_AUTHORED_GEARS_VERTEX_NIR_FIXTURE_H
#include "nir_builder.h"
#include "mesa/program/prog_statevars.h"
#include "util/ralloc.h"
#include <string.h>
static nir_def *alu(nir_builder*b,nir_op op,unsigned n,nir_def*a,const char*sa,nir_def*d,const char*sd){
 nir_alu_instr*i=nir_alu_instr_create(b->shader,op);nir_def_init(&i->instr,&i->def,n,32);nir_def*v[]={a,d};const char*sw[]={sa,sd};
 for(unsigned j=0;j<nir_op_infos[op].num_inputs;j++){i->src[j].src=nir_src_for_ssa(v[j]);for(unsigned k=0;k<strlen(sw[j]);k++)i->src[j].swizzle[k]=sw[j][k]-'0';}
 nir_builder_instr_insert(b,&i->instr);return &i->def;
}
static nir_variable *var(nir_builder*b,nir_variable_mode mode,unsigned n,const char*name,unsigned location,unsigned driver){
 const struct glsl_type*t=n==1?glsl_float_type():glsl_vector_type(GLSL_TYPE_FLOAT,n);nir_variable*v=nir_variable_create(b->shader,mode,t,name);v->data.location=location;v->data.driver_location=driver;v->data.interpolation=INTERP_MODE_NONE;
 if(mode==nir_var_shader_in||mode==nir_var_shader_out)v->data.precision=GLSL_PRECISION_HIGH;
 return v;
}
static nir_shader *make(bool attr,unsigned mode){
 static const nir_shader_compiler_options options={0};nir_builder b=nir_builder_init_simple_shader(MESA_SHADER_VERTEX,&options,"full gears captured semantics");unsigned uniforms=attr?9:10;
 nir_variable*u[10];
 for(unsigned k=0;k<uniforms;k++){
  u[k]=var(&b,nir_var_uniform,k>=8?3:4,"state",0,k);u[k]->num_state_slots=1;u[k]->state_slots=rzalloc_array(u[k],nir_state_slot,1);gl_state_index16*t=u[k]->state_slots[0].tokens;
  if(k==0)t[0]=STATE_LIGHTMODEL_SCENECOLOR;
  else if(k<4){t[0]=STATE_LIGHTPROD;t[2]=(k-1)*2;}
  else if(k<8){t[0]=STATE_MVP_MATRIX_TRANSPOSE;t[2]=t[3]=k-4;}
  else if(!attr&&k==8){t[0]=STATE_CURRENT_ATTRIB;t[1]=1;}
  else t[0]=STATE_LIGHT_POSITION_NORMALIZED;
 }
 nir_variable*pi=var(&b,nir_var_shader_in,4,"VERT_ATTRIB_POS",VERT_ATTRIB_POS,0),*ni=attr?var(&b,nir_var_shader_in,4,"VERT_ATTRIB_NORMAL",VERT_ATTRIB_NORMAL,1):NULL;
 nir_variable*po=var(&b,nir_var_shader_out,4,"VARYING_SLOT_POS",VARYING_SLOT_POS,0),*co=var(&b,nir_var_shader_out,4,"VARYING_SLOT_COL0",VARYING_SLOT_COL0,1),*ps=var(&b,nir_var_shader_out,1,"VARYING_SLOT_PSIZ",VARYING_SLOT_PSIZ,2);ps->data.interpolation=INTERP_MODE_SMOOTH;ps->data.precision=GLSL_PRECISION_NONE;
 nir_deref_instr*psd=nir_build_deref_var(&b,ps);nir_def*one=nir_imm_float(&b,mode==1?2:1);if(mode!=22)nir_store_deref(&b,psd,one,1);
 nir_def*z=nir_imm_int(&b,0),*rows[4];
 for(unsigned k=0;k<4;k++)rows[k]=nir_load_uniform(&b,4,32,z,.base=4+k+(mode==3&&k==0?1:0),.range=1,.dest_type=nir_type_float32);
 nir_def*p=nir_load_deref(&b,nir_build_deref_var(&b,pi));
 nir_def*x=alu(&b,nir_op_fmul,4,rows[0],"0123",p,"0000"),*y=alu(&b,nir_op_fmul,4,rows[1],"0123",p,"1111"),*acc=alu(&b,nir_op_fadd,4,y,"0123",x,"0123");
 x=alu(&b,nir_op_fmul,4,rows[2],"0123",p,mode==11?"0000":"2222");acc=alu(&b,nir_op_fadd,4,x,"0123",acc,"0123");
 x=alu(&b,nir_op_fmul,4,rows[3],"0123",p,"3333");acc=alu(&b,nir_op_fadd,4,x,"0123",acc,"0123");nir_store_deref(&b,nir_build_deref_var(&b,po),acc,mode==21?7:15);
 nir_def*n=attr?nir_load_deref(&b,nir_build_deref_var(&b,ni)):nir_load_uniform(&b,3,32,z,.base=8,.range=1,.dest_type=nir_type_float32);
 nir_def*len=alu(&b,nir_op_fdot3,1,n,mode==2?"212":"012",n,"012"),*rsq=alu(&b,mode==4?nir_op_frcp:nir_op_frsq,1,len,"0",NULL,NULL),*nv=alu(&b,nir_op_fmul,3,n,"012",rsq,"000");
 nir_def*scene=nir_load_uniform(&b,4,32,z,.base=0,.range=1,.dest_type=nir_type_float32),*alpha=alu(&b,nir_op_mov,4,scene,"3333",NULL,NULL);nir_deref_instr*cod=nir_build_deref_var(&b,co);nir_store_deref(&b,cod,alpha,mode==7?4:8);
 nir_def*ambient=nir_load_uniform(&b,4,32,z,.base=1,.range=1,.dest_type=nir_type_float32),*diffuse=nir_load_uniform(&b,4,32,z,.base=2,.range=1,.dest_type=nir_type_float32),*spec=nir_load_uniform(&b,4,32,z,.base=3,.range=1,.dest_type=nir_type_float32),*light=nir_load_uniform(&b,3,32,z,.base=attr?8:9,.range=1,.dest_type=nir_type_float32);
 nir_def*dp=alu(&b,nir_op_fdot3,1,nv,"012",light,"012");nir_const_value cv[4]={{.u32=0},{.u32=0},{.u32=0},{.u32=0x3f800000}};nir_def*bound=nir_build_imm(&b,4,32,cv);
 nir_def*clamp=alu(&b,mode==5?nir_op_fmin:nir_op_fmax,4,bound,"0123",dp,"0000"),*positive=alu(&b,mode==6?nir_op_sge:nir_op_slt,1,z,"0",dp,"0");
 nir_def*sum=alu(&b,nir_op_fadd,4,mode==8?scene:ambient,"0123",mode==8?ambient:scene,"0123"),*product=alu(&b,nir_op_fmul,4,clamp,"1111",mode==9?ambient:diffuse,"0123");
 sum=alu(&b,nir_op_fadd,4,product,"0123",sum,"0123");product=alu(&b,nir_op_fmul,4,mode==10?clamp:positive,"0000",spec,"0123");sum=alu(&b,nir_op_fadd,4,mode==27?sum:product,"0123",mode==27?product:sum,"0123");nir_store_deref(&b,cod,sum,7);
 if(mode==15)(void)nir_mov(&b,sum);
 b.shader->num_uniforms=uniforms;b.shader->num_inputs=attr?2:1;b.shader->num_outputs=3;b.shader->info.inputs_read=BITFIELD64_BIT(VERT_ATTRIB_POS)|(attr?BITFIELD64_BIT(VERT_ATTRIB_NORMAL):0);b.shader->info.outputs_written=BITFIELD64_BIT(VARYING_SLOT_POS)|BITFIELD64_BIT(VARYING_SLOT_COL0)|BITFIELD64_BIT(VARYING_SLOT_PSIZ);
 if(mode==12)u[uniforms-1]->state_slots[0].tokens[1]=1;
 if(mode==13){if(attr)ni->data.location++;else u[8]->state_slots[0].tokens[1]=0;}
 if(mode==14)co->data.interpolation=INTERP_MODE_FLAT;
 if(mode==16)b.shader->info.io_lowered=true;
 if(mode==17)b.shader->info.num_ubos=1;
 if(mode==18)b.shader->info.stage=MESA_SHADER_FRAGMENT;
 if(mode==19)nir_instr_as_alu(sum->parent_instr)->exact=true;
 if(mode==20)b.shader->num_uniforms--;
 if(mode==23)b.shader->info.float_controls_execution_mode=1;
 if(mode==24)co->data.is_xfb=true;
 if(mode==25)u[uniforms-1]->type=glsl_vec4_type();
 if(mode==26){if(attr)ni->data.driver_location=0;else u[8]->data.driver_location=9;}
 if(mode==28)b.shader->global_mem_size=4;
 if(mode==0)nir_validate_shader(b.shader,"complete authored gears profile");
 return b.shader;
}
#endif
