/* SPDX-License-Identifier: MIT */
/* External original-x86 records arrive on stdin, never encoder templates.
 * Wire record: legacy request848 bytes; CODE byte length32le; original CODE.
 * No physical ISA/HALT/dispatch qualification is asserted. */
#include "remesa_pds_structure.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do {assert(x);checks++;} while(0)
static uint32_t get32(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void rejection(const struct remesa_pds_code_instruction *ir,size_t count,int error)
{
 unsigned char bytes[64],old[64];memset(bytes,0xa5,sizeof(bytes));memcpy(old,bytes,sizeof(bytes));
 size_t written=71;CHECK(remesa_pds_structure_encode_code(ir,count,bytes,sizeof(bytes),&written)==error);
 CHECK(written==71&&!memcmp(bytes,old,sizeof(bytes)));
}
int main(void)
{
 _Static_assert(offsetof(struct remesa_pds_structure_request,code_policy)==848,"legacy wire prefix");
 const uint32_t none=REMESA_PDS_STRUCTURE_NO_CELL;
 struct remesa_pds_code_instruction ir[3]={
  {REMESA_PDS_STRUCTURE_PAIR_SINGLE,0,0},
  {REMESA_PDS_STRUCTURE_CPU_TRAILER_AF,none,none},
  {REMESA_PDS_STRUCTURE_CPU_TRAILER_AF,none,none}};
 rejection(ir+1,1,EINVAL);rejection(ir,3,EINVAL);
 struct remesa_pds_code_instruction invalid[3];memcpy(invalid,ir,sizeof(ir));
 invalid[1].ds0_index=0;rejection(invalid,2,EINVAL);
 memcpy(invalid,ir,sizeof(ir));invalid[0].ds1_index=1;rejection(invalid,2,EINVAL);
 memcpy(invalid,ir,sizeof(ir));invalid[1].kind=REMESA_PDS_STRUCTURE_FINAL_HOLE;
 rejection(invalid,2,ENOTSUP);
 struct remesa_pds_structure_request bad={.code_policy=2};
 struct remesa_pds_structure untouched,saved;memset(&untouched,0xa5,sizeof(untouched));saved=untouched;
 CHECK(remesa_pds_structure_encode(&bad,&untouched)==ENOTSUP);
 CHECK(!memcmp(&untouched,&saved,sizeof(saved)));
 unsigned cases=0;unsigned char raw[848];
 while(1) {
  size_t got=fread(raw,1,sizeof(raw),stdin);if(!got){CHECK(feof(stdin));break;}
  CHECK(got==sizeof(raw));
  unsigned char count_bytes[4],expected[REMESA_PDS_STRUCTURE_CODE_SLOTS*4];
  CHECK(fread(count_bytes,1,4,stdin)==4);uint32_t count=get32(count_bytes);
  CHECK(count>=8&&count<=sizeof(expected)&&!(count&3));
  CHECK(fread(expected,1,count,stdin)==count);
  struct remesa_pds_structure_request request={0};uint32_t words[848/4];
  for(unsigned i=0;i<848/4;i++)words[i]=get32(raw+i*4);
  memcpy(&request,words,sizeof(words));
  struct remesa_pds_structure legacy,initial,full;
  CHECK(!remesa_pds_structure_encode(&request,&legacy));
  CHECK(!remesa_pds_structure_resolve_initial(&legacy,&initial));
  CHECK(initial.code[initial.code_slot_count-1].kind==REMESA_PDS_STRUCTURE_FINAL_HOLE);
  request.code_policy=REMESA_PDS_STRUCTURE_CODE_CPU_COMPLETE;
  CHECK(!remesa_pds_structure_encode(&request,&full));
  CHECK(full.code_slot_count*4==count);
  CHECK(!memcmp(full.data,legacy.data,sizeof(full.data)));
  CHECK(!memcmp(full.data_known_mask,legacy.data_known_mask,sizeof(full.data_known_mask)));
  CHECK(!memcmp(full.relocations,legacy.relocations,sizeof(full.relocations)));
  CHECK(full.data_known_mask[0]==0); /* Symbolic USE address remains symbolic. */
  struct remesa_pds_code_instruction code[REMESA_PDS_STRUCTURE_CODE_SLOTS];
  for(unsigned i=0;i<full.code_slot_count;i++) {
   CHECK(full.code[i].known_mask==UINT32_MAX);
   code[i]=(struct remesa_pds_code_instruction){full.code[i].kind,full.code[i].ds0_index,full.code[i].ds1_index};
  }
  unsigned char encoded[sizeof(expected)];size_t written=99;
  CHECK(!remesa_pds_structure_encode_code(code,full.code_slot_count,encoded,sizeof(encoded),&written));
  CHECK(written==count&&!memcmp(encoded,expected,count));
  unsigned char image[REMESA_PDS_STRUCTURE_IMAGE_BYTES],mask[sizeof(image)];
  CHECK(!remesa_pds_structure_serialize(&full,image,mask,sizeof(image),&written));
  unsigned offset=full.data_dwords*4;CHECK(written==offset+count);
  CHECK(!memcmp(image+offset,expected,count));
  for(unsigned i=0;i<count;i++)CHECK(mask[offset+i]==255);
  CHECK(!memcmp(mask,"\0\0\0\0",4));
  /* An operand or mask mutation cannot masquerade as the proven trailer. */
  struct remesa_pds_structure malformed=full;malformed.code[full.code_slot_count-1].ds0_index=0;
  memset(image,0xa5,sizeof(image));memset(mask,0x5a,sizeof(mask));written=77;
  CHECK(remesa_pds_structure_serialize(&malformed,image,mask,sizeof(image),&written)==EINVAL);
  CHECK(image[0]==0xa5&&mask[0]==0x5a&&written==77);
  cases++;
 }
 CHECK(cases==20);
 printf("{\"status\":\"PASS_CPU_PRIMARY_CODE_ONLY\",\"checks\":%u,\"original_oracle_cases\":%u,\"complete_CODE_comparisons\":%u,\"physical_semantics_verified\":false,\"GPU_submissions\":0}\n",checks,cases,cases);
 return 0;
}
