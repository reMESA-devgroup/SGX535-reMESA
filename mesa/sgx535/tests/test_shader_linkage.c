/* SPDX-License-Identifier: MIT. Semantic IO linking only; no GPU permission. */
#include "remesa_shader_program.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static struct remesa_shader_program vertex(unsigned kind)
{
 struct remesa_shader_ir ir={.schema=REMESA_SHADER_IR_SCHEMA,.stage=REMESA_IR_VERTEX};
 if(kind<2){
  ir.count=1;ir.instructions[0]=(struct remesa_ir_instruction){.op=REMESA_IR_GEARS_VERTEX,
   .dst=REMESA_SHADER_IR_NO_REGISTER,.components=4,.type=REMESA_IR_FLOAT32,
   .input_slot=kind,.bits={kind?9:10,kind?37:41,0x3f800000,1}};
 }else if(kind==2){
  ir.count=8;
  for(unsigned n=0;n<6;n++)ir.instructions[n]=(struct remesa_ir_instruction){
   .dst=n,.components=n<3||n==5?4:1,.type=REMESA_IR_FLOAT32};
  for(unsigned n=0;n<3;n++){ir.instructions[n].op=REMESA_IR_INPUT;ir.instructions[n].input_slot=n;}
  ir.instructions[3].op=REMESA_IR_DOT3;ir.instructions[3].src[0].reg=1;ir.instructions[3].src[1].reg=2;
  ir.instructions[4].op=REMESA_IR_SATURATE;ir.instructions[4].src[0].reg=3;
  ir.instructions[5].op=REMESA_IR_COMPOSE4;
  for(unsigned n=0;n<4;n++)ir.instructions[5].src[n].reg=4;
  ir.instructions[6]=(struct remesa_ir_instruction){.op=REMESA_IR_POSITION,.dst=REMESA_SHADER_IR_NO_REGISTER,.components=4,.type=REMESA_IR_FLOAT32};
  ir.instructions[7]=(struct remesa_ir_instruction){.op=REMESA_IR_COLOR,.dst=REMESA_SHADER_IR_NO_REGISTER,.components=4,.type=REMESA_IR_FLOAT32,.src={{5,0}}};
 }else{
  ir.count=2;
  ir.instructions[0]=(struct remesa_ir_instruction){.op=REMESA_IR_INPUT,.components=4,.type=REMESA_IR_FLOAT32};
  ir.instructions[1]=(struct remesa_ir_instruction){.op=REMESA_IR_POSITION,.components=4,.type=REMESA_IR_FLOAT32,.dst=REMESA_SHADER_IR_NO_REGISTER};
 }
 struct remesa_shader_program p;assert(!remesa_shader_program_prepare(&ir,&p));return p;
}
static struct remesa_shader_program fragment(unsigned kind)
{
 struct remesa_shader_program p;
 if(!kind){assert(!remesa_shader_program_prepare_color(0xff00ff00,&p));return p;}
 struct remesa_shader_ir ir={.schema=REMESA_SHADER_IR_SCHEMA,.stage=REMESA_IR_FRAGMENT};
 if(kind==1){
  ir.count=2;ir.instructions[0]=(struct remesa_ir_instruction){.op=REMESA_IR_INPUT,.components=4,.type=REMESA_IR_FLOAT32,.input_slot=1};
  ir.instructions[1]=(struct remesa_ir_instruction){.op=REMESA_IR_COLOR,.components=4,.type=REMESA_IR_FLOAT32,.dst=REMESA_SHADER_IR_NO_REGISTER};
 }else{
  ir.count=1;ir.instructions[0]=(struct remesa_ir_instruction){.op=REMESA_IR_FLAT_COLOR_INPUT,.components=4,.type=REMESA_IR_FLOAT32,.dst=REMESA_SHADER_IR_NO_REGISTER,.input_slot=1};
 }
 assert(!remesa_shader_program_prepare(&ir,&p));return p;
}
int main(void)
{
 unsigned cases=0;
 for(unsigned vk=0;vk<4;vk++)for(unsigned fk=0;fk<3;fk++){
  struct remesa_shader_program v=vertex(vk),f=fragment(fk);
  struct remesa_shader_linkage link,before;memset(&link,0xa5,sizeof(link));before=link;
  int error=remesa_shader_linkage_prepare(&v,&f,&link);
  if(vk==3&&fk){assert(error==ENOTSUP&&!memcmp(&link,&before,sizeof(link)));cases++;continue;}
  assert(!error&&!remesa_shader_linkage_validate(&link));cases++;
  assert(link.fragment_input==(enum remesa_fragment_input_profile)(fk+1));
  assert(link.fragment_argb==(fk?0:0xff00ff00));
  assert(link.vertex_layout.input_count==(vk==2?3:vk==1?2:1));
  assert(!remesa_shader_linkage_revalidate(&link,&v,&f));
  struct remesa_shader_program other;assert(!remesa_shader_program_prepare_color(0xffff00ff,&other));
  assert(remesa_shader_linkage_revalidate(&link,&v,&other)==ESTALE);cases++;
  v.schema++;assert(remesa_shader_linkage_revalidate(&link,&v,&f));v.schema--;
  /* No borrowed program/NIR lifetime: owned link survives source destruction. */
  memset(&v,0,sizeof(v));memset(&f,0,sizeof(f));assert(!remesa_shader_linkage_validate(&link));cases++;
  for(unsigned fault=0;fault<7;fault++){
   struct remesa_shader_linkage bad=link;
   switch(fault){
   case 0:bad.vertex_layout.input_count++;break;
   case 1:bad.vertex_layout.input_slot[2]++;break;
   case 2:bad.vertex_layout.input_components[2]++;break;
   case 3:bad.vertex_layout.output_dwords++;break;
   case 4:bad.vertex_layout.color_components++;break;
   case 5:bad.fragment_input=(enum remesa_fragment_input_profile)99;break;
   case 6:bad.fragment_argb^=1;break;
   }
   assert(remesa_shader_linkage_validate(&bad)==EPROTO);cases++;
  }
 }
 struct remesa_shader_program v=vertex(0),f=fragment(1);
 struct remesa_shader_pipeline legacy;assert(remesa_shader_pipeline_prepare(&v,&f,&legacy)==ENOTSUP);cases++;
 struct remesa_shader_linkage out,before;memset(&out,0xa5,sizeof(out));before=out;
 assert(remesa_shader_linkage_prepare(&f,&v,&out)==ENOTSUP&&!memcmp(&out,&before,sizeof(out)));cases++;
 assert(remesa_shader_linkage_prepare(NULL,&f,&out)==EINVAL);
 assert(remesa_shader_linkage_prepare(&v,&f,NULL)==EINVAL);
 assert(remesa_shader_linkage_validate(NULL)==EINVAL);
 assert(remesa_shader_linkage_revalidate(NULL,&v,&f)==EINVAL);
 union {struct remesa_shader_linkage link;struct remesa_shader_program program;} alias;
 alias.program=v;assert(remesa_shader_linkage_prepare(&alias.program,&f,&alias.link)==EINVAL);cases++;
 printf("PASS common desktop/GLES semantic linkage: %u cases, varying IO mismatch, immutable program snapshots, stale inputs, metadata tamper, stage/alias rejection and unchanged legacy execution gate; GPU0\n",cases);
 return 0;
}
