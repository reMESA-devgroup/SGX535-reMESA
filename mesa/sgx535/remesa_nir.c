/* SPDX-License-Identifier: MIT */
#include "remesa_nir.h"
#include <errno.h>
#include <string.h>
#include "nir.h"
int remesa_constant_rgba32(const uint32_t rgba[4],uint32_t *out)
{
 uint32_t bytes[4],argb;
 if(!rgba||!out)return EINVAL;
 for(unsigned i=0;i<4;i++) {
  if(rgba[i]!=0 && rgba[i]!=0x3f800000)return ENOTSUP;
  bytes[i]=rgba[i] ? 255 : 0;
 }
 argb=(bytes[3]<<24)|(bytes[0]<<16)|(bytes[1]<<8)|bytes[2];
 /* These exact opaque outputs were physically observed. Zero background
  * pixels are not evidence for a transparent fragment export. */
 if(argb!=0xffff00ff && argb!=0xff00ff00 && argb!=0xffffff00)return ENOTSUP;
 *out=argb;return 0;
}
static bool location(unsigned n) {return n==FRAG_RESULT_COLOR||n==FRAG_RESULT_DATA0;}
static bool vector4(const struct glsl_type *t)
{return t&&t->base_type==GLSL_TYPE_FLOAT&&t->vector_elements==4&&t->matrix_columns==1;}
/* Track only bit-preserving SSA channel routing from the sole POSITION input.
 * Scalarization and vector reconstruction do not require new SGX instructions:
 * the accepted final value is provably the same float4 position profile. */
#define REMESA_POSITION_VALUES 16
struct position_value {
 nir_def *def;
 unsigned components,channel[4];
 uint32_t dependencies;
};
static int position_index(const struct position_value values[],unsigned count,
                          const nir_def *def)
{
 for(unsigned i=0;i<count;i++)if(values[i].def==def)return (int)i;
 return -1;
}
static void position_input(struct position_value values[],unsigned *count,nir_def *def)
{
 values[0]=(struct position_value){.def=def,.components=4,.channel={0,1,2,3},.dependencies=1};
 *count=1;
}
static int position_copy(struct position_value values[],unsigned *count,nir_alu_instr *alu)
{
 struct position_value value={0};
 if(!*count||*count>=REMESA_POSITION_VALUES||alu->exact||
    alu->def.parent_instr!=&alu->instr||alu->def.bit_size!=32||
    !alu->def.num_components||alu->def.num_components>4)return ENOTSUP;
 value.def=&alu->def;value.components=alu->def.num_components;
 if(alu->op==nir_op_mov) {
  int src=position_index(values,*count,alu->src[0].src.ssa);
  if(src<0)return ENOTSUP;
  for(unsigned i=0;i<value.components;i++) {
   unsigned channel=alu->src[0].swizzle[i];
   if(channel>=values[src].components)return ENOTSUP;
   value.channel[i]=values[src].channel[channel];
  }
  value.dependencies=values[src].dependencies;
 } else if((alu->op==nir_op_vec2&&value.components==2)||
           (alu->op==nir_op_vec3&&value.components==3)||
           (alu->op==nir_op_vec4&&value.components==4)) {
  for(unsigned i=0;i<value.components;i++) {
   int src=position_index(values,*count,alu->src[i].src.ssa);
   unsigned channel=alu->src[i].swizzle[0];
   if(src<0||channel>=values[src].components)return ENOTSUP;
   value.channel[i]=values[src].channel[channel];
   value.dependencies|=values[src].dependencies;
  }
 } else return ENOTSUP;
 value.dependencies|=1u<<*count;values[(*count)++]=value;return 0;
}
static bool position_identity(const struct position_value values[],unsigned count,nir_def *def)
{
 int index=position_index(values,count,def);
 if(index<0||values[index].components!=4||values[index].dependencies!=((1u<<count)-1u))return false;
 for(unsigned i=0;i<4;i++)if(values[index].channel[i]!=i)return false;
 return true;
}
/* Compile-time construction of a constant does not imply native ALU support.
 * Every admitted value is defined earlier in this one block, and every value
 * must contribute to the complete export (or its zero offset). The finite table
 * rejects cycles, forward references, dead instructions and unbounded chains.
 * Integer bit operations and constant Boolean selection below are evaluated
 * on HOST uint32 bits. No GPU ALU, floating arithmetic or conversion is added. */
#define REMESA_CONSTANT_VALUES 16
struct constant_value {
 nir_def *def;
 uint32_t bits[4];
 uint32_t dependencies;
 unsigned components;
 unsigned bit_size;
};
static int constant_index(const struct constant_value values[],unsigned count,
                          const nir_def *def)
{
 for(unsigned i=0;i<count;i++)if(values[i].def==def)return (int)i;
 return -1;
}
static int append_constant(struct constant_value values[],unsigned *count,
                           nir_instr *instr)
{
 struct constant_value value={0};
 if(*count>=REMESA_CONSTANT_VALUES)return ENOTSUP;
 if(instr->type==nir_instr_type_load_const) {
  nir_load_const_instr *load=nir_instr_as_load_const(instr);
  if(load->def.parent_instr!=instr||(load->def.bit_size!=32&&load->def.bit_size!=1)||
     !load->def.num_components||load->def.num_components>4)return ENOTSUP;
  value.def=&load->def;value.components=load->def.num_components;value.bit_size=load->def.bit_size;
  for(unsigned i=0;i<value.components;i++) {
   value.bits[i]=load->value[i].u32;
   if(value.bit_size==1&&value.bits[i]>1)return ENOTSUP;
  }
 } else {
  nir_alu_instr *alu=nir_instr_as_alu(instr);
  if(alu->def.parent_instr!=instr||alu->def.bit_size!=32||alu->exact||
     !alu->def.num_components||alu->def.num_components>4)return ENOTSUP;
  value.def=&alu->def;value.components=alu->def.num_components;value.bit_size=32;
  if(alu->op==nir_op_mov) {
   int index=constant_index(values,*count,alu->src[0].src.ssa);
   if(index<0||values[index].bit_size!=32)return ENOTSUP;
   for(unsigned i=0;i<value.components;i++) {
    unsigned channel=alu->src[0].swizzle[i];
    if(channel>=values[index].components)return ENOTSUP;
    value.bits[i]=values[index].bits[channel];
   }
   value.dependencies=values[index].dependencies;
  } else if((alu->op==nir_op_vec2&&value.components==2)||
            (alu->op==nir_op_vec3&&value.components==3)||
            (alu->op==nir_op_vec4&&value.components==4)) {
   for(unsigned i=0;i<value.components;i++) {
    int index=constant_index(values,*count,alu->src[i].src.ssa);
    unsigned channel=alu->src[i].swizzle[0];
    if(index<0||values[index].bit_size!=32||channel>=values[index].components)return ENOTSUP;
    value.bits[i]=values[index].bits[channel];
    value.dependencies|=values[index].dependencies;
   }
  } else if(alu->op==nir_op_iand||alu->op==nir_op_ior||alu->op==nir_op_ixor||
            alu->op==nir_op_inot||alu->op==nir_op_bcsel) {
   unsigned sources=alu->op==nir_op_inot?1:alu->op==nir_op_bcsel?3:2;
   uint32_t operands[3][4]={{0}};
   for(unsigned src=0;src<sources;src++) {
    int index=constant_index(values,*count,alu->src[src].src.ssa);
    unsigned bits=alu->op==nir_op_bcsel&&src==0?1:32;
    if(index<0||values[index].bit_size!=bits)return ENOTSUP;
    for(unsigned c=0;c<value.components;c++) {
     unsigned channel=alu->src[src].swizzle[c];
     if(channel>=values[index].components)return ENOTSUP;
     operands[src][c]=values[index].bits[channel];
    }
    value.dependencies|=values[index].dependencies;
   }
   for(unsigned c=0;c<value.components;c++) {
    switch(alu->op) {
    case nir_op_iand:value.bits[c]=operands[0][c]&operands[1][c];break;
    case nir_op_ior:value.bits[c]=operands[0][c]|operands[1][c];break;
    case nir_op_ixor:value.bits[c]=operands[0][c]^operands[1][c];break;
    case nir_op_inot:value.bits[c]=~operands[0][c];break;
    case nir_op_bcsel:value.bits[c]=operands[0][c]?operands[1][c]:operands[2][c];break;
    default:return ENOTSUP;
    }
   }
  } else return ENOTSUP;
 }
 value.dependencies|=1u<<*count;
 values[(*count)++]=value;return 0;
}
int remesa_nir_constant_fragment(const nir_shader *shader,uint32_t *out)
{
 nir_function *entry=NULL;nir_variable *output=NULL;unsigned functions=0,variables=0,blocks=0,instructions=0,stores=0,values_count=0,derefs=0,loc=0;
 struct constant_value values[REMESA_CONSTANT_VALUES]={0};
 nir_deref_instr *deref=NULL;uint32_t used=0,bits[4]={0},written=0;
 bool medium_output=false;
 if(!shader||!out)return EINVAL;
 if(shader->info.stage!=MESA_SHADER_FRAGMENT)return ENOTSUP;
 if(shader->num_inputs||shader->num_outputs>1||shader->num_uniforms||shader->global_mem_size||shader->scratch_size||shader->constant_data_size||shader->xfb_info||
    shader->info.num_textures||shader->info.num_ubos||shader->info.num_abos||shader->info.num_ssbos||shader->info.num_images||shader->info.outputs_read||
    shader->info.inputs_read||shader->info.writes_memory||shader->info.fs.uses_discard||shader->info.fs.uses_fbfetch_output||
    shader->info.fs.color_is_dual_source||shader->info.fs.uses_sample_shading||shader->info.fs.uses_sample_qualifier||
    shader->info.fs.require_full_quads||shader->info.fs.quad_derivatives||shader->info.fs.early_fragment_tests||
    shader->info.fs.post_depth_coverage||shader->info.fs.inner_coverage||shader->info.fs.origin_upper_left||
    shader->info.fs.pixel_center_integer||shader->info.fs.needs_coarse_quad_helper_invocations||shader->info.fs.needs_full_quad_helper_invocations)return ENOTSUP;
 for(unsigned i=0;i<ARRAY_SIZE(shader->info.system_values_read);i++)if(shader->info.system_values_read[i])return ENOTSUP;
 for(unsigned i=0;i<ARRAY_SIZE(shader->info.xfb_stride);i++)if(shader->info.xfb_stride[i])return ENOTSUP;
 nir_foreach_variable_in_shader(var,shader) {
  if(++variables>1||var->data.mode!=nir_var_shader_out||!location(var->data.location)||
     var->data.index||var->data.driver_location||var->data.location_frac||var->data.patch||var->data.per_view||var->data.per_primitive||var->data.compact||var->data.centroid||var->data.sample||
     var->data.interpolation||var->data.fb_fetch_output||var->num_members||var->members||var->interface_type||
     var->num_state_slots||var->state_slots||var->constant_initializer||var->pointer_initializer||
     var->data.access||var->data.bindless||var->data.binding||var->data.descriptor_set||var->data.explicit_binding||
     var->data.is_xfb||var->data.is_xfb_only||var->data.stream||var->data.read_only||
     var->data.explicit_xfb_buffer||var->data.explicit_xfb_stride||var->data.explicit_offset||
     var->data.matrix_layout||var->data.from_named_ifc_block||var->data.from_ssbo_unsized_array||
     var->data.offset||var->data.ray_query||var->data.has_initializer||var->data.is_implicit_initializer||
     (var->data.precision!=GLSL_PRECISION_NONE&&var->data.precision!=GLSL_PRECISION_HIGH&&
      var->data.precision!=GLSL_PRECISION_MEDIUM)||!vector4(var->type))return ENOTSUP;
  medium_output=var->data.precision==GLSL_PRECISION_MEDIUM;
  output=var;
 }
 nir_foreach_function(fn,shader) {
  if(++functions>1||!fn->is_entrypoint||fn->is_preamble||fn->is_exported||fn->shader!=shader||fn->num_params||!fn->impl)return ENOTSUP;
  entry=fn;
 }
 if(!entry)return EINVAL;
 nir_function_impl *impl=entry->impl;
 if(impl->function!=entry||impl->preamble||!impl->end_block||!exec_list_is_empty(&impl->end_block->instr_list)||!impl->structured||!exec_list_is_empty(&impl->locals))return ENOTSUP;
 foreach_list_typed(nir_cf_node,node,node,&impl->body) {
  if(++blocks>1||node->type!=nir_cf_node_block||node->parent!=&impl->cf_node)return ENOTSUP;
  nir_block *block=nir_cf_node_as_block(node);
  nir_foreach_instr(instr,block) {
   if(++instructions>REMESA_CONSTANT_VALUES+5||instr->block!=block)return EINVAL;
   if(instr->type==nir_instr_type_load_const||instr->type==nir_instr_type_alu) {
    /* GLSL ES gl_FragColor is mediump. The exact direct constants 0 and 1
     * survive every required precision, so this bounded output needs no
     * conversion instruction. Keep all arithmetic under mediump rejected:
     * evaluating its intermediate values at 32 bits could change semantics.
     * remesa_constant_rgba32 still rejects final components outside 0/1. */
    if(medium_output&&instr->type==nir_instr_type_alu)return ENOTSUP;
    int error=append_constant(values,&values_count,instr);
    if(error)return error;
    continue;
   }
   if(instr->type==nir_instr_type_deref) {
    deref=nir_instr_as_deref(instr);
    if(++derefs>1||!output||deref->deref_type!=nir_deref_type_var||deref->var!=output||
       deref->modes!=nir_var_shader_out||deref->type!=output->type||deref->def.parent_instr!=instr)return ENOTSUP;
    continue;
   }
   if(instr->type!=nir_instr_type_intrinsic)return ENOTSUP;
   nir_intrinsic_instr *store=nir_instr_as_intrinsic(instr);nir_def *value=NULL,*offset=NULL;
   unsigned store_loc,component=0;
   if(store->intrinsic==nir_intrinsic_store_deref) {
    if(shader->info.io_lowered||!output||!deref||store->src[0].ssa!=&deref->def||nir_intrinsic_access(store))return ENOTSUP;
    value=store->src[1].ssa;store_loc=output->data.location;
   } else if(store->intrinsic==nir_intrinsic_store_output) {
    nir_io_semantics sem=nir_intrinsic_io_semantics(store),canonical={.location=sem.location,.num_slots=1};
    nir_io_xfb xfb=nir_intrinsic_io_xfb(store),xfb2=nir_intrinsic_io_xfb2(store),none={0};
    if(!shader->info.io_lowered||variables||derefs||!location(sem.location)||sem.num_slots!=1||
       sem.dual_source_blend_index||sem.medium_precision||memcmp(&sem,&canonical,sizeof(sem))||memcmp(&xfb,&none,sizeof(xfb))||memcmp(&xfb2,&none,sizeof(xfb2))||
       nir_intrinsic_range(store)>1||nir_intrinsic_base(store)||
       nir_intrinsic_src_type(store)!=nir_type_float32)return ENOTSUP;
    store_loc=sem.location;component=nir_intrinsic_component(store);
    value=store->src[0].ssa;offset=store->src[1].ssa;
   } else return ENOTSUP;
   /* Scalar/vector IO lowering may split one constant vec4 export into
    * interleaved channel definitions and stores. Admit only a complete,
    * single-location, non-overlapping color: no last-write-wins ambiguity. */
   if(stores>=4||(stores&&loc!=store_loc)||!value||component>3||
      !store->num_components||store->num_components>4-component)return ENOTSUP;
   unsigned mask=nir_intrinsic_write_mask(store);
   if(!mask||(mask&~((1u<<store->num_components)-1u))||
      (written&(mask<<component)))return ENOTSUP;
   int color=constant_index(values,values_count,value);
   if(color<0||values[color].bit_size!=32||values[color].components!=store->num_components)return ENOTSUP;
   used|=values[color].dependencies;
   if(offset) {
    int zero=constant_index(values,values_count,offset);
    if(zero<0||values[zero].bit_size!=32||values[zero].components!=1||values[zero].bits[0])return ENOTSUP;
    used|=values[zero].dependencies;
   }
   for(unsigned i=0;i<store->num_components;i++)
    if(mask&(1u<<i))bits[component+i]=values[color].bits[i];
   written|=mask<<component;loc=store_loc;stores++;
  }
 }
 if(blocks!=1||!stores||written!=15||!values_count||used!=((1u<<values_count)-1u)||
    derefs!=(shader->info.io_lowered?0u:1u))return ENOTSUP;
 if(shader->info.outputs_written && shader->info.outputs_written!=BITFIELD64_BIT(loc))return ENOTSUP;
 return remesa_constant_rgba32(bits,out);
}
static bool position_attribute(unsigned location)
{
 return location==VERT_ATTRIB_GENERIC0||location==VERT_ATTRIB_POS;
}
/* Variable IO representation of exactly POSITION = one position attribute.
 * Mesa compatibility gl_Vertex and programmable generic0 have the same
 * independently recognized float4 passthrough semantics; both use input slot0.
 * NIR itself is
 * inspected without mutation; this produces the same restricted descriptor as
 * lowered IO. It does not encode or claim a new SGX shader program. */
static int position_vertex_deref(const nir_shader *shader,unsigned *input_slot)
{
 nir_variable *input_var=NULL,*output_var=NULL;nir_function *entry=NULL;
 nir_deref_instr *input_deref=NULL,*output_deref=NULL;nir_intrinsic_instr *load=NULL;
 struct position_value values[REMESA_POSITION_VALUES]={0};unsigned values_count=0;
 unsigned variables=0,functions=0,blocks=0,instructions=0,stores=0;
 nir_foreach_variable_in_shader(var,shader) {
  if(++variables>2 || !vector4(var->type) || var->num_members || var->members ||
     var->interface_type || var->num_state_slots || var->state_slots ||
     var->constant_initializer || var->pointer_initializer ||
     var->data.index || var->data.driver_location || var->data.location_frac ||
     var->data.patch || var->data.per_view || var->data.per_primitive ||
     var->data.compact || var->data.centroid || var->data.sample ||
     (var->data.interpolation && !(var->data.mode==nir_var_shader_out && var->data.interpolation==INTERP_MODE_SMOOTH)) || var->data.fb_fetch_output || var->data.is_xfb ||
     var->data.is_xfb_only || var->data.stream || var->data.access ||
     var->data.bindless || var->data.explicit_binding || var->data.binding || var->data.descriptor_set ||
     var->data.explicit_xfb_buffer || var->data.explicit_xfb_stride || var->data.explicit_offset ||
     var->data.matrix_layout || var->data.from_named_ifc_block || var->data.from_ssbo_unsized_array ||
     var->data.offset || var->data.ray_query ||
     var->data.has_initializer || var->data.is_implicit_initializer ||
     (var->data.precision!=GLSL_PRECISION_NONE&&var->data.precision!=GLSL_PRECISION_HIGH))return ENOTSUP;
  if(var->data.mode==nir_var_shader_in) {
   if(input_var||!position_attribute(var->data.location))return ENOTSUP;
   input_var=var;
  } else if(var->data.mode==nir_var_shader_out) {
   if(output_var||var->data.location!=VARYING_SLOT_POS||var->data.read_only)return ENOTSUP;
   output_var=var;
  } else return ENOTSUP;
 }
 if(variables!=2||!input_var||!output_var)return ENOTSUP;
 if(shader->info.inputs_read&&shader->info.inputs_read!=BITFIELD64_BIT(input_var->data.location))return ENOTSUP;
 nir_foreach_function(fn,shader) {
  if(++functions>1||!fn->is_entrypoint||fn->is_preamble||fn->is_exported||fn->shader!=shader||fn->num_params||!fn->impl)return ENOTSUP;
  entry=fn;
 }
 if(!entry)return EINVAL;
 nir_function_impl *impl=entry->impl;
 if(impl->function!=entry||impl->preamble||!impl->end_block||!exec_list_is_empty(&impl->end_block->instr_list)||!impl->structured||!exec_list_is_empty(&impl->locals))return ENOTSUP;
 foreach_list_typed(nir_cf_node,node,node,&impl->body) {
  if(++blocks>1||node->type!=nir_cf_node_block||node->parent!=&impl->cf_node)return ENOTSUP;
  nir_block *block=nir_cf_node_as_block(node);
  nir_foreach_instr(instr,block) {
   if(++instructions>REMESA_POSITION_VALUES+3||instr->block!=block)return EINVAL;
   if(instr->type==nir_instr_type_deref) {
    nir_deref_instr *d=nir_instr_as_deref(instr);
    if(d->deref_type!=nir_deref_type_var||d->def.parent_instr!=instr||
       d->def.num_components!=1||d->def.bit_size!=32)return ENOTSUP;
    if(d->var==input_var) {
     if(input_deref||d->modes!=nir_var_shader_in||d->type!=input_var->type)return ENOTSUP;
     input_deref=d;
    } else if(d->var==output_var) {
     if(output_deref||d->modes!=nir_var_shader_out||d->type!=output_var->type)return ENOTSUP;
     output_deref=d;
    } else return ENOTSUP;
    continue;
   }
   if(instr->type==nir_instr_type_alu) {
    if(stores||!load||position_copy(values,&values_count,nir_instr_as_alu(instr)))return ENOTSUP;
    continue;
   }
   if(instr->type!=nir_instr_type_intrinsic)return ENOTSUP;
   nir_intrinsic_instr *io=nir_instr_as_intrinsic(instr);
   if(io->intrinsic==nir_intrinsic_load_deref) {
    if(load||stores||!input_deref||io->src[0].ssa!=&input_deref->def||
       io->num_components!=4||io->def.parent_instr!=instr||io->def.num_components!=4||
       io->def.bit_size!=32||nir_intrinsic_access(io))return ENOTSUP;
    load=io;position_input(values,&values_count,&load->def);continue;
   }
   if(io->intrinsic!=nir_intrinsic_store_deref||++stores>1||!output_deref||!load||
      io->src[0].ssa!=&output_deref->def||!position_identity(values,values_count,io->src[1].ssa)||
      io->num_components!=4||nir_intrinsic_write_mask(io)!=15||nir_intrinsic_access(io))return ENOTSUP;
  }
 }
 if(blocks!=1||instructions!=values_count+3||stores!=1||!load||!input_deref||!output_deref)return ENOTSUP;
 *input_slot=0;return 0;
}
/* A description of an identity position shader, not a general NIR compiler.
 * The caller separately validates vertex layout and clip-to-window preparation.
 * Exact lowered or variable IO: no uniforms, transforms, texture/memory or sysvalues. */
int remesa_nir_position_vertex(const nir_shader *shader,unsigned *input_slot)
{
 nir_function *entry=NULL;nir_load_const_instr *zero=NULL;nir_intrinsic_instr *input=NULL;
 struct position_value values[REMESA_POSITION_VALUES]={0};unsigned values_count=0;
 unsigned functions=0,blocks=0,instructions=0,stores=0;
 if(!shader||!input_slot)return EINVAL;
 if(shader->info.stage!=MESA_SHADER_VERTEX||
    (shader->info.io_lowered&&!exec_list_is_empty(&shader->variables))||shader->num_inputs>1||shader->num_outputs>1||shader->num_uniforms||
    shader->global_mem_size||shader->scratch_size||shader->constant_data_size||shader->xfb_info||
    shader->info.writes_memory||shader->info.num_textures||shader->info.num_ubos||shader->info.num_abos||
    shader->info.num_ssbos||shader->info.num_images||shader->info.dual_slot_inputs||shader->info.outputs_read||
    shader->info.per_primitive_inputs||shader->info.per_primitive_outputs||shader->info.clip_distance_array_size||
    shader->info.cull_distance_array_size||shader->info.vs.double_inputs||shader->info.vs.blit_sgprs_amd||
    shader->info.vs.tes_agx||shader->info.vs.window_space_position||shader->info.vs.needs_edge_flag)return ENOTSUP;
 if((shader->info.inputs_read&&shader->info.inputs_read!=BITFIELD64_BIT(VERT_ATTRIB_GENERIC0)&&
     shader->info.inputs_read!=BITFIELD64_BIT(VERT_ATTRIB_POS))||
    (shader->info.outputs_written&&shader->info.outputs_written!=BITFIELD64_BIT(VARYING_SLOT_POS)))return ENOTSUP;
 for(unsigned i=0;i<ARRAY_SIZE(shader->info.system_values_read);i++)if(shader->info.system_values_read[i])return ENOTSUP;
 for(unsigned i=0;i<ARRAY_SIZE(shader->info.xfb_stride);i++)if(shader->info.xfb_stride[i])return ENOTSUP;
 if(!shader->info.io_lowered)return position_vertex_deref(shader,input_slot);
 nir_foreach_function(fn,shader) {
  if(++functions>1||!fn->is_entrypoint||fn->is_preamble||fn->is_exported||fn->shader!=shader||fn->num_params||!fn->impl)return ENOTSUP;
  entry=fn;
 }
 if(!entry)return EINVAL;
 nir_function_impl *impl=entry->impl;
 if(impl->function!=entry||impl->preamble||!impl->end_block||!exec_list_is_empty(&impl->end_block->instr_list)||!impl->structured||!exec_list_is_empty(&impl->locals))return ENOTSUP;
 foreach_list_typed(nir_cf_node,node,node,&impl->body) {
  if(++blocks>1||node->type!=nir_cf_node_block||node->parent!=&impl->cf_node)return ENOTSUP;
  nir_block *block=nir_cf_node_as_block(node);
  nir_foreach_instr(instr,block) {
   if(++instructions>REMESA_POSITION_VALUES+2||instr->block!=block)return EINVAL;
   if(instr->type==nir_instr_type_load_const) {
    if(zero)return ENOTSUP;
    zero=nir_instr_as_load_const(instr);
    if(zero->def.parent_instr!=instr||zero->def.bit_size!=32||zero->def.num_components!=1||zero->value[0].u32)return ENOTSUP;
    continue;
   }
   if(instr->type==nir_instr_type_alu) {
    if(stores||!input||position_copy(values,&values_count,nir_instr_as_alu(instr)))return ENOTSUP;
    continue;
   }
   if(instr->type!=nir_instr_type_intrinsic)return ENOTSUP;
   nir_intrinsic_instr *io=nir_instr_as_intrinsic(instr);
   if(io->intrinsic==nir_intrinsic_load_input) {
    nir_io_semantics sem=nir_intrinsic_io_semantics(io),expected={.location=sem.location,.num_slots=1};
    if(input||!zero||stores||io->src[0].ssa!=&zero->def||io->num_components!=4||
       !position_attribute(sem.location)||
       (shader->info.inputs_read&&shader->info.inputs_read!=BITFIELD64_BIT(sem.location))||
       io->def.parent_instr!=instr||io->def.bit_size!=32||io->def.num_components!=4||
       nir_intrinsic_dest_type(io)!=nir_type_float32||nir_intrinsic_base(io)||nir_intrinsic_component(io)||
       nir_intrinsic_range(io)>1||memcmp(&sem,&expected,sizeof(sem)))return ENOTSUP;
    input=io;position_input(values,&values_count,&input->def);continue;
   }
   if(io->intrinsic!=nir_intrinsic_store_output)return ENOTSUP;
   nir_io_semantics sem=nir_intrinsic_io_semantics(io),expected={.location=VARYING_SLOT_POS,.num_slots=1};
   nir_io_xfb xfb=nir_intrinsic_io_xfb(io),xfb2=nir_intrinsic_io_xfb2(io),none={0};
   if(++stores>1||!input||!position_identity(values,values_count,io->src[0].ssa)||io->src[1].ssa!=&zero->def||
      io->num_components!=4||nir_intrinsic_write_mask(io)!=15||nir_intrinsic_src_type(io)!=nir_type_float32||
      nir_intrinsic_base(io)||nir_intrinsic_component(io)||nir_intrinsic_range(io)>1||
      memcmp(&sem,&expected,sizeof(sem))||memcmp(&xfb,&none,sizeof(xfb))||memcmp(&xfb2,&none,sizeof(xfb2)))return ENOTSUP;
  }
 }
 if(blocks!=1||!zero||!input||stores!=1||instructions!=values_count+2)return ENOTSUP;
 *input_slot=0;return 0;
}

/* Opt-in semantic boundary for the separately reconstructed COLOR0/1 CPU
 * profile. Variable IO only: interpolated lowered IO has a separate contract.
 * No ALU/swizzle, split stores, second varying or hidden side effects. */
int remesa_nir_fragment_color_profile(const nir_shader *shader,unsigned *input_slot,unsigned *flat)
{
 nir_variable *input=NULL,*output=NULL;nir_function *entry=NULL;
 nir_deref_instr *id=NULL,*od=NULL;nir_intrinsic_instr *load=NULL;
 unsigned variables=0,functions=0,blocks=0,instructions=0,stores=0;
 if(!shader||!input_slot||!flat||input_slot==flat)return EINVAL;
 if(shader->info.stage!=MESA_SHADER_FRAGMENT||shader->info.io_lowered||
    shader->num_inputs>1||shader->num_outputs>1||shader->num_uniforms||
    shader->global_mem_size||shader->scratch_size||shader->constant_data_size||shader->xfb_info||shader->info.float_controls_execution_mode||
    shader->info.writes_memory||shader->info.num_textures||shader->info.num_ubos||shader->info.num_abos||
    shader->info.num_ssbos||shader->info.num_images||shader->info.outputs_read||
    shader->info.per_primitive_inputs||shader->info.per_primitive_outputs||
    shader->info.fs.uses_discard||shader->info.fs.uses_fbfetch_output||
    shader->info.fs.color_is_dual_source||shader->info.fs.uses_sample_shading||
    shader->info.fs.uses_sample_qualifier||shader->info.fs.require_full_quads||
    shader->info.fs.quad_derivatives||shader->info.fs.early_fragment_tests||
    shader->info.fs.post_depth_coverage||shader->info.fs.inner_coverage||
    shader->info.fs.origin_upper_left||shader->info.fs.pixel_center_integer||
    shader->info.fs.needs_coarse_quad_helper_invocations||shader->info.fs.needs_full_quad_helper_invocations)
  return ENOTSUP;
 for(unsigned n=0;n<ARRAY_SIZE(shader->info.system_values_read);n++)
  if(shader->info.system_values_read[n])return ENOTSUP;
 for(unsigned n=0;n<ARRAY_SIZE(shader->info.xfb_stride);n++)
  if(shader->info.xfb_stride[n])return ENOTSUP;
 nir_foreach_variable_in_shader(var,shader) {
  if(++variables>2||!vector4(var->type)||var->num_members||var->members||
     var->interface_type||var->num_state_slots||var->state_slots||
     var->constant_initializer||var->pointer_initializer||var->data.index||
     var->data.driver_location||var->data.location_frac||var->data.patch||
     var->data.per_view||var->data.per_primitive||var->data.compact||
     var->data.centroid||var->data.sample||
     (var->data.interpolation&&var->data.interpolation!=INTERP_MODE_SMOOTH&&
      !(var->data.mode==nir_var_shader_in&&var->data.interpolation==INTERP_MODE_FLAT))||
     var->data.fb_fetch_output||var->data.is_xfb||var->data.is_xfb_only||
     var->data.stream||var->data.access||var->data.bindless||
     var->data.explicit_binding||var->data.binding||var->data.descriptor_set||
     var->data.explicit_xfb_buffer||var->data.explicit_xfb_stride||var->data.explicit_offset||
     var->data.matrix_layout||var->data.from_named_ifc_block||var->data.from_ssbo_unsized_array||
     var->data.offset||var->data.ray_query||var->data.has_initializer||
     var->data.is_implicit_initializer||
     (var->data.precision!=GLSL_PRECISION_NONE&&var->data.precision!=GLSL_PRECISION_HIGH))return ENOTSUP;
  if(var->data.mode==nir_var_shader_in) {
   if(input||(var->data.location!=VARYING_SLOT_COL0&&var->data.location!=VARYING_SLOT_COL1))return ENOTSUP;
   input=var;
  } else if(var->data.mode==nir_var_shader_out) {
   if(output||!location(var->data.location)||var->data.read_only)return ENOTSUP;
   output=var;
  } else return ENOTSUP;
 }
 if(variables!=2||!input||!output||
    (shader->info.inputs_read&&shader->info.inputs_read!=BITFIELD64_BIT(input->data.location))||
    (shader->info.outputs_written&&shader->info.outputs_written!=BITFIELD64_BIT(output->data.location)))return ENOTSUP;
 nir_foreach_function(fn,shader) {
  if(++functions>1||!fn->is_entrypoint||fn->is_preamble||fn->is_exported||fn->shader!=shader||fn->num_params||!fn->impl)return ENOTSUP;
  entry=fn;
 }
 if(!entry)return EINVAL;
 nir_function_impl *impl=entry->impl;
 if(impl->function!=entry||impl->preamble||!impl->end_block||
    !exec_list_is_empty(&impl->end_block->instr_list)||!impl->structured||!exec_list_is_empty(&impl->locals))return ENOTSUP;
 foreach_list_typed(nir_cf_node,node,node,&impl->body) {
  if(++blocks>1||node->type!=nir_cf_node_block||node->parent!=&impl->cf_node)return ENOTSUP;
  nir_block *block=nir_cf_node_as_block(node);
  nir_foreach_instr(instr,block) {
   if(++instructions>4||instr->block!=block)return ENOTSUP;
   if(instr->type==nir_instr_type_deref) {
    nir_deref_instr *d=nir_instr_as_deref(instr);
    if(stores||d->deref_type!=nir_deref_type_var||d->def.parent_instr!=instr||
       d->def.num_components!=1||d->def.bit_size!=32)return ENOTSUP;
    if(d->var==input) {
     if(id||d->modes!=nir_var_shader_in||d->type!=input->type)return ENOTSUP;
     id=d;
    } else if(d->var==output) {
     if(od||d->modes!=nir_var_shader_out||d->type!=output->type)return ENOTSUP;
     od=d;
    } else return ENOTSUP;
    continue;
   }
   if(instr->type!=nir_instr_type_intrinsic)return ENOTSUP;
   nir_intrinsic_instr *io=nir_instr_as_intrinsic(instr);
   if(io->intrinsic==nir_intrinsic_load_deref) {
    if(load||stores||!id||io->src[0].ssa!=&id->def||io->num_components!=4||
       io->def.parent_instr!=instr||io->def.num_components!=4||
       io->def.bit_size!=32||nir_intrinsic_access(io))return ENOTSUP;
    load=io;continue;
   }
   if(io->intrinsic!=nir_intrinsic_store_deref||++stores>1||!od||!load||
      io->src[0].ssa!=&od->def||io->src[1].ssa!=&load->def||
      io->num_components!=4||nir_intrinsic_write_mask(io)!=15||nir_intrinsic_access(io))return ENOTSUP;
  }
 }
 if(blocks!=1||instructions!=4||stores!=1||!id||!od||!load)return ENOTSUP;
 *input_slot=input->data.location==VARYING_SLOT_COL0?1:2;
 *flat=input->data.interpolation==INTERP_MODE_FLAT;return 0;
}

int remesa_nir_fragment_color_input(const nir_shader *shader,unsigned *input_slot)
{
 unsigned slot,flat;if(!input_slot)return EINVAL;
 int error=remesa_nir_fragment_color_profile(shader,&slot,&flat);
 if(error)return error;
 if(flat)return ENOTSUP;
 *input_slot=slot;return 0;
}

int remesa_nir_white_lambert_vertex(const nir_shader *shader)
{
 nir_function *entry=NULL;nir_def *input[3]={0},*dot=NULL,*sat=NULL,*color=NULL;
 nir_load_const_instr *zero=NULL;unsigned functions=0,blocks=0,instructions=0,stores=0;
 const uint64_t inputs=BITFIELD64_BIT(VERT_ATTRIB_GENERIC0)|
  BITFIELD64_BIT(VERT_ATTRIB_GENERIC1)|BITFIELD64_BIT(VERT_ATTRIB_GENERIC2);
 const uint64_t outputs=BITFIELD64_BIT(VARYING_SLOT_POS)|BITFIELD64_BIT(VARYING_SLOT_COL0);
 if(!shader)return EINVAL;
 if(shader->info.stage!=MESA_SHADER_VERTEX||!shader->info.io_lowered||
    !exec_list_is_empty(&shader->variables)||
    (shader->num_inputs&&shader->num_inputs!=3)||(shader->num_outputs&&shader->num_outputs!=2)||
    shader->num_uniforms||shader->global_mem_size||shader->scratch_size||
    shader->constant_data_size||shader->xfb_info||shader->info.float_controls_execution_mode||shader->info.writes_memory||
    shader->info.num_textures||shader->info.num_ubos||shader->info.num_abos||
    shader->info.num_ssbos||shader->info.num_images||shader->info.dual_slot_inputs||
    shader->info.outputs_read||shader->info.per_primitive_inputs||shader->info.per_primitive_outputs||
    shader->info.clip_distance_array_size||shader->info.cull_distance_array_size||
    shader->info.vs.double_inputs||shader->info.vs.blit_sgprs_amd||shader->info.vs.tes_agx||
    shader->info.vs.window_space_position||shader->info.vs.needs_edge_flag||
    (shader->info.inputs_read&&shader->info.inputs_read!=inputs)||
    (shader->info.outputs_written&&shader->info.outputs_written!=outputs))return ENOTSUP;
 for(unsigned i=0;i<ARRAY_SIZE(shader->info.system_values_read);i++)if(shader->info.system_values_read[i])return ENOTSUP;
 for(unsigned i=0;i<ARRAY_SIZE(shader->info.xfb_stride);i++)if(shader->info.xfb_stride[i])return ENOTSUP;
 nir_foreach_function(fn,shader) {
  if(++functions>1||!fn->is_entrypoint||fn->is_preamble||fn->is_exported||
     fn->shader!=shader||fn->num_params||!fn->impl)return ENOTSUP;
  entry=fn;
 }
 if(!entry)return EINVAL;
 nir_function_impl *impl=entry->impl;
 if(impl->function!=entry||impl->preamble||!impl->end_block||
    !exec_list_is_empty(&impl->end_block->instr_list)||!impl->structured||
    !exec_list_is_empty(&impl->locals))return ENOTSUP;
 foreach_list_typed(nir_cf_node,node,node,&impl->body) {
  if(++blocks>1||node->type!=nir_cf_node_block||node->parent!=&impl->cf_node)return ENOTSUP;
  nir_block *block=nir_cf_node_as_block(node);
  nir_foreach_instr(instr,block) {
   if(++instructions>9||instr->block!=block)return ENOTSUP;
   if(instr->type==nir_instr_type_load_const) {
    if(zero)return ENOTSUP;
    zero=nir_instr_as_load_const(instr);
    if(zero->def.parent_instr!=instr||zero->def.bit_size!=32||
       zero->def.num_components!=1||zero->value[0].u32)return ENOTSUP;
   } else if(instr->type==nir_instr_type_alu) {
    nir_alu_instr *alu=nir_instr_as_alu(instr);
    if(alu->exact||alu->fp_fast_math||alu->no_signed_wrap||alu->no_unsigned_wrap||
       alu->def.parent_instr!=instr||alu->def.bit_size!=32)return ENOTSUP;
    if(alu->op==nir_op_fdot3) {
     if(dot||!input[1]||!input[2]||alu->def.num_components!=1||
        alu->src[0].src.ssa!=input[1]||alu->src[1].src.ssa!=input[2])return ENOTSUP;
     for(unsigned s=0;s<2;s++)for(unsigned c=0;c<3;c++)
      if(alu->src[s].swizzle[c]!=c)return ENOTSUP;
     dot=&alu->def;
    } else if(alu->op==nir_op_fsat) {
     if(sat||!dot||alu->def.num_components!=1||alu->src[0].src.ssa!=dot||
        alu->src[0].swizzle[0])return ENOTSUP;
     sat=&alu->def;
    } else if(alu->op==nir_op_vec4) {
     if(color||!sat||alu->def.num_components!=4)return ENOTSUP;
     for(unsigned c=0;c<4;c++)if(alu->src[c].src.ssa!=sat||alu->src[c].swizzle[0])return ENOTSUP;
     color=&alu->def;
    } else return ENOTSUP;
   } else if(instr->type==nir_instr_type_intrinsic) {
    nir_intrinsic_instr *io=nir_instr_as_intrinsic(instr);
    if(io->intrinsic==nir_intrinsic_load_input) {
     nir_io_semantics sem=nir_intrinsic_io_semantics(io),expected={.location=sem.location,.num_slots=1};
     if(!zero||sem.location<VERT_ATTRIB_GENERIC0||sem.location>VERT_ATTRIB_GENERIC2)return ENOTSUP;
     unsigned slot=sem.location-VERT_ATTRIB_GENERIC0;
     if(input[slot]||dot||io->src[0].ssa!=&zero->def||io->num_components!=4||
        io->def.parent_instr!=instr||io->def.bit_size!=32||io->def.num_components!=4||
        nir_intrinsic_dest_type(io)!=nir_type_float32||nir_intrinsic_base(io)!=slot||
        nir_intrinsic_component(io)||nir_intrinsic_range(io)>1||memcmp(&sem,&expected,sizeof(sem)))return ENOTSUP;
     input[slot]=&io->def;
    } else if(io->intrinsic==nir_intrinsic_store_output) {
     nir_io_semantics sem=nir_intrinsic_io_semantics(io),expected={.location=sem.location,.num_slots=1};
     nir_io_xfb xfb=nir_intrinsic_io_xfb(io),xfb2=nir_intrinsic_io_xfb2(io),none={0};
     unsigned flag=sem.location==VARYING_SLOT_POS?1:sem.location==VARYING_SLOT_COL0?2:0;
     nir_def *value=flag==1?input[0]:color;
     if(!flag||(stores&flag)||!value||!zero||io->src[0].ssa!=value||
        io->src[1].ssa!=&zero->def||io->num_components!=4||nir_intrinsic_write_mask(io)!=15||
        nir_intrinsic_src_type(io)!=nir_type_float32||nir_intrinsic_base(io)!=(flag==1?0u:1u)||
        nir_intrinsic_component(io)||nir_intrinsic_range(io)>1||
        memcmp(&sem,&expected,sizeof(sem))||memcmp(&xfb,&none,sizeof(xfb))||memcmp(&xfb2,&none,sizeof(xfb2)))return ENOTSUP;
     stores|=flag;
    } else return ENOTSUP;
   } else return ENOTSUP;
  }
 }
 return blocks==1&&instructions==9&&stores==3&&input[0]&&input[1]&&input[2]&&dot&&sat&&color?0:ENOTSUP;
}

/* Complete captured ff-vs profile, CPU semantics only. Keeping this separate
 * from the generic IR avoids accepting a partial POSITION/COLOR graph. */
#include "mesa/program/prog_statevars.h"
struct gears_nir_cursor { nir_instr *list[48];unsigned count,at; };
static nir_instr *gears_nir_next(struct gears_nir_cursor *c)
{return c->at<c->count?c->list[c->at++]:NULL;}
static nir_def *gears_nir_const(struct gears_nir_cursor *c,unsigned n,const uint32_t *bits)
{
 nir_instr *i=gears_nir_next(c);if(!i||i->type!=nir_instr_type_load_const)return NULL;
 nir_load_const_instr *v=nir_instr_as_load_const(i);
 if(v->def.parent_instr!=i||v->def.bit_size!=32||v->def.num_components!=n)return NULL;
 for(unsigned k=0;k<n;k++)if(v->value[k].u32!=bits[k])return NULL;
 return &v->def;
}
static nir_def *gears_nir_deref(struct gears_nir_cursor *c,nir_variable *v)
{
 nir_instr *i=gears_nir_next(c);if(!i||i->type!=nir_instr_type_deref||!v)return NULL;
 nir_deref_instr *d=nir_instr_as_deref(i);
 if(d->deref_type!=nir_deref_type_var||d->var!=v||d->modes!=v->data.mode||
    d->type!=v->type||d->def.parent_instr!=i||d->def.num_components!=1||d->def.bit_size!=32)return NULL;
 return &d->def;
}
static nir_def *gears_nir_input(struct gears_nir_cursor *c,nir_def *deref)
{
 nir_instr *i=gears_nir_next(c);if(!i||i->type!=nir_instr_type_intrinsic)return NULL;
 nir_intrinsic_instr *v=nir_instr_as_intrinsic(i);
 if(v->intrinsic!=nir_intrinsic_load_deref||!deref||v->src[0].ssa!=deref||
    v->num_components!=4||v->def.parent_instr!=i||v->def.num_components!=4||
    v->def.bit_size!=32||nir_intrinsic_access(v))return NULL;
 return &v->def;
}
static nir_def *gears_nir_uniform(struct gears_nir_cursor *c,nir_def *zero,unsigned slot,unsigned n)
{
 nir_instr *i=gears_nir_next(c);if(!i||i->type!=nir_instr_type_intrinsic)return NULL;
 nir_intrinsic_instr *v=nir_instr_as_intrinsic(i);
 if(v->intrinsic!=nir_intrinsic_load_uniform||!zero||v->src[0].ssa!=zero||
    v->num_components!=n||v->def.parent_instr!=i||v->def.num_components!=n||v->def.bit_size!=32||
    nir_intrinsic_base(v)!=slot||nir_intrinsic_range(v)!=1||nir_intrinsic_dest_type(v)!=nir_type_float32)return NULL;
 return &v->def;
}
static int gears_nir_store(struct gears_nir_cursor *c,nir_def *deref,nir_def *value,unsigned components,unsigned mask)
{
 nir_instr *i=gears_nir_next(c);if(!i||i->type!=nir_instr_type_intrinsic)return 0;
 nir_intrinsic_instr *v=nir_instr_as_intrinsic(i);
 return v->intrinsic==nir_intrinsic_store_deref&&deref&&value&&v->src[0].ssa==deref&&
  v->src[1].ssa==value&&v->num_components==components&&nir_intrinsic_write_mask(v)==mask&&!nir_intrinsic_access(v);
}
static nir_def *gears_nir_alu(struct gears_nir_cursor *c,nir_op op,unsigned n,
 nir_def *a,const char *sa,nir_def *b,const char *sb)
{
 nir_instr *i=gears_nir_next(c);if(!i||i->type!=nir_instr_type_alu||!a)return NULL;
 nir_alu_instr *v=nir_instr_as_alu(i);
 if(v->op!=op||v->def.parent_instr!=i||v->def.num_components!=n||v->def.bit_size!=32||
    v->exact||v->fp_fast_math||v->no_signed_wrap||v->no_unsigned_wrap||v->src[0].src.ssa!=a)return NULL;
 /* This exact profile uses only these named public NIR operators. Keep the
  * selected arities here so host-only builds need no generated opcode table. */
 unsigned arity;
 switch(op) {
 case nir_op_mov:case nir_op_frsq:arity=1;break;
 case nir_op_fmul:case nir_op_fadd:case nir_op_fdot3:case nir_op_fmax:case nir_op_slt:arity=2;break;
 default:return NULL;
 }
 if(arity!=(b?2u:1u))return NULL;
 for(unsigned j=0;j<arity;j++) {
  nir_def *src=j?b:a;const char *sw=j?sb:sa;
  unsigned used=op==nir_op_fdot3?3u:n;
  if(!sw||strlen(sw)!=used||v->src[j].src.ssa!=src)return NULL;
  for(unsigned k=0;k<used;k++)if(sw[k]<'0'||sw[k]>'3'||(unsigned)(sw[k]-'0')>=src->num_components||v->src[j].swizzle[k]!=(unsigned)(sw[k]-'0'))return NULL;
 }
 return &v->def;
}
static int gears_nir_variables(const nir_shader *s,bool attr,nir_variable **input,nir_variable **output)
{
 const unsigned uniforms=attr?9:10;unsigned seen=0,ins=0,outs=0;
 nir_foreach_variable_in_shader(v,s) {
  if(!v->type||v->num_members||v->members||v->interface_type||v->constant_initializer||v->pointer_initializer||
     v->data.index||v->data.location_frac||v->data.patch||v->data.per_view||v->data.per_primitive||
     v->data.compact||v->data.centroid||v->data.sample||v->data.fb_fetch_output||v->data.is_xfb||
     v->data.is_xfb_only||v->data.stream||v->data.access||v->data.bindless||v->data.explicit_binding||
     v->data.binding||v->data.descriptor_set||v->data.explicit_xfb_buffer||v->data.explicit_xfb_stride||
     v->data.explicit_offset||v->data.matrix_layout||v->data.from_named_ifc_block||v->data.from_ssbo_unsized_array||
     v->data.offset||v->data.ray_query||v->data.has_initializer||v->data.is_implicit_initializer||
     (v->data.precision!=GLSL_PRECISION_NONE&&v->data.precision!=GLSL_PRECISION_HIGH)||
     glsl_get_base_type(v->type)!=GLSL_TYPE_FLOAT)return 0;
  unsigned n=4;
  if(v->data.mode==nir_var_uniform) {
   unsigned slot=v->data.driver_location;
   if(slot>=uniforms||(seen&(1u<<slot))||v->data.location||v->data.interpolation||
      v->num_state_slots!=1||!v->state_slots)return 0;
   gl_state_index16 tokens[STATE_LENGTH]={0};
   if(slot==0)tokens[0]=STATE_LIGHTMODEL_SCENECOLOR;
   else if(slot<4){tokens[0]=STATE_LIGHTPROD;tokens[2]=(slot-1)*2;}
   else if(slot<8){tokens[0]=STATE_MVP_MATRIX_TRANSPOSE;tokens[2]=tokens[3]=slot-4;}
   else if(!attr&&slot==8){tokens[0]=STATE_CURRENT_ATTRIB;tokens[1]=1;n=3;}
   else {tokens[0]=STATE_LIGHT_POSITION_NORMALIZED;n=3;}
   if(memcmp(tokens,v->state_slots[0].tokens,sizeof(tokens)))return 0;
   seen|=1u<<slot;
  } else {
   if(v->num_state_slots||v->state_slots)return 0;
   if(v->data.mode==nir_var_shader_in) {
    unsigned k=v->data.location==VERT_ATTRIB_POS?0:v->data.location==VERT_ATTRIB_NORMAL?1:2;
    if(k>1||(!attr&&k)||input[k]||v->data.driver_location!=k||v->data.interpolation)return 0;
    input[k]=v;ins++;
   } else if(v->data.mode==nir_var_shader_out) {
    unsigned k=v->data.location==VARYING_SLOT_POS?0:v->data.location==VARYING_SLOT_COL0?1:v->data.location==VARYING_SLOT_PSIZ?2:3;
    if(k>2||output[k]||v->data.driver_location!=k||v->data.read_only||
       v->data.interpolation!=(k==2?INTERP_MODE_SMOOTH:INTERP_MODE_NONE))return 0;
    output[k]=v;outs++;if(k==2)n=1;
   } else return 0;
  }
  if(v->type->matrix_columns!=1||glsl_get_vector_elements(v->type)!=n)return 0;
 }
 return seen==(1u<<uniforms)-1&&ins==(attr?2u:1u)&&outs==3;
}
int remesa_nir_gears_vertex(const nir_shader *s,struct remesa_nir_gears_vertex_profile *out)
{
 if(!s||!out)return EINVAL;
 if((uintptr_t)out%_Alignof(struct remesa_nir_gears_vertex_profile)||
    sizeof(*out)>UINTPTR_MAX-(uintptr_t)out)return EINVAL;
 if((s->num_uniforms!=9&&s->num_uniforms!=10)||s->info.stage!=MESA_SHADER_VERTEX||s->info.io_lowered||
    s->global_mem_size||s->scratch_size||s->constant_data_size||s->xfb_info||s->info.float_controls_execution_mode||
    s->info.writes_memory||s->info.num_textures||s->info.num_ubos||s->info.num_abos||s->info.num_ssbos||s->info.num_images||
    s->info.dual_slot_inputs||s->info.outputs_read||s->info.per_primitive_inputs||s->info.per_primitive_outputs||
    s->info.clip_distance_array_size||s->info.cull_distance_array_size||s->info.vs.double_inputs||
    s->info.vs.blit_sgprs_amd||s->info.vs.tes_agx||s->info.vs.window_space_position||s->info.vs.needs_edge_flag)return ENOTSUP;
 bool attr=s->num_uniforms==9;nir_variable *input[2]={0},*output[3]={0};
 const uint64_t inputs=BITFIELD64_BIT(VERT_ATTRIB_POS)|(attr?BITFIELD64_BIT(VERT_ATTRIB_NORMAL):0);
 const uint64_t outputs=BITFIELD64_BIT(VARYING_SLOT_POS)|BITFIELD64_BIT(VARYING_SLOT_COL0)|BITFIELD64_BIT(VARYING_SLOT_PSIZ);
 if(s->num_inputs!=(attr?2u:1u)||s->num_outputs!=3||s->info.inputs_read!=inputs||s->info.outputs_written!=outputs||
    !gears_nir_variables(s,attr,input,output))return ENOTSUP;
 for(unsigned k=0;k<ARRAY_SIZE(s->info.system_values_read);k++)if(s->info.system_values_read[k])return ENOTSUP;
 for(unsigned k=0;k<ARRAY_SIZE(s->info.xfb_stride);k++)if(s->info.xfb_stride[k])return ENOTSUP;
 nir_function *entry=NULL;unsigned functions=0,blocks=0;struct gears_nir_cursor c={0};
 nir_foreach_function(f,s) {
  if(++functions!=1||!f->is_entrypoint||f->is_preamble||f->is_exported||f->shader!=s||f->num_params||!f->impl)return ENOTSUP;
  entry=f;
 }
 if(!entry)return EINVAL;
 nir_function_impl *impl=entry->impl;
 if(impl->function!=entry||impl->preamble||!impl->end_block||!exec_list_is_empty(&impl->end_block->instr_list)||
    !impl->structured||!exec_list_is_empty(&impl->locals))return ENOTSUP;
 foreach_list_typed(nir_cf_node,node,node,&impl->body) {
  if(++blocks!=1||node->type!=nir_cf_node_block||node->parent!=&impl->cf_node)return ENOTSUP;
  nir_block *block=nir_cf_node_as_block(node);
  nir_foreach_instr(i,block){if(c.count==ARRAY_SIZE(c.list)||i->block!=block)return ENOTSUP;c.list[c.count++]=i;}
 }
 if(blocks!=1)return ENOTSUP;
 nir_def *ps,*one,*zero,*u[4],*pd,*p,*x,*y,*acc,*nd,*normal,*len,*rsq,*nv,*scene,*alpha,*co,*ambient,*diffuse,*spec,*light,*dp,*bounds,*clamp,*positive,*sum,*product;
 const uint32_t onebits[]={0x3f800000},zerobits[]={0},maxbits[]={0,0,0,0x3f800000};
#define GN(dst,expression) do{dst=(expression);if(!dst)return ENOTSUP;}while(0)
#define GA(dst,op,n,a,sa,b,sb) GN(dst,gears_nir_alu(&c,nir_op_##op,n,a,sa,b,sb))
 GN(ps,gears_nir_deref(&c,output[2]));GN(one,gears_nir_const(&c,1,onebits));
 if(!gears_nir_store(&c,ps,one,1,1))return ENOTSUP;
 GN(zero,gears_nir_const(&c,1,zerobits));
 for(unsigned k=0;k<4;k++)GN(u[k],gears_nir_uniform(&c,zero,4+k,4));
 GN(pd,gears_nir_deref(&c,input[0]));GN(p,gears_nir_input(&c,pd));
 GA(x,fmul,4,u[0],"0123",p,"0000");GA(y,fmul,4,u[1],"0123",p,"1111");GA(acc,fadd,4,y,"0123",x,"0123");
 GA(x,fmul,4,u[2],"0123",p,"2222");GA(acc,fadd,4,x,"0123",acc,"0123");
 GA(x,fmul,4,u[3],"0123",p,"3333");GA(acc,fadd,4,x,"0123",acc,"0123");
 GN(pd,gears_nir_deref(&c,output[0]));if(!gears_nir_store(&c,pd,acc,4,15))return ENOTSUP;
 if(attr){GN(nd,gears_nir_deref(&c,input[1]));GN(normal,gears_nir_input(&c,nd));}
 else GN(normal,gears_nir_uniform(&c,zero,8,3));
 GA(len,fdot3,1,normal,"012",normal,"012");GA(rsq,frsq,1,len,"0",NULL,NULL);
 GA(nv,fmul,3,normal,"012",rsq,"000");GN(scene,gears_nir_uniform(&c,zero,0,4));
 GA(alpha,mov,4,scene,"3333",NULL,NULL);GN(co,gears_nir_deref(&c,output[1]));
 if(!gears_nir_store(&c,co,alpha,4,8))return ENOTSUP;
 GN(ambient,gears_nir_uniform(&c,zero,1,4));GN(diffuse,gears_nir_uniform(&c,zero,2,4));GN(spec,gears_nir_uniform(&c,zero,3,4));
 GN(light,gears_nir_uniform(&c,zero,attr?8:9,3));GA(dp,fdot3,1,nv,"012",light,"012");
 GN(bounds,gears_nir_const(&c,4,maxbits));GA(clamp,fmax,4,bounds,"0123",dp,"0000");GA(positive,slt,1,zero,"0",dp,"0");
 GA(sum,fadd,4,ambient,"0123",scene,"0123");GA(product,fmul,4,clamp,"1111",diffuse,"0123");
 GA(sum,fadd,4,product,"0123",sum,"0123");GA(product,fmul,4,positive,"0000",spec,"0123");GA(sum,fadd,4,product,"0123",sum,"0123");
 if(!gears_nir_store(&c,co,sum,4,7)||c.at!=c.count)return ENOTSUP;
#undef GA
#undef GN
 const struct remesa_nir_gears_vertex_profile result={attr,attr?9u:10u,attr?37u:41u,attr?2u:1u,8,11,0x3f800000};
 memcpy(out,&result,sizeof(result));return 0;
}
