/* SPDX-License-Identifier: MIT */
#include "remesa_usse_arithmetic.h"
#include "remesa_usse.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
enum gears_record_type { GEARS_REPEAT=1,GEARS_MOVE,GEARS_ARITHMETIC,GEARS_SELECT };
struct gears_record {
 enum gears_record_type type;
 union {
  struct remesa_usse_component_control repeat;
  struct remesa_usse_cpu_move move;
  struct remesa_usse_cpu_arithmetic arithmetic;
 } data;
};
static void gears_lighting_records(bool attribute,struct gears_record records[26]);
static bool gears_arithmetic_valid(const struct remesa_usse_cpu_arithmetic *r);
static int span(const void *p,size_t n)
{ return p&&n<=UINTPTR_MAX-(uintptr_t)p; }
static int overlap(const void *a,size_t n,const void *b,size_t m)
{ return (uintptr_t)a<(uintptr_t)b+m&&(uintptr_t)b<(uintptr_t)a+n; }
static void put32(unsigned char *p,uint32_t v)
{ for(unsigned n=0;n<4;n++)p[n]=(unsigned char)(v>>(n*8)); }
static void words(uint32_t op,uint32_t left,uint32_t right,uint32_t right_class,
 unsigned char bytes[8])
{
 /* Normalizer249ccf: ADD9d inserts numeric class6/index52 into slot1
  * and moves RHS to slot2; MUL9c inserts class6/index48 into slot2.
  * Both become selected kind0. These are CPU assignments, not claims about
  * architectural constants or a generic native floating-point opcode. */
 uint32_t middle=op==REMESA_USSE_ARITHMETIC_ADD?52u:right;
 uint32_t last=op==REMESA_USSE_ARITHMETIC_ADD?right:48u;
 uint32_t middle_bank=op==REMESA_USSE_ARITHMETIC_ADD?1u:right_class;
 uint32_t last_bank=op==REMESA_USSE_ARITHMETIC_ADD?right_class:1u;
 uint32_t extension_bit=op==REMESA_USSE_ARITHMETIC_ADD?17u:16u;
 uint32_t first=(left<<14)|(middle<<7)|last|
  (middle_bank<<30)|(last_bank<<28);
 /* flags bit0→23 at24b72d; repeat count4→(4-1)<<12 plus bit21 at
  * 24ad4a..58; class1 destination low2 bits; source0class2 contributes bit2.
  * Class6 extension follows slot1/slot2 mask at24bb09/24bb5d. */
 uint32_t second=(UINT32_C(1)<<23)|(UINT32_C(1)<<21)|(3u<<12)|
  (UINT32_C(1)<<extension_bit)|1u|(1u<<2);
 put32(bytes,first);put32(bytes+4,second);
}
int remesa_usse_arithmetic_encode(const struct remesa_usse_arithmetic *request,
 void *output,size_t capacity,size_t *written)
{
 if((uintptr_t)request%_Alignof(struct remesa_usse_arithmetic)||
    (uintptr_t)written%_Alignof(size_t))return EINVAL;
 if(!span(request,sizeof(*request))||!span(output,capacity)||
    !span(written,sizeof(*written)))return EINVAL;
 if(overlap(request,sizeof(*request),output,capacity)||
    overlap(request,sizeof(*request),written,sizeof(*written))||
    overlap(output,capacity,written,sizeof(*written)))return EINVAL;
 struct remesa_usse_arithmetic r=*request;
 if(r.reserved)return EINVAL;
 if(r.op!=REMESA_USSE_ARITHMETIC_ADD&&
    r.op!=REMESA_USSE_ARITHMETIC_MULTIPLY)return ENOTSUP;
 if(r.left_base>124||r.right_base>124||(r.left_base&3)||(r.right_base&3))return ENOTSUP;
 if(capacity<8)return ENOSPC;
 unsigned char bytes[8];words(r.op,r.left_base,r.right_base,2,bytes);
 memcpy(output,bytes,sizeof(bytes));*written=sizeof(bytes);return 0;
}

int remesa_usse_arithmetic_sources_encode(const struct remesa_usse_arithmetic_sources *request,
 void *output,size_t capacity,size_t *written)
{
 if((uintptr_t)request%_Alignof(struct remesa_usse_arithmetic_sources)||
    (uintptr_t)written%_Alignof(size_t))return EINVAL;
 if(!span(request,sizeof(*request))||!span(output,capacity)||!span(written,sizeof(*written)))return EINVAL;
 if(overlap(request,sizeof(*request),output,capacity)||
    overlap(request,sizeof(*request),written,sizeof(*written))||
    overlap(output,capacity,written,sizeof(*written)))return EINVAL;
 struct remesa_usse_arithmetic_sources r=*request;
 if(r.reserved)return EINVAL;
 if(r.op==REMESA_USSE_ARITHMETIC_MAXIMUM) {
  if(r.left.cpu_class!=2||r.right.cpu_class!=2||r.left.base>124||r.right.base>124||
     (r.left.base&3)||(r.right.base&3))return ENOTSUP;
  if(capacity<8)return ENOSPC;
  /* Handler24b1c7 kind0xd: major3<<27, sub1<<9, arity2.
   * Calls source1 helper2410df for LHS (<<7) and source2 helper241580
   * for RHS (<<0); unlike ADD/MUL there is no source0 bank in W1.
   * Flags0x41, repeat4 and destination(class1,index0) are fixed here. */
  uint32_t first=(r.left.base<<7)|r.right.base|(2u<<30)|(2u<<28);
  uint32_t second=(3u<<27)|(1u<<9)|(1u<<23)|(1u<<21)|(3u<<12)|1u;
  unsigned char bytes[8];put32(bytes,first);put32(bytes+4,second);
  memcpy(output,bytes,8);*written=8;return 0;
 }
 if(r.op!=REMESA_USSE_ARITHMETIC_ADD&&r.op!=REMESA_USSE_ARITHMETIC_MULTIPLY)return ENOTSUP;
 /* Original frontend canonicalizes either public source order to class2 LHS,
  * class3 RHS. Only the observed single live coefficient slice is admitted;
  * its numeric index1 does NOT establish physical bank/DS delivery. */
 if(r.left.cpu_class==3&&r.right.cpu_class==2) {
  struct remesa_usse_arithmetic_source t=r.left;r.left=r.right;r.right=t;
 }
 if(r.left.cpu_class!=2)return ENOTSUP;
 if(r.right.cpu_class==2) {
  if(r.left.base>124||r.right.base>124||(r.left.base&3)||(r.right.base&3))return ENOTSUP;
 } else if(r.right.cpu_class==3) {
  if(r.left.base!=0||r.right.base!=1)return ENOTSUP;
 } else return ENOTSUP;
 if(capacity<8)return ENOSPC;
 unsigned char bytes[8];words(r.op,r.left.base,r.right.base,r.right.cpu_class,bytes);
 memcpy(output,bytes,sizeof(bytes));*written=sizeof(bytes);return 0;
}

int remesa_usse_cpu_parameter_vertex_encode(
 const struct remesa_usse_cpu_parameter_vertex *request,void *out,size_t cap,
 size_t *written)
{
 if((uintptr_t)request%_Alignof(struct remesa_usse_cpu_parameter_vertex)||
    (uintptr_t)written%_Alignof(size_t))return EINVAL;
 if(!span(request,sizeof(*request))||!span(out,cap)||!span(written,sizeof(*written)))return EINVAL;
 if(overlap(request,sizeof(*request),out,cap)||
    overlap(request,sizeof(*request),written,sizeof(*written))||
    overlap(out,cap,written,sizeof(*written)))return EINVAL;
 const struct remesa_usse_cpu_parameter_vertex r=*request;
 if(r.reserved)return EINVAL;
 if(r.position_base||r.parameter_base!=1u)return ENOTSUP;
 if(r.op!=REMESA_USSE_ARITHMETIC_ADD&&r.op!=REMESA_USSE_ARITHMETIC_MULTIPLY)return ENOTSUP;
 if(cap<REMESA_USSE_CPU_PARAMETER_VERTEX_BYTES)return ENOSPC;
 unsigned char bytes[REMESA_USSE_CPU_PARAMETER_VERTEX_BYTES];size_t n;
 const struct remesa_usse_arithmetic_sources mul={
  .op=r.op,.left={2,r.position_base},
  .right={3,r.parameter_base}};
 int e=remesa_usse_arithmetic_sources_encode(&mul,bytes,8,&n);if(e)return e;
 /* Whole original4252d/30a09 linked result, auxiliary_count0. Same
  * checked tail as the independent POSITION profile, not copied USE bytes. */
 const struct remesa_usse_cpu_three_source tail={.source={{0,0},{2,0},{2,0}},
  .source_extension={0,1,1},.word0_control21=1,
  .word1_control12_13=1,.word1_control18=1};
 e=remesa_usse_cpu_three_source_encode(&tail,bytes+8,8,&n);if(e)return e;
 memcpy(out,bytes,sizeof(bytes));*written=sizeof(bytes);return 0;
}
int remesa_usse_cpu_parameter_mad_vertex_encode(
 const struct remesa_usse_cpu_parameter_mad_vertex *request,void *out,size_t cap,
 size_t *written)
{
 if((uintptr_t)request%_Alignof(struct remesa_usse_cpu_parameter_mad_vertex)||
    (uintptr_t)written%_Alignof(size_t))return EINVAL;
 if(!span(request,sizeof(*request))||!span(out,cap)||!span(written,sizeof(*written)))return EINVAL;
 if(overlap(request,sizeof(*request),out,cap)||
    overlap(request,sizeof(*request),written,sizeof(*written))||
    overlap(out,cap,written,sizeof(*written)))return EINVAL;
 const struct remesa_usse_cpu_parameter_mad_vertex r=*request;
 if(r.reserved)return EINVAL;
 if(r.position_base||r.multiply_parameter_base!=1u||r.add_parameter_base!=5u)
  return ENOTSUP;
 if(r.source_dwords<REMESA_USSE_CPU_PARAMETER_MAD_MIN_SOURCE_DWORDS)return ENOSPC;
 if(r.source_dwords>128u)return ENOTSUP;
 if(cap<REMESA_USSE_CPU_PARAMETER_MAD_VERTEX_BYTES)return ENOSPC;
 unsigned char bytes[REMESA_USSE_CPU_PARAMETER_MAD_VERTEX_BYTES];size_t n;
 const struct remesa_usse_cpu_arithmetic mad={.kind=0,.flags=0x41,
  .destination={1,0},.source={{2,r.position_base},
   {3,r.multiply_parameter_base},{3,r.add_parameter_base}}};
 int e=remesa_usse_cpu_arithmetic_encode(&mad,bytes,8,&n);if(e)return e;
 /* The separately replayed original4252d linker returns auxiliary_count0
  * and appends this typed30a09 tail to the original one-record body. */
 const struct remesa_usse_cpu_three_source tail={.source={{0,0},{2,0},{2,0}},
  .source_extension={0,1,1},.word0_control21=1,
  .word1_control12_13=1,.word1_control18=1};
 e=remesa_usse_cpu_three_source_encode(&tail,bytes+8,8,&n);if(e)return e;
 memcpy(out,bytes,sizeof(bytes));*written=sizeof(bytes);return 0;
}
int remesa_usse_component_control_encode(const struct remesa_usse_component_control *request,
 void *output,size_t capacity,size_t *written)
{
 if((uintptr_t)request%_Alignof(struct remesa_usse_component_control)||
    (uintptr_t)written%_Alignof(size_t))return EINVAL;
 if(!span(request,sizeof(*request))||!span(output,capacity)||!span(written,sizeof(*written)))return EINVAL;
 if(overlap(request,sizeof(*request),output,capacity)||
    overlap(request,sizeof(*request),written,sizeof(*written))||
    overlap(output,capacity,written,sizeof(*written)))return EINVAL;
 struct remesa_usse_component_control r=*request;
 if(r.reserved)return EINVAL;
 uint32_t first=0,modes=0;
 for(unsigned i=0;i<4;i++) {
  const struct remesa_usse_component_operand *s=&r.operand[i];uint32_t value=0;
  if(s->format==REMESA_USSE_COMPONENT_SIGNED) {
   if(s->value < -128||s->value>127)return ENOTSUP;
   for(unsigned c=0;c<4;c++)if(s->selector[c])return EINVAL;
   value=(uint8_t)s->value;
  } else if(s->format==REMESA_USSE_COMPONENT_SELECTORS) {
   if(s->value)return EINVAL;
   for(unsigned c=0;c<4;c++) {
    if(s->selector[c]>3)return ENOTSUP;
    value|=s->selector[c]<<(2*c);
   }
   modes|=1u<<(3-i);
  } else return ENOTSUP;
  /* Original251e24..6e converts four3-bit public selectors to2-bit fields;
   * 25207b..a4 places all four resulting bytes in descending order.
   * Explicit unsigned conversions avoid negative-shift UB. */
  first|=value<<(24-8*i);
 }
 if(capacity<8)return ENOSPC;
 /* Original25211f..13b flags bit3 clear: signed arithmetic expression
  * produces subgroup31, bit25 and bit20. The alternate flag and trailing
  * offset fields are deliberately not part of this selected CPU profile. */
 uint32_t second=(31u<<27)|(1u<<25)|(1u<<20)|modes;
 unsigned char bytes[8];put32(bytes,first);put32(bytes+4,second);
 memcpy(output,bytes,8);*written=8;return 0;
}

int remesa_usse_cpu_parameter_first_term_vertex_encode(
 const struct remesa_usse_cpu_parameter_first_term_vertex *request,void *out,
 size_t cap,size_t *written)
{
 if((uintptr_t)request%_Alignof(struct remesa_usse_cpu_parameter_first_term_vertex)||
    (uintptr_t)written%_Alignof(size_t))return EINVAL;
 if(!span(request,sizeof(*request))||!span(out,cap)||!span(written,sizeof(*written)))return EINVAL;
 if(overlap(request,sizeof(*request),out,cap)||
    overlap(request,sizeof(*request),written,sizeof(*written))||
    overlap(out,cap,written,sizeof(*written)))return EINVAL;
 const struct remesa_usse_cpu_parameter_first_term_vertex r=*request;
 if(r.reserved)return EINVAL;
 if(r.position_base||r.parameter_base!=1u)return ENOTSUP;
 if(r.source_dwords<5u)return ENOSPC;
 if(r.source_dwords>128u)return ENOTSUP;
 if(cap<REMESA_USSE_CPU_PARAMETER_FIRST_TERM_VERTEX_BYTES)return ENOSPC;
 unsigned char bytes[REMESA_USSE_CPU_PARAMETER_FIRST_TERM_VERTEX_BYTES];size_t n;
 /* Original public MUL POSITION,INPUT2.xxxx,PARAM0.xyzw preserves the
  * input scalar while destination and coefficient advance. Its B5 writer
  * receives signed deltas1,0,1,0; no trailing reset is emitted before the
  * original4252d tail. Construct fields, never append an opaque byte image. */
 struct remesa_usse_component_control control={0};
 const int32_t repeat_delta[4]={1,0,1,0};
 for(unsigned i=0;i<4;i++)control.operand[i]=
  (struct remesa_usse_component_operand){
   .format=REMESA_USSE_COMPONENT_SIGNED,.value=repeat_delta[i]};
 int e=remesa_usse_component_control_encode(&control,bytes,8,&n);if(e)return e;
 const struct remesa_usse_cpu_parameter_vertex mul={
  .position_base=r.position_base,.parameter_base=r.parameter_base,
  .op=REMESA_USSE_ARITHMETIC_MULTIPLY};
 e=remesa_usse_cpu_parameter_vertex_encode(&mul,bytes+8,16,&n);if(e)return e;
 memcpy(out,bytes,sizeof(bytes));*written=sizeof(bytes);return 0;
}

int remesa_usse_lighting_body_encode(const struct remesa_usse_lighting_body *request,
 void *output,size_t capacity,size_t *written)
{
 if((uintptr_t)request%_Alignof(struct remesa_usse_lighting_body)||
    (uintptr_t)written%_Alignof(size_t))return EINVAL;
 if(!span(request,sizeof(*request))||!span(output,capacity)||!span(written,sizeof(*written)))return EINVAL;
 if(overlap(request,sizeof(*request),output,capacity)||
    overlap(request,sizeof(*request),written,sizeof(*written))||
    overlap(output,capacity,written,sizeof(*written)))return EINVAL;
 struct remesa_usse_lighting_body r=*request;
 if(r.reserved)return EINVAL;
 uint32_t first,second;
 if(r.op==REMESA_USSE_LIGHTING_DOT3) {
  if(r.left_base>124||r.right_base>124||(r.left_base&3)||(r.right_base&3))return ENOTSUP;
  /* kindB: major2/sub0, two source helpers, repeat3 from flags31. */
  first=(r.left_base<<7)|r.right_base|(2u<<30)|(2u<<28);
  second=(2u<<27)|(1u<<23)|(1u<<21)|(2u<<12)|1u;
 } else if(r.op==REMESA_USSE_LIGHTING_RSQ) {
  if(r.left_base>3||r.right_base)return ENOTSUP;
  /* kind5: major1/sub1. Flags201 select mask1 instead of a repeat;
   * modifier2 is preserved by helper241a79 and inserted at bit5.
   * This numeric modifier is not renamed from a hardware assumption. */
  first=(r.left_base<<7)|(2u<<30);
  second=(1u<<27)|(1u<<9)|(1u<<23)|(1u<<12)|(2u<<5)|1u;
 } else return ENOTSUP;
 if(capacity<8)return ENOSPC;
 unsigned char bytes[8];put32(bytes,first);put32(bytes+4,second);
 memcpy(output,bytes,8);*written=8;return 0;
}

int remesa_usse_cpu_arithmetic_encode(const struct remesa_usse_cpu_arithmetic *request,
 void *output,size_t capacity,size_t *written)
{
 if((uintptr_t)request%_Alignof(struct remesa_usse_cpu_arithmetic)||
    (uintptr_t)written%_Alignof(size_t))return EINVAL;
 if(!span(request,sizeof(*request))||!span(output,capacity)||!span(written,sizeof(*written)))return EINVAL;
 if(overlap(request,sizeof(*request),output,capacity)||
    overlap(request,sizeof(*request),written,sizeof(*written))||
    overlap(output,capacity,written,sizeof(*written)))return EINVAL;
 struct remesa_usse_cpu_arithmetic r=*request;
 if(r.reserved)return EINVAL;
 if(r.destination.cpu_class>2)return ENOTSUP;
 uint32_t repeat;
 if(r.flags==0x201)repeat=1u<<12;
 else if(r.flags==0x31)repeat=(2u<<12)|(1u<<21);
 else if(r.flags==0x41)repeat=(3u<<12)|(1u<<21);
 else return ENOTSUP;
 const uint32_t destination_last=r.flags==0x201?127:r.flags==0x31?125:124;
 if(r.destination.base>destination_last)return ENOTSUP;
 const bool gears=gears_arithmetic_valid(&r);
 uint32_t first=0,second=(1u<<23)|repeat|r.destination.cpu_class;
 if(r.kind==11) {
  /* Two-source DOT3 uses the same disjoint destination fields as the
   * other arithmetic records. Keep this extension to checked destinations. */
  if(r.flags!=0x31||r.destination.cpu_class!=1||
     (r.destination.base!=0&&r.destination.base!=2))return ENOTSUP;
  if(r.source[0].cpu_class||r.source[0].base)return EINVAL;
  for(unsigned i=0;i<3;i++)if(r.modifier[i])return ENOTSUP;
  for(unsigned i=1;i<3;i++)
   if(r.source[i].cpu_class!=2||r.source[i].base>124||
      (r.source[i].base&3u))return ENOTSUP;
  first=(r.source[1].base<<7)|r.source[2].base|(2u<<30)|(2u<<28);
  second|=2u<<27;
 } else if(r.kind==5) {
  if(r.flags!=0x201||r.modifier[0]!=2)return ENOTSUP;
  if(r.source[0].cpu_class!=0&&r.source[0].cpu_class!=2)return ENOTSUP;
  if(r.source[0].base>127)return ENOTSUP;
  for(unsigned i=1;i<3;i++)if(r.source[i].cpu_class||r.source[i].base||r.modifier[i])return EINVAL;
  first=(r.source[0].cpu_class<<30)|(r.source[0].base<<7);
  second|=(1u<<27)|(1u<<9)|(r.modifier[0]<<5);
 } else if(r.kind==12||r.kind==13) {
  if(gears) {
   /* Exact scalar max(dot,0) for the captured lighting graph; original
    * writer24b1c7 oracle. Other generic clamp admission is unchanged. */
   first=(r.source[1].cpu_class<<30)|(1u<<28)|
         (r.source[1].base<<7)|r.source[2].base;
   second|=(3u<<27)|(1u<<9)|(1u<<16);
  } else {
  /* Selected original POSITION+COLOR0 lowering only. Kind13 clamps the
   * copied color against CPU constant48; kind12 against52. These numeric
   * classes/indices do not qualify architectural banks or interpolation. */
  if(r.flags!=0x41||r.destination.cpu_class!=1||r.destination.base!=4)return ENOTSUP;
  if(r.source[0].cpu_class||r.source[0].base)return EINVAL;
  for(unsigned i=0;i<3;i++)if(r.modifier[i])return ENOTSUP;
  if(r.source[2].cpu_class!=6||r.source[2].base!=(r.kind==13?48u:52u))return ENOTSUP;
  if(r.kind==13) {
   if(!((r.source[1].cpu_class==2&&
         (r.source[1].base==0||r.source[1].base==4))||
        (r.source[1].cpu_class==1&&r.source[1].base==4)))return ENOTSUP;
  } else if(r.source[1].cpu_class!=1||r.source[1].base!=4)return ENOTSUP;
  first=(r.source[1].cpu_class<<30)|(1u<<28)|(r.source[1].base<<7)|r.source[2].base;
  second|=(3u<<27)|((r.kind==13?1u:0u)<<9)|(1u<<16);
  }
 } else if(r.kind==0||r.kind==2) {
  for(unsigned i=0;i<3;i++)if(r.modifier[i]&&
    !(gears&&!i&&r.modifier[i]==1))return ENOTSUP;
  if(r.modifier[0])second|=1u<<7;
  /* Original public MUL tmp,P,PARAM0; ADD POSITION,tmp,PARAM1 lowers to
   * this exact normalized kind0 record. Do not generalize coefficient
   * indices/classes from a single dense pair or the seven-bit field width. */
  const bool parameter_mad=r.kind==0&&r.flags==0x41&&
   r.destination.cpu_class==1&&r.destination.base==0&&
   r.source[0].cpu_class==2&&r.source[0].base==0&&
   r.source[1].cpu_class==3&&r.source[1].base==1&&
   r.source[2].cpu_class==3&&r.source[2].base==5;
  /* Four parameterized original writer oracles cover the actual captured
   * uniform slots4..7 with preserved full state mapping and input channels.
   * This extends only these MUL bodies, not arbitrary class3 arithmetic. */
  const bool mvp_parameter_mul=r.kind==0&&r.flags==0x41&&
   r.destination.cpu_class==0&&r.source[0].cpu_class==2&&
   r.source[0].base<4&&
   r.destination.base==(r.source[0].base==0?4u:0u)&&
   r.source[1].cpu_class==3&&r.source[1].base==17u+4u*r.source[0].base&&
   r.source[2].cpu_class==6&&r.source[2].base==48;
  if(r.source[0].cpu_class!=0&&r.source[0].cpu_class!=2)return ENOTSUP;
  /* Conservative encoded index span for ordinary numeric sources. This is
   * CPU field validation, not physical resource allocation or stride proof. */
  const uint32_t last=r.flags==0x201?127:r.flags==0x31?125:124;
  if(r.source[0].base>last)return ENOTSUP;
  first=r.source[0].base<<14;
  if(r.source[0].cpu_class==2)second|=1u<<2;
  for(unsigned i=1;i<3;i++) {
   const struct remesa_usse_arithmetic_source *s=&r.source[i];uint32_t bank=s->cpu_class;
   if(bank==6) {
    if(s->base!=48&&s->base!=52)return ENOTSUP;
    bank=1;second|=1u<<(18-i);
   } else if(bank!=0&&bank!=2&&
             !((parameter_mad||mvp_parameter_mul||gears)&&bank==3))return ENOTSUP;
   else if(s->base>last)return ENOTSUP;
   first|=s->base<<(i==1?7:0);
   first|=bank<<(i==1?30:28);
  }
  second|=r.kind<<9;
 } else return ENOTSUP;
 first|=r.destination.base<<21;
 if(capacity<8)return ENOSPC;
 unsigned char bytes[8];put32(bytes,first);put32(bytes+4,second);
 memcpy(output,bytes,8);*written=8;return 0;
}

int remesa_usse_cpu_mvp_position_body_encode(
 const struct remesa_usse_cpu_mvp_position_body *request,void *out,size_t cap,
 size_t *written)
{
 if((uintptr_t)request%_Alignof(struct remesa_usse_cpu_mvp_position_body)||
    (uintptr_t)written%_Alignof(size_t))return EINVAL;
 if(!span(request,sizeof(*request))||!span(out,cap)||!span(written,sizeof(*written)))return EINVAL;
 if(overlap(request,sizeof(*request),out,cap)||
    overlap(request,sizeof(*request),written,sizeof(*written))||
    overlap(out,cap,written,sizeof(*written)))return EINVAL;
 const struct remesa_usse_cpu_mvp_position_body r=*request;
 if(r.reserved)return EINVAL;
 if(r.position_base||(r.uniform_vectors!=9u&&r.uniform_vectors!=10u))return ENOTSUP;
 if(r.source_dwords<1u+4u*r.uniform_vectors)return ENOSPC;
 if(r.source_dwords>128u)return ENOTSUP;
 if(cap<REMESA_USSE_CPU_MVP_POSITION_BODY_BYTES)return ENOSPC;
 uint8_t bytes[REMESA_USSE_CPU_MVP_POSITION_BODY_BYTES];size_t at=0,n;
 for(unsigned term=0;term<4;term++) {
  /* MUL state already remains active for the second product. All other
   * changes below switch between scalar*vector and vector+vector access;
   * they are actual dependency controls, not CODE padding. */
  if(term!=1) {
   struct remesa_usse_component_control control={0};
   const int32_t delta[4]={1,0,1,0};
   for(unsigned i=0;i<4;i++)control.operand[i]=
    (struct remesa_usse_component_operand){
     .format=REMESA_USSE_COMPONENT_SIGNED,.value=delta[i]};
   int e=remesa_usse_component_control_encode(&control,bytes+at,sizeof(bytes)-at,&n);
   if(e)return e;
   at+=n;
  }
  /* Preserve P and all uniform values. Product and accumulator occupy
   * disjoint class0 vec4 spans; each ADD reads an accumulator component
   * before overwriting that same component. */
  struct remesa_usse_cpu_arithmetic arithmetic={.kind=0,.flags=0x41,
   .destination={0,term?0u:4u},
   .source={{2,r.position_base+term},{3,1u+4u*(4u+term)},{6,48}}};
  int e=remesa_usse_cpu_arithmetic_encode(&arithmetic,bytes+at,sizeof(bytes)-at,&n);
  if(e)return e;
  at+=n;
  if(term) {
   struct remesa_usse_component_control control={0};
   const int32_t delta[4]={1,1,0,1};
   for(unsigned i=0;i<4;i++)control.operand[i]=
    (struct remesa_usse_component_operand){
     .format=REMESA_USSE_COMPONENT_SIGNED,.value=delta[i]};
   e=remesa_usse_component_control_encode(&control,bytes+at,sizeof(bytes)-at,&n);
   if(e)return e;
   at+=n;
   arithmetic=(struct remesa_usse_cpu_arithmetic){.kind=0,.flags=0x41,
    .destination={term==3?1u:0u,term==3?0u:4u},
    .source={{0,0},{6,52},{0,4}}};
   e=remesa_usse_cpu_arithmetic_encode(&arithmetic,bytes+at,sizeof(bytes)-at,&n);
   if(e)return e;
   at+=n;
  }
 }
 if(at!=sizeof(bytes))return EPROTO;
 memcpy(out,bytes,sizeof(bytes));*written=sizeof(bytes);return 0;
}

static void gears_lighting_records(bool attribute,struct gears_record records[26])
{
 static const struct gears_record selected[2][26]={
 { /* state normal,10uniforms */
  /* copy_normal_repeat */
  {.type=GEARS_REPEAT,.data.repeat={.operand={{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=0},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=0}}}},
  /* copy_normal */
  {.type=GEARS_MOVE,.data.move={.source={3u,33u},.destination={0u,4u},.control12=2u,.control21=1u,.control23=1}},
  /* normal_length_xy */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=2u,.flags=0x201u,.destination={0u,0u},.source={{0u,4u},{0u,5u},{0u,5u}},.modifier={0u,0u,0u}}},
  /* normal_length_xyz */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x201u,.destination={0u,0u},.source={{0u,6u},{0u,6u},{0u,0u}},.modifier={0u,0u,0u}}},
  /* normal_rsq */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=5u,.flags=0x201u,.destination={0u,0u},.source={{0u,0u},{0u,0u},{0u,0u}},.modifier={2u,0u,0u}}},
  /* normalize_normal */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x31u,.destination={0u,4u},.source={{0u,0u},{0u,4u},{6u,48u}},.modifier={0u,0u,0u}}},
  /* light_dot_x */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x201u,.destination={0u,0u},.source={{0u,4u},{3u,37u},{6u,48u}},.modifier={0u,0u,0u}}},
  /* light_dot_y */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x201u,.destination={0u,1u},.source={{0u,5u},{3u,38u},{6u,48u}},.modifier={0u,0u,0u}}},
  /* light_dot_yx */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x201u,.destination={0u,0u},.source={{0u,1u},{6u,52u},{0u,0u}},.modifier={0u,0u,0u}}},
  /* light_dot_z */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x201u,.destination={0u,1u},.source={{0u,6u},{3u,39u},{6u,48u}},.modifier={0u,0u,0u}}},
  /* light_dot_zyx */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x201u,.destination={0u,0u},.source={{0u,1u},{6u,52u},{0u,0u}},.modifier={0u,0u,0u}}},
  /* diffuse_factor_max0 */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=13u,.flags=0x201u,.destination={0u,1u},.source={{0u,0u},{0u,0u},{6u,48u}},.modifier={0u,0u,0u}}},
  /* slt_zero_delta */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x201u,.destination={0u,2u},.source={{0u,0u},{6u,52u},{6u,48u}},.modifier={1u,0u,0u}}},
  /* slt_zero_select */
  {.type=GEARS_SELECT},
  /* copy_material_ambient */
  {.type=GEARS_MOVE,.data.move={.source={3u,5u},.destination={0u,4u},.control12=2u,.control21=1u,.control23=1}},
  /* add_scene_repeat */
  {.type=GEARS_REPEAT,.data.repeat={.operand={{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=0},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1}}}},
  /* material_ambient_plus_scene */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x31u,.destination={0u,4u},.source={{0u,4u},{6u,52u},{3u,1u}},.modifier={0u,0u,0u}}},
  /* diffuse_repeat */
  {.type=GEARS_REPEAT,.data.repeat={.operand={{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=0},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=0}}}},
  /* diffuse_product */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x31u,.destination={0u,8u},.source={{0u,1u},{3u,9u},{6u,48u}},.modifier={0u,0u,0u}}},
  /* add_diffuse_repeat */
  {.type=GEARS_REPEAT,.data.repeat={.operand={{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=0},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1}}}},
  /* diffuse_plus_ambient_scene */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x31u,.destination={0u,4u},.source={{0u,8u},{6u,52u},{0u,4u}},.modifier={0u,0u,0u}}},
  /* specular_repeat */
  {.type=GEARS_REPEAT,.data.repeat={.operand={{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=0},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=0}}}},
  /* specular_product */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x31u,.destination={0u,8u},.source={{0u,3u},{3u,13u},{6u,48u}},.modifier={0u,0u,0u}}},
  /* add_specular_repeat */
  {.type=GEARS_REPEAT,.data.repeat={.operand={{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=0},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1}}}},
  /* COLOR0_rgb */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x31u,.destination={1u,4u},.source={{0u,8u},{6u,52u},{0u,4u}},.modifier={0u,0u,0u}}},
  /* COLOR0_alpha_scene_w */
  {.type=GEARS_MOVE,.data.move={.source={3u,4u},.destination={1u,7u},.control12=1u,.control21=0u,.control23=1}},
 },
 { /* attribute normal,9uniforms */
  /* copy_normal_repeat */
  {.type=GEARS_REPEAT,.data.repeat={.operand={{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=0},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=0}}}},
  /* copy_normal */
  {.type=GEARS_MOVE,.data.move={.source={2u,4u},.destination={0u,4u},.control12=2u,.control21=1u,.control23=1}},
  /* normal_length_xy */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=2u,.flags=0x201u,.destination={0u,0u},.source={{0u,4u},{0u,5u},{0u,5u}},.modifier={0u,0u,0u}}},
  /* normal_length_xyz */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x201u,.destination={0u,0u},.source={{0u,6u},{0u,6u},{0u,0u}},.modifier={0u,0u,0u}}},
  /* normal_rsq */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=5u,.flags=0x201u,.destination={0u,0u},.source={{0u,0u},{0u,0u},{0u,0u}},.modifier={2u,0u,0u}}},
  /* normalize_normal */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x31u,.destination={0u,4u},.source={{0u,0u},{0u,4u},{6u,48u}},.modifier={0u,0u,0u}}},
  /* light_dot_x */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x201u,.destination={0u,0u},.source={{0u,4u},{3u,33u},{6u,48u}},.modifier={0u,0u,0u}}},
  /* light_dot_y */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x201u,.destination={0u,1u},.source={{0u,5u},{3u,34u},{6u,48u}},.modifier={0u,0u,0u}}},
  /* light_dot_yx */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x201u,.destination={0u,0u},.source={{0u,1u},{6u,52u},{0u,0u}},.modifier={0u,0u,0u}}},
  /* light_dot_z */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x201u,.destination={0u,1u},.source={{0u,6u},{3u,35u},{6u,48u}},.modifier={0u,0u,0u}}},
  /* light_dot_zyx */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x201u,.destination={0u,0u},.source={{0u,1u},{6u,52u},{0u,0u}},.modifier={0u,0u,0u}}},
  /* diffuse_factor_max0 */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=13u,.flags=0x201u,.destination={0u,1u},.source={{0u,0u},{0u,0u},{6u,48u}},.modifier={0u,0u,0u}}},
  /* slt_zero_delta */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x201u,.destination={0u,2u},.source={{0u,0u},{6u,52u},{6u,48u}},.modifier={1u,0u,0u}}},
  /* slt_zero_select */
  {.type=GEARS_SELECT},
  /* copy_material_ambient */
  {.type=GEARS_MOVE,.data.move={.source={3u,5u},.destination={0u,4u},.control12=2u,.control21=1u,.control23=1}},
  /* add_scene_repeat */
  {.type=GEARS_REPEAT,.data.repeat={.operand={{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=0},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1}}}},
  /* material_ambient_plus_scene */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x31u,.destination={0u,4u},.source={{0u,4u},{6u,52u},{3u,1u}},.modifier={0u,0u,0u}}},
  /* diffuse_repeat */
  {.type=GEARS_REPEAT,.data.repeat={.operand={{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=0},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=0}}}},
  /* diffuse_product */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x31u,.destination={0u,8u},.source={{0u,1u},{3u,9u},{6u,48u}},.modifier={0u,0u,0u}}},
  /* add_diffuse_repeat */
  {.type=GEARS_REPEAT,.data.repeat={.operand={{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=0},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1}}}},
  /* diffuse_plus_ambient_scene */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x31u,.destination={0u,4u},.source={{0u,8u},{6u,52u},{0u,4u}},.modifier={0u,0u,0u}}},
  /* specular_repeat */
  {.type=GEARS_REPEAT,.data.repeat={.operand={{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=0},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=0}}}},
  /* specular_product */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x31u,.destination={0u,8u},.source={{0u,3u},{3u,13u},{6u,48u}},.modifier={0u,0u,0u}}},
  /* add_specular_repeat */
  {.type=GEARS_REPEAT,.data.repeat={.operand={{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=0},{.format=REMESA_USSE_COMPONENT_SIGNED,.value=1}}}},
  /* COLOR0_rgb */
  {.type=GEARS_ARITHMETIC,.data.arithmetic={.kind=0u,.flags=0x31u,.destination={1u,4u},.source={{0u,8u},{6u,52u},{0u,4u}},.modifier={0u,0u,0u}}},
  /* COLOR0_alpha_scene_w */
  {.type=GEARS_MOVE,.data.move={.source={3u,4u},.destination={1u,7u},.control12=1u,.control21=0u,.control23=1}},
 },
 };
 memcpy(records,selected[attribute?1:0],sizeof(selected[0]));
}
static bool gears_arithmetic_valid(const struct remesa_usse_cpu_arithmetic *r)
{
 struct gears_record records[26];
 for(unsigned variant=0;variant<2;variant++) {
  gears_lighting_records(variant!=0,records);
  for(unsigned i=0;i<26;i++)if(records[i].type==GEARS_ARITHMETIC&&
    !memcmp(r,&records[i].data.arithmetic,sizeof(*r)))return true;
 }
 return false;
}
static void gears_slt_select_encode(uint8_t bytes[8])
{
 /* Original247229 selected EF/flags20000201/control200000/test11.
  * The public SLT normalizer first forms zero-dot, then selects literal52
  * (one) versus48 (zero). Fixed condition class0/base2, result class0/base3.
  * These are CPU-writer fields, not a generic conditional opcode claim. */
 put32(bytes,(3u<<21)|(2u<<14)|(52u<<7)|48u|(1u<<30)|(1u<<28));
 put32(bytes+4,(5u<<27)|(1u<<23)|(1u<<22)|(1u<<17)|(1u<<16)|
               (1u<<12)|(1u<<10));
}
int remesa_usse_cpu_gears_vertex_body_encode(
 const struct remesa_usse_cpu_gears_vertex_body *request,void *out,size_t cap,
 size_t *written)
{
 if((uintptr_t)request%_Alignof(struct remesa_usse_cpu_gears_vertex_body)||
    (uintptr_t)written%_Alignof(size_t))return EINVAL;
 if(!span(request,sizeof(*request))||!span(out,cap)||!span(written,sizeof(*written)))return EINVAL;
 if(overlap(request,sizeof(*request),out,cap)||
    overlap(request,sizeof(*request),written,sizeof(*written))||
    overlap(out,cap,written,sizeof(*written)))return EINVAL;
 const struct remesa_usse_cpu_gears_vertex_body r=*request;
 if(r.reserved)return EINVAL;
 if(r.normal_from_attribute>1||r.uniform_vectors!=(r.normal_from_attribute?9u:10u)||
    r.primitive!=REMESA_USSE_CPU_GEARS_TRIANGLES)return ENOTSUP;
 if(r.source_dwords<1u+4u*r.uniform_vectors)return ENOSPC;
 if(r.source_dwords>128u)return ENOTSUP;
 if(cap<REMESA_USSE_CPU_GEARS_VERTEX_BODY_BYTES)return ENOSPC;
 uint8_t bytes[REMESA_USSE_CPU_GEARS_VERTEX_BODY_BYTES];size_t at=0,n;
 const struct remesa_usse_cpu_mvp_position_body position={
  .uniform_vectors=r.uniform_vectors,.source_dwords=r.source_dwords};
 int error=remesa_usse_cpu_mvp_position_body_encode(&position,bytes,sizeof(bytes),&n);
 if(error)return error;
 at=n;
 struct gears_record records[26];gears_lighting_records(r.normal_from_attribute!=0,records);
 for(unsigned i=0;i<26;i++) {
  const struct gears_record *e=&records[i];
  if(e->type==GEARS_REPEAT)error=remesa_usse_component_control_encode(&e->data.repeat,bytes+at,sizeof(bytes)-at,&n);
  else if(e->type==GEARS_MOVE)error=remesa_usse_cpu_move_encode(&e->data.move,bytes+at,sizeof(bytes)-at,&n);
  else if(e->type==GEARS_ARITHMETIC)error=remesa_usse_cpu_arithmetic_encode(&e->data.arithmetic,bytes+at,sizeof(bytes)-at,&n);
  else if(e->type==GEARS_SELECT){gears_slt_select_encode(bytes+at);n=8;error=0;}
  else return EPROTO;
  if(error)return error;
  if(n!=8)return EPROTO;
  at+=n;
 }
 if(at!=sizeof(bytes))return EPROTO;
 memcpy(out,bytes,sizeof(bytes));*written=sizeof(bytes);return 0;
}

int remesa_usse_cpu_color_output_clamp_encode(void *out,size_t capacity,size_t *written)
{
 if((uintptr_t)written%_Alignof(size_t)||!span(out,capacity)||
    !span(written,sizeof(*written))||overlap(out,capacity,written,sizeof(*written)))return EINVAL;
 if(capacity<REMESA_USSE_CPU_COLOR_OUTPUT_CLAMP_BYTES)return ENOSPC;
 uint8_t bytes[REMESA_USSE_CPU_COLOR_OUTPUT_CLAMP_BYTES];size_t n;
 struct remesa_usse_component_control control={0};
 for(unsigned i=0;i<4;i++)control.operand[i]=(struct remesa_usse_component_operand){
  .format=REMESA_USSE_COMPONENT_SIGNED,.value=i==3?0:1};
 int error=remesa_usse_component_control_encode(&control,bytes,8,&n);if(error)return error;
 struct remesa_usse_cpu_arithmetic clamp={.kind=13,.flags=0x41,.destination={1,4},
  .source={{0,0},{1,4},{6,48}}};
 error=remesa_usse_cpu_arithmetic_encode(&clamp,bytes+8,8,&n);if(error)return error;
 clamp.kind=12;clamp.source[2].base=52;
 error=remesa_usse_cpu_arithmetic_encode(&clamp,bytes+16,8,&n);if(error)return error;
 memcpy(out,bytes,sizeof(bytes));*written=sizeof(bytes);return 0;
}

int remesa_usse_cpu_normalize_scale_encode(const struct remesa_usse_cpu_normalize_scale *request,
 void *output,size_t capacity,size_t *written)
{
 if((uintptr_t)request%_Alignof(struct remesa_usse_cpu_normalize_scale)||
    (uintptr_t)written%_Alignof(size_t))return EINVAL;
 if(!span(request,sizeof(*request))||!span(output,capacity)||!span(written,sizeof(*written)))return EINVAL;
 if(overlap(request,sizeof(*request),output,capacity)||
    overlap(request,sizeof(*request),written,sizeof(*written))||
    overlap(output,capacity,written,sizeof(*written)))return EINVAL;
 struct remesa_usse_cpu_normalize_scale r=*request;
 if(r.reserved)return EINVAL;
 if(r.normal_base>124||r.value_base>124)return ENOTSUP;
 if(r.normal_base!=r.value_base&&r.normal_base<r.value_base+4&&r.value_base<r.normal_base+3)return ENOTSUP;
 if(capacity<REMESA_USSE_CPU_NORMALIZE_SCALE_BYTES)return ENOSPC;
 const struct remesa_usse_arithmetic_source normal={2,r.normal_base},value={2,r.value_base};
 /* This profile has no later normal-input read unless it is also the value
  * input. Reuse only that dead input; preserve the shared live input in a
  * separate class0 temporary. This is the observed CPU allocation contract,
  * not a physical bank-alias or resource-allocation assertion. */
 const struct remesa_usse_arithmetic_source temporary=
  r.normal_base==r.value_base?(struct remesa_usse_arithmetic_source){0,0}:normal;
 struct remesa_usse_cpu_arithmetic ir={.kind=2,.flags=0x201,.destination=temporary,
  .source={normal,{2,r.normal_base+1},{2,r.normal_base+1}}};
 uint8_t bytes[REMESA_USSE_CPU_NORMALIZE_SCALE_BYTES];size_t n;
 int e=remesa_usse_cpu_arithmetic_encode(&ir,bytes,8,&n);if(e)return e;
 ir.kind=0;ir.source[0]=(struct remesa_usse_arithmetic_source){2,r.normal_base+2};
 ir.source[1]=ir.source[0];ir.source[2]=temporary;
 e=remesa_usse_cpu_arithmetic_encode(&ir,bytes+8,8,&n);if(e)return e;
 ir=(struct remesa_usse_cpu_arithmetic){.kind=5,.flags=0x201,.destination=temporary,
  .source={temporary},.modifier={2}};
 e=remesa_usse_cpu_arithmetic_encode(&ir,bytes+16,8,&n);if(e)return e;
 /* The repeated output is contiguous; scalar temporary and constant are
  * reused, while the value vector advances. These per-iteration deltas
  * produce the control fields through the typed codec, never a byte template. */
 const uint32_t repeat_access[4][2]={{0,1},{temporary.base,temporary.base},
  {value.base,value.base+1},{48,48}};
 struct remesa_usse_component_control control={0};
 for(unsigned i=0;i<4;i++) {
  control.operand[i].format=REMESA_USSE_COMPONENT_SIGNED;
  control.operand[i].value=(int32_t)(repeat_access[i][1]-repeat_access[i][0]);
 }
 e=remesa_usse_component_control_encode(&control,bytes+24,8,&n);if(e)return e;
 ir=(struct remesa_usse_cpu_arithmetic){.kind=0,.flags=0x41,.destination={1,0},
  .source={temporary,value,{6,48}}};
 e=remesa_usse_cpu_arithmetic_encode(&ir,bytes+32,8,&n);if(e)return e;
 memcpy(output,bytes,sizeof(bytes));*written=sizeof(bytes);return 0;
}

int remesa_usse_cpu_normalize_scale_vertex_encode(const struct remesa_usse_cpu_normalize_scale *request,
 void *output,size_t capacity,size_t *written)
{
 if((uintptr_t)request%_Alignof(struct remesa_usse_cpu_normalize_scale)||
    (uintptr_t)written%_Alignof(size_t))return EINVAL;
 if(!span(request,sizeof(*request))||!span(output,capacity)||!span(written,sizeof(*written)))return EINVAL;
 if(overlap(request,sizeof(*request),output,capacity)||
    overlap(request,sizeof(*request),written,sizeof(*written))||
    overlap(output,capacity,written,sizeof(*written)))return EINVAL;
 uint8_t bytes[REMESA_USSE_CPU_NORMALIZE_SCALE_VERTEX_BYTES];size_t n;
 int e=remesa_usse_cpu_normalize_scale_encode(request,bytes,40,&n);if(e)return e;
 if(capacity<sizeof(bytes))return ENOSPC;
 /* Actual setter2e7f2/link2e72c returns auxiliary_count0 for all three
  * valid profile variants. Original4252d appends the same typed30a09 tail
  * and finalizer30fb7; no physical export/END meaning is assigned. */
 const struct remesa_usse_cpu_three_source tail={.source={{0,0},{2,0},{2,0}},
  .source_extension={0,1,1},.word0_control21=1,
  .word1_control12_13=1,.word1_control18=1};
 e=remesa_usse_cpu_three_source_encode(&tail,bytes+40,8,&n);if(e)return e;
 memcpy(output,bytes,sizeof(bytes));*written=sizeof(bytes);return 0;
}

int remesa_usse_cpu_position_color_vertex_encode(const struct remesa_usse_cpu_position_color *request,
 void *output,size_t capacity,size_t *written)
{
 if((uintptr_t)request%_Alignof(struct remesa_usse_cpu_position_color)||
    (uintptr_t)written%_Alignof(size_t))return EINVAL;
 if(!span(request,sizeof(*request))||!span(output,capacity)||!span(written,sizeof(*written)))return EINVAL;
 if(overlap(request,sizeof(*request),output,capacity)||
    overlap(request,sizeof(*request),written,sizeof(*written))||
    overlap(output,capacity,written,sizeof(*written)))return EINVAL;
 const struct remesa_usse_cpu_position_color r=*request;
 if(r.reserved)return EINVAL;
 if((r.position_base!=0&&r.position_base!=4)||(r.color_base!=0&&r.color_base!=4)||
    (r.position_base==4&&r.color_base==4))return ENOTSUP;
 if(capacity<REMESA_USSE_CPU_POSITION_COLOR_BYTES)return ENOSPC;
 uint8_t bytes[REMESA_USSE_CPU_POSITION_COLOR_BYTES];size_t n;int e;
 const struct remesa_usse_cpu_move position={.source={2,r.position_base},.destination={1,0},
  .control12=3,.control21=1,.control23=1};
 e=remesa_usse_cpu_move_encode(&position,bytes,8,&n);if(e)return e;
 struct remesa_usse_component_control control={0};
 for(unsigned i=0;i<4;i++)control.operand[i]=(struct remesa_usse_component_operand){
  .format=REMESA_USSE_COMPONENT_SIGNED,.value=i==3?0:1};
 e=remesa_usse_component_control_encode(&control,bytes+8,8,&n);if(e)return e;
 struct remesa_usse_cpu_arithmetic clamp={.kind=13,.flags=0x41,.destination={1,4},
  .source={{0,0},{2,r.color_base},{6,48}}};
 e=remesa_usse_cpu_arithmetic_encode(&clamp,bytes+16,8,&n);if(e)return e;
 clamp.kind=12;clamp.source[1]=(struct remesa_usse_arithmetic_source){1,4};clamp.source[2].base=52;
 e=remesa_usse_cpu_arithmetic_encode(&clamp,bytes+24,8,&n);if(e)return e;
 const struct remesa_usse_cpu_three_source tail={.source={{0,0},{2,0},{2,0}},
  .source_extension={0,1,1},.word0_control21=1,.word1_control12_13=1,.word1_control18=1};
 e=remesa_usse_cpu_three_source_encode(&tail,bytes+32,8,&n);if(e)return e;
 memcpy(output,bytes,sizeof(bytes));*written=sizeof(bytes);return 0;
}

int remesa_usse_cpu_white_lambert_vertex_encode(
 const struct remesa_usse_cpu_white_lambert *request,void *output,
 size_t capacity,size_t *written)
{
 if((uintptr_t)request%_Alignof(struct remesa_usse_cpu_white_lambert)||
    (uintptr_t)written%_Alignof(size_t))return EINVAL;
 if(!span(request,sizeof(*request))||!span(output,capacity)||!span(written,sizeof(*written)))return EINVAL;
 if(overlap(request,sizeof(*request),output,capacity)||
    overlap(request,sizeof(*request),written,sizeof(*written))||
    overlap(output,capacity,written,sizeof(*written)))return EINVAL;
 const struct remesa_usse_cpu_white_lambert r=*request;
 if(r.reserved)return EINVAL;
 const bool two=(r.position_base==0&&
                 ((r.normal_base==4&&r.light_base==0)||
                  (r.normal_base==0&&r.light_base==4)))||
                (r.position_base==4&&r.normal_base==0&&r.light_base==4);
 const bool three=r.position_base==0&&
                  ((r.normal_base==4&&r.light_base==8)||
                   (r.normal_base==8&&r.light_base==4));
 if(!two&&!three)return ENOTSUP;
 if(capacity<REMESA_USSE_CPU_WHITE_LAMBERT_VERTEX_BYTES)return ENOSPC;
 uint8_t bytes[REMESA_USSE_CPU_WHITE_LAMBERT_VERTEX_BYTES];size_t n;int e;
 struct remesa_usse_cpu_move move={.source={2,r.position_base},
  .destination={1,0},.control12=3,.control21=1,.control23=1};
 e=remesa_usse_cpu_move_encode(&move,bytes,8,&n);if(e)return e;
 struct remesa_usse_cpu_arithmetic ir={.kind=11,.flags=0x31,
  .destination={1,2},.source={{0,0},{2,r.normal_base},{2,r.light_base}}};
 e=remesa_usse_cpu_arithmetic_encode(&ir,bytes+8,8,&n);if(e)return e;
 struct remesa_usse_component_control control={0};
 for(unsigned i=0;i<4;i++)control.operand[i]=
  (struct remesa_usse_component_operand){.format=REMESA_USSE_COMPONENT_SIGNED,
   .value=(i==0||i==3)?1:0};
 e=remesa_usse_component_control_encode(&control,bytes+16,8,&n);if(e)return e;
 move=(struct remesa_usse_cpu_move){.source={1,4},.destination={1,5},
  .control12=2,.control21=1,.control23=1};
 e=remesa_usse_cpu_move_encode(&move,bytes+24,8,&n);if(e)return e;
 for(unsigned i=0;i<4;i++)control.operand[i].value=i==3?0:1;
 e=remesa_usse_component_control_encode(&control,bytes+32,8,&n);if(e)return e;
 ir=(struct remesa_usse_cpu_arithmetic){.kind=13,.flags=0x41,
  .destination={1,4},.source={{0,0},{1,4},{6,48}}};
 e=remesa_usse_cpu_arithmetic_encode(&ir,bytes+40,8,&n);if(e)return e;
 ir.kind=12;ir.source[2].base=52;
 e=remesa_usse_cpu_arithmetic_encode(&ir,bytes+48,8,&n);if(e)return e;
 const struct remesa_usse_cpu_three_source tail={.source={{0,0},{2,0},{2,0}},
  .source_extension={0,1,1},.word0_control21=1,
  .word1_control12_13=1,.word1_control18=1};
 e=remesa_usse_cpu_three_source_encode(&tail,bytes+56,8,&n);if(e)return e;
 memcpy(output,bytes,sizeof(bytes));*written=sizeof(bytes);return 0;
}
