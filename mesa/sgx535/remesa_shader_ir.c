/* SPDX-License-Identifier: MIT; original host IR, no hardware encodings. */
#include "remesa_shader_ir.h"
#include "remesa_nir.h"
#include "remesa_usse.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
struct value { unsigned components, input; uint32_t bits[4],channel[4],used; };
static int zero_words(const uint32_t *v,unsigned n)
{for(unsigned i=0;i<n;i++)if(v[i])return 0;return 1;}
static int unused_sources(const struct remesa_ir_instruction *i,unsigned first)
{for(unsigned n=first;n<4;n++)if(i->src[n].reg||i->src[n].channel)return 0;return 1;}
static void gears_vertex_ir(unsigned attribute,struct remesa_shader_ir *p)
{
 *p=(struct remesa_shader_ir){.schema=REMESA_SHADER_IR_SCHEMA,
  .stage=REMESA_IR_VERTEX,.count=1};
 p->instructions[0]=(struct remesa_ir_instruction){.op=REMESA_IR_GEARS_VERTEX,
  .dst=REMESA_SHADER_IR_NO_REGISTER,.components=4,.type=REMESA_IR_FLOAT32,
  .input_slot=attribute,.bits={attribute?9u:10u,attribute?37u:41u,0x3f800000,1}};
}
int remesa_shader_ir_gears_vertex(const struct remesa_shader_ir *p)
{
 if(!p)return EINVAL;
 if(p->instructions[0].input_slot>1)return ENOTSUP;
 struct remesa_shader_ir expected;gears_vertex_ir(p->instructions[0].input_slot,&expected);
 /* Canonical host uint32 records, including all unused storage. No generic
  * arithmetic, incomplete export, resource or runtime admission is added. */
 return memcmp(p,&expected,sizeof(expected))?ENOTSUP:0;
}
int remesa_shader_ir_gears_vertex_clamped(const struct remesa_shader_ir *p)
{
 if(!p)return EINVAL;
 if(p->instructions[0].input_slot>1)return ENOTSUP;
 struct remesa_shader_ir expected;gears_vertex_ir(p->instructions[0].input_slot,&expected);
 expected.instructions[0].op=REMESA_IR_GEARS_VERTEX_CLAMPED;
 return memcmp(p,&expected,sizeof(expected))?ENOTSUP:0;
}
static void white_lambert_ir(struct remesa_shader_ir *p)
{
 *p=(struct remesa_shader_ir){.schema=REMESA_SHADER_IR_SCHEMA,.stage=REMESA_IR_VERTEX,.count=8};
 for(unsigned n=0;n<6;n++)p->instructions[n]=(struct remesa_ir_instruction){
  .dst=n,.components=n<3||n==5?4:1,.type=REMESA_IR_FLOAT32};
 for(unsigned n=0;n<3;n++){p->instructions[n].op=REMESA_IR_INPUT;p->instructions[n].input_slot=n;}
 p->instructions[3].op=REMESA_IR_DOT3;p->instructions[3].src[0].reg=1;p->instructions[3].src[1].reg=2;
 p->instructions[4].op=REMESA_IR_SATURATE;p->instructions[4].src[0].reg=3;
 p->instructions[5].op=REMESA_IR_COMPOSE4;
 for(unsigned c=0;c<4;c++)p->instructions[5].src[c].reg=4;
 p->instructions[6]=(struct remesa_ir_instruction){.op=REMESA_IR_POSITION,
  .dst=REMESA_SHADER_IR_NO_REGISTER,.components=4,.type=REMESA_IR_FLOAT32};
 p->instructions[7]=(struct remesa_ir_instruction){.op=REMESA_IR_COLOR,
  .dst=REMESA_SHADER_IR_NO_REGISTER,.components=4,.type=REMESA_IR_FLOAT32,.src={{5,0}}};
}
int remesa_shader_ir_white_lambert(const struct remesa_shader_ir *p)
{
 if(!p)return EINVAL;
 struct remesa_shader_ir expected;white_lambert_ir(&expected);
 if(p->schema!=expected.schema||p->stage!=expected.stage||p->count!=expected.count)return ENOTSUP;
 for(unsigned n=0;n<REMESA_SHADER_IR_INSTRUCTIONS;n++) {
  const struct remesa_ir_instruction *a=&p->instructions[n],*b=&expected.instructions[n];
  if(a->op!=b->op||a->dst!=b->dst||a->components!=b->components||a->type!=b->type||
     a->input_slot!=b->input_slot)return ENOTSUP;
  for(unsigned c=0;c<4;c++)if(a->src[c].reg!=b->src[c].reg||a->src[c].channel!=b->src[c].channel||a->bits[c]!=b->bits[c])return ENOTSUP;
 }
 return 0;
}
static void fragment_color_input_ir(struct remesa_shader_ir *p)
{
 *p=(struct remesa_shader_ir){.schema=REMESA_SHADER_IR_SCHEMA,.stage=REMESA_IR_FRAGMENT,.count=2};
 p->instructions[0]=(struct remesa_ir_instruction){.op=REMESA_IR_INPUT,.components=4,
  .type=REMESA_IR_FLOAT32,.input_slot=1};
 p->instructions[1]=(struct remesa_ir_instruction){.op=REMESA_IR_COLOR,.dst=REMESA_SHADER_IR_NO_REGISTER,
  .components=4,.type=REMESA_IR_FLOAT32};
}
int remesa_shader_ir_fragment_color_input(const struct remesa_shader_ir *p)
{
 if(!p)return EINVAL;
 struct remesa_shader_ir expected;fragment_color_input_ir(&expected);
 /* All fields are uint32 host records with no padding. Unused storage must
  * be canonical too; this never admits a generalized fragment arithmetic IR. */
 return memcmp(p,&expected,sizeof(expected))?ENOTSUP:0;
}
static void fragment_flat_color_ir(struct remesa_shader_ir *p)
{
 *p=(struct remesa_shader_ir){.schema=REMESA_SHADER_IR_SCHEMA,.stage=REMESA_IR_FRAGMENT,.count=1};
 p->instructions[0]=(struct remesa_ir_instruction){.op=REMESA_IR_FLAT_COLOR_INPUT,
  .dst=REMESA_SHADER_IR_NO_REGISTER,.components=4,.type=REMESA_IR_FLOAT32,.input_slot=1};
}
int remesa_shader_ir_fragment_flat_color_input(const struct remesa_shader_ir *p)
{
 if(!p)return EINVAL;
 struct remesa_shader_ir expected;fragment_flat_color_ir(&expected);
 return memcmp(p,&expected,sizeof(expected))?ENOTSUP:0;
}
int remesa_shader_ir_validate(const struct remesa_shader_ir *p)
{
 struct value values[REMESA_SHADER_IR_VALUES]={0};unsigned defined=0,exports=0;
 uint32_t all_used=0;
 if(!p)return EINVAL;
 if(!remesa_shader_ir_white_lambert(p)||!remesa_shader_ir_fragment_color_input(p)||
    !remesa_shader_ir_gears_vertex(p)||!remesa_shader_ir_gears_vertex_clamped(p)||
    !remesa_shader_ir_fragment_flat_color_input(p))return 0;
 if(p->schema!=REMESA_SHADER_IR_SCHEMA||
    (p->stage!=REMESA_IR_VERTEX&&p->stage!=REMESA_IR_FRAGMENT)||
    !p->count||p->count>REMESA_SHADER_IR_INSTRUCTIONS)return ENOTSUP;
 for(unsigned n=0;n<p->count;n++) {
  const struct remesa_ir_instruction *i=&p->instructions[n];struct value v={0};
  if(i->type!=REMESA_IR_FLOAT32||!i->components||i->components>4)return ENOTSUP;
  if(i->op==REMESA_IR_POSITION||i->op==REMESA_IR_COLOR) {
   if(++exports!=1||n+1!=p->count||i->dst!=REMESA_SHADER_IR_NO_REGISTER||
      i->components!=4||i->input_slot||!zero_words(i->bits,4)||
      !unused_sources(i,1)||i->src[0].channel||i->src[0].reg>=defined)return ENOTSUP;
   v=values[i->src[0].reg];if(v.components!=4)return ENOTSUP;
   if(i->op==REMESA_IR_POSITION) {
    if(p->stage!=REMESA_IR_VERTEX||!v.input)return ENOTSUP;
    for(unsigned c=0;c<4;c++)if(v.channel[c]!=c)return ENOTSUP;
   } else {
    uint32_t argb;
    if(p->stage!=REMESA_IR_FRAGMENT||v.input||remesa_constant_rgba32(v.bits,&argb))return ENOTSUP;
   }
   all_used=v.used;continue;
  }
  if(exports||defined>=REMESA_SHADER_IR_VALUES||i->dst!=defined||i->input_slot)return ENOTSUP;
  v.components=i->components;v.used=1u<<defined;
  switch(i->op) {
  case REMESA_IR_INPUT:
   if(p->stage!=REMESA_IR_VERTEX||defined||i->components!=4||
      !zero_words(i->bits,4)||!unused_sources(i,0))return ENOTSUP;
   v.input=1;for(unsigned c=0;c<4;c++)v.channel[c]=c;break;
  case REMESA_IR_CONSTANT:
   if(p->stage!=REMESA_IR_FRAGMENT||!unused_sources(i,0)||
      !zero_words(i->bits+i->components,4-i->components))return ENOTSUP;
   /* Endpoint float bits are IEEE754 +0/+1, as accepted by remesa_nir.
    * This does not establish native immediate/register encoding. */
   for(unsigned c=0;c<i->components;c++) {
    if(i->bits[c]!=0&&i->bits[c]!=UINT32_C(0x3f800000))return ENOTSUP;
    v.bits[c]=i->bits[c];
   }
   break;
  case REMESA_IR_COPY:
  case REMESA_IR_COMPOSE4:
   if(!zero_words(i->bits,4))return ENOTSUP;
   if(i->op==REMESA_IR_COMPOSE4&&i->components!=4)return ENOTSUP;
   if(!unused_sources(i,i->op==REMESA_IR_COPY?i->components:4))return ENOTSUP;
   for(unsigned c=0;c<i->components;c++) {
    unsigned r=i->src[c].reg,k=i->src[c].channel;
    if(r>=defined||k>=values[r].components)return ENOTSUP;
    if(c&&v.input!=values[r].input)return ENOTSUP;
    v.input=values[r].input;v.bits[c]=values[r].bits[k];
    v.channel[c]=values[r].channel[k];v.used|=values[r].used;
   }
   break;
  default:return ENOTSUP;
  }
  values[defined++]=v;
 }
 if(exports!=1||!defined||all_used!=((1u<<defined)-1u))return ENOTSUP;
 /* Unused instruction storage must be canonical; rejects hidden payloads. */
 const unsigned char *unused=(const unsigned char *)&p->instructions[p->count];
 for(size_t n=0;n<(REMESA_SHADER_IR_INSTRUCTIONS-p->count)*sizeof(p->instructions[0]);n++)
  if(unused[n])return ENOTSUP;
 return 0;
}
int remesa_shader_ir_vertex_input(const struct remesa_shader_ir *p,unsigned *slot)
{
 if(!slot)return EINVAL;
 int ret=remesa_shader_ir_validate(p);if(ret)return ret;
 if(p->stage!=REMESA_IR_VERTEX)return ENOTSUP;
 *slot=0;return 0;
}
int remesa_shader_ir_vertex_layout(const struct remesa_shader_ir *p,
 struct remesa_shader_vertex_layout *out)
{
 if(!p||!out||sizeof(*p)>UINTPTR_MAX-(uintptr_t)p||
    sizeof(*out)>UINTPTR_MAX-(uintptr_t)out||
    ((uintptr_t)p<(uintptr_t)out+sizeof(*out)&&
     (uintptr_t)out<(uintptr_t)p+sizeof(*p)))return EINVAL;
 int ret=remesa_shader_ir_validate(p);if(ret)return ret;
 if(p->stage!=REMESA_IR_VERTEX)return ENOTSUP;
 if(!remesa_shader_ir_gears_vertex(p)||!remesa_shader_ir_gears_vertex_clamped(p)) {
  const unsigned attribute=p->instructions[0].input_slot;
  struct remesa_shader_vertex_layout layout={.input_count=attribute?2u:1u,
   .stride_dwords=attribute?8u:4u,.position_components=4,.color_components=4,.output_dwords=8};
  for(unsigned i=0;i<layout.input_count;i++){layout.input_slot[i]=i;layout.input_components[i]=4;}
  *out=layout;return 0;
 }
 const int white=!remesa_shader_ir_white_lambert(p);
 struct remesa_shader_vertex_layout layout={.input_count=white?3u:1u,
  .stride_dwords=white?12u:4u,.position_components=4,
  .color_components=white?4u:0u,.output_dwords=white?8u:4u};
 for(unsigned i=0;i<layout.input_count;i++){layout.input_slot[i]=i;layout.input_components[i]=4;}
 *out=layout;return 0;
}
int remesa_shader_ir_fragment_color(const struct remesa_shader_ir *p,uint32_t *argb)
{
 uint32_t bits[REMESA_SHADER_IR_VALUES][4]={{0}};
 if(!argb)return EINVAL;
 int ret=remesa_shader_ir_validate(p);if(ret)return ret;
 if(p->stage!=REMESA_IR_FRAGMENT)return ENOTSUP;
 for(unsigned n=0;n<p->count;n++) {
  const struct remesa_ir_instruction *i=&p->instructions[n];
  if(i->op==REMESA_IR_CONSTANT)memcpy(bits[i->dst],i->bits,sizeof(i->bits));
  else if(i->op==REMESA_IR_COPY||i->op==REMESA_IR_COMPOSE4)
   for(unsigned c=0;c<i->components;c++)bits[i->dst][c]=bits[i->src[c].reg][i->src[c].channel];
  else if(i->op==REMESA_IR_COLOR)return remesa_constant_rgba32(bits[i->src[0].reg],argb);
  else return ENOTSUP;
 }
 return ENOTSUP;
}
int remesa_shader_ir_lower(const struct nir_shader *nir,struct remesa_shader_ir *out)
{
 struct remesa_shader_ir p={.schema=REMESA_SHADER_IR_SCHEMA,.count=2};
 unsigned slot;uint32_t color;int vertex,fragment;
 if(!nir||!out)return EINVAL;
 struct remesa_nir_gears_vertex_profile gears;
 if(!remesa_nir_gears_vertex(nir,&gears)) {
  gears_vertex_ir(gears.normal_from_attribute,&p);*out=p;return 0;
 }
 if(!remesa_nir_white_lambert_vertex(nir)) {
  white_lambert_ir(&p);*out=p;return 0;
 }
 unsigned flat;
 if(!remesa_nir_fragment_color_profile(nir,&slot,&flat)&&slot==1) {
  if(flat)fragment_flat_color_ir(&p);else fragment_color_input_ir(&p);
  *out=p;return 0;
 }
 vertex=remesa_nir_position_vertex(nir,&slot);
 if(!vertex) {
  if(slot)return ENOTSUP;
  p.stage=REMESA_IR_VERTEX;p.instructions[0].op=REMESA_IR_INPUT;
  p.instructions[1].op=REMESA_IR_POSITION;
 } else {
  fragment=remesa_nir_constant_fragment(nir,&color);
  if(fragment)return fragment;
  p.stage=REMESA_IR_FRAGMENT;p.instructions[0].op=REMESA_IR_CONSTANT;
  const unsigned shift[4]={16,8,0,24};
  for(unsigned c=0;c<4;c++)p.instructions[0].bits[c]=((color>>shift[c])&255)?UINT32_C(0x3f800000):0;
  p.instructions[1].op=REMESA_IR_COLOR;
 }
 for(unsigned n=0;n<2;n++) {
  p.instructions[n].type=REMESA_IR_FLOAT32;p.instructions[n].components=4;
 }
 p.instructions[1].dst=REMESA_SHADER_IR_NO_REGISTER;
 int ret=remesa_shader_ir_validate(&p);if(ret)return ret;
 *out=p;return 0;
}
int remesa_shader_ir_dump(const struct remesa_shader_ir *p,char *out,size_t size,size_t *needed)
{
 char text[4096];size_t used=0;int ret;
 if(!needed||(!out&&size))return EINVAL;
 ret=remesa_shader_ir_validate(p);if(ret)return ret;
 int written=snprintf(text,sizeof(text),"reMESA host semantic IR v1 (NOT SGX instructions) stage=%s\n",p->stage==REMESA_IR_VERTEX?"vertex":"fragment");
 if(written<0||(size_t)written>=sizeof(text))return EOVERFLOW;
 used=(size_t)written;
 static const char *ops[]={"invalid","input","constant","copy","compose4","position","color","dot3","saturate","gears-vertex","flat-color-input","gears-vertex-clamped"};
 for(unsigned n=0;n<p->count;n++) {
  const struct remesa_ir_instruction *i=&p->instructions[n];
  written=snprintf(text+used,sizeof(text)-used,"%u %s dst=%u float32x%u src=%u.%u,%u.%u,%u.%u,%u.%u bits=%08x,%08x,%08x,%08x\n",n,ops[i->op],i->dst,i->components,i->src[0].reg,i->src[0].channel,i->src[1].reg,i->src[1].channel,i->src[2].reg,i->src[2].channel,i->src[3].reg,i->src[3].channel,i->bits[0],i->bits[1],i->bits[2],i->bits[3]);
  if(written<0||(size_t)written>=sizeof(text)-used)return EOVERFLOW;
  used+=(size_t)written;
 }
 *needed=used+1;if(size<used+1)return ENOSPC;
 memcpy(out,text,used+1);return 0;
}
int remesa_shader_ir_encode_sgx535(const struct remesa_shader_ir *p,void *out,size_t size,size_t *written)
{
 if(!written||(!out&&size))return EINVAL;
 struct remesa_usse_program program;
 int ret=remesa_usse_lower(p,&program);
 return ret?ret:remesa_usse_encode(&program,out,size,written);
}
