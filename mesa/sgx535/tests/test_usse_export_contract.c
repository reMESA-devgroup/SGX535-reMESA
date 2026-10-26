/* SPDX-License-Identifier: MIT
 * Original semantic POSITION routes, numeric CPU resources and exact binding.
 * No proprietary programs, physical bank names, GPU submission or END claim.
 */
#include "remesa_usse.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do {assert(x);checks++;} while(0)
static struct remesa_usse_program vertex(unsigned aliases)
{
 struct remesa_usse_program p={.schema=REMESA_USSE_SCHEMA,.stage=REMESA_IR_VERTEX};
 for(unsigned c=0;c<4;c++)p.instructions[p.count++]=(struct remesa_usse_instruction){
  .op=REMESA_USSE_INPUT,.dst=c,.src=REMESA_USSE_NO_VALUE,.channel=c};
 for(unsigned n=4;n<4+aliases;n++)p.instructions[p.count++]=(struct remesa_usse_instruction){
  .op=REMESA_USSE_MOVE,.dst=n,.src=n-4};
 for(unsigned c=0;c<4;c++)p.instructions[p.count++]=(struct remesa_usse_instruction){
  .op=REMESA_USSE_EXPORT_POSITION,.dst=REMESA_USSE_NO_VALUE,.src=aliases+c,.channel=c};
 return p;
}
static struct remesa_usse_vertex_mapping mapping(unsigned source,unsigned destination)
{
 struct remesa_usse_vertex_mapping m={0};
 for(unsigned c=0;c<4;c++) {
  m.input[c]=(struct remesa_usse_cpu_register){source,c};
  m.output[c]=(struct remesa_usse_cpu_register){destination,124+c};
 }
 return m;
}
static void positive(unsigned aliases,unsigned sc,unsigned dc)
{
 struct remesa_usse_program p=vertex(aliases),saved=p;
 struct remesa_usse_vertex_mapping m=mapping(sc,dc),mapping_saved=m;
 struct remesa_usse_vertex_export_plan plan,again;
 memset(&plan,0xa5,sizeof(plan));
 CHECK(!remesa_usse_validate(&p));
 CHECK(!remesa_usse_vertex_export_plan(&p,&m,&plan));
 CHECK(!remesa_usse_vertex_export_plan(&p,&m,&again));
 CHECK(!memcmp(&plan,&again,sizeof(plan)));
 CHECK(!memcmp(&p,&saved,sizeof(p))&&!memcmp(&m,&mapping_saved,sizeof(m)));
 CHECK(!remesa_usse_vertex_export_validate(&p,&m,&plan));
 CHECK(plan.schema==REMESA_USSE_VERTEX_EXPORT_SCHEMA&&plan.stage==REMESA_IR_VERTEX&&
  plan.route_count==4&&plan.candidate_bytes==32&&!plan.reserved);
 uint64_t identity=0;
 CHECK(!remesa_usse_identity(&p,&identity)&&plan.semantic_identity==identity);
 CHECK(plan.used_values==(aliases==60?UINT64_MAX:((UINT64_C(1)<<(4+aliases))-1)));
 CHECK(plan.input_bank_error==ENOTSUP&&plan.export_bank_error==ENOTSUP&&
  plan.termination_error==ENOTSUP);
 unsigned char bytes[40],before[40];size_t written=99;
 memset(bytes,0xa5,sizeof(bytes));memcpy(before,bytes,sizeof(bytes));
 CHECK(!remesa_usse_vertex_candidate(&p,&m,bytes,sizeof(bytes),&written)&&written==32);
 CHECK(!memcmp(bytes+32,before+32,8));
 for(unsigned c=0;c<4;c++) {
  const struct remesa_usse_vertex_export_route *r=&plan.route[c];
  CHECK(r->channel==c&&r->bit_size==32&&r->export_instruction==4+aliases+c&&
   r->source_value==aliases+c&&r->input_value==c&&r->input_instruction==c&&
   r->input_channel==c&&r->candidate_offset==8*c);
  uint64_t dependency=0;
  for(unsigned v=c;v<4+aliases;v+=4)dependency|=UINT64_C(1)<<v;
  CHECK(r->dependencies==dependency);
  CHECK(r->source.class_id==sc&&r->source.number==c&&
   r->destination.class_id==dc&&r->destination.number==124+c);
  struct remesa_usse_cpu_move decoded;
  CHECK(!remesa_usse_cpu_move_decode(bytes+r->candidate_offset,8,&decoded));
  CHECK(!memcmp(&decoded.source,&r->source,sizeof(r->source))&&
   !memcmp(&decoded.destination,&r->destination,sizeof(r->destination))&&
   !decoded.control11&&!decoded.control12&&!decoded.control18&&!decoded.control21&&!decoded.control23);
 }
 for(unsigned k=0;k<3;k++) {
  const struct remesa_usse_vertex_export_resource *r=&plan.resource[k];
  CHECK(r->read_max_index_plus_one==(k==sc?4u:0u));
  CHECK(r->write_max_index_plus_one==(k==dc?128u:0u));
  for(unsigned w=0;w<4;w++) {
   CHECK(r->read_mask[w]==(k==sc&&w==0?UINT32_C(15):0));
   CHECK(r->write_mask[w]==(k==dc&&w==3?UINT32_C(15)<<28:0));
  }
 }
 /* Every byte, including reserved metadata and status fields, is bound to the
  * canonical host contract; spoofing a zero error cannot certify execution. */
 for(size_t byte=0;byte<sizeof(plan);byte++) {
  ((unsigned char *)&plan)[byte]^=1;
  CHECK(remesa_usse_vertex_export_validate(&p,&m,&plan)==EPROTO);
  ((unsigned char *)&plan)[byte]^=1;
 }
 CHECK(!remesa_usse_vertex_export_validate(&p,&m,&plan));
 m.output[0].number=123;
 CHECK(remesa_usse_vertex_export_validate(&p,&m,&plan)==EPROTO);
}
static void compiled_body(void)
{
 for(unsigned aliases=0;aliases<=60;aliases+=4) {
  struct remesa_usse_program p=vertex(aliases);
  struct remesa_usse_vertex_mapping m={0};
  for(unsigned c=0;c<4;c++) {
   m.input[c]=(struct remesa_usse_cpu_register){2,c};
   m.output[c]=(struct remesa_usse_cpu_register){1,c};
  }
  struct remesa_usse_vertex_export_plan plan;
  CHECK(!remesa_usse_vertex_export_plan(&p,&m,&plan));
  CHECK(plan.candidate_bytes==16&&plan.route_count==4);
  CHECK(plan.input_bank_error==ENOTSUP&&plan.export_bank_error==ENOTSUP&&plan.termination_error==ENOTSUP);
  for(unsigned c=0;c<4;c++)CHECK(plan.route[c].candidate_offset==0);
  unsigned char b[24],before[24];size_t n=99;
  memset(b,0xa5,sizeof(b));memcpy(before,b,sizeof(b));
  CHECK(remesa_usse_vertex_candidate(&p,&m,b,15,&n)==ENOSPC);
  CHECK(n==99&&!memcmp(b,before,sizeof(b)));
  CHECK(!remesa_usse_vertex_candidate(&p,&m,b,16,&n)&&n==16);
  CHECK(!memcmp(b+16,before+16,8));
  struct remesa_usse_cpu_move decoded;
  CHECK(!remesa_usse_cpu_move_decode(b,8,&decoded));
  CHECK(decoded.source.class_id==2&&decoded.source.number==0&&decoded.destination.class_id==1&&decoded.destination.number==0);
  CHECK(decoded.control12==3&&decoded.control21==1&&decoded.control23==1&&!decoded.control11&&!decoded.control18);
  struct remesa_usse_cpu_three_source tail;
  CHECK(!remesa_usse_cpu_three_source_decode(b+8,8,&tail));
  CHECK(tail.source[0].class_id==0&&tail.source[0].number==0&&
   tail.source[1].class_id==2&&tail.source[1].number==0&&tail.source[2].class_id==2&&tail.source[2].number==0);
  CHECK(tail.source_extension[0]==0&&tail.source_extension[1]==1&&tail.source_extension[2]==1&&
   tail.word0_control21==1&&tail.word0_control22_27==0&&tail.word1_control0_1==0&&
   tail.word1_control12_13==1&&tail.word1_control18==1);
  CHECK(remesa_usse_encode(&p,b,sizeof(b),&n)==ENOTSUP);
  n=99;
  CHECK(remesa_usse_vertex_candidate(&p,&m,(void *)&p,sizeof(p),&n)==EINVAL);
  CHECK(n==99);
  CHECK(remesa_usse_vertex_candidate(&p,&m,b,sizeof(b),(void *)b)==EINVAL);
  /* A valid changed numeric mapping must take the generic 32-byte fallback. */
  m.output[3].number=4;
  CHECK(!remesa_usse_vertex_export_plan(&p,&m,&plan)&&plan.candidate_bytes==32);
  memcpy(before,b,sizeof(b));
  CHECK(remesa_usse_vertex_candidate(&p,&m,b,sizeof(b),&n)==ENOSPC);
  CHECK(n==99&&!memcmp(b,before,sizeof(b)));
 }
}
static void negative(void)
{
 struct remesa_usse_program p=vertex(0),bad;
 struct remesa_usse_vertex_mapping m=mapping(0,2),changed;
 struct remesa_usse_vertex_export_plan out,before;
 memset(&out,0xa5,sizeof(out));before=out;
 for(unsigned mode=0;mode<14;mode++) {
  bad=p;changed=m;int expected=ENOTSUP;
  switch(mode) {
  case 0:bad.stage=REMESA_IR_FRAGMENT;break;
  case 1:bad.schema++;break;
  case 2:bad.count=0;break;
  case 3:bad.instructions[0].reserved=1;break;
  case 4:bad.instructions[4].src=63;break;
  case 5:bad.instructions[4].channel=1;break;
  case 6:bad.instructions[4].op=REMESA_USSE_EXPORT_COLOR;break;
  case 7:bad.instructions[8].bits=1;break;
  case 8:changed.input[0].class_id=3;break;
  case 9:changed.output[0].number=128;break;
  case 10:changed.output[0]=m.input[3];expected=EINVAL;break;
  case 11:changed.output[0]=m.output[3];expected=EINVAL;break;
  case 12:changed.input[0]=m.input[3];expected=EINVAL;break;
  case 13:bad.count=REMESA_USSE_INSTRUCTIONS+1;break;
  }
  CHECK(remesa_usse_vertex_export_plan(&bad,&changed,&out)==expected);
  CHECK(!memcmp(&out,&before,sizeof(out)));
  CHECK(remesa_usse_vertex_export_validate(&bad,&changed,&out)==expected);
 }
 CHECK(remesa_usse_vertex_export_plan(NULL,&m,&out)==EINVAL);
 CHECK(remesa_usse_vertex_export_plan(&p,NULL,&out)==EINVAL);
 CHECK(remesa_usse_vertex_export_plan(&p,&m,NULL)==EINVAL);
 CHECK(remesa_usse_vertex_export_validate(&p,&m,NULL)==EINVAL);
 CHECK(remesa_usse_vertex_export_plan(&p,&m,(void *)&p)==EINVAL);
 CHECK(remesa_usse_vertex_export_plan(&p,&m,(void *)&m)==EINVAL);
 CHECK(remesa_usse_vertex_export_plan(&p,&m,(void *)((unsigned char *)&p+sizeof(p)-1))==EINVAL);
 CHECK(remesa_usse_vertex_export_plan(&p,&m,(void *)((unsigned char *)&m+sizeof(m)-1))==EINVAL);
 CHECK(remesa_usse_vertex_export_validate(&p,&m,(void *)&p)==EINVAL);
 CHECK(remesa_usse_vertex_export_validate(&p,&m,(void *)&m)==EINVAL);
 CHECK(remesa_usse_vertex_export_plan((void *)(UINTPTR_MAX-1),&m,&out)==EINVAL);
 CHECK(remesa_usse_vertex_export_plan(&p,(void *)(UINTPTR_MAX-1),&out)==EINVAL);
 CHECK(remesa_usse_vertex_export_plan(&p,&m,(void *)(UINTPTR_MAX-1))==EINVAL);
 CHECK(remesa_usse_vertex_export_validate(&p,&m,(void *)(UINTPTR_MAX-1))==EINVAL);
 CHECK(!memcmp(&out,&before,sizeof(out)));
 /* Semantic aliasing changes identity/dependencies even when candidate CPU
  * bytes remain identical. Revalidating against a different program rejects. */
 CHECK(!remesa_usse_vertex_export_plan(&p,&m,&out));
 bad=vertex(4);
 CHECK(remesa_usse_vertex_export_validate(&bad,&m,&out)==EPROTO);
}
int main(void)
{
 for(unsigned aliases=0;aliases<=60;aliases+=4)
  for(unsigned sc=0;sc<3;sc++)for(unsigned dc=0;dc<3;dc++)positive(aliases,sc,dc);
 compiled_body();negative();
 printf("PASS: %u semantic VS export-contract checks; hardware export/termination unsupported\n",checks);
 return 0;
}
