/* SPDX-License-Identifier: MIT
 * Independent host semantic backend validation, never SGX encoding proof.
 * All values below are host schema/IEEE754 values, not hardware instructions.
 */
#include "remesa_shader_ir.h"
#include "remesa_usse.h"
#include "remesa_pds.h"
#include "remesa_fragment_body.h"
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { assert(x); checks++; } while (0)
static struct remesa_shader_ir profile(unsigned fragment,unsigned copies)
{
 struct remesa_shader_ir ir={.schema=REMESA_SHADER_IR_SCHEMA,.stage=fragment?REMESA_IR_FRAGMENT:REMESA_IR_VERTEX,.count=copies+2};
 ir.instructions[0]=(struct remesa_ir_instruction){.op=fragment?REMESA_IR_CONSTANT:REMESA_IR_INPUT,.type=REMESA_IR_FLOAT32,.components=4};
 if(fragment)ir.instructions[0].bits[0]=ir.instructions[0].bits[2]=ir.instructions[0].bits[3]=UINT32_C(0x3f800000);
 for(unsigned n=1;n<=copies;n++) {
  ir.instructions[n]=(struct remesa_ir_instruction){.op=REMESA_IR_COPY,.dst=n,.type=REMESA_IR_FLOAT32,.components=4};
  for(unsigned c=0;c<4;c++)ir.instructions[n].src[c]=(struct remesa_ir_operand){.reg=n-1,.channel=c};
 }
 ir.instructions[copies+1]=(struct remesa_ir_instruction){.op=fragment?REMESA_IR_COLOR:REMESA_IR_POSITION,.dst=REMESA_SHADER_IR_NO_REGISTER,.type=REMESA_IR_FLOAT32,.components=4,.src={{.reg=copies}}};
 return ir;
}
static void unchanged_bytes(const unsigned char *p,size_t n,unsigned char value)
{for(size_t k=0;k<n;k++)CHECK(p[k]==value);}
static void test_copy_propagation(void)
{
 /* A nonidentity compose followed by its inverse is an identity shader.
  * Check channel provenance rather than assuming every copy is passthrough. */
 const unsigned permutation[4]={2,0,3,1},inverse[4]={1,3,0,2};
 for(unsigned stage=0;stage<2;stage++) {
  struct remesa_shader_ir ir=profile(stage,2),direct=profile(stage,0);
  for(unsigned c=0;c<4;c++) {
   ir.instructions[1].src[c].channel=permutation[c];
   ir.instructions[2].src[c].channel=inverse[c];
  }
  ir.instructions[1].op=ir.instructions[2].op=REMESA_IR_COMPOSE4;
  struct remesa_shader_ir snapshot=ir;
  struct remesa_usse_program optimized,minimal;
  CHECK(!remesa_usse_lower(&ir,&optimized));
  CHECK(!remesa_usse_lower(&direct,&minimal));
  CHECK(!memcmp(&optimized,&minimal,sizeof(minimal)));
  CHECK(!memcmp(&ir,&snapshot,sizeof(ir)));
  if(!stage) {
   /* Missing inverse would change POSITION and is still rejected. */
   ir.instructions[2].src[0].channel=0;
   struct remesa_usse_program before=optimized;
   CHECK(remesa_usse_lower(&ir,&optimized)==ENOTSUP);
   CHECK(!memcmp(&optimized,&before,sizeof(before)));
  }
 }
 /* The transport schema still accepts explicitly authored semantic MOVs.
  * Optimizing compilation must not invalidate existing host schema records. */
 struct remesa_shader_ir direct=profile(0,0);
 struct remesa_usse_program moved;
 CHECK(!remesa_usse_lower(&direct,&moved));
 for(unsigned c=0;c<4;c++) {
  moved.instructions[8+c]=moved.instructions[4+c];
  moved.instructions[8+c].src=4+c;
  moved.instructions[4+c]=(struct remesa_usse_instruction){
   .op=REMESA_USSE_MOVE,.dst=4+c,.src=c};
 }
 moved.count=12;CHECK(!remesa_usse_validate(&moved));
}
static void test_usse(void)
{
 struct remesa_usse_encoding_status status;
 memset(&status,0xa5,sizeof(status));remesa_usse_encoding_status(&status);
 CHECK(status.opcode==REMESA_ENCODING_UNKNOWN&&status.instruction_width==REMESA_ENCODING_UNKNOWN&&status.alignment==REMESA_ENCODING_UNKNOWN);
 CHECK(status.register_banks==REMESA_ENCODING_UNKNOWN&&status.register_addressing==REMESA_ENCODING_UNKNOWN&&status.predicate==REMESA_ENCODING_UNKNOWN);
 CHECK(status.execution_control==REMESA_ENCODING_UNKNOWN&&status.input_output==REMESA_ENCODING_UNKNOWN&&status.immediate==REMESA_ENCODING_UNKNOWN);
 for(unsigned stage=0;stage<2;stage++)for(unsigned copies=0;copies<16;copies++) {
  struct remesa_shader_ir ir=profile(stage,copies);
  struct remesa_usse_program a,b,minimal;
  CHECK(!remesa_usse_lower(&ir,&a));CHECK(!remesa_usse_lower(&ir,&b));
  CHECK(!memcmp(&a,&b,sizeof(a))&&!remesa_usse_validate(&a));
  CHECK(a.stage==ir.stage&&a.count==(stage?6u:8u));
  struct remesa_shader_ir direct=profile(stage,0),snapshot=ir;
  CHECK(!remesa_usse_lower(&direct,&minimal)&&!memcmp(&a,&minimal,sizeof(a)));
  CHECK(!memcmp(&ir,&snapshot,sizeof(ir)));
  for(unsigned c=0;c<(stage?2u:4u);c++) {
   CHECK(a.instructions[c].op==(stage?REMESA_USSE_CONSTANT:REMESA_USSE_INPUT));
   CHECK(a.instructions[c].dst==c&&a.instructions[c].src==REMESA_USSE_NO_VALUE);
  }
  for(unsigned c=0;c<4;c++) {
   CHECK(a.instructions[a.count-4+c].op==(stage?REMESA_USSE_EXPORT_COLOR:REMESA_USSE_EXPORT_POSITION));
   CHECK(a.instructions[a.count-4+c].channel==c&&a.instructions[a.count-4+c].src==(stage?(c==1?1u:0u):c));
  }
  unsigned char bytes[16+REMESA_USSE_INSTRUCTIONS*24],again[sizeof(bytes)];
  size_t n=999,m=999;
  CHECK(!remesa_usse_serialize_host(&a,bytes,sizeof(bytes),&n));
  CHECK(n==REMESA_USSE_HOST_HEADER_BYTES+a.count*REMESA_USSE_HOST_INSTRUCTION_BYTES);
  CHECK(!memcmp(bytes,"RUSI",4));
  CHECK(!remesa_usse_serialize_host(&b,again,sizeof(again),&m)&&m==n&&!memcmp(bytes,again,n));
  memset(&b,0xa5,sizeof(b));CHECK(!remesa_usse_deserialize_host(bytes,n,&b)&&!memcmp(&a,&b,sizeof(a)));
  uint64_t ha=0,hb=1;CHECK(!remesa_usse_identity(&a,&ha)&&!remesa_usse_identity(&b,&hb)&&ha==hb);
  memset(again,0xa5,sizeof(again));m=999;
  CHECK(remesa_usse_serialize_host(&a,again,n-1,&m)==ENOSPC&&m==999);
  unchanged_bytes(again,sizeof(again),0xa5);
  if(stage) {
   CHECK(!remesa_usse_encode(&a,again,sizeof(again),&m)&&m==8);
   struct remesa_fragment_constant constant;
   CHECK(!remesa_fragment_constant_decode(again,m,&constant)&&constant.argb==0xffff00ff);
   unchanged_bytes(again+m,sizeof(again)-m,0xa5);
  } else {
   CHECK(remesa_usse_encode(&a,again,sizeof(again),&m)==ENOTSUP&&m==999);
   unchanged_bytes(again,sizeof(again),0xa5);
  }
  memset(again,0xa5,sizeof(again));m=999;
  CHECK(remesa_usse_encode(&a,NULL,0,&m)==(stage?ENOSPC:ENOTSUP)&&m==999);
  CHECK(remesa_usse_encode(&a,NULL,1,&m)==EINVAL&&m==999);
  CHECK(remesa_usse_encode(&a,again,sizeof(again),NULL)==EINVAL);
  CHECK(remesa_usse_lower(&ir,NULL)==EINVAL);
 }
 struct remesa_shader_ir ir=profile(0,0);
 struct remesa_usse_program p;CHECK(!remesa_usse_lower(&ir,&p));
 struct remesa_usse_program sentinel,bad;memset(&sentinel,0xa5,sizeof(sentinel));
 bad=sentinel;CHECK(remesa_usse_lower(NULL,&bad)==EINVAL&&!memcmp(&bad,&sentinel,sizeof(bad)));
 for(unsigned mode=0;mode<14;mode++) {
  struct remesa_shader_ir malformed=profile(0,0);
  switch(mode) {
  case 0:malformed.schema=0;break;case 1:malformed.stage=0;break;case 2:malformed.count=0;break;
  case 3:malformed.instructions[0].op=99;break;case 4:malformed.instructions[0].type=0;break;
  case 5:malformed.instructions[0].components=3;break;case 6:malformed.instructions[0].input_slot=1;break;
  case 7:malformed.instructions[1].src[0].reg=1;break;case 8:malformed.instructions[1].src[0].channel=1;break;
  case 9:malformed.instructions[1].op=REMESA_IR_COLOR;break;case 10:malformed.instructions[1].bits[0]=1;break;
  case 11:malformed.instructions[1].dst=0;break;case 12:malformed.instructions[2].op=REMESA_IR_CONSTANT;break;
  case 13:malformed.count=REMESA_SHADER_IR_INSTRUCTIONS+1;break;
  }
  bad=sentinel;CHECK(remesa_usse_lower(&malformed,&bad)!=0&&!memcmp(&bad,&sentinel,sizeof(bad)));
 }
 for(unsigned color=0;color<6;color++) {
  struct remesa_shader_ir fi=profile(1,0);
  if(color==1) {fi.instructions[0].bits[0]=fi.instructions[0].bits[2]=0;fi.instructions[0].bits[1]=UINT32_C(0x3f800000);}
  if(color==2) {fi.instructions[0].bits[1]=UINT32_C(0x3f800000);fi.instructions[0].bits[2]=0;}
  if(color==3)fi.instructions[0].bits[3]=0;
  if(color==4)fi.instructions[0].bits[0]=UINT32_C(0x3f000000);
  if(color==5)fi.instructions[0].bits[0]=UINT32_C(0x7fc00000);
  bad=sentinel;int ret=remesa_usse_lower(&fi,&bad);
  if(color<3)CHECK(!ret&&!remesa_usse_validate(&bad));
  else CHECK(ret!=0&&!memcmp(&bad,&sentinel,sizeof(bad)));
 }
 for(unsigned mode=0;mode<24;mode++) {
  bad=p;
  switch(mode) {
  case 0:bad.schema=0;break;case 1:bad.stage=0;break;case 2:bad.count=0;break;
  case 3:bad.count=REMESA_USSE_INSTRUCTIONS+1;break;
  case 4:bad.instructions[0].op=UINT32_MAX;break;
  case 5:bad.instructions[0].dst=1;break;
  case 6:bad.instructions[0].src=0;break;
  case 7:bad.instructions[0].channel=4;break;
  case 8:bad.instructions[0].bits=1;break;
  case 9:bad.instructions[0].reserved=1;break;
  case 10:bad.instructions[4].dst=0;break;
  case 11:bad.instructions[4].src=64;break;
  case 12:bad.instructions[4].channel=4;break;
  case 13:bad.instructions[4].bits=1;break;
  case 14:bad.instructions[4].op=REMESA_USSE_EXPORT_COLOR;break;
  case 15:bad.instructions[5].channel=0;break;
  case 16:bad.count=7;break;
  case 17:bad.instructions[8].op=REMESA_USSE_INPUT;break;
  case 18:bad.stage=REMESA_IR_FRAGMENT;break;
  case 19:bad.instructions[3].dst=64;break;
  case 20:bad.instructions[3].op=REMESA_USSE_MOVE;bad.instructions[3].src=3;break;
  case 21:bad.instructions[3].op=REMESA_USSE_CONSTANT;break;
  case 22:bad.instructions[4].src=1;break;
  case 23:bad.instructions[7].reserved=1;break;
  }
  CHECK(remesa_usse_validate(&bad)!=0);
  unsigned char out[4096];memset(out,0xa5,sizeof(out));size_t written=77;
  CHECK(remesa_usse_serialize_host(&bad,out,sizeof(out),&written)!=0&&written==77);
  unchanged_bytes(out,sizeof(out),0xa5);
  CHECK(remesa_usse_encode(&bad,out,sizeof(out),&written)!=0&&written==77);
  uint64_t id=7;CHECK(remesa_usse_identity(&bad,&id)!=0&&id==7);
 }
 unsigned char bytes[4096];size_t n=0;CHECK(!remesa_usse_serialize_host(&p,bytes,sizeof(bytes),&n));
 for(size_t length=0;length<n;length++) {
  bad=sentinel;CHECK(remesa_usse_deserialize_host(bytes,length,&bad)!=0&&!memcmp(&bad,&sentinel,sizeof(bad)));
 }
 bad=sentinel;CHECK(remesa_usse_deserialize_host(bytes,n+1,&bad)!=0&&!memcmp(&bad,&sentinel,sizeof(bad)));
 /* Deterministic mutation corpus: accepted host data must remain canonical;
  * rejected data must not modify the destination. Neither result proves ISA. */
 uint32_t seed=UINT32_C(0x12345678);
 for(unsigned mutation=0;mutation<2048;mutation++) {
  unsigned char changed[4096],canonical[4096];memcpy(changed,bytes,n);
  seed=seed*UINT32_C(1664525)+UINT32_C(1013904223);
  changed[seed%n]^=(unsigned char)(1u<<(seed%8));bad=sentinel;
  int ret=remesa_usse_deserialize_host(changed,n,&bad);
  if(ret)CHECK(!memcmp(&bad,&sentinel,sizeof(bad)));
  else {
   size_t length=0;CHECK(!remesa_usse_validate(&bad));
   CHECK(!remesa_usse_serialize_host(&bad,canonical,sizeof(canonical),&length)&&length==n&&!memcmp(changed,canonical,n));
   size_t written=55;CHECK(remesa_usse_encode(&bad,canonical,sizeof(canonical),&written)==ENOTSUP&&written==55);
  }
 }
 CHECK(remesa_usse_validate(NULL)==EINVAL);
 CHECK(remesa_usse_deserialize_host(NULL,0,&bad)==EINVAL);
 CHECK(remesa_usse_deserialize_host(bytes,n,NULL)==EINVAL);
 CHECK(remesa_usse_identity(&p,NULL)==EINVAL);
}
static void test_constant_sharing(void)
{
 /* Three admitted opaque endpoint RGB colors retain exact channels. Other
  * endpoint colors are still rejected by the existing profile admission.
  * Values are defined in first-use order, independently of host float rules.
  * The old unshared schema remains valid, not silently reinterpreted. */
 for(unsigned rgb=0;rgb<8;rgb++) {
  struct remesa_shader_ir ir=profile(1,0);
  for(unsigned c=0;c<3;c++)ir.instructions[0].bits[c]=(rgb&(1u<<c))?UINT32_C(0x3f800000):0;
  struct remesa_shader_ir snapshot=ir;
  struct remesa_usse_program p,again,unshared={.schema=REMESA_USSE_SCHEMA,.stage=REMESA_IR_FRAGMENT,.count=8};
  if(rgb!=2&&rgb!=3&&rgb!=5) {
   memset(&p,0xa5,sizeof(p));again=p;
   CHECK(remesa_usse_lower(&ir,&p)==ENOTSUP&&!memcmp(&p,&again,sizeof(p)));
   continue;
  }
  CHECK(!remesa_usse_lower(&ir,&p)&&!remesa_usse_validate(&p));
  CHECK(p.count==6);
  CHECK(!memcmp(&ir,&snapshot,sizeof(ir)));
  for(unsigned c=0;c<4;c++) {
   const struct remesa_usse_instruction *export=&p.instructions[p.count-4+c];
   CHECK(export->op==REMESA_USSE_EXPORT_COLOR&&export->channel==c&&export->src<p.count-4);
   CHECK(p.instructions[export->src].op==REMESA_USSE_CONSTANT&&p.instructions[export->src].bits==ir.instructions[0].bits[c]);
   unshared.instructions[c]=(struct remesa_usse_instruction){.op=REMESA_USSE_CONSTANT,.dst=c,.src=REMESA_USSE_NO_VALUE,.bits=ir.instructions[0].bits[c]};
   unshared.instructions[4+c]=(struct remesa_usse_instruction){.op=REMESA_USSE_EXPORT_COLOR,.dst=REMESA_USSE_NO_VALUE,.src=c,.channel=c};
  }
  CHECK(!remesa_usse_validate(&unshared));
  /* Four distinct semantic literal definitions composed into a color lower
   * identically to one float4 literal. Semantic dependency validation still
   * precedes sharing: duplicated definitions cannot hide unused IR nodes. */
  struct remesa_shader_ir split={.schema=REMESA_SHADER_IR_SCHEMA,.stage=REMESA_IR_FRAGMENT,.count=6};
  for(unsigned c=0;c<4;c++) {
   split.instructions[c]=(struct remesa_ir_instruction){.op=REMESA_IR_CONSTANT,.dst=c,.type=REMESA_IR_FLOAT32,.components=1,.bits={snapshot.instructions[0].bits[c]}};
   split.instructions[4].src[c].reg=c;
  }
  split.instructions[4].op=REMESA_IR_COMPOSE4;split.instructions[4].dst=4;
  split.instructions[4].type=REMESA_IR_FLOAT32;split.instructions[4].components=4;
  split.instructions[5]=(struct remesa_ir_instruction){.op=REMESA_IR_COLOR,.dst=REMESA_SHADER_IR_NO_REGISTER,.type=REMESA_IR_FLOAT32,.components=4,.src={{.reg=4}}};
  CHECK(!remesa_usse_lower(&split,&again)&&!memcmp(&again,&p,sizeof(p)));
  split.instructions[4].src[2].reg=split.instructions[4].src[0].reg;
  again=p;CHECK(remesa_usse_lower(&split,&again)==ENOTSUP&&!memcmp(&again,&p,sizeof(p)));
  unsigned char bytes[4096],before[4096];size_t length=0;
  CHECK(!remesa_usse_serialize_host(&p,bytes,sizeof(bytes),&length));
  CHECK(!remesa_usse_deserialize_host(bytes,length,&again)&&!memcmp(&p,&again,sizeof(p)));
  memset(bytes,0xa5,sizeof(bytes));memcpy(before,bytes,sizeof(bytes));length=99;
  if(rgb==3) {
   /* Keep transactional rejection coverage with a truly unsupported value. */
   struct remesa_usse_program unsupported=p;
   unsupported.instructions[0].bits=UINT32_C(0x3f000000);
   CHECK(remesa_usse_encode(&unsupported,bytes,sizeof(bytes),&length)==ENOTSUP&&length==99&&!memcmp(bytes,before,sizeof(bytes)));
  }
  CHECK(!remesa_usse_encode(&p,bytes,sizeof(bytes),&length)&&length==8);
  struct remesa_fragment_constant constant;
  CHECK(!remesa_fragment_constant_decode(bytes,length,&constant)&&
   constant.argb==(rgb==2?UINT32_C(0xff00ff00):rgb==3?UINT32_C(0xffffff00):UINT32_C(0xffff00ff)));
  CHECK(!memcmp(bytes+length,before+length,sizeof(bytes)-length));
  /* Unsupported -0, non-endpoint, NaN and alpha cannot be optimized into a
   * supported color. Rejection must precede any output modification. */
  const uint32_t invalid[3]={UINT32_C(0x80000000),UINT32_C(0x3f000000),UINT32_C(0x7fc00000)};
  for(unsigned k=0;k<4;k++) {
   ir=snapshot;ir.instructions[0].bits[k<3?0:3]=k<3?invalid[k]:0;
   again=p;CHECK(remesa_usse_lower(&ir,&again)==ENOTSUP&&!memcmp(&again,&p,sizeof(p)));
  }
 }
}
static void test_aliases(void)
{
 union storage {struct remesa_shader_ir ir;struct remesa_usse_program u;struct remesa_pds_plan p;struct remesa_pds_request r;unsigned char bytes[4096];uint64_t alignment;size_t written;} box,snapshot;
 memset(&box,0,sizeof(box));box.ir=profile(0,0);snapshot=box;
 CHECK(remesa_usse_lower(&box.ir,&box.u)==EINVAL&&!memcmp(&box,&snapshot,sizeof(box)));
 struct remesa_shader_ir ir=profile(0,0);CHECK(!remesa_usse_lower(&ir,&box.u));snapshot=box;
 size_t n=99;
 CHECK(remesa_usse_serialize_host(&box.u,&box.u,sizeof(box.u),&n)==EINVAL&&n==99&&!memcmp(&box,&snapshot,sizeof(box)));
 unsigned char separate[4096];memset(separate,0xa5,sizeof(separate));
 CHECK(remesa_usse_serialize_host(&box.u,separate,sizeof(separate),&box.written)==EINVAL&&!memcmp(&box,&snapshot,sizeof(box)));
 unchanged_bytes(separate,sizeof(separate),0xa5);
 CHECK(remesa_usse_identity(&box.u,&box.alignment)==EINVAL&&!memcmp(&box,&snapshot,sizeof(box)));
 union {unsigned char bytes[4096];size_t size;} output,before;
 memset(&output,0xa5,sizeof(output));before=output;
 CHECK(remesa_usse_serialize_host(&box.u,output.bytes,sizeof(output.bytes),&output.size)==EINVAL&&!memcmp(&output,&before,sizeof(output)));
 CHECK(!remesa_usse_serialize_host(&box.u,separate,sizeof(separate),&n));
 memset(&box,0xa5,sizeof(box));memcpy(box.bytes,separate,n);snapshot=box;
 CHECK(remesa_usse_deserialize_host(box.bytes,n,&box.u)==EINVAL&&!memcmp(&box,&snapshot,sizeof(box)));
}
static struct remesa_pds_request request(void)
{
 struct remesa_pds_request r={.operation=11,.generation=3,.owner_cookie=5};
 r.code=(struct remesa_pds_binding){.allocation_cookie=7,.owner_cookie=5,.mapping_generation=2,.address=0x10000,.offset=64,.bytes=256,.allocation_bytes=4096};
 r.data=(struct remesa_pds_binding){.allocation_cookie=8,.owner_cookie=5,.mapping_generation=2,.address=0x20000,.offset=128,.bytes=64,.allocation_bytes=4096};
 return r;
}
static void test_pds(void)
{
 struct remesa_shader_ir ir=profile(0,0),fs=profile(1,0);
 struct remesa_usse_program u,f;CHECK(!remesa_usse_lower(&ir,&u)&&!remesa_usse_lower(&fs,&f));
 struct remesa_pds_encoding_status status;memset(&status,0xa5,sizeof(status));remesa_pds_encoding_status(&status);
 CHECK(status.instruction_width==REMESA_ENCODING_UNKNOWN&&status.alignment==REMESA_ENCODING_UNKNOWN&&status.opcode==REMESA_ENCODING_UNKNOWN);
 CHECK(status.task_control==REMESA_ENCODING_UNKNOWN&&status.usse_address==REMESA_ENCODING_UNKNOWN&&status.constant_address==REMESA_ENCODING_UNKNOWN);
 CHECK(status.register_banks==REMESA_ENCODING_UNKNOWN&&status.launch_semantics==REMESA_ENCODING_UNKNOWN);
 struct remesa_pds_request r=request();struct remesa_pds_plan a,b,sentinel;
 memset(&sentinel,0xa5,sizeof(sentinel));
 CHECK(!remesa_pds_prepare(&u,&r,&a));CHECK(!remesa_pds_prepare(&u,&r,&b)&&!memcmp(&a,&b,sizeof(a)));
 CHECK(!remesa_pds_validate(&u,&a));
 CHECK(a.relocation_count==2&&a.relocations[0].address==r.code.address+r.code.offset&&a.relocations[1].address==r.data.address+r.data.offset);
 CHECK(a.relocations[0].bytes==256&&a.relocations[1].bytes==64&&a.request.operation==11&&a.request.generation==3);
 unsigned char out[4096];memset(out,0xa5,sizeof(out));size_t written=79;
 CHECK(remesa_pds_encode(&u,&a,out,sizeof(out),&written)==ENOTSUP&&written==79);
 unchanged_bytes(out,sizeof(out),0xa5);
 CHECK(remesa_pds_encode(&u,&a,NULL,0,&written)==ENOTSUP&&written==79);
 CHECK(remesa_pds_encode(&u,&a,NULL,1,&written)==EINVAL&&written==79);
 CHECK(remesa_pds_encode(&u,&a,out,sizeof(out),NULL)==EINVAL);
 CHECK(remesa_pds_validate(&f,&a)==ESTALE);
 for(unsigned mode=0;mode<30;mode++) {
  struct remesa_pds_request bad=request();
  switch(mode) {
  case 0:bad.operation=0;break;case 1:bad.generation=0;break;case 2:bad.owner_cookie=0;break;
  case 3:bad.code.allocation_cookie=0;break;case 4:bad.code.owner_cookie=9;break;
  case 5:bad.code.mapping_generation=0;break;case 6:bad.code.address=0;break;
  case 7:bad.code.bytes=0;break;case 8:bad.code.allocation_bytes=0;break;
  case 9:bad.code.offset=4096;break;case 10:bad.code.bytes=4096;break;
  case 11:bad.code.address=UINT64_MAX-64;break;
  case 12:bad.data.allocation_cookie=0;break;case 13:bad.data.owner_cookie=9;break;
  case 14:bad.data.mapping_generation=0;break;case 15:bad.data.address=0;break;
  case 16:bad.data.bytes=0;break;case 17:bad.data.allocation_bytes=0;break;
  case 18:bad.data.offset=4096;break;case 19:bad.data.bytes=4096;break;
  case 20:bad.data.address=UINT64_MAX-64;break;
  case 21:bad.data.address=bad.code.address;bad.data.offset=128;break;
  case 22:bad.data=bad.code;break;
  case 23:bad.data.allocation_cookie=bad.code.allocation_cookie;bad.data.address=bad.code.address;bad.data.offset=1024;bad.data.mapping_generation=7;break;
  case 24:bad.data.allocation_cookie=bad.code.allocation_cookie;break;
  case 25:bad.data.allocation_cookie=bad.code.allocation_cookie;bad.data.address=bad.code.address;bad.data.offset=1024;bad.data.allocation_bytes=8192;break;
  case 26:bad.code.offset=UINT64_MAX;break;case 27:bad.data.bytes=UINT64_MAX;break;
  case 28:bad.code.allocation_bytes=UINT64_MAX;break;case 29:bad.data.offset=UINT64_MAX;break;
  }
  b=sentinel;CHECK(remesa_pds_prepare(&u,&bad,&b)!=0&&!memcmp(&b,&sentinel,sizeof(b)));
 }
 /* Shared allocation is permissible only with one mapping identity and
  * disjoint ranges. Host validity is not proof of a device mapping. */
 r=request();r.data=r.code;r.data.offset=320;r.data.bytes=64;
 CHECK(!remesa_pds_prepare(&u,&r,&b)&&!remesa_pds_validate(&u,&b));
 r=request();r.code.address=UINT64_MAX-4096;r.code.offset=4095;r.code.bytes=1;
 CHECK(!remesa_pds_prepare(&u,&r,&b)&&b.relocations[0].address==UINT64_MAX-1);
 for(unsigned mode=0;mode<17;mode++) {
  b=a;
  switch(mode) {
  case 0:b.schema=0;break;case 1:b.stage=REMESA_IR_FRAGMENT;break;case 2:b.program_identity^=1;break;
  case 3:b.relocation_count=0;break;case 4:b.relocation_count=3;break;case 5:b.reserved=1;break;
  case 6:b.relocations[0].role=REMESA_PDS_DATA;break;case 7:b.relocations[1].kind=99;break;
  case 8:b.relocations[0].address++;break;case 9:b.relocations[1].bytes++;break;
  case 10:b.request.generation=0;break;case 11:b.request.code.mapping_generation=0;break;
  case 12:b.request.owner_cookie++;break;case 13:b.request.code.address++;break;
  case 14:b.program.instructions[0].reserved=1;break;
  case 15:b.program.instructions[4].src=1;break;
  case 16:b.program.count=0;break;
  }
  CHECK(remesa_pds_validate(&u,&b)!=0);written=79;
  CHECK(remesa_pds_encode(&u,&b,out,sizeof(out),&written)!=0&&written==79);
  unchanged_bytes(out,sizeof(out),0xa5);
 }
 for(uint64_t n=0;n<100;n++) {
  r=request();r.operation+=n;r.generation+=n;r.code.mapping_generation+=n;r.data.mapping_generation+=n;
  CHECK(!remesa_pds_prepare(&u,&r,&b)&&!remesa_pds_validate(&u,&b));
  CHECK(b.request.operation==11+n&&b.request.generation==3+n&&b.request.code.mapping_generation==2+n);
 }
 r=request();CHECK(!remesa_pds_validate_binding(&u,&a,&r));
 for(unsigned mode=0;mode<17;mode++) {
  r=request();switch(mode) {
  case 0:r.operation++;break;case 1:r.generation++;break;
  case 2:r.owner_cookie++;r.code.owner_cookie=r.data.owner_cookie=r.owner_cookie;break;
  case 3:r.code.allocation_cookie++;break;case 4:r.code.mapping_generation++;break;
  case 5:r.code.address++;break;case 6:r.code.offset++;break;case 7:r.code.bytes++;break;case 8:r.code.allocation_bytes++;break;
  case 9:r.data.allocation_cookie++;break;case 10:r.data.mapping_generation++;break;
  case 11:r.data.address++;break;case 12:r.data.offset++;break;case 13:r.data.bytes++;break;case 14:r.data.allocation_bytes++;break;
  case 15:r.operation=0;break;case 16:r.generation=0;break;
  }
  int ret=remesa_pds_validate_binding(&u,&a,&r);CHECK(mode<15?ret==ESTALE:ret==EINVAL);
 }
 union {struct remesa_pds_plan p;struct remesa_pds_request r;struct remesa_usse_program u;} alias,before;
 memset(&alias,0,sizeof(alias));alias.u=u;before=alias;r=request();
 CHECK(remesa_pds_prepare(&alias.u,&r,&alias.p)==EINVAL&&!memcmp(&alias,&before,sizeof(alias)));
 alias.r=r;before=alias;
 CHECK(remesa_pds_prepare(&u,&alias.r,&alias.p)==EINVAL&&!memcmp(&alias,&before,sizeof(alias)));
 CHECK(remesa_pds_validate_binding(&u,&a,NULL)==EINVAL);
 CHECK(remesa_pds_prepare(&u,NULL,&b)==EINVAL);
 CHECK(remesa_pds_prepare(&u,&r,NULL)==EINVAL);
 CHECK(remesa_pds_validate(&u,NULL)==EINVAL);
 CHECK(remesa_pds_prepare(NULL,&r,&b)==EINVAL);
}
int main(void)
{
 test_copy_propagation();
 test_constant_sharing();
 test_usse();
 test_pds();
 test_aliases();
 printf("PASS %u independent host semantic checks; zero hardware execution\n",checks);
 return 0;
}
