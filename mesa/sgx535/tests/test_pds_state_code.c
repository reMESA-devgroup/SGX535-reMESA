/* SPDX-License-Identifier: MIT */
/* stdin: original state CODE12 bytes, then independently produced GLES
 * pair words8 bytes. No historical bytes are embedded in the encoder/test. */
#include "remesa_pds_structure.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do {assert(x);checks++;} while(0)
static void reject(const struct remesa_pds_code_instruction *ir,size_t n,int err)
{
 unsigned char out[32],old[32];memset(out,0xa5,sizeof(out));memcpy(old,out,sizeof(out));size_t written=53;
 CHECK(remesa_pds_structure_encode_code(ir,n,out,sizeof(out),&written)==err);
 CHECK(written==53&&!memcmp(out,old,sizeof(out)));
}
int main(void)
{
 unsigned char oracle[20];CHECK(fread(oracle,1,sizeof(oracle),stdin)==sizeof(oracle));CHECK(fgetc(stdin)==EOF);
 uint32_t none=REMESA_PDS_STRUCTURE_NO_CELL;
 struct remesa_pds_code_instruction ir[3]={
  {REMESA_PDS_STRUCTURE_CPU_TRANSFER_PAIR,0,none},
  {REMESA_PDS_STRUCTURE_PAIR_SINGLE,2,2},
  {REMESA_PDS_STRUCTURE_CPU_TRAILER_AF,none,none}};
 unsigned char out[32];memset(out,0xa5,sizeof(out));size_t written=53;
 CHECK(!remesa_pds_structure_encode_code(ir,3,out,sizeof(out),&written));CHECK(written==12&&!memcmp(out,oracle,12));
 for(unsigned i=12;i<sizeof(out);i++)CHECK(out[i]==0xa5);
 for(unsigned k=0;k<2;k++) {
  struct remesa_pds_code_instruction pair={REMESA_PDS_STRUCTURE_CPU_TRANSFER_PAIR,2*k,none};
  CHECK(!remesa_pds_structure_encode_code(&pair,1,out,sizeof(out),&written));CHECK(written==4&&!memcmp(out,oracle+12+4*k,4));
 }
 struct remesa_pds_code_instruction bad[3];memcpy(bad,ir,sizeof(ir));bad[0].ds0_index=2;reject(bad,3,EINVAL);
 memcpy(bad,ir,sizeof(ir));bad[1].ds1_index=1;reject(bad,3,EINVAL);
 memcpy(bad,ir,sizeof(ir));bad[0].ds1_index=0;reject(bad,1,EINVAL);
 for(unsigned k=1;k<=4;k++)if(k!=2) {memcpy(bad,ir,sizeof(ir));bad[0].ds0_index=k;reject(bad,1,ENOTSUP);}
 memcpy(bad,ir,sizeof(ir));bad[0].ds0_index=64;reject(bad,1,ERANGE);
 memcpy(bad,ir,sizeof(ir));bad[1].ds0_index=0;bad[1].ds1_index=2;reject(bad+1,1,ENOTSUP);
 memcpy(bad,ir,sizeof(ir));bad[1].ds0_index=2;bad[1].ds1_index=3;reject(bad+1,1,ENOTSUP);
 reject(ir+2,1,EINVAL); /* No generic AF promotion. */
 puts("PASS state CPU CODE; independent state/pair oracles matched");printf("checks=%u\n",checks);return 0;
}
