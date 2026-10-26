/* SPDX-License-Identifier: MIT */
#include "remesa_pds_structure.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do {assert(x);checks++;} while(0)
static void reject(const struct remesa_pds_code_instruction *ir,size_t count,int error)
{unsigned char out[32],old[32];memset(out,0xa5,32);memcpy(old,out,32);size_t n=91;
 CHECK(remesa_pds_structure_encode_code(ir,count,out,32,&n)==error);CHECK(n==91&&!memcmp(old,out,32));}
int main(void)
{
 unsigned char oracle[40];CHECK(fread(oracle,1,40,stdin)==40);CHECK(fgetc(stdin)==EOF);
 const uint32_t no=REMESA_PDS_STRUCTURE_NO_CELL;
 struct remesa_pds_code_instruction one[]={
  {REMESA_PDS_STRUCTURE_CPU_VERTEX_SETUP,no,no},
  {REMESA_PDS_STRUCTURE_CPU_VERTEX_PAIR,0,no},
  {REMESA_PDS_STRUCTURE_CPU_VERTEX_TASK,4,1},
  {REMESA_PDS_STRUCTURE_CPU_TRAILER_AF,no,no}};
 struct remesa_pds_code_instruction two[5];memcpy(two,one,2*sizeof(*one));
 two[2]=(struct remesa_pds_code_instruction){REMESA_PDS_STRUCTURE_CPU_VERTEX_PAIR,2,no};
 two[3]=one[2];two[4]=one[3];unsigned char out[32];size_t n=91;
 CHECK(!remesa_pds_structure_encode_code(one,4,out,32,&n));CHECK(n==16&&!memcmp(out,oracle,16));
 CHECK(!remesa_pds_structure_encode_code(two,5,out,32,&n));CHECK(n==20&&!memcmp(out,oracle+16,20));
 CHECK(!remesa_pds_structure_encode_code(one+2,1,out,32,&n));CHECK(n==4&&!memcmp(out,oracle+36,4));
 struct remesa_pds_code_instruction bad[5];
 memcpy(bad,one,sizeof(one));bad[0].ds0_index=0;reject(bad,4,EINVAL);
 memcpy(bad,one,sizeof(one));bad[1].ds0_index=2;reject(bad,4,EINVAL);
 memcpy(bad,one,sizeof(one));bad[1].ds0_index=1;reject(bad+1,1,ENOTSUP);
 memcpy(bad,one,sizeof(one));bad[1].ds0_index=64;reject(bad+1,1,ERANGE);
 memcpy(bad,one,sizeof(one));bad[1].ds1_index=0;reject(bad+1,1,EINVAL);
 memcpy(bad,one,sizeof(one));bad[2].ds0_index=2;reject(bad+2,1,ENOTSUP);
 memcpy(bad,one,sizeof(one));bad[2].ds1_index=0;reject(bad+2,1,ENOTSUP);
 memcpy(bad,one,sizeof(one));bad[2].ds1_index=64;reject(bad+2,1,ERANGE);
 memcpy(bad,two,sizeof(two));bad[2].ds0_index=0;reject(bad,5,EINVAL);
 reject(one+3,1,EINVAL);reject(one+1,3,EINVAL);
 printf("PASS vertex CPU CODE checks=%u; independent producer/03 packer matched\n",checks);return 0;
}
