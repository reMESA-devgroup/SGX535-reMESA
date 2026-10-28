/* SPDX-License-Identifier: MIT */
/* CPU constant-loader profile only; no physical DMA or shader execution. */
#include "remesa_pds_structure.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef NDEBUG
#error Assertions must remain enabled
#endif
static unsigned checks;
static void reject(struct remesa_pds_constant_image_request r,size_t cap,int error)
{
 unsigned char b[80],m[80],old[80];memset(old,0xa5,sizeof old);
 memcpy(b,old,sizeof b);memcpy(m,old,sizeof m);size_t n=37;
 assert(remesa_pds_structure_encode_constant_image(&r,b,m,cap,&n)==error);
 assert(n==37&&!memcmp(b,old,sizeof b)&&!memcmp(m,old,sizeof m));checks++;
}
static uint32_t le(const unsigned char *p)
{return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int main(void)
{
 struct remesa_pds_constant_image_request r={.source_allocation_address=0x40001000,
  .source_allocation_bytes=512,.constant_dwords=5,.use_lower19=4};
 unsigned char b[80],m[80];size_t n;
 memset(b,0xa5,sizeof b);memset(m,0xa5,sizeof m);
 assert(!remesa_pds_structure_encode_constant_image(&r,b,m,80,&n)&&n==60);
 assert(le(b)==0x40001000&&le(b+4)==4&&le(b+8)==4&&!le(b+12)&&!le(b+32));
 assert(le(b+48)==0x07030223&&le(b+52)==0x07040345&&le(b+56)==0xaf000000);
 for(unsigned i=60;i<80;i++)assert(b[i]==0xa5&&m[i]==0xa5);
 checks++;
 for(unsigned c=0;c<60;c++)reject(r,c,ENOSPC);
 r.constant_dwords=17;reject(r,63,ENOSPC);
 assert(!remesa_pds_structure_encode_constant_image(&r,b,m,80,&n)&&n==64);
 assert(le(b+4)==0x01e0000f&&le(b+8)==0x40001040&&le(b+12)==0x1000);
 assert(le(b+48)==0x07030223&&le(b+52)==0x07070223&&le(b+56)==0x07080345);checks++;
 r.constant_dwords=0;reject(r,80,EINVAL);r.constant_dwords=129;reject(r,80,ENOTSUP);
 r.constant_dwords=128;r.source_allocation_address=0xfffffe00;
 assert(!remesa_pds_structure_encode_constant_image(&r,b,m,80,&n));checks++;
 r.source_allocation_bytes=513;reject(r,80,EOVERFLOW);r.source_allocation_bytes=512;
 r.source_offset=4;reject(r,80,ENOSPC);r.source_offset=UINT64_MAX-3;reject(r,80,ENOSPC);
 r.source_offset=1;reject(r,80,EINVAL);r.source_offset=0;
 r.source_allocation_address=0;reject(r,80,EINVAL);
 r.source_allocation_address=UINT64_C(0x100000000);reject(r,80,EINVAL);
 r.source_allocation_address=0x40001001;reject(r,80,EINVAL);
 r.source_allocation_address=0x40001000;r.use_lower19=0x80000;reject(r,80,ERANGE);
 r.use_lower19=4;n=37;memset(b,0xa5,sizeof b);
 assert(remesa_pds_structure_encode_constant_image(&r,b,b,80,&n)==EINVAL);
 assert(n==37);for(unsigned i=0;i<80;i++)assert(b[i]==0xa5);
 checks++;
 assert(remesa_pds_structure_encode_constant_image(NULL,b,m,80,&n)==EINVAL);checks++;
 const uint32_t none=REMESA_PDS_STRUCTURE_NO_CELL;
 struct remesa_pds_code_instruction code[]={
  {REMESA_PDS_STRUCTURE_CPU_TRANSFER_PAIR,0,none},
  {REMESA_PDS_STRUCTURE_CPU_TRANSFER_PAIR,2,none},
  {REMESA_PDS_STRUCTURE_PAIR_SINGLE,4,0},
  {REMESA_PDS_STRUCTURE_CPU_TRAILER_AF,none,none}};
 assert(!remesa_pds_structure_encode_code(code,4,b,80,&n));checks++;
 code[1].ds0_index=0;n=37;memset(b,0xa5,sizeof b);
 assert(remesa_pds_structure_encode_code(code,4,b,80,&n)==EINVAL);
 assert(n==37);for(unsigned i=0;i<80;i++)assert(b[i]==0xa5);
 checks++;
 printf("PDS constant image CPU checks=%u GPU operations=0\n",checks);
 return 0;
}
