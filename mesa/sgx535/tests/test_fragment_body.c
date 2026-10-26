/* SPDX-License-Identifier: MIT; original candidate grammar/dataflow tests. */
#include "remesa_fragment_body.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do {assert(x);checks++;} while(0)
int main(void);

static struct remesa_usse_program color_program(unsigned color,unsigned values,unsigned reverse)
{
 struct remesa_usse_program p={.schema=REMESA_USSE_SCHEMA,.stage=REMESA_IR_FRAGMENT};
 const unsigned channels[3][4]={{0,1,0,1},{1,0,1,1},{1,1,0,1}};
 for(unsigned k=0;k<values;k++) {
  struct remesa_usse_instruction *i=&p.instructions[p.count++];
  i->dst=k;i->src=REMESA_USSE_NO_VALUE;
  if(k<2) {i->op=REMESA_USSE_CONSTANT;i->bits=((k^reverse)&1)?UINT32_C(0x3f800000):0;}
  else {i->op=REMESA_USSE_MOVE;i->src=k-2;}
 }
 for(unsigned c=0;c<4;c++) {
  struct remesa_usse_instruction *i=&p.instructions[p.count++];
  i->op=REMESA_USSE_EXPORT_COLOR;i->dst=REMESA_USSE_NO_VALUE;i->channel=c;
  i->src=values-2+(channels[color][c]^reverse);
 }
 return p;
}
static uint32_t get32(const unsigned char *p)
{uint32_t v=0;for(unsigned n=0;n<4;n++)v|=(uint32_t)p[n]<<(8*n);return v;}
static void put32(unsigned char *p,uint32_t v)
{for(unsigned n=0;n<4;n++)p[n]=(unsigned char)(v>>(8*n));}
static void reject_bytes(const unsigned char *bytes,size_t length)
{
 struct remesa_fragment_body out,saved;memset(&out,0xa5,sizeof(out));saved=out;
 CHECK(remesa_fragment_body_decode(bytes,length,&out)!=0);
 CHECK(!memcmp(&out,&saved,sizeof(out)));
}
static void reject_program(const struct remesa_usse_program *p,
 const struct remesa_fragment_registers *r)
{
 struct remesa_fragment_body out,saved;memset(&out,0xa5,sizeof(out));saved=out;
 CHECK(remesa_fragment_body_prepare_at(p,r,&out)!=0);
 CHECK(!memcmp(&out,&saved,sizeof(out)));
}
int main(void)
{
 const struct remesa_fragment_registers assignments[]={{{0,1},2},{{127,63},31},{{31,0},127}};
 const unsigned counts[]={2,4,64};
 struct remesa_fragment_body green={0};
 for(unsigned color=0;color<3;color++)for(unsigned reverse=0;reverse<2;reverse++)
  for(unsigned r=0;r<3;r++) {
   struct remesa_fragment_body reference={0};
   for(unsigned k=0;k<3;k++) {
    struct remesa_usse_program p=color_program(color,counts[k],reverse),saved=p;
    struct remesa_fragment_body body,decoded;
    CHECK(!remesa_fragment_body_prepare_at(&p,&assignments[r],&body));
    CHECK(!memcmp(&p,&saved,sizeof(p)));
    CHECK(body.schema==REMESA_FRAGMENT_BODY_SCHEMA&&
          body.status==REMESA_FRAGMENT_BODY_CPU_CANDIDATE);
    CHECK(!remesa_fragment_body_validate(&body));
    CHECK(!remesa_fragment_body_decode(body.records,sizeof(body.records),&decoded));
    CHECK(!memcmp(&body,&decoded,sizeof(body)));
    if(!k)reference=body;else CHECK(!memcmp(&body,&reference,sizeof(body)));
    memset(&p,0,sizeof(p));CHECK(!remesa_fragment_body_validate(&body));
    if(!color&&!reverse&&!r&&!k)green=body;
   }
  }
 struct remesa_usse_program p=color_program(0,2,0);
 struct remesa_fragment_body body;
 CHECK(!remesa_fragment_body_prepare(&p,&body));CHECK(!memcmp(&body,&green,sizeof(body)));
 /* Change exactly one numeric register. This independent XOR expectation
  * checks its complete cross-record footprint and every unchanged second word. */
 for(unsigned selected=0;selected<3;selected++) {
  struct remesa_fragment_registers r={{0,1},2};
  uint32_t difference;
  if(selected<2) {difference=r.literals[selected]^127u;r.literals[selected]=127;}
  else {difference=r.packed^127u;r.packed=127;}
  CHECK(!remesa_fragment_body_prepare_at(&p,&r,&body));
  for(unsigned record=0;record<7;record++) {
   uint32_t expected=0;
   if(selected<2) {
    if(record==selected)expected=difference<<21;
    if(record>=2&&record<6&&green.channel_sources[record-2]==selected)expected=difference<<7;
   } else {
    if(record>=2&&record<6)expected=difference<<21;
    if(record==6)expected=difference<<7;
   }
   CHECK((get32(body.records+8*record)^get32(green.records+8*record))==expected);
   CHECK(get32(body.records+8*record+4)==get32(green.records+8*record+4));
  }
 }
 /* Exact framing, controls, unsupported bits, undefined sources and masks. */
 reject_bytes(green.records,55);reject_bytes(green.records,57);reject_bytes(NULL,56);
 const unsigned word_offsets[]={0,4,16,20,48,52};
 const uint32_t forbidden[]={UINT32_C(1)<<29,UINT32_C(1)<<22,
  UINT32_C(1)<<30,UINT32_C(1)<<11,UINT32_C(1)<<21,UINT32_C(1)<<11};
 for(unsigned n=0;n<6;n++) {
  body=green;put32(body.records+word_offsets[n],get32(body.records+word_offsets[n])^forbidden[n]);
  reject_bytes(body.records,sizeof(body.records));CHECK(remesa_fragment_body_validate(&body)!=0);
 }
 body=green;put32(body.records+16,(UINT32_C(1)<<29)|(UINT32_C(2)<<21)|(UINT32_C(3)<<7));
 reject_bytes(body.records,sizeof(body.records));
 body=green;put32(body.records+20,get32(body.records+20)|(UINT32_C(2)<<2));
 reject_bytes(body.records,sizeof(body.records));
 body=green;put32(body.records+52,get32(body.records+52)&~UINT32_C(1));
 reject_bytes(body.records,sizeof(body.records));
 body=green;body.schema++;CHECK(remesa_fragment_body_validate(&body)==ENOTSUP);
 body=green;body.status=0;CHECK(remesa_fragment_body_validate(&body)==ENOTSUP);
 body=green;body.registers.packed=3;CHECK(remesa_fragment_body_validate(&body)==EPROTO);
 body=green;body.channel_sources[0]=1;CHECK(remesa_fragment_body_validate(&body)==EPROTO);
 body=green;body.literal_payloads[0]^=UINT32_C(1);CHECK(remesa_fragment_body_validate(&body)==EPROTO);
 const struct remesa_fragment_registers invalid[]={{{0,0},2},{{0,1},0},{{0,1},1},
  {{128,1},2},{{0,128},2},{{0,1},128},{{UINT32_MAX,1},2}};
 for(unsigned n=0;n<sizeof(invalid)/sizeof(invalid[0]);n++)reject_program(&p,&invalid[n]);
 reject_program(&p,NULL);reject_program(NULL,&assignments[0]);
 p.stage=REMESA_IR_VERTEX;reject_program(&p,&assignments[0]);
 p=color_program(0,2,0);p.instructions[0].bits=UINT32_C(0x3f000000);reject_program(&p,&assignments[0]);
 p=color_program(0,2,0);p.instructions[2].src=2;reject_program(&p,&assignments[0]);
 body=green;struct remesa_fragment_body saved=body;
 p=color_program(0,2,0);
 CHECK(remesa_fragment_body_prepare_at(&p,&body.registers,&body)==EINVAL);
 CHECK(!memcmp(&body,&saved,sizeof(body)));
 CHECK(remesa_fragment_body_decode(body.records,sizeof(body.records),&body)==EINVAL);
 CHECK(!memcmp(&body,&saved,sizeof(body)));
 CHECK(remesa_fragment_body_prepare_at(&p,(const void *)&p.instructions[0],&body)==EINVAL);
 CHECK(!memcmp(&body,&saved,sizeof(body)));
 CHECK(remesa_fragment_body_validate(NULL)==EINVAL);
 CHECK(remesa_fragment_body_prepare(&p,NULL)==EINVAL);
 /* The existing packed constant body is now reachable through the public
  * stage encoder. Full pipeline/dispatch admission is still independent. */
 unsigned char bytes[56],saved_bytes[56];memset(bytes,0xa5,sizeof(bytes));memcpy(saved_bytes,bytes,sizeof(bytes));
 size_t written=999;
 CHECK(!remesa_usse_encode(&p,bytes,sizeof(bytes),&written)&&written==8);
 struct remesa_fragment_constant constant;
 CHECK(!remesa_fragment_constant_decode(bytes,written,&constant)&&constant.argb==0xff00ff00);
 CHECK(!memcmp(bytes+written,saved_bytes+written,sizeof(bytes)-written));
 printf("PASS %u fragment body checks; original CPU candidate, no executable qualification\n",checks);
 return 0;
}
