/* SPDX-License-Identifier: MIT
 * Independent original CPU field composition, centralized from the project's
 * private original candidate_fragment_body/literal/pack/output_move codecs.
 * No proprietary program, sequence, implementation or table is included.
 *
 * Provenance (private research, 2026-10-11):
 * psb-usse-encoder-isolated-20261011/independent-shader-readiness-20261011/
 *   literal-codec/FORMAT.md and critical-output/SELECTED-PACK-FIELDS.md;
 * candidate-mov-codec-session/candidate_mov.c.
 * These conditional CPU writer relationships do not prove GPU semantics.
 * Selected numeric classes are0 for literal/packing sources and destinations,
 * and1/number0 for the final output binding. All modifiers/controls are zero.
 */
#include "remesa_fragment_body.h"
#include "remesa_nir.h"
#include <errno.h>
#include <stdbool.h>
#include <string.h>

enum {
 NUMBER_SHIFT=21, SOURCE_SHIFT=7,
 PAYLOAD_MIDDLE_SHIFT=4, PAYLOAD_HIGH_SHIFT=12,
 CPU_CLASSIFIER_SHIFT=27, CPU_FORMAT_SHIFT=24
};
static int overlaps(const void *a,size_t na,const void *b,size_t nb)
{
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return a&&b&&na&&nb&&(x<=y?y-x<na:x-y<nb);
}
static uint32_t get32(const unsigned char *p)
{uint32_t v=0;for(unsigned n=0;n<4;n++)v|=(uint32_t)p[n]<<(8*n);return v;}
static void put32(unsigned char *p,uint32_t v)
{for(unsigned n=0;n<4;n++)p[n]=(unsigned char)(v>>(8*n));}
static int registers_validate(const struct remesa_fragment_registers *r)
{
 if(r->literals[0]>127||r->literals[1]>127||r->packed>127)return ERANGE;
 if(r->literals[0]==r->literals[1]||r->literals[0]==r->packed||
    r->literals[1]==r->packed)return ENOTSUP;
 return 0;
}
static uint32_t literal_fixed(void)
{return (UINT32_C(31)<<CPU_CLASSIFIER_SHIFT)|(UINT32_C(4)<<CPU_FORMAT_SHIFT)|
        (UINT32_C(1)<<21);}
static uint32_t pack_fixed(void)
{return (UINT32_C(1)<<30)|(UINT32_C(1)<<16)|(UINT32_C(6)<<9);}
static uint32_t output_fixed(void)
{return (UINT32_C(5)<<CPU_CLASSIFIER_SHIFT)|(UINT32_C(1)<<16)|UINT32_C(1);}

/* Independently reconstructed field relationships, not an embedded shader
 * template. Provenance: phase9-native-program-resources-20261011/
 * retained-constant-export-verification.json (32 CPU/source/evidence checks).
 * Selected legacy CPU constructor: payload[20:0] -> word0[20:0],
 * payload[25:21] -> word1[8:4], payload[31:26] -> word1[17:12].
 * Numeric descriptor1 selects class1/number0/extension0. Default control
 * maps to bit23; finalizer byte6 bit2 maps to bit18. Those are CPU flag
 * relationships; neither is named a hardware termination control here.
 */
static uint32_t constant_fixed(void)
{return literal_fixed()|(UINT32_C(1)<<23)|(UINT32_C(1)<<18)|UINT32_C(1);}
static bool constant_supported(uint32_t argb)
{return argb==UINT32_C(0xffff00ff)||argb==UINT32_C(0xff00ff00)||
        argb==UINT32_C(0xffffff00);}
int remesa_fragment_constant_decode(const void *input,size_t bytes,
 struct remesa_fragment_constant *out)
{
 if(!input||!out||bytes!=REMESA_FRAGMENT_CONSTANT_BYTES||
    overlaps(input,bytes,out,sizeof(*out)))return EINVAL;
 const unsigned char *p=input;
 uint32_t first=get32(p),second=get32(p+4);
 const uint32_t middle=(UINT32_C(31)<<PAYLOAD_MIDDLE_SHIFT);
 const uint32_t high=(UINT32_C(63)<<PAYLOAD_HIGH_SHIFT);
 if((first&~UINT32_C(0x001fffff)) ||
    (second&~(middle|high))!=constant_fixed())return ENOTSUP;
 uint32_t argb=first|(((second>>PAYLOAD_MIDDLE_SHIFT)&31u)<<21)|
                    (((second>>PAYLOAD_HIGH_SHIFT)&63u)<<26);
 if(!constant_supported(argb))return ENOTSUP;
 struct remesa_fragment_constant record={.schema=REMESA_FRAGMENT_CONSTANT_SCHEMA,
  .status=REMESA_FRAGMENT_BODY_CPU_CANDIDATE,.argb=argb};
 memcpy(record.record,input,sizeof(record.record));*out=record;return 0;
}
int remesa_fragment_constant_validate(const struct remesa_fragment_constant *record)
{
 if(!record)return EINVAL;
 if(record->schema!=REMESA_FRAGMENT_CONSTANT_SCHEMA ||
    record->status!=REMESA_FRAGMENT_BODY_CPU_CANDIDATE)return ENOTSUP;
 struct remesa_fragment_constant decoded;
 int error=remesa_fragment_constant_decode(record->record,sizeof(record->record),&decoded);
 return error?error:decoded.argb==record->argb?0:EPROTO;
}
int remesa_fragment_constant_prepare(const struct remesa_usse_program *p,
 struct remesa_fragment_constant *out)
{
 if(!p||!out||overlaps(p,sizeof(*p),out,sizeof(*out)))return EINVAL;
 int error=remesa_usse_validate(p);if(error)return error;
 if(p->stage!=REMESA_IR_FRAGMENT)return ENOTSUP;
 uint32_t values[REMESA_USSE_VALUES]={0},channels[4]={0},argb;
 for(uint32_t n=0;n<p->count;n++) {
  const struct remesa_usse_instruction *i=&p->instructions[n];
  switch(i->op) {
  case REMESA_USSE_CONSTANT:values[i->dst]=i->bits;break;
  case REMESA_USSE_MOVE:values[i->dst]=values[i->src];break;
  case REMESA_USSE_EXPORT_COLOR:channels[i->channel]=values[i->src];break;
  default:return ENOTSUP;
  }
 }
 error=remesa_constant_rgba32(channels,&argb);if(error)return error;
 if(!constant_supported(argb))return ENOTSUP;
 struct remesa_fragment_constant record={.schema=REMESA_FRAGMENT_CONSTANT_SCHEMA,
  .status=REMESA_FRAGMENT_BODY_CPU_CANDIDATE,.argb=argb};
 put32(record.record,argb&UINT32_C(0x001fffff));
 put32(record.record+4,constant_fixed()|(((argb>>21)&31u)<<PAYLOAD_MIDDLE_SHIFT)|
                                 (((argb>>26)&63u)<<PAYLOAD_HIGH_SHIFT));
 error=remesa_fragment_constant_validate(&record);if(error)return error;
 *out=record;return 0;
}

int remesa_fragment_body_decode(const void *input,size_t bytes,
 struct remesa_fragment_body *out)
{
 if(!input||!out||bytes!=REMESA_FRAGMENT_BODY_BYTES||
    overlaps(input,bytes,out,sizeof(*out)))return EINVAL;
 const unsigned char *p=input;
 struct remesa_fragment_body b={.schema=REMESA_FRAGMENT_BODY_SCHEMA,
  .status=REMESA_FRAGMENT_BODY_CPU_CANDIDATE};
 const uint32_t payload_mask=UINT32_C(0x001fffff);
 const uint32_t high_mask=(UINT32_C(31)<<PAYLOAD_MIDDLE_SHIFT)|
                          (UINT32_C(63)<<PAYLOAD_HIGH_SHIFT);
 for(unsigned k=0;k<2;k++) {
  uint32_t first=get32(p+8*k),second=get32(p+8*k+4);
  /* Only numeric class0 and zero control22 are supported by this body. */
  if((first&~UINT32_C(0x0fffffff))||
     (second&~high_mask)!=literal_fixed())return ENOTSUP;
  b.registers.literals[k]=(first>>NUMBER_SHIFT)&127u;
  b.literal_payloads[k]=(first&payload_mask)|
   (((second>>PAYLOAD_MIDDLE_SHIFT)&31u)<<21)|
   (((second>>PAYLOAD_HIGH_SHIFT)&63u)<<26);
  if(b.literal_payloads[k]!=0&&b.literal_payloads[k]!=UINT32_C(0x3f800000))
   return ENOTSUP;
 }
 if(b.literal_payloads[0]==b.literal_payloads[1])return ENOTSUP;
 const uint32_t number_fields=(UINT32_C(127)<<SOURCE_SHIFT)|
                              (UINT32_C(127)<<NUMBER_SHIFT);
 uint32_t colors[4],used=0;
 for(unsigned c=0;c<4;c++) {
  uint32_t first=get32(p+16+8*c),second=get32(p+20+8*c);
  if((first&~number_fields)!=(UINT32_C(1)<<29)||
     second!=(pack_fixed()|((UINT32_C(1)<<c)<<2)))return ENOTSUP;
  uint32_t source=(first>>SOURCE_SHIFT)&127u;
  uint32_t destination=(first>>NUMBER_SHIFT)&127u;
  if(!c)b.registers.packed=destination;
  if(destination!=b.registers.packed)return ENOTSUP;
  if(source==b.registers.literals[0])b.channel_sources[c]=0;
  else if(source==b.registers.literals[1])b.channel_sources[c]=1;
  else return ENOTSUP;
  used|=1u<<b.channel_sources[c];
  colors[c]=b.literal_payloads[b.channel_sources[c]];
 }
 int error=registers_validate(&b.registers);if(error)return error;
 if(used!=3)return ENOTSUP;
 uint32_t color;
 error=remesa_constant_rgba32(colors,&color);if(error)return error;
 /* Zero control11 is a restriction, not a bounded hardware END contract. */
 if(get32(p+48)!=((UINT32_C(1)<<29)|(b.registers.packed<<SOURCE_SHIFT))||
    get32(p+52)!=output_fixed())return ENOTSUP;
 memcpy(b.records,p,sizeof(b.records));
 *out=b;return 0;
}

int remesa_fragment_body_validate(const struct remesa_fragment_body *b)
{
 if(!b)return EINVAL;
 if(b->schema!=REMESA_FRAGMENT_BODY_SCHEMA||
    b->status!=REMESA_FRAGMENT_BODY_CPU_CANDIDATE)return ENOTSUP;
 struct remesa_fragment_body decoded;
 int error=remesa_fragment_body_decode(b->records,sizeof(b->records),&decoded);
 if(error)return error;
 if(memcmp(b->literal_payloads,decoded.literal_payloads,sizeof(b->literal_payloads))||
    memcmp(b->channel_sources,decoded.channel_sources,sizeof(b->channel_sources))||
    b->registers.literals[0]!=decoded.registers.literals[0]||
    b->registers.literals[1]!=decoded.registers.literals[1]||
    b->registers.packed!=decoded.registers.packed)return EPROTO;
 return 0;
}

int remesa_fragment_body_prepare(const struct remesa_usse_program *p,
 struct remesa_fragment_body *out)
{
 const struct remesa_fragment_registers registers={{0,1},2};
 return remesa_fragment_body_prepare_at(p,&registers,out);
}

int remesa_fragment_body_prepare_at(const struct remesa_usse_program *p,
 const struct remesa_fragment_registers *r,struct remesa_fragment_body *out)
{
 if(!p||!r||!out||overlaps(p,sizeof(*p),out,sizeof(*out))||
    overlaps(r,sizeof(*r),out,sizeof(*out))||
    overlaps(p,sizeof(*p),r,sizeof(*r)))return EINVAL;
 int error=registers_validate(r);if(error)return error;
 error=remesa_usse_validate(p);if(error)return error;
 if(p->stage!=REMESA_IR_FRAGMENT)return ENOTSUP;
 struct remesa_fragment_body b={.schema=REMESA_FRAGMENT_BODY_SCHEMA,
  .status=REMESA_FRAGMENT_BODY_CPU_CANDIDATE,.registers=*r};
 uint32_t values[REMESA_USSE_VALUES]={0};unsigned literals=0;
 for(uint32_t n=0;n<p->count;n++) {
  const struct remesa_usse_instruction *i=&p->instructions[n];
  switch(i->op) {
  case REMESA_USSE_CONSTANT: {
   unsigned k=0;for(;k<literals;k++)if(b.literal_payloads[k]==i->bits)break;
   if(k==literals) {
    if(literals==2)return ENOTSUP;
    b.literal_payloads[literals++]=i->bits;
   }
   values[i->dst]=k;break;
  }
  case REMESA_USSE_MOVE:values[i->dst]=values[i->src];break;
  case REMESA_USSE_EXPORT_COLOR:b.channel_sources[i->channel]=values[i->src];break;
  default:return ENOTSUP;
  }
 }
 if(literals!=2)return ENOTSUP;
 for(unsigned k=0;k<2;k++) {
  uint32_t payload=b.literal_payloads[k];
  uint32_t first=(payload&UINT32_C(0x001fffff))|(r->literals[k]<<NUMBER_SHIFT);
  uint32_t second=literal_fixed()|((payload>>26)<<PAYLOAD_HIGH_SHIFT)|
   (((payload>>21)&31u)<<PAYLOAD_MIDDLE_SHIFT);
  put32(b.records+8*k,first);put32(b.records+8*k+4,second);
 }
 for(unsigned c=0;c<4;c++) {
  put32(b.records+16+8*c,(UINT32_C(1)<<29)|(r->packed<<NUMBER_SHIFT)|
   (r->literals[b.channel_sources[c]]<<SOURCE_SHIFT));
  put32(b.records+20+8*c,pack_fixed()|((UINT32_C(1)<<c)<<2));
 }
 put32(b.records+48,(UINT32_C(1)<<29)|(r->packed<<SOURCE_SHIFT));
 put32(b.records+52,output_fixed());
 error=remesa_fragment_body_validate(&b);if(error)return error;
 *out=b;return 0;
}
