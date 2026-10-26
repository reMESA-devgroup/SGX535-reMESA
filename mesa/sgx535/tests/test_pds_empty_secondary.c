/* SPDX-License-Identifier: MIT */
/* Oracle bytes on stdin come from the independently executed original
 * empty-secondary producer or the relocation-bound retained auxiliary image.
 * This tests CPU construction equivalence, not AF hardware semantics. */
#include "remesa_pds_structure.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { assert(x); checks++; } while(0)
static void reject(const struct remesa_pds_code_instruction *ir,size_t n,
 size_t capacity,int error)
{
 unsigned char bytes[16],old[16];memset(bytes,0xa5,sizeof(bytes));
 memcpy(old,bytes,sizeof(bytes));size_t written=91;
 CHECK(remesa_pds_structure_encode_code(ir,n,bytes,capacity,&written)==error);
 CHECK(written==91&&!memcmp(bytes,old,sizeof(bytes)));
}
int main(void)
{
 unsigned char oracle[4],bytes[12];CHECK(fread(oracle,1,4,stdin)==4);
 CHECK(fgetc(stdin)==EOF);const uint32_t none=REMESA_PDS_STRUCTURE_NO_CELL;
 struct remesa_pds_code_instruction ir[2]={
  {REMESA_PDS_STRUCTURE_CPU_EMPTY_SECONDARY_AF,none,none},
  {REMESA_PDS_STRUCTURE_CPU_EMPTY_SECONDARY_AF,none,none}};
 memset(bytes,0xa5,sizeof(bytes));size_t written=91;
 CHECK(!remesa_pds_structure_encode_code(ir,1,bytes,sizeof(bytes),&written));
 CHECK(written==4&&!memcmp(bytes,oracle,4));
 for(unsigned i=4;i<sizeof(bytes);i++)CHECK(bytes[i]==0xa5);
 reject(ir,1,3,ENOSPC);reject(ir,2,16,EINVAL);
 ir[0].ds0_index=0;reject(ir,1,16,EINVAL);ir[0].ds0_index=none;
 ir[0].ds1_index=0;reject(ir,1,16,EINVAL);ir[0].ds1_index=none;
 ir[1]=(struct remesa_pds_code_instruction){REMESA_PDS_STRUCTURE_PAIR_SINGLE,0,0};
 reject(ir,2,16,EINVAL);
 ir[0]=(struct remesa_pds_code_instruction){REMESA_PDS_STRUCTURE_PAIR_SINGLE,0,0};
 ir[1]=(struct remesa_pds_code_instruction){REMESA_PDS_STRUCTURE_CPU_EMPTY_SECONDARY_AF,none,none};
 reject(ir,2,16,EINVAL);
 ir[0]=(struct remesa_pds_code_instruction){REMESA_PDS_STRUCTURE_CPU_TRAILER_AF,none,none};
 reject(ir,1,16,EINVAL); /* Existing primary grammar remains distinct. */
 ir[0].kind=REMESA_PDS_STRUCTURE_FINAL_HOLE;reject(ir,1,16,ENOTSUP);
 ir[0].kind=999;reject(ir,1,16,ENOTSUP);
 printf("PASS empty-secondary CPU grammar: %u checks; independent oracle matched\n",checks);
 return 0;
}
