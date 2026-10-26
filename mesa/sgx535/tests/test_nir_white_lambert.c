/* SPDX-License-Identifier: MIT: authored NIR, concrete offline encoding only. */
#include "remesa_nir.h"
#include "remesa_shader_program.h"
#include "remesa_shader_storage.h"
#include "remesa_usse_arithmetic.h"
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
uint32_t nir_debug=NIR_DEBUG_EXTENDED_VALIDATION;
static nir_shader *make(unsigned mode)
{
 static const nir_shader_compiler_options options={0};
 nir_builder b=nir_builder_init_simple_shader(MESA_SHADER_VERTEX,&options,"strict white Lambert offline");
 b.shader->info.io_lowered=true;nir_def *z=nir_imm_int(&b,0),*v[3];
 for(unsigned j=0;j<3;j++){unsigned i=mode==16?2-j:j;v[i]=nir_load_input(&b,mode==10?3:4,32,z,.base=i,.dest_type=nir_type_float32,.io_semantics={.location=VERT_ATTRIB_GENERIC0+i+(mode==1&&i==2?2:0),.num_slots=1});}
 if(mode==15){b.shader->num_inputs=3;b.shader->num_outputs=2;b.shader->info.inputs_read=BITFIELD64_BIT(VERT_ATTRIB_GENERIC0)|BITFIELD64_BIT(VERT_ATTRIB_GENERIC1)|BITFIELD64_BIT(VERT_ATTRIB_GENERIC2);b.shader->info.outputs_written=BITFIELD64_BIT(VARYING_SLOT_POS)|BITFIELD64_BIT(VARYING_SLOT_COL0);}
 nir_def *dot=mode==3?nir_fdot4(&b,v[1],v[2]):nir_fdot3(&b,v[1],mode==2?v[1]:v[2]);
 if(mode==4)nir_instr_as_alu(dot->parent_instr)->src[0].swizzle[0]=1;
 if(mode==11)nir_instr_as_alu(dot->parent_instr)->exact=true;
 nir_def *sat=mode==5?dot:nir_fsat(&b,dot),*color=nir_vec4(&b,sat,sat,sat,sat);
 if(mode==6)(void)nir_mov(&b,color);
 if(mode==7)b.shader->info.num_ubos=1;
 if(mode==12)b.shader->info.float_controls_execution_mode=1;
 if(mode==13)nir_push_if(&b,nir_imm_bool(&b,true));
 for(unsigned j=0;j<2;j++) {
  unsigned i=mode==14?1-j:j;
  nir_store_output(&b,i?color:v[0],z,.base=i,.write_mask=mode==9?7:15,.src_type=nir_type_float32,.io_semantics={.location=i?(mode==8?VARYING_SLOT_COL1:VARYING_SLOT_COL0):VARYING_SLOT_POS,.num_slots=1});
 }
 if(mode==13)nir_pop_if(&b,NULL);
 if(mode!=10)nir_validate_shader(b.shader,"strict Lambert authored test");
 return b.shader;
}
static int alloc(void *p,size_t n,void **out){(void)p;(void)n;(void)out;assert(!"no BO allocation");return ENOTSUP;}
static void release(void *p,void *o){(void)p;(void)o;}
static int map(void *p,void *o,bool w,void **out){(void)p;(void)o;(void)w;(void)out;return ENOTSUP;}
static void unmap(void *p,void *o,bool w){(void)p;(void)o;(void)w;}
static int submit(void *p,void *o,const struct remesa_draw_packet *q,void **f){(void)p;(void)o;(void)q;(void)f;assert(!"no submit");return ENOTSUP;}
static int wait(void *p,void *o,uint64_t n,bool v){(void)p;(void)o;(void)n;(void)v;return ENOTSUP;}
int main(int argc,char **argv)
{
 glsl_type_singleton_init_or_ref();unsigned cases=0;
 unsigned char reference[64];size_t n=0;struct remesa_usse_cpu_white_lambert req={0,4,8,0};
 assert(!remesa_usse_cpu_white_lambert_vertex_encode(&req,reference,64,&n)&&n==64);
 if(argc==2){FILE *f=fopen(argv[1],"wb");assert(f&&fwrite(reference,1,64,f)==64&&!fclose(f));}else assert(argc==1);
 struct remesa_shader_ir canonical;
 for(unsigned mode=0;mode<17;mode++) {
  nir_shader *nir=make(mode);struct remesa_shader_ir ir,before;memset(&ir,0xa5,sizeof(ir));before=ir;
  int e=remesa_shader_ir_lower(nir,&ir);
  if(mode!=0&&mode<14){assert(e==ENOTSUP);assert(!memcmp(&ir,&before,sizeof(ir)));cases++;ralloc_free(nir);continue;}
  assert(!e&&!remesa_shader_ir_validate(&ir)&&!remesa_shader_ir_white_lambert(&ir));
  if(!mode)canonical=ir;else assert(!memcmp(&canonical,&ir,sizeof(ir)));
  struct remesa_shader_vertex_layout layout;assert(!remesa_shader_ir_vertex_layout(&ir,&layout));
  assert(layout.input_count==3&&layout.stride_dwords==12&&layout.output_dwords==8&&layout.color_components==4);
  for(unsigned i=0;i<3;i++)assert(layout.input_slot[i]==i&&layout.input_components[i]==4);
  struct remesa_shader_program p;assert(!remesa_shader_program_prepare(&ir,&p));ralloc_free(nir);
  struct remesa_shader_program_report report;assert(!remesa_shader_program_report(&p,&report));
  assert(report.status==REMESA_PROGRAM_COMPILE_OFFLINE&&report.binary_bytes==64&&report.launch_error==ENOTSUP);
  unsigned char bytes[80];memset(bytes,0xa5,sizeof(bytes));n=55;
  assert(remesa_shader_program_encode(&p,bytes,63,&n)==ENOSPC&&n==55&&bytes[0]==0xa5);
  assert(!remesa_shader_program_encode(&p,bytes,64,&n)&&n==64&&!memcmp(bytes,reference,64)&&bytes[64]==0xa5);
  struct remesa_shader_storage *storage=NULL;
  struct remesa_shader_storage_request sr={REMESA_IR_VERTEX,REMESA_SHADER_STORAGE_HOST_CPU_RECORDS,bytes,64,32};
  assert(!remesa_shader_storage_create(&sr,&storage));memset(bytes,0,64);
  struct remesa_shader_storage_view view;assert(!remesa_shader_storage_inspect(storage,&view));
  assert(view.byte_length==64&&!((uintptr_t)view.bytes%32)&&!memcmp(view.bytes,reference,64));
  assert(remesa_shader_storage_validate_execution(storage)==ENOTSUP);assert(!remesa_shader_storage_reference(&storage,NULL));
  struct remesa_shader_program fs;assert(!remesa_shader_program_prepare_color(0xff00ff00,&fs));
  struct remesa_shader_pipeline pair,pair_before;memset(&pair,0xa5,sizeof(pair));pair_before=pair;
  assert(remesa_shader_pipeline_prepare(&p,&fs,&pair)==ENOTSUP&&!memcmp(&pair,&pair_before,sizeof(pair)));
  struct remesa_usse_program absent,absent_before;memset(&absent,0xa5,sizeof(absent));absent_before=absent;
  assert(remesa_usse_lower(&ir,&absent)==ENOTSUP&&!memcmp(&absent,&absent_before,sizeof(absent)));
  n=87;assert(remesa_shader_ir_encode_sgx535(&ir,bytes,sizeof(bytes),&n)==ENOTSUP&&n==87);
  struct remesa_pds_plan plan,plan_before;memset(&plan,0xa5,sizeof(plan));plan_before=plan;
  assert(remesa_shader_program_prepare_launch(&p,NULL,&plan)==ENOTSUP&&!memcmp(&plan,&plan_before,sizeof(plan)));
  struct remesa_shader_ir ir_before=ir;
  assert(remesa_shader_ir_vertex_layout(&ir,(void *)&ir)==EINVAL&&!memcmp(&ir,&ir_before,sizeof(ir)));
  struct remesa_shader_program p_before=p;
  assert(remesa_shader_program_encode(&p,&p,sizeof(p),&n)==EINVAL&&!memcmp(&p,&p_before,sizeof(p)));
  p.usse.schema=1;assert(remesa_shader_program_validate(&p)==EPROTO);cases++;
 }
 for(unsigned i=0;i<8;i++){struct remesa_shader_ir ir=canonical;ir.instructions[i].bits[0]=1;assert(remesa_shader_ir_validate(&ir)!=0);cases++;}
 static const struct remesa_device_ops ops={.allocate=alloc,.destroy_bo=release,.map=map,.unmap=unmap,.submit=submit,.wait=wait,.destroy_fence=release};
 struct remesa_device dev={.ops=&ops,.offline_test_backend=true};
 struct pipe_screen *screen=remesa_screen_create(&dev);assert(screen);
 struct pipe_context *ctx=screen->context_create(screen,NULL,0);assert(ctx);
 struct pipe_shader_state state={.type=PIPE_SHADER_IR_NIR,.ir.nir=make(0)};
 void *vs=ctx->create_vs_state(ctx,&state);assert(vs);ctx->bind_vs_state(ctx,vs);assert(!remesa_context_error(ctx));
 struct remesa_shader_program_report report;
 assert(!remesa_context_shader_program_report(ctx,PIPE_SHADER_VERTEX,vs,&report));
 assert(report.status==REMESA_PROGRAM_COMPILE_OFFLINE&&report.binary_bytes==64&&report.launch_error==ENOTSUP);
 unsigned char bytes[64];n=0;
 assert(!remesa_context_shader_program_encode(ctx,PIPE_SHADER_VERTEX,vs,bytes,64,&n)&&n==64&&!memcmp(bytes,reference,64));cases++;
 state.ir.nir=make(1);assert(!ctx->create_vs_state(ctx,&state)&&remesa_context_error(ctx)==ENOTSUP);cases++;
 /* A failed replacement must preserve the prior immutable shader. */
 assert(!remesa_context_shader_program_encode(ctx,PIPE_SHADER_VERTEX,vs,bytes,64,&n)&&n==64&&!memcmp(bytes,reference,64));cases++;
 ctx->delete_vs_state(ctx,vs);assert(remesa_context_shader_program_report(ctx,PIPE_SHADER_VERTEX,vs,&report)==EINVAL);cases++;
 ctx->destroy(ctx);screen->destroy(screen);
 glsl_type_singleton_decref();printf("PASS %u real NIR/codegen/storage cases; COMPILE_OFFLINE only\n",cases);return 0;
}
