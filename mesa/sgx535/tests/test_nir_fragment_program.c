/* SPDX-License-Identifier: MIT: authored NIR -> concrete offline FS records. */
#include "remesa_nir.h"
#include "remesa_shader_program.h"
#include "remesa_shader_storage.h"
#include "remesa_screen.h"
#include "remesa_device.h"
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"
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
static nir_shader *make(unsigned mode)
{
 static const nir_shader_compiler_options options={0};
 nir_builder b=nir_builder_init_simple_shader(MESA_SHADER_FRAGMENT,&options,"strict COLOR0 offline program");
 nir_variable *in=nir_variable_create(b.shader,nir_var_shader_in,mode==13?glsl_array_type(glsl_vec4_type(),2,0):glsl_vec4_type(),"color_in");
 nir_variable *out=nir_variable_create(b.shader,nir_var_shader_out,glsl_vec4_type(),"color_out");
 in->data.location=mode==3?VARYING_SLOT_COL1:mode==4?VARYING_SLOT_TEX0:VARYING_SLOT_COL0;
 out->data.location=mode==1?FRAG_RESULT_COLOR:mode==12?FRAG_RESULT_DEPTH:FRAG_RESULT_DATA0;
 nir_deref_instr *d=nir_build_deref_var(&b,in);
 if(mode==13)d=nir_build_deref_array_imm(&b,d,0);
 nir_def *value=nir_load_deref(&b,d);
 if(mode==5)value=nir_mov(&b,value);
 if(mode==6)value=nir_swizzle(&b,value,(unsigned[]){2,1,0,3},4);
 if(mode==7)value=nir_fadd(&b,value,nir_imm_vec4(&b,0,0,0,0));
 if(mode==17)nir_push_if(&b,nir_imm_bool(&b,true));
 nir_store_deref(&b,nir_build_deref_var(&b,out),value,mode==8?7:15);
 if(mode==17)nir_pop_if(&b,NULL);
 if(mode==18)(void)nir_mov(&b,value);
 if(mode!=12)nir_validate_shader(b.shader,"strict fragment program test");
 if(mode==2){in->data.interpolation=INTERP_MODE_SMOOTH;b.shader->num_inputs=1;b.shader->num_outputs=1;b.shader->info.inputs_read=BITFIELD64_BIT(VARYING_SLOT_COL0);b.shader->info.outputs_written=BITFIELD64_BIT(FRAG_RESULT_DATA0);}
 if(mode==9)in->data.interpolation=INTERP_MODE_FLAT;
 if(mode==10)in->data.sample=true;
 if(mode==11)b.shader->info.io_lowered=true;
 if(mode==14)b.shader->info.num_ubos=1;
 if(mode==15)b.shader->info.fs.uses_discard=true;
 if(mode==16)b.shader->info.float_controls_execution_mode=1;
 return b.shader;
}
static nir_shader *make_vertex(void)
{
 static const nir_shader_compiler_options options={0};
 nir_builder b=nir_builder_init_simple_shader(MESA_SHADER_VERTEX,&options,"old POSITION profile");b.shader->info.io_lowered=true;
 nir_def *zero=nir_imm_int(&b,0),*p=nir_load_input(&b,4,32,zero,.dest_type=nir_type_float32,.io_semantics={.location=VERT_ATTRIB_GENERIC0,.num_slots=1});
 nir_store_output(&b,p,zero,.write_mask=15,.src_type=nir_type_float32,.io_semantics={.location=VARYING_SLOT_POS,.num_slots=1});
 nir_validate_shader(b.shader,"POSITION pair rejection control");return b.shader;
}
static int alloc(void *p,size_t n,void **out){(void)p;(void)n;(void)out;assert(!"no BO allocation");return ENOTSUP;}
static void release(void *p,void *o){(void)p;(void)o;}
static int map(void *p,void *o,bool w,void **out){(void)p;(void)o;(void)w;(void)out;assert(!"no map");return ENOTSUP;}
static void unmap(void *p,void *o,bool w){(void)p;(void)o;(void)w;}
static int submit(void *p,void *o,const struct remesa_draw_packet *q,void **f){(void)p;(void)o;(void)q;(void)f;assert(!"no submit");return ENOTSUP;}
static int wait(void *p,void *o,uint64_t n,bool v){(void)p;(void)o;(void)n;(void)v;return ENOTSUP;}
int main(int argc,char **argv)
{
 glsl_type_singleton_init_or_ref();unsigned checks=0;
 struct remesa_shader_ir canonical={0};struct remesa_shader_program fs={0};
 unsigned char reference[8];struct remesa_usse_fragment_color_extent direct;
 const struct remesa_usse_fragment_color_request req={1,0};
 assert(!remesa_usse_fragment_color_encode(&req,reference,8,&direct));
 assert(argc==1||argc==3);
 for(unsigned mode=0;mode<19;mode++) {
  nir_shader *nir=make(mode);struct remesa_shader_ir ir,before;memset(&ir,0xa5,sizeof(ir));before=ir;
  if(mode==3){unsigned slot=999;assert(!remesa_nir_fragment_color_input(nir,&slot)&&slot==2);}
  int e=remesa_shader_ir_lower(nir,&ir);ralloc_free(nir);
  if(mode>2){assert(e==ENOTSUP&&!memcmp(&ir,&before,sizeof(ir)));checks++;continue;}
  assert(!e&&!remesa_shader_ir_fragment_color_input(&ir)&&!remesa_shader_ir_validate(&ir));
  if(!mode)canonical=ir;else assert(!memcmp(&ir,&canonical,sizeof(ir)));
  uint32_t constant=123;assert(remesa_shader_ir_fragment_color(&ir,&constant)==ENOTSUP&&constant==123);
  assert(!remesa_shader_program_prepare(&ir,&fs)&&!remesa_shader_program_validate(&fs));
  struct remesa_shader_program_report report;assert(!remesa_shader_program_report(&fs,&report));
  assert(report.stage==REMESA_IR_FRAGMENT&&report.status==REMESA_PROGRAM_COMPILE_OFFLINE&&report.virtual_instructions==2&&report.binary_bytes==8&&report.launch_error==ENOTSUP);
  struct remesa_usse_fragment_color_extent extent;assert(!remesa_shader_program_fragment_color_extent(&fs,&extent));
  assert(!memcmp(&extent,&direct,sizeof(extent))&&extent.logical_bytes==8&&extent.allocation_bytes==16&&extent.alignment_bytes==32&&extent.r_id==10&&extent.r_components==4&&extent.r_mode==3&&!extent.r_flag);
  unsigned char bytes[16];memset(bytes,0xa5,16);size_t written=99;
  assert(remesa_shader_program_encode(&fs,bytes,7,&written)==ENOSPC&&written==99&&bytes[0]==0xa5);
  assert(!remesa_shader_program_encode(&fs,bytes,16,&written)&&written==8&&!memcmp(bytes,reference,8));for(unsigned i=8;i<16;i++)assert(bytes[i]==0xa5);
  struct remesa_pds_structure_request pr={.entry_count=1,.code_policy=REMESA_PDS_STRUCTURE_CODE_CPU_COMPLETE};
  pr.entries[0]=(struct remesa_pds_structure_entry){.kind=REMESA_PDS_STRUCTURE_R,.id=extent.r_id,
   .components=extent.r_components,.mode=extent.r_mode,.flag_r=extent.r_flag,.format_present=1,.explicit_format=0x0c000000};
  struct remesa_pds_structure ps;assert(!remesa_pds_structure_encode(&pr,&ps));
  unsigned char pds[60],mask[60];size_t pds_written=0;
  assert(!remesa_pds_structure_serialize(&ps,pds,mask,60,&pds_written)&&pds_written==60&&ps.data_dwords==12&&ps.code_slot_count==3);
  if(!mode&&argc==3){FILE *f=fopen(argv[1],"wb");assert(f&&fwrite(bytes,1,8,f)==8&&!fclose(f));f=fopen(argv[2],"wb");assert(f&&fwrite(pds,1,60,f)==60&&fwrite(mask,1,60,f)==60&&!fclose(f));}
  struct remesa_shader_storage *storage=NULL;struct remesa_shader_storage_request sr={REMESA_IR_FRAGMENT,REMESA_SHADER_STORAGE_HOST_CPU_RECORDS,bytes,8,32};
  assert(!remesa_shader_storage_create(&sr,&storage));memset(bytes,0,16);
  struct remesa_shader_storage_view view;assert(!remesa_shader_storage_inspect(storage,&view)&&view.byte_length==8&&view.byte_alignment==32&&!memcmp(view.bytes,reference,8));
  assert(remesa_shader_storage_validate_execution(storage)==ENOTSUP);assert(!remesa_shader_storage_reference(&storage,NULL));
  struct remesa_shader_ir vir={.schema=1,.stage=REMESA_IR_VERTEX,.count=2};
  vir.instructions[0]=(struct remesa_ir_instruction){.op=REMESA_IR_INPUT,.components=4,.type=REMESA_IR_FLOAT32};
  vir.instructions[1]=(struct remesa_ir_instruction){.op=REMESA_IR_POSITION,.dst=REMESA_SHADER_IR_NO_REGISTER,.components=4,.type=REMESA_IR_FLOAT32};
  struct remesa_shader_program vs;assert(!remesa_shader_program_prepare(&vir,&vs));
  struct remesa_shader_pipeline pair,old;memset(&pair,0xa5,sizeof(pair));old=pair;
  assert(remesa_shader_pipeline_prepare(&vs,&fs,&pair)==ENOTSUP&&!memcmp(&pair,&old,sizeof(pair)));
  struct remesa_usse_program absent,absent_before;memset(&absent,0xa5,sizeof(absent));absent_before=absent;
  assert(remesa_usse_lower(&ir,&absent)==ENOTSUP&&!memcmp(&absent,&absent_before,sizeof(absent)));
  struct remesa_pds_plan plan,plan_before;memset(&plan,0xa5,sizeof(plan));plan_before=plan;
  assert(remesa_shader_program_prepare_launch(&fs,NULL,&plan)==ENOTSUP&&!memcmp(&plan,&plan_before,sizeof(plan)));
  struct remesa_shader_program before_program=fs;
  assert(remesa_shader_program_encode(&fs,&fs,sizeof(fs),&written)==EINVAL&&!memcmp(&fs,&before_program,sizeof(fs)));
  assert(remesa_shader_program_fragment_color_extent(&fs,(void *)&fs)==EINVAL&&!memcmp(&fs,&before_program,sizeof(fs)));
  struct remesa_shader_program bad=fs;bad.usse.schema=1;assert(remesa_shader_program_validate(&bad)==EPROTO);checks++;
 }
 for(unsigned i=0;i<20;i++){struct remesa_shader_ir ir=canonical;ir.instructions[i].bits[0]=1;assert(remesa_shader_ir_validate(&ir)==ENOTSUP);checks++;}
 static const struct remesa_device_ops ops={.allocate=alloc,.destroy_bo=release,.map=map,.unmap=unmap,.submit=submit,.wait=wait,.destroy_fence=release};
 struct remesa_device dev={.ops=&ops,.offline_test_backend=true};struct pipe_screen *screen=remesa_screen_create(&dev);assert(screen);
 struct pipe_context *ctx=screen->context_create(screen,NULL,0),*other=screen->context_create(screen,NULL,0);assert(ctx&&other);
 struct pipe_shader_state state={.type=PIPE_SHADER_IR_NIR,.ir.nir=make(0)};void *handle=ctx->create_fs_state(ctx,&state);assert(handle);
 ctx->bind_fs_state(ctx,handle);assert(!remesa_context_error(ctx));checks++;
 struct remesa_shader_program_report report;assert(!remesa_context_shader_program_report(ctx,PIPE_SHADER_FRAGMENT,handle,&report)&&report.status==REMESA_PROGRAM_COMPILE_OFFLINE&&report.binary_bytes==8&&report.launch_error==ENOTSUP);
 unsigned char bytes[16];size_t written=99;assert(!remesa_context_shader_program_encode(ctx,PIPE_SHADER_FRAGMENT,handle,bytes,16,&written)&&written==8&&!memcmp(bytes,reference,8));checks++;
 state.ir.nir=make_vertex();void *vs=ctx->create_vs_state(ctx,&state);assert(vs);ctx->bind_vs_state(ctx,vs);
 struct remesa_shader_pipeline pair,pair_before;memset(&pair,0xa5,sizeof(pair));pair_before=pair;
 assert(remesa_context_shader_pipeline_report(ctx,&pair)==ENOTSUP&&!memcmp(&pair,&pair_before,sizeof(pair)));checks++;
 ctx->delete_vs_state(ctx,vs);
 other->bind_fs_state(other,handle);assert(remesa_context_error(other)==EINVAL);checks++;
 state.ir.nir=make(3);assert(!ctx->create_fs_state(ctx,&state)&&remesa_context_error(ctx)==ENOTSUP);checks++;
 assert(!remesa_context_shader_program_encode(ctx,PIPE_SHADER_FRAGMENT,handle,bytes,16,&written)&&written==8&&!memcmp(bytes,reference,8));checks++;
 ctx->delete_fs_state(ctx,handle);assert(remesa_context_shader_program_report(ctx,PIPE_SHADER_FRAGMENT,handle,&report)==EINVAL);checks++;
 ctx->bind_fs_state(ctx,handle);assert(remesa_context_error(ctx)==EINVAL);checks++;
 state.ir.nir=make(0);void *next=ctx->create_fs_state(ctx,&state);assert(next&&next!=handle);checks++;
 assert(remesa_context_shader_program_encode(ctx,PIPE_SHADER_FRAGMENT,handle,bytes,16,&written)==EINVAL);checks++;
 ctx->delete_fs_state(ctx,next);ctx->destroy(ctx);other->destroy(other);screen->destroy(screen);
 glsl_type_singleton_decref();printf("PASS %u real NIR fragment/codegen/storage/Gallium cases; COMPILE_OFFLINE only\n",checks);return 0;
}
