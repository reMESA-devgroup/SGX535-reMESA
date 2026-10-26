/* SPDX-License-Identifier: MIT */
#include "remesa_render_target.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned word(const unsigned char *p)
{return (unsigned)p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24);}
int main(int argc,char **argv)
{
 bool dump=argc==2&&!strcmp(argv[1],"--dump");assert(argc==1||dump);
 const unsigned counts[]={2,3,4,5,16,24},groups[]={1,1,1,2,4,6};
 unsigned char bytes[8],before[8];unsigned n=0;
 for(unsigned c=0;c<6;c++)for(unsigned flag=0;flag<2;flag++)for(unsigned offset=0;offset<2;offset++) {
  uint64_t address=UINT64_C(0x20000160)+offset*16;
  assert(!remesa_ta_state_reference_encode(address,counts[c],flag,bytes));
  assert(word(bytes)==((flag?0x62000016u:0x42000016u)+offset));
  assert(word(bytes+4)==(0x0c02a200u|groups[c]));
  if(dump) {
   printf("%u %llu %u %u ",n,(unsigned long long)address,counts[c],flag);
   for(unsigned i=0;i<8;i++)printf("%02x",bytes[i]);
   puts("");
  }
  n++;
 }
 /* Separate known carrier command oracles. Counts come from the referenced
  * state-source DATA control, not a copied PDS CODE program. */
 const uint64_t addresses[]={UINT64_C(0x20004380),UINT64_C(0x20004400),UINT64_C(0x20004460)};
 const unsigned sizes[]={11,14,2};
 const unsigned expected[3][2]={{0x42000438,0x0c02a203},{0x42000440,0x0c02a204},{0x62000446,0x0c02a201}};
 for(unsigned c=0;c<3;c++) {
  bool flag=c==2;
  assert(!remesa_ta_state_reference_encode(addresses[c],sizes[c],flag,bytes));
  assert(word(bytes)==expected[c][0]&&word(bytes+4)==expected[c][1]);
  if(dump) {
   printf("%u %llu %u %u ",n,(unsigned long long)addresses[c],sizes[c],flag);
   for(unsigned i=0;i<8;i++)printf("%02x",bytes[i]);
   puts("");
  }
  n++;
 }
 memset(bytes,0xa5,sizeof(bytes));memcpy(before,bytes,8);
 const uint64_t invalid[]={0,UINT64_C(0x20000161),UINT64_C(0x100000000),UINT64_MAX};
 for(unsigned i=0;i<4;i++)assert(remesa_ta_state_reference_encode(invalid[i],2,false,bytes)==EINVAL&&!memcmp(bytes,before,8));
 assert(remesa_ta_state_reference_encode(0x20000160,0,false,bytes)==EINVAL&&!memcmp(bytes,before,8));
 assert(remesa_ta_state_reference_encode(0x20000160,1,false,bytes)==EINVAL&&!memcmp(bytes,before,8));
 assert(remesa_ta_state_reference_encode(0x20000160,25,false,bytes)==ENOTSUP&&!memcmp(bytes,before,8));
 assert(remesa_ta_state_reference_encode(0x20000160,UINT32_MAX,false,bytes)==ENOTSUP&&!memcmp(bytes,before,8));
 assert(remesa_ta_state_reference_encode(0x20000160,2,false,NULL)==EINVAL);
 if(!dump)puts("PASS:24 original-x86 and3 carrier TA references, DATA-domain separation, address/length limits and transactional rejection; CPU ONLY, no complete job");
 return 0;
}
