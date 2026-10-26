/* SPDX-License-Identifier: MIT */
/* Original semantic/field tests; no retained shader bytes or GPU execution. */
#include "remesa_fragment_body.h"
#include "remesa_shader_program.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(c) do {assert(c);++checks;} while(0)
static uint32_t word(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int main(void)
{
 const uint32_t colors[]={UINT32_C(0xff00ff00),UINT32_C(0xffff00ff),UINT32_C(0xffffff00)};
 for(unsigned k=0;k<3;k++) {
  struct remesa_shader_program program;
  struct remesa_fragment_constant record,decoded,saved;
  CHECK(!remesa_shader_program_prepare_color(colors[k],&program));
  CHECK(!remesa_fragment_constant_prepare(&program.usse,&record));
  CHECK(!remesa_fragment_constant_validate(&record));
  CHECK(record.argb==colors[k]&&record.status==REMESA_FRAGMENT_BODY_CPU_CANDIDATE);
  uint32_t a=word(record.record),b=word(record.record+4);
  CHECK(a==(colors[k]&((UINT32_C(1)<<21)-1)));
  CHECK(((b>>4)&31u)==((colors[k]>>21)&31u));
  CHECK(((b>>12)&63u)==((colors[k]>>26)&63u));
  CHECK((b>>27)==31&&((b>>24)&7u)==4&&(b&15u)==1);
  CHECK((b&(UINT32_C(1)<<18))&&(b&(UINT32_C(1)<<23)));
  CHECK(!remesa_fragment_constant_decode(record.record,sizeof(record.record),&decoded));
  CHECK(!memcmp(&record,&decoded,sizeof(record)));
  saved=record;
  for(unsigned bit=0;bit<64;bit++) {
   unsigned char changed[8];memcpy(changed,record.record,sizeof(changed));
   changed[bit/8]^=(unsigned char)(1u<<(bit%8));
   CHECK(remesa_fragment_constant_decode(changed,sizeof(changed),&decoded)==ENOTSUP);
   CHECK(!memcmp(&decoded,&saved,sizeof(saved)));
  }
  CHECK(remesa_fragment_constant_decode(record.record,7,&decoded)==EINVAL);
  CHECK(remesa_fragment_constant_decode(record.record,9,&decoded)==EINVAL);
  CHECK(remesa_fragment_constant_decode(record.record,8,(void *)record.record)==EINVAL);
  record.argb=colors[(k+1)%3];CHECK(remesa_fragment_constant_validate(&record)==EPROTO);
  record=saved;record.schema++;CHECK(remesa_fragment_constant_validate(&record)==ENOTSUP);
  record=saved;record.status++;CHECK(remesa_fragment_constant_validate(&record)==ENOTSUP);
 }
 /* Payload-field support alone does not admit more semantic color profiles. */
 for(unsigned mask=0;mask<16;mask++) {
  uint32_t color=((mask&8)?UINT32_C(0xff000000):0)|
   ((mask&1)?UINT32_C(0x00ff0000):0)|
   ((mask&2)?UINT32_C(0x0000ff00):0)|((mask&4)?UINT32_C(0x000000ff):0);
  if(color==colors[0]||color==colors[1]||color==colors[2])continue;
  struct remesa_shader_program rejected;
  CHECK(remesa_shader_program_prepare_color(color,&rejected)==ENOTSUP);
 }
 struct remesa_shader_program yellow;
 struct remesa_fragment_constant untouched,saved;
 memset(&untouched,0x5a,sizeof(untouched));saved=untouched;
 CHECK(!remesa_shader_program_prepare_color(UINT32_C(0xffffff00),&yellow));
 for(unsigned n=0;n<yellow.usse.count;n++)
  if(yellow.usse.instructions[n].op==REMESA_USSE_CONSTANT &&
     yellow.usse.instructions[n].bits==UINT32_C(0x3f800000))
   yellow.usse.instructions[n].bits=UINT32_C(0x3f000000);
 CHECK(remesa_fragment_constant_prepare(&yellow.usse,&untouched)==ENOTSUP);
 CHECK(!memcmp(&untouched,&saved,sizeof(saved)));
 CHECK(remesa_fragment_constant_prepare(NULL,&untouched)==EINVAL);
 CHECK(remesa_fragment_constant_validate(NULL)==EINVAL);
 printf("PASS: %u packed-constant CPU field/semantic checks; hardware execution gated\n",checks);
}
