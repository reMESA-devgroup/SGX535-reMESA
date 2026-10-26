/* SPDX-License-Identifier: MIT. Authored complete ff-vs; CPU recognition only. */
#include "remesa_nir.h"
#include "remesa_usse_arithmetic.h"
#include "remesa_shader_ir.h"
#include "remesa_shader_program.h"
#include "nir_builder.h"
#include "mesa/program/prog_statevars.h"
#include "util/ralloc.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
uint32_t nir_debug=NIR_DEBUG_EXTENDED_VALIDATION;
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
int main(void){
 unsigned char oracles[648];FILE*carrier=fopen("body-oracles.bin","rb");assert(carrier&&fread(oracles,1,sizeof(oracles),carrier)==sizeof(oracles)&&fgetc(carrier)==EOF&&!fclose(carrier));
 unsigned char tail[8];carrier=fopen("tail-oracle.bin","rb");assert(carrier&&fread(tail,1,8,carrier)==8&&fgetc(carrier)==EOF&&!fclose(carrier));
 glsl_type_singleton_init_or_ref();unsigned checks=0,mutations=0;
 for(unsigned attr=0;attr<2;attr++)for(unsigned mode=0;mode<29;mode++){
  nir_shader*s=make(attr,mode);struct remesa_nir_gears_vertex_profile result,before;memset(&result,0xa5,sizeof(result));before=result;
  int error=remesa_nir_gears_vertex(s,&result);
  if(mode){assert(error==ENOTSUP&&!memcmp(&result,&before,sizeof(result)));}
  else {
   assert(!error&&result.normal_from_attribute==attr&&result.uniform_vectors==(attr?9u:10u)&&result.source_dwords==(attr?37u:41u)&&result.input_count==(attr?2u:1u)&&result.output_dwords==8&&result.temporary_dwords==11&&result.point_size_bits==0x3f800000);
   unsigned char encoded[312];size_t written=0;
   const struct remesa_usse_cpu_gears_vertex_body request={result.normal_from_attribute,result.uniform_vectors,result.source_dwords,REMESA_USSE_CPU_GEARS_TRIANGLES,0};
   assert(!remesa_usse_cpu_gears_vertex_body_encode(&request,encoded,sizeof(encoded),&written)&&written==312&&!memcmp(encoded,oracles+324*attr+12,312));
   assert(oracles[324*attr]==attr&&oracles[324*attr+4]==result.uniform_vectors&&oracles[324*attr+8]==0x38&&oracles[324*attr+9]==1);
   struct remesa_shader_ir ir;assert(!remesa_shader_ir_lower(s,&ir)&&!remesa_shader_ir_gears_vertex(&ir)&&!remesa_shader_ir_validate(&ir));
   struct remesa_shader_vertex_layout layout;assert(!remesa_shader_ir_vertex_layout(&ir,&layout)&&layout.input_count==(attr?2u:1u)&&layout.stride_dwords==(attr?8u:4u)&&layout.color_components==4&&layout.output_dwords==8);
   struct remesa_shader_program program;assert(!remesa_shader_program_prepare(&ir,&program)&&program.status==REMESA_PROGRAM_COMPILE_OFFLINE&&!remesa_shader_program_validate(&program));
   struct remesa_shader_program_report report;assert(!remesa_shader_program_report(&program,&report)&&!report.binary_error&&report.binary_bytes==320&&report.launch_error==ENOTSUP);
   unsigned char full[336],saved[336];memset(full,0xa5,sizeof(full));memcpy(saved,full,sizeof(full));written=777;
   assert(remesa_shader_program_encode(&program,full,319,&written)==ENOSPC&&written==777&&!memcmp(full,saved,sizeof(full)));
   assert(!remesa_shader_program_encode(&program,full,sizeof(full),&written)&&written==320&&!memcmp(full,oracles+324*attr+12,312)&&!memcmp(full+312,tail,8)&&!memcmp(full+320,saved+320,16));
   struct remesa_shader_selected_resources resources,old_resources;memset(&resources,0xa5,sizeof(resources));old_resources=resources;
   assert(remesa_shader_program_selected_resources(&program,&resources)==ENOTSUP&&!memcmp(&resources,&old_resources,sizeof(resources)));
   struct remesa_pds_request launch={0};struct remesa_pds_plan plan,old_plan;memset(&plan,0xa5,sizeof(plan));old_plan=plan;
   assert(remesa_shader_program_prepare_launch(&program,&launch,&plan)==ENOTSUP&&!memcmp(&plan,&old_plan,sizeof(plan)));
   for(unsigned word=0;word<sizeof(ir)/4;word++) {
    struct remesa_shader_ir bad=ir;uint32_t value;memcpy(&value,(unsigned char*)&bad+4*word,4);value^=1;memcpy((unsigned char*)&bad+4*word,&value,4);
    assert(remesa_shader_ir_validate(&bad)!=0);mutations++;
   }
   char dump[4096];size_t needed;assert(!remesa_shader_ir_dump(&ir,dump,sizeof(dump),&needed)&&strstr(dump,"gears-vertex"));
   FILE*semantic=fopen(attr?"semantic-attr.bin":"semantic-state.bin","wb");assert(semantic&&fwrite(&ir,1,sizeof(ir),semantic)==sizeof(ir)&&!fclose(semantic));
   remesa_shader_program_finish(&program);
   FILE*f=fopen(attr?"authored-attr.nir.txt":"authored-state.nir.txt","w");assert(f);nir_print_shader(s,f);assert(!fclose(f));
  }
  ralloc_free(s);checks++;
 }
 struct remesa_nir_gears_vertex_profile out;assert(remesa_nir_gears_vertex(NULL,&out)==EINVAL);nir_shader*s=make(true,0);assert(remesa_nir_gears_vertex(s,NULL)==EINVAL);ralloc_free(s);
 glsl_type_singleton_decref();printf("IR mutation checks: %u\n",mutations);printf("PASS full ff-vs POSITION/COLOR0/PSIZ recognition, uniform state slots/swizzles/normal variants, %u cases, both complete NIR->typed312byte bodies match independent original CPU writers; full canonical IR/program320 offline; launch/resource guards unchanged; CPU ONLY\n",checks);return 0;
}
