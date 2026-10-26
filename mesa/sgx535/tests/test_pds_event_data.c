/* SPDX-License-Identifier: MIT */
/* Original Xpsb constructor output is stdin; relocation ABI cuts are excluded
 * from oracle matches and separately checked as symbolic fields. */
#include "remesa_pds_structure.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;assert(x);}while(0)
static unsigned read32(const unsigned char *p)
{return (unsigned)p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
int main(void)
{
 unsigned offset,oracle[16],cases=0;
 while(scanf("%u",&offset)==1){
  for(unsigned i=0;i<16;i++)CHECK(scanf("%u",oracle+i)==1);
  for(unsigned variant=0;variant<2;variant++){
   struct remesa_pds_event_data_request r={.source_allocation_address=0x20000000,
    .source_allocation_bytes=0x20000,.source_offset=offset+64,
    .use_lower19=variant?0x12340:0};
   unsigned char out[72],mask[72];memset(out,0xa5,72);memset(mask,0x5a,72);size_t n=999;
   CHECK(!remesa_pds_structure_encode_event_data(&r,out,mask,72,&n));CHECK(n==64);
   CHECK(read32(out)==0x20000000+offset+64);
   CHECK(read32(out+8)==(0x200000|r.use_lower19));
   for(unsigned i=0;i<16;i++){
    int known=i<4||(i>=8&&i<=12);
    for(unsigned j=0;j<4;j++)CHECK(mask[4*i+j]==(known?255:0));
    if(known&&i!=0&&i!=2)CHECK(read32(out+4*i)==oracle[i]);
    if(!known)CHECK(!read32(out+4*i));
   }
   for(unsigned i=64;i<72;i++)CHECK(out[i]==0xa5&&mask[i]==0x5a);
  }
  cases++;
 }
 CHECK(cases==3);
 const struct remesa_pds_event_data_request good={0x20000000,4096,64,0x7ffff};
 for(unsigned mode=0;mode<12;mode++){
  struct remesa_pds_event_data_request r=good;
  unsigned char out[64],mask[64],saved[64],savedmask[64];
  memset(out,0xa5,64);memset(mask,0x5a,64);memcpy(saved,out,64);memcpy(savedmask,mask,64);
  size_t n=999,capacity=64;int expected=EINVAL;
  switch(mode){
  case 0:r.use_lower19=0x80000;expected=ERANGE;break;
  case 1:r.source_allocation_address=0;break;
  case 2:r.source_allocation_address++;break;
  case 3:r.source_offset++;break;
  case 4:r.source_allocation_bytes=0;expected=EOVERFLOW;break;
  case 5:r.source_allocation_address=0xfffffffc;r.source_allocation_bytes=8;expected=EOVERFLOW;break;
  case 6:r.source_offset=4092;expected=ENOSPC;break;
  case 7:r.source_offset=UINT64_MAX-3;expected=ENOSPC;break;
  case 8:capacity=63;expected=ENOSPC;break;
  case 9:r.source_allocation_address=UINT64_C(0x100000000);break;
  case 10:r.source_allocation_bytes=UINT64_MAX;expected=EOVERFLOW;break;
  case 11:r.source_offset=4096;expected=ENOSPC;break;
  }
  CHECK(remesa_pds_structure_encode_event_data(&r,out,mask,capacity,&n)==expected);
  CHECK(n==999&&!memcmp(out,saved,64)&&!memcmp(mask,savedmask,64));
 }
 unsigned char out[64],mask[64];size_t n=999;
 CHECK(remesa_pds_structure_encode_event_data(&good,out,out,64,&n)==EINVAL);
 CHECK(remesa_pds_structure_encode_event_data(&good,out,mask,64,(void *)out)==EINVAL);
 CHECK(remesa_pds_structure_encode_event_data(&good,(void *)&good,mask,64,&n)==EINVAL);
 CHECK(remesa_pds_structure_encode_event_data(NULL,out,mask,64,&n)==EINVAL);
 printf("PASS CPU_ONLY event DATA3originals/2symbolicvariants, known36bytes/object, transactional bounds; CODE remains unsupported (%u checks)\n",checks);
 return 0;
}
