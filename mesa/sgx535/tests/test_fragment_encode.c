/* SPDX-License-Identifier: MIT
 * Public NIR/Gallium -> bounded original FS encoding. No device or GPU use.
 * Expected colors are semantic values, not proprietary instruction fixtures.
 */
#include "remesa_shader_program.h"
#include "remesa_fragment_body.h"
void *__wrap_calloc(size_t,size_t);
int inherited_nir_main(void);
#define main inherited_nir_main
#include "test_nir.c"
#undef main
static unsigned checks;
#define CHECK(x) do {assert(x);checks++;} while(0)
static nir_block *block_of(nir_shader *s)
{
 nir_function *f=exec_node_data(nir_function,exec_list_get_head(&s->functions),node);
 return exec_node_data(nir_block,exec_list_get_head(&f->impl->body),cf_node.node);
}
static nir_load_const_instr *constant(nir_shader *s,nir_block *b,unsigned bits,unsigned width)
{
 nir_load_const_instr *v=rzalloc_size(s,sizeof(*v)+width*sizeof(nir_const_value));
 v->instr.type=nir_instr_type_load_const;v->instr.block=b;
 v->def.parent_instr=&v->instr;v->def.bit_size=bits;v->def.num_components=width;
 return v;
}
static nir_shader *folded_fixture(unsigned mode,nir_alu_instr **alu_out)
{
 nir_shader *s=fixture(UINT32_C(0xff00ff00));nir_block *b=block_of(s);
 nir_load_const_instr *a=exec_node_data(nir_load_const_instr,exec_list_get_head(&b->instr_list),instr.node);
 nir_intrinsic_instr *store=exec_node_data(nir_intrinsic_instr,exec_list_get_tail(&b->instr_list),instr.node);
 unsigned sources=mode==3?1:mode>=4?3:2;
 nir_alu_instr *alu=rzalloc_size(s,sizeof(*alu)+sources*sizeof(nir_alu_src));
 alu->instr.type=nir_instr_type_alu;alu->instr.block=b;
 alu->def.parent_instr=&alu->instr;alu->def.bit_size=32;alu->def.num_components=4;
 const nir_op ops[]={nir_op_iand,nir_op_ior,nir_op_ixor,nir_op_inot,nir_op_bcsel,nir_op_bcsel};
 alu->op=ops[mode];
 if(mode>=4) {
  nir_load_const_instr *condition=constant(s,b,1,1),*other=constant(s,b,32,4);
  condition->value[0]=nir_const_value_for_bool(mode==4,1);
  for(unsigned c=0;c<4;c++)other->value[c].u32=c==1?0:UINT32_C(0x3f800000);
  exec_node_insert_node_before(&store->instr.node,&condition->instr.node);
  exec_node_insert_node_before(&store->instr.node,&other->instr.node);
  alu->src[0].src.ssa=&condition->def;alu->src[1].src.ssa=&a->def;alu->src[2].src.ssa=&other->def;
  for(unsigned c=0;c<4;c++){alu->src[1].swizzle[c]=c;alu->src[2].swizzle[c]=c;}
 } else {
  alu->src[0].src.ssa=&a->def;
  for(unsigned c=0;c<4;c++)alu->src[0].swizzle[c]=c;
  if(mode==3)for(unsigned c=0;c<4;c++)a->value[c].u32=~a->value[c].u32;
  else {
   nir_load_const_instr *mask=constant(s,b,32,4);
   for(unsigned c=0;c<4;c++)mask->value[c].u32=mode==0?UINT32_MAX:
    mode==1?0:(c==3?0:UINT32_C(0x3f800000));
   exec_node_insert_node_before(&store->instr.node,&mask->instr.node);
   alu->src[1].src.ssa=&mask->def;
   for(unsigned c=0;c<4;c++)alu->src[1].swizzle[c]=c;
  }
 }
 exec_node_insert_node_before(&store->instr.node,&alu->instr.node);store->src[0].ssa=&alu->def;
 if(alu_out)*alu_out=alu;return s;
}
static void encoded(const struct remesa_usse_program *p,uint32_t argb)
{
 unsigned char output[24],before[24];memset(output,0xa5,sizeof(output));memcpy(before,output,sizeof(output));
 size_t bytes=91,written=92;
 CHECK(!remesa_usse_encoded_size(p,&bytes)&&bytes==8);
 CHECK(!remesa_usse_encode(p,output,sizeof(output),&written)&&written==bytes);
 struct remesa_fragment_constant result;
 CHECK(!remesa_fragment_constant_decode(output,written,&result)&&result.argb==argb);
 CHECK(!memcmp(output+bytes,before+bytes,sizeof(output)-bytes));
 memcpy(output,before,sizeof(output));written=92;
 CHECK(remesa_usse_encode(p,output,7,&written)==ENOSPC&&written==92&&!memcmp(output,before,sizeof(output)));
 CHECK(remesa_usse_encode(p,NULL,0,&written)==ENOSPC&&written==92);
 CHECK(remesa_usse_encode(p,NULL,8,&written)==EINVAL&&written==92);
 CHECK(remesa_usse_encode(p,output,sizeof(output),(void *)(output+1))==EINVAL&&!memcmp(output,before,sizeof(output)));
 struct remesa_usse_program copy=*p,saved=copy;
 CHECK(remesa_usse_encode(&copy,(void *)&copy,8,&written)==EINVAL&&!memcmp(&copy,&saved,sizeof(copy)));
 CHECK(remesa_usse_encoded_size(&copy,(void *)&copy.instructions[0])==EINVAL&&!memcmp(&copy,&saved,sizeof(copy)));
 CHECK(remesa_usse_encode(p,(void *)(UINTPTR_MAX-3),8,&written)==EINVAL&&written==92);
}
static void program_aliases(const struct remesa_shader_program *p)
{
 struct remesa_shader_program copy=*p,saved=copy;
 size_t written=92;unsigned char bytes[16],before[16];
 memset(bytes,0xa5,sizeof(bytes));memcpy(before,bytes,sizeof(bytes));
 CHECK(remesa_shader_program_encode(&copy,&copy.semantic,8,&written)==EINVAL);
 CHECK(written==92&&!memcmp(&copy,&saved,sizeof(copy)));
 CHECK(remesa_shader_program_encode(&copy,bytes,sizeof(bytes),(void *)&copy.semantic)==EINVAL);
 CHECK(!memcmp(bytes,before,sizeof(bytes))&&!memcmp(&copy,&saved,sizeof(copy)));
 CHECK(remesa_shader_program_encode((void *)(UINTPTR_MAX-3),bytes,sizeof(bytes),&written)==EINVAL);
 CHECK(written==92&&!memcmp(bytes,before,sizeof(bytes)));
}
int main(void)
{
 static const struct remesa_device_ops ops={.allocate=alloc,.destroy_bo=release,.map=map,.unmap=unmap,.submit=submit,.wait=wait,.destroy_fence=release};
 struct remesa_device dev={.ops=&ops,.offline_test_backend=true};
 struct pipe_screen *screen=remesa_screen_create(&dev);CHECK(screen);
 struct pipe_context *ctx=screen->context_create(screen,NULL,0);CHECK(ctx);
 for(unsigned mode=0;mode<6;mode++) {
  nir_alu_instr *alu;nir_shader *s=folded_fixture(mode,&alu);
  uint32_t expected=mode==2||mode==5?UINT32_C(0xffff00ff):UINT32_C(0xff00ff00),color=0;
  nir_alu_instr saved=*alu;
  CHECK(!remesa_nir_constant_fragment(s,&color)&&color==expected);
  CHECK(!memcmp(alu,&saved,sizeof(saved))); /* recognizer does not mutate NIR */
  struct remesa_shader_ir ir;struct remesa_shader_program program;
  CHECK(!remesa_shader_ir_lower(s,&ir)&&!remesa_shader_program_prepare(&ir,&program));
  encoded(&program.usse,expected);
  program_aliases(&program);
  unsigned freed=destroyed;
  struct pipe_shader_state state={.type=PIPE_SHADER_IR_NIR,.ir.nir=s};
  void *fs=ctx->create_fs_state(ctx,&state);CHECK(fs&&destroyed==freed+1);
  struct remesa_shader_program_report report;
  CHECK(!remesa_context_shader_program_report(ctx,PIPE_SHADER_FRAGMENT,fs,&report));
  CHECK(!report.binary_error&&report.binary_bytes==8&&report.launch_error==ENOTSUP);
  unsigned char bytes[8];size_t written=99;
  CHECK(!remesa_context_shader_program_encode(ctx,PIPE_SHADER_FRAGMENT,fs,bytes,sizeof(bytes),&written)&&written==8);
  struct remesa_fragment_constant result;
  CHECK(!remesa_fragment_constant_decode(bytes,written,&result)&&result.argb==expected);
  ctx->delete_fs_state(ctx,fs);
 }
 for(unsigned mode=0;mode<8;mode++) {
  nir_alu_instr *alu;nir_shader *s=folded_fixture(mode<4?0:4,&alu);
  if(mode==0)alu->src[1].swizzle[0]=4;
  if(mode==1)alu->src[1].src.ssa=&alu->def;
  if(mode==2)alu->exact=true;
  if(mode==3)alu->op=nir_op_fadd; /* no GPU or HOST float arithmetic added */
  if(mode==4)alu->src[0].src.ssa->bit_size=32;
  if(mode==5)nir_instr_as_load_const(alu->src[0].src.ssa->parent_instr)->value[0].u32=2;
  if(mode==6)alu->src[0].swizzle[0]=1;
  if(mode==7)alu->def.bit_size=16;
  uint32_t unchanged=123;CHECK(remesa_nir_constant_fragment(s,&unchanged)==ENOTSUP&&unchanged==123);
  ralloc_free(s);
 }
 struct remesa_shader_program yellow;
 CHECK(!remesa_shader_program_prepare_color(UINT32_C(0xffffff00),&yellow));
 unsigned char bytes[16],old[16];memset(bytes,0xa5,sizeof(bytes));memcpy(old,bytes,sizeof(bytes));
 size_t written=99;
 encoded(&yellow.usse,UINT32_C(0xffffff00));
 nir_shader *yellow_nir=fixture(UINT32_C(0xffffff00));
 struct pipe_shader_state yellow_state={.type=PIPE_SHADER_IR_NIR,.ir.nir=yellow_nir};
 void *yellow_fs=ctx->create_fs_state(ctx,&yellow_state);CHECK(yellow_fs);
 struct remesa_shader_program_report yellow_report;
 CHECK(!remesa_context_shader_program_report(ctx,PIPE_SHADER_FRAGMENT,yellow_fs,&yellow_report));
 CHECK(!yellow_report.binary_error&&yellow_report.binary_bytes==8&&yellow_report.launch_error==ENOTSUP);
 size_t yellow_written=99;unsigned char yellow_bytes[8];
 CHECK(!remesa_context_shader_program_encode(ctx,PIPE_SHADER_FRAGMENT,yellow_fs,
  yellow_bytes,sizeof(yellow_bytes),&yellow_written)&&yellow_written==8);
 struct remesa_fragment_constant yellow_result;
 CHECK(!remesa_fragment_constant_decode(yellow_bytes,yellow_written,&yellow_result)&&
  yellow_result.argb==UINT32_C(0xffffff00));
 ctx->delete_fs_state(ctx,yellow_fs);
 struct remesa_usse_program unsupported=yellow.usse;
 for(unsigned n=0;n<unsupported.count;n++)
  if(unsupported.instructions[n].op==REMESA_USSE_CONSTANT &&
     unsupported.instructions[n].bits==UINT32_C(0x3f800000))
   unsupported.instructions[n].bits=UINT32_C(0x3f000000);
 CHECK(remesa_usse_encode(&unsupported,bytes,sizeof(bytes),&written)==ENOTSUP&&written==99&&!memcmp(bytes,old,sizeof(bytes)));
 struct remesa_shader_program_report report;
 CHECK(!remesa_shader_program_report(&yellow,&report)&&!report.binary_error&&
  report.binary_bytes==8&&report.launch_error==ENOTSUP);
 ctx->destroy(ctx);screen->destroy(screen);
 printf("PASS %u constant NIR/stage encoding checks; no GPU/submission admission\n",checks);return 0;
}
