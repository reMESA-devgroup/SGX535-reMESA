/* SPDX-License-Identifier: MIT. CPU semantic/profile checks, ZERO GPU. */
#include "gears_nir_fixture.h"
#include "remesa_nir.h"
#include "remesa_usse_fragment_color.h"
int inherited_flat_profile_driver_main(void);
#define main inherited_flat_profile_driver_main
#include "test_driver.c"
#undef main
static nir_shader *flat_fixture(unsigned color,unsigned interpolation){
 nir_shader*s=rzalloc(NULL,nir_shader);struct gf_builder*f=rzalloc(s,struct gf_builder);f->s=s;
 f->types[2].base_type=GLSL_TYPE_FLOAT;f->types[2].vector_elements=4;f->types[2].matrix_columns=1;
 exec_list_make_empty(&s->variables);exec_list_make_empty(&s->functions);s->info.stage=MESA_SHADER_FRAGMENT;s->num_inputs=s->num_outputs=1;
 nir_function*fn=rzalloc(s,nir_function);fn->shader=s;fn->is_entrypoint=true;exec_list_push_tail(&s->functions,&fn->node);
 nir_function_impl*impl=rzalloc(s,nir_function_impl);fn->impl=impl;impl->function=fn;impl->structured=true;exec_list_make_empty(&impl->body);exec_list_make_empty(&impl->locals);impl->end_block=rzalloc(s,nir_block);exec_list_make_empty(&impl->end_block->instr_list);
 f->b=rzalloc(s,nir_block);f->b->cf_node.type=nir_cf_node_block;f->b->cf_node.parent=&impl->cf_node;exec_list_push_tail(&impl->body,&f->b->cf_node.node);exec_list_make_empty(&f->b->instr_list);
 nir_variable*in=gf_var(f,nir_var_shader_in,4,color?VARYING_SLOT_COL1:VARYING_SLOT_COL0,0);
 nir_variable*out=gf_var(f,nir_var_shader_out,4,FRAG_RESULT_DATA0,0);in->data.interpolation=interpolation;
 nir_def*v=gf_input(f,in),*d=gf_deref(f,out);gf_store(f,d,v,4,15);return s;
}
static nir_variable *variable(nir_shader*s,bool output){return exec_node_data(nir_variable,output?exec_list_get_tail(&s->variables):exec_list_get_head(&s->variables),node);}
static struct remesa_shader_ir canonical(void){
 struct remesa_shader_ir ir={.schema=REMESA_SHADER_IR_SCHEMA,.stage=REMESA_IR_FRAGMENT,.count=1};
 ir.instructions[0]=(struct remesa_ir_instruction){.op=REMESA_IR_FLAT_COLOR_INPUT,.dst=REMESA_SHADER_IR_NO_REGISTER,.components=4,.type=REMESA_IR_FLOAT32,.input_slot=1};return ir;
}
int main(void){
 unsigned fixtures=0,mutations=0;
 for(unsigned color=0;color<2;color++)for(unsigned mode=0;mode<12;mode++){
  nir_shader*s=flat_fixture(color,mode==1?INTERP_MODE_NONE:mode==2?INTERP_MODE_SMOOTH:INTERP_MODE_FLAT);
  if(mode==3)variable(s,true)->data.interpolation=INTERP_MODE_FLAT;
  if(mode==4)variable(s,false)->data.interpolation=INTERP_MODE_NOPERSPECTIVE;
  if(mode==5)variable(s,false)->data.centroid=1;
  if(mode==6)variable(s,false)->data.precision=GLSL_PRECISION_MEDIUM;
  if(mode==7)s->info.fs.uses_discard=true;
  if(mode==8)s->info.io_lowered=true;
  if(mode==9)variable(s,false)->data.location=VARYING_SLOT_TEX0;
  if(mode==10)s->info.num_textures=1;
  if(mode==11)variable(s,true)->data.read_only=true;
  unsigned slot=777,flat=888;int e=remesa_nir_fragment_color_profile(s,&slot,&flat);
  unsigned smooth_slot=999;int old=remesa_nir_fragment_color_input(s,&smooth_slot);
  struct remesa_shader_ir ir,saved;memset(&ir,0xa5,sizeof(ir));saved=ir;int lower=remesa_shader_ir_lower(s,&ir);
  if(mode<3){
   assert(!e&&slot==color+1&&flat==(mode==0));
   if(!mode)assert(old==ENOTSUP&&smooth_slot==999);else assert(!old&&smooth_slot==color+1);
   if(!color){assert(!lower);assert(!mode?!remesa_shader_ir_fragment_flat_color_input(&ir):!remesa_shader_ir_fragment_color_input(&ir));}
   else assert(lower==ENOTSUP&&!memcmp(&ir,&saved,sizeof(ir)));
  }else assert(e==ENOTSUP&&slot==777&&flat==888&&old==ENOTSUP&&smooth_slot==999&&lower==ENOTSUP&&!memcmp(&ir,&saved,sizeof(ir)));
  ralloc_free(s);fixtures++;
 }
 struct remesa_shader_ir ir=canonical();assert(!remesa_shader_ir_validate(&ir)&&!remesa_shader_ir_fragment_flat_color_input(&ir)&&remesa_shader_ir_fragment_color_input(&ir)==ENOTSUP);
 for(unsigned word=0;word<sizeof(ir)/4;word++)for(unsigned which=0;which<2;which++){
  struct remesa_shader_ir bad=ir;uint32_t value;memcpy(&value,(unsigned char*)&bad+word*4,4);value^=which?0x80000000u:1u;memcpy((unsigned char*)&bad+word*4,&value,4);
  assert(remesa_shader_ir_validate(&bad)==ENOTSUP&&remesa_shader_ir_fragment_flat_color_input(&bad)==ENOTSUP);
  struct remesa_shader_program program,saved;memset(&program,0xa5,sizeof(program));saved=program;assert(remesa_shader_program_prepare(&bad,&program)==ENOTSUP&&!memcmp(&program,&saved,sizeof(program)));mutations++;
 }
 struct remesa_shader_program p;assert(!remesa_shader_program_prepare(&ir,&p)&&!remesa_shader_program_validate(&p));
 struct remesa_usse_program scalar,scalar_saved;memset(&scalar,0xa5,sizeof(scalar));scalar_saved=scalar;
 assert(remesa_usse_lower(&ir,&scalar)==ENOTSUP&&!memcmp(&scalar,&scalar_saved,sizeof(scalar)));
 for(unsigned attr=0;attr<2;attr++){
  nir_shader*s=gears_nir_fixture(attr);struct remesa_shader_ir gears;assert(!remesa_shader_ir_lower(s,&gears));
  assert(remesa_usse_lower(&gears,&scalar)==ENOTSUP&&!memcmp(&scalar,&scalar_saved,sizeof(scalar)));ralloc_free(s);
 }
 struct remesa_shader_program_report report;assert(!remesa_shader_program_report(&p,&report)&&report.status==REMESA_PROGRAM_COMPILE_OFFLINE&&!report.binary_error&&report.binary_bytes==8&&report.launch_error==ENOTSUP);
 /* Original COL0 FS oracle from the preserved independent writer replay. */
 const unsigned char oracle[8]={0,0,0,0xa0,1,0x10,0x85,0x28};unsigned char bytes[16],saved[16];memset(bytes,0xa5,16);memcpy(saved,bytes,16);size_t written=777;
 assert(remesa_shader_program_encode(&p,bytes,7,&written)==ENOSPC&&written==777&&!memcmp(bytes,saved,16));
 assert(remesa_shader_ir_encode_sgx535(&ir,bytes,16,&written)==ENOTSUP&&written==777&&!memcmp(bytes,saved,16));
 assert(!remesa_shader_program_encode(&p,bytes,16,&written)&&written==8&&!memcmp(bytes,oracle,8)&&!memcmp(bytes+8,saved+8,8));
 struct remesa_shader_selected_resources resources,resources_saved;memset(&resources,0xa5,sizeof(resources));resources_saved=resources;
 assert(remesa_shader_program_selected_resources(&p,&resources)==ENOTSUP&&!memcmp(&resources,&resources_saved,sizeof(resources)));
 struct remesa_usse_fragment_color_extent extent,extent_saved;memset(&extent,0xa5,sizeof(extent));extent_saved=extent;assert(remesa_shader_program_fragment_color_extent(&p,&extent)==ENOTSUP&&!memcmp(&extent,&extent_saved,sizeof(extent)));
 struct remesa_pds_request request={0};struct remesa_pds_plan plan;assert(remesa_shader_program_prepare_launch(&p,&request,&plan)==ENOTSUP);
 char dump[4096];size_t needed;assert(!remesa_shader_ir_dump(&ir,dump,sizeof(dump),&needed)&&strstr(dump,"flat-color-input"));
 struct recorder rec={0};struct remesa_device dev={.ops=&ops,.priv=&rec,.offline_test_backend=true};struct pipe_screen*screen=remesa_screen_create(&dev);assert(screen);
 for(unsigned mode=0;mode<3;mode++){
  struct pipe_context*c=screen->context_create(screen,NULL,0);struct pipe_shader_state state={.type=PIPE_SHADER_IR_NIR,.ir.nir=flat_fixture(0,mode==0?INTERP_MODE_FLAT:mode==1?INTERP_MODE_SMOOTH:INTERP_MODE_NONE)};
  void*fs=c->create_fs_state(c,&state);assert(fs&&!remesa_context_error(c));c->bind_fs_state(c,fs);assert(!remesa_context_error(c));
  assert(!remesa_context_shader_program_report(c,PIPE_SHADER_FRAGMENT,fs,&report)&&report.binary_bytes==8&&report.launch_error==ENOTSUP);
  c->delete_fs_state(c,fs);c->destroy(c);
 }
 screen->destroy(screen);assert(rec.allocs==rec.frees&&rec.maps==rec.unmaps&&!rec.draws&&!rec.shader_prepares&&!rec.shader_submits);
 printf("PASS %u NIR profile fixtures +%u complete IR mutations; distinct FLAT semantic, original FS8 bytes, smooth-only query/extent/resource/launch gates preserved, actual callbacks; ZERO GPU\n",fixtures,mutations);return 0;
}
