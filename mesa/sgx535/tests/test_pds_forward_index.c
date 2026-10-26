/* SPDX-License-Identifier: MIT */
/* Independent original x86 constructor results, not a matching decoder,
 * supply reference current/delta/total/word quadruples on stdin. */
#include "remesa_pds_structure.h"
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { assert(x); checks++; } while(0)
static uint32_t le32(const unsigned char *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8u|(uint32_t)p[2]<<16u|(uint32_t)p[3]<<24u; }
static void reject(uint32_t current,uint32_t delta,uint32_t total,int error)
{
 uint32_t word=0xa5a5a5a5u;
 CHECK(remesa_pds_structure_forward_index(current,delta,total,&word)==error);
 CHECK(word==0xa5a5a5a5u);
}
static void reject_ir(struct remesa_pds_code_instruction *ir,size_t count,
 size_t capacity,int error)
{
 unsigned char bytes[REMESA_PDS_STRUCTURE_CODE_SLOTS*4u],old[sizeof(bytes)];
 memset(bytes,0xa5,sizeof(bytes));memcpy(old,bytes,sizeof(bytes));size_t written=91;
 CHECK(remesa_pds_structure_encode_code(ir,count,bytes,capacity,&written)==error);
 CHECK(written==91&&!memcmp(bytes,old,sizeof(bytes)));
}
int main(void)
{
 const uint32_t none=REMESA_PDS_STRUCTURE_NO_CELL;
 unsigned char input[48];CHECK(fread(input,1,sizeof(input),stdin)==sizeof(input));
 CHECK(fgetc(stdin)==EOF);
 for(unsigned r=0;r<3;r++) {
  uint32_t at=le32(input+r*16),delta=le32(input+r*16+4);
  uint32_t total=le32(input+r*16+8),oracle=le32(input+r*16+12),word=0;
  CHECK(total<=REMESA_PDS_STRUCTURE_CODE_SLOTS&&at<total);
  CHECK(!remesa_pds_structure_forward_index(at,delta,total,&word));
  CHECK(word==oracle);
  struct remesa_pds_code_instruction ir[REMESA_PDS_STRUCTURE_CODE_SLOTS];
  for(uint32_t i=0;i<total;i++)ir[i]=(struct remesa_pds_code_instruction){
   REMESA_PDS_STRUCTURE_PAIR_SINGLE,0,0};
  ir[at]=(struct remesa_pds_code_instruction){REMESA_PDS_STRUCTURE_CPU_FORWARD_INDEX,delta,none};
  unsigned char bytes[REMESA_PDS_STRUCTURE_CODE_SLOTS*4u+4u];
  memset(bytes,0xa5,sizeof(bytes));size_t written=91;
  CHECK(!remesa_pds_structure_encode_code(ir,total,bytes,sizeof(bytes),&written));
  CHECK(written==total*4u&&le32(bytes+at*4u)==oracle);
  for(size_t i=written;i<sizeof(bytes);i++)CHECK(bytes[i]==0xa5);
  /* Invalid late operand/target must not leave the valid earlier words. */
  ir[at].ds1_index=0;reject_ir(ir,total,sizeof(bytes)-4u,EINVAL);
  ir[at].ds1_index=none;ir[at].ds0_index=2;
  reject_ir(ir,total,sizeof(bytes)-4u,ENOTSUP);
  ir[at].ds0_index=delta;reject_ir(ir,total,total*4u-1u,ENOSPC);
 }
 reject(0,3,0,EINVAL);reject(4,3,4,EINVAL);
 reject(UINT32_MAX,3,21,EINVAL);reject(0,3,UINT32_MAX,E2BIG);
 reject(0,0,21,ENOTSUP);reject(0,UINT32_MAX,21,ENOTSUP);
 reject(0,2,21,ENOTSUP);reject(1,3,4,ERANGE);
 reject(1,4,5,ERANGE);reject(7,13,20,ERANGE);
 /* Original psb_drv_video ELF1bbd7 writes 0x90000010 at ordinal2 of
  * sixteen CODE words. Its delta14 and one-past value are outside this
  * conservative encoder policy, not proof of a malformed CPU producer.
  * The adjacent delta13/end case above must remain ERANGE as policy. */
 reject(2,14,16,ENOTSUP);
 CHECK(remesa_pds_structure_forward_index(0,3,21,NULL)==EINVAL);
 struct remesa_pds_code_instruction late[4];
 for(unsigned i=0;i<4;i++)late[i]=(struct remesa_pds_code_instruction){
  REMESA_PDS_STRUCTURE_PAIR_SINGLE,0,0};
 late[3]=(struct remesa_pds_code_instruction){REMESA_PDS_STRUCTURE_CPU_FORWARD_INDEX,3,none};
 reject_ir(late,4,16,ERANGE);
 CHECK(remesa_pds_structure_forward_index(0,3,4,(uint32_t *)(uintptr_t)1)==EINVAL);
 printf("PASS CPU forward-index: %u checks; three independent producer sites matched; no branch ISA claim\n",checks);
 return 0;
}
