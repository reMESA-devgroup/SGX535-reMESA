/* SPDX-License-Identifier: MIT. Manual pinned Mesa NIR fixture for ownership
 * tests on all hosts. Actual client/builder NIR is qualified separately.
 * This is not a shader execution model or GPU evidence. */
#ifndef REMESA_GEARS_NIR_FIXTURE_H
#define REMESA_GEARS_NIR_FIXTURE_H
#include "nir.h"
#include "mesa/program/prog_statevars.h"
#include "util/ralloc.h"
struct gf_builder {nir_shader*s;nir_block*b;struct glsl_type types[3];};
static void gf_append(struct gf_builder*f,nir_instr*i){i->block=f->b;exec_list_push_tail(&f->b->instr_list,&i->node);}
static void gf_def(nir_def*d,nir_instr*i,unsigned n){d->parent_instr=i;d->num_components=n;d->bit_size=32;}
static nir_variable *gf_var(struct gf_builder*f,nir_variable_mode mode,unsigned n,unsigned loc,unsigned driver){
 nir_variable*v=rzalloc(f->s,nir_variable);v->type=&f->types[n==1?0:n==3?1:2];v->name=ralloc_strdup(v,"state");v->data.mode=mode;v->data.location=loc;v->data.driver_location=driver;v->data.read_only=mode!=nir_var_shader_out;
 if(mode==nir_var_shader_in||mode==nir_var_shader_out)v->data.precision=GLSL_PRECISION_HIGH;
 exec_list_push_tail(&f->s->variables,&v->node);return v;
}
static nir_def *gf_const(struct gf_builder*f,unsigned n,const uint32_t*bits){
 nir_load_const_instr*i=rzalloc_size(f->s,sizeof(*i)+n*sizeof(nir_const_value));i->instr.type=nir_instr_type_load_const;gf_def(&i->def,&i->instr,n);for(unsigned j=0;j<n;j++)i->value[j].u32=bits[j];gf_append(f,&i->instr);return &i->def;
}
static nir_def *gf_deref(struct gf_builder*f,nir_variable*v){
 nir_deref_instr*i=rzalloc(f->s,nir_deref_instr);i->instr.type=nir_instr_type_deref;i->deref_type=nir_deref_type_var;i->var=v;i->modes=v->data.mode;i->type=v->type;gf_def(&i->def,&i->instr,1);gf_append(f,&i->instr);return &i->def;
}
static nir_def *gf_uniform(struct gf_builder*f,unsigned n,unsigned slot,nir_def*zero){
 nir_intrinsic_instr*i=rzalloc_size(f->s,sizeof(*i)+sizeof(nir_src));i->instr.type=nir_instr_type_intrinsic;i->intrinsic=nir_intrinsic_load_uniform;i->num_components=n;i->src[0].ssa=zero;gf_def(&i->def,&i->instr,n);nir_intrinsic_set_base(i,slot);nir_intrinsic_set_range(i,1);nir_intrinsic_set_dest_type(i,nir_type_float32);gf_append(f,&i->instr);return &i->def;
}
static nir_def *gf_input(struct gf_builder*f,nir_variable*v){
 nir_def*d=gf_deref(f,v);nir_intrinsic_instr*i=rzalloc_size(f->s,sizeof(*i)+sizeof(nir_src));i->instr.type=nir_instr_type_intrinsic;i->intrinsic=nir_intrinsic_load_deref;i->num_components=4;i->src[0].ssa=d;gf_def(&i->def,&i->instr,4);gf_append(f,&i->instr);return &i->def;
}
static void gf_store(struct gf_builder*f,nir_def*d,nir_def*v,unsigned n,unsigned mask){
 nir_intrinsic_instr*i=rzalloc_size(f->s,sizeof(*i)+2*sizeof(nir_src));i->instr.type=nir_instr_type_intrinsic;i->intrinsic=nir_intrinsic_store_deref;i->num_components=n;i->src[0].ssa=d;i->src[1].ssa=v;nir_intrinsic_set_write_mask(i,mask);gf_append(f,&i->instr);
}
static nir_def *gf_alu(struct gf_builder*f,nir_op op,unsigned n,nir_def*a,const char*sa,nir_def*b,const char*sb){
 unsigned arity=b?2:1;nir_alu_instr*i=rzalloc_size(f->s,sizeof(*i)+arity*sizeof(nir_alu_src));i->instr.type=nir_instr_type_alu;i->op=op;gf_def(&i->def,&i->instr,n);nir_def*src[]={a,b};const char*sw[]={sa,sb};
 for(unsigned j=0;j<arity;j++){i->src[j].src.ssa=src[j];for(unsigned k=0;sw[j][k];k++)i->src[j].swizzle[k]=sw[j][k]-'0';}gf_append(f,&i->instr);return &i->def;
}
static nir_shader *gears_nir_fixture(bool attr){
 nir_shader*s=rzalloc(NULL,nir_shader);struct gf_builder*f=rzalloc(s,struct gf_builder);f->s=s;
 for(unsigned i=0;i<3;i++){f->types[i].base_type=GLSL_TYPE_FLOAT;f->types[i].vector_elements=i==0?1:i==1?3:4;f->types[i].matrix_columns=1;}
 exec_list_make_empty(&s->variables);exec_list_make_empty(&s->functions);s->info.stage=MESA_SHADER_VERTEX;s->num_uniforms=attr?9:10;s->num_inputs=attr?2:1;s->num_outputs=3;
 s->info.inputs_read=BITFIELD64_BIT(VERT_ATTRIB_POS)|(attr?BITFIELD64_BIT(VERT_ATTRIB_NORMAL):0);s->info.outputs_written=BITFIELD64_BIT(VARYING_SLOT_POS)|BITFIELD64_BIT(VARYING_SLOT_COL0)|BITFIELD64_BIT(VARYING_SLOT_PSIZ);
 nir_function*fn=rzalloc(s,nir_function);fn->shader=s;fn->is_entrypoint=true;exec_list_push_tail(&s->functions,&fn->node);nir_function_impl*impl=rzalloc(s,nir_function_impl);fn->impl=impl;impl->function=fn;impl->structured=true;exec_list_make_empty(&impl->body);exec_list_make_empty(&impl->locals);impl->end_block=rzalloc(s,nir_block);exec_list_make_empty(&impl->end_block->instr_list);
 f->b=rzalloc(s,nir_block);f->b->cf_node.type=nir_cf_node_block;f->b->cf_node.parent=&impl->cf_node;exec_list_push_tail(&impl->body,&f->b->cf_node.node);exec_list_make_empty(&f->b->instr_list);
 for(unsigned k=0;k<s->num_uniforms;k++){
  nir_variable*v=gf_var(f,nir_var_uniform,k>=8?3:4,0,k);v->num_state_slots=1;v->state_slots=rzalloc_array(v,nir_state_slot,1);gl_state_index16*t=v->state_slots[0].tokens;
  if(!k)t[0]=STATE_LIGHTMODEL_SCENECOLOR;else if(k<4){t[0]=STATE_LIGHTPROD;t[2]=(k-1)*2;}else if(k<8){t[0]=STATE_MVP_MATRIX_TRANSPOSE;t[2]=t[3]=k-4;}else if(!attr&&k==8){t[0]=STATE_CURRENT_ATTRIB;t[1]=1;}else t[0]=STATE_LIGHT_POSITION_NORMALIZED;
 }
 nir_variable*pi=gf_var(f,nir_var_shader_in,4,VERT_ATTRIB_POS,0),*ni=attr?gf_var(f,nir_var_shader_in,4,VERT_ATTRIB_NORMAL,1):NULL;
 nir_variable*po=gf_var(f,nir_var_shader_out,4,VARYING_SLOT_POS,0),*co=gf_var(f,nir_var_shader_out,4,VARYING_SLOT_COL0,1),*ps=gf_var(f,nir_var_shader_out,1,VARYING_SLOT_PSIZ,2);ps->data.interpolation=INTERP_MODE_SMOOTH;ps->data.precision=GLSL_PRECISION_NONE;
 const uint32_t onebits[]={0x3f800000},zerobits[]={0},boundbits[]={0,0,0,0x3f800000};nir_def*psd=gf_deref(f,ps),*one=gf_const(f,1,onebits);gf_store(f,psd,one,1,1);nir_def*z=gf_const(f,1,zerobits),*row[4];for(unsigned k=0;k<4;k++)row[k]=gf_uniform(f,4,k+4,z);nir_def*p=gf_input(f,pi);
 nir_def*x=gf_alu(f,nir_op_fmul,4,row[0],"0123",p,"0000"),*y=gf_alu(f,nir_op_fmul,4,row[1],"0123",p,"1111"),*acc=gf_alu(f,nir_op_fadd,4,y,"0123",x,"0123");
 x=gf_alu(f,nir_op_fmul,4,row[2],"0123",p,"2222");acc=gf_alu(f,nir_op_fadd,4,x,"0123",acc,"0123");x=gf_alu(f,nir_op_fmul,4,row[3],"0123",p,"3333");acc=gf_alu(f,nir_op_fadd,4,x,"0123",acc,"0123");nir_def*pod=gf_deref(f,po);gf_store(f,pod,acc,4,15);
 nir_def*n=attr?gf_input(f,ni):gf_uniform(f,3,8,z),*len=gf_alu(f,nir_op_fdot3,1,n,"012",n,"012"),*rsq=gf_alu(f,nir_op_frsq,1,len,"0",NULL,NULL),*nv=gf_alu(f,nir_op_fmul,3,n,"012",rsq,"000"),*scene=gf_uniform(f,4,0,z),*alpha=gf_alu(f,nir_op_mov,4,scene,"3333",NULL,NULL),*cod=gf_deref(f,co);gf_store(f,cod,alpha,4,8);
 nir_def*ambient=gf_uniform(f,4,1,z),*diffuse=gf_uniform(f,4,2,z),*spec=gf_uniform(f,4,3,z),*light=gf_uniform(f,3,attr?8:9,z),*dp=gf_alu(f,nir_op_fdot3,1,nv,"012",light,"012"),*bound=gf_const(f,4,boundbits),*clamp=gf_alu(f,nir_op_fmax,4,bound,"0123",dp,"0000"),*positive=gf_alu(f,nir_op_slt,1,z,"0",dp,"0");
 nir_def*sum=gf_alu(f,nir_op_fadd,4,ambient,"0123",scene,"0123"),*product=gf_alu(f,nir_op_fmul,4,clamp,"1111",diffuse,"0123");sum=gf_alu(f,nir_op_fadd,4,product,"0123",sum,"0123");product=gf_alu(f,nir_op_fmul,4,positive,"0000",spec,"0123");sum=gf_alu(f,nir_op_fadd,4,product,"0123",sum,"0123");gf_store(f,cod,sum,4,7);return s;
}
#endif
