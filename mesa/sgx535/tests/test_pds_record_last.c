/* SPDX-License-Identifier: MIT */
/* Oracle pairs come from execution of the entire original Windows CPU
 * constructor, not an encoder/decoder sharing the same field hypothesis. */
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
static void reject(struct remesa_pds_code_instruction *ir,size_t count,
 size_t capacity,int error)
{
 unsigned char bytes[16],old[16];memset(bytes,0xa5,sizeof(bytes));
 memcpy(old,bytes,sizeof(bytes));size_t written=99;
 CHECK(remesa_pds_structure_encode_code(ir,count,bytes,capacity,&written)==error);
 CHECK(written==99&&!memcmp(bytes,old,sizeof(bytes)));
}
int main(void)
{
 unsigned char oracle[34*8];
 const uint32_t none=REMESA_PDS_STRUCTURE_NO_CELL;
 CHECK(fread(oracle,1,sizeof(oracle),stdin)==sizeof(oracle));
 CHECK(fgetc(stdin)==EOF);
 for(unsigned i=0;i<34;i++) {
  uint32_t cursor=le32(oracle+8*i),reference=le32(oracle+8*i+4);
  CHECK(cursor==2*i);
  struct remesa_pds_code_instruction ir={REMESA_PDS_STRUCTURE_CPU_RECORD_LAST_E6,cursor,none};
  unsigned char bytes[8];memset(bytes,0xa5,sizeof(bytes));size_t written=99;
  CHECK(!remesa_pds_structure_encode_code(&ir,1,bytes,sizeof(bytes),&written));
  CHECK(written==4&&le32(bytes)==reference);
  CHECK(bytes[4]==0xa5&&bytes[5]==0xa5&&bytes[6]==0xa5&&bytes[7]==0xa5);
  /* Controls are deliberately represented by a distinct kind, with no
   * generic low-byte override to admit E0/68 or another unknown form. */
  ir.ds1_index=0;reject(&ir,1,sizeof(bytes),EINVAL);
  ir.ds1_index=none;reject(&ir,1,3,ENOSPC);
 }
 struct remesa_pds_code_instruction ir[2]={{REMESA_PDS_STRUCTURE_PAIR_SINGLE,0,0},
  {REMESA_PDS_STRUCTURE_CPU_RECORD_LAST_E6,1,none}};
 reject(ir,2,8,EINVAL);ir[1].ds0_index=68;reject(ir,2,8,ENOTSUP);
 ir[1].ds0_index=UINT32_MAX-1;reject(ir,2,8,ENOTSUP);
 ir[1].ds0_index=UINT32_MAX;reject(ir,2,8,EINVAL);
 ir[1].ds0_index=66;ir[1].kind=UINT32_MAX;reject(ir,2,8,ENOTSUP);
 ir[1].kind=REMESA_PDS_STRUCTURE_CPU_RECORD_LAST_E6;
 unsigned char output[8];size_t written=99;
 CHECK(remesa_pds_structure_encode_code(ir,2,(unsigned char*)ir,8,&written)==EINVAL);
 CHECK(written==99);
 CHECK(remesa_pds_structure_encode_code(ir,2,output,8,(size_t*)ir)==EINVAL);
 CHECK(remesa_pds_structure_encode_code(ir,2,output,8,NULL)==EINVAL);
 printf("PASS CPU record-last E6: %u checks; 34 original constructor oracles; no event E0 or GPU admission\n",checks);
 return 0;
}
