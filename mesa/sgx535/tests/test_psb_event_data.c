/* SPDX-License-Identifier: MIT. Original DATA only; no event CODE or GPU. */
#include "remesa_pds_structure.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define C(x) do{checks++;assert(x);}while(0)
static uint32_t word(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int main(void)
{
 unsigned offset,end,transfer,cases=0;char hex[129];
 while(scanf("%u %u %u %128s",&offset,&end,&transfer,hex)==4){
  C(strlen(hex)==128);uint8_t original[64];
  for(unsigned i=0;i<64;i++){unsigned v;C(sscanf(hex+2*i,"%2x",&v)==1);original[i]=(uint8_t)v;}
  struct remesa_pds_psb_event_data_request r={0x40001000,4096,offset,end,transfer};
  uint8_t out[80],mask[80];memset(out,0xa5,80);memset(mask,0x5a,80);size_t n=999;
  C(!remesa_pds_structure_encode_psb_event_data(&r,out,mask,80,&n)&&n==64);
  unsigned count=0;
  for(unsigned i=0;i<16;i++)for(unsigned b=0;b<4;b++){
   int known=i<6||(i>=8&&i<=14);C(mask[i*4+b]==(known?255:0));count+=known;
   if(known&&i!=0&&i!=2&&i!=4)C(out[i*4+b]==original[i*4+b]);
   if(!known)C(out[i*4+b]==0&&original[i*4+b]==0xa5);
  }
  C(count==52&&word(out)==end&&word(out+8)==0x40001000+offset&&word(out+16)==(0x200000|transfer));
  C(word(out+32)==8&&word(out+40)==2);
  for(unsigned i=64;i<80;i++)C(out[i]==0xa5&&mask[i]==0x5a);
  cases++;
 }
 C(cases==3);
 struct remesa_pds_psb_event_data_request base={0x20000000,0x20000,0x449c,0x1003,0xe03};
 uint8_t physical[64],mask[64];size_t n=0;
 C(!remesa_pds_structure_encode_psb_event_data(&base,physical,mask,64,&n)&&n==64);
 const uint32_t retained[16]={0x1003,0,0x2000449c,0x80000003,0x200e03,0,0,0,8,0,2,0x10000000,0x7f800,0x7f00000,0x8000000,0};
 for(unsigned i=0;i<16;i++)C(word(physical+4*i)==retained[i]);
 const struct remesa_pds_event_data_request xpsb={base.source_allocation_address,base.source_allocation_bytes,base.source_offset,base.transfer_use_lower19};
 uint8_t other[64],othermask[64];
 C(!remesa_pds_structure_encode_event_data(&xpsb,other,othermask,64,&n));
 C(word(other)==0x2000449c&&word(other+8)==0x200e03&&word(other+32)==2&&memcmp(other,physical,64));
 for(unsigned mode=0;mode<12;mode++){
  struct remesa_pds_psb_event_data_request r=base;int expected=EINVAL;size_t capacity=64;
  if(mode==0){r.end_use_lower19=0x80000;expected=ERANGE;}
  if(mode==1){r.transfer_use_lower19=0x80000;expected=ERANGE;}
  if(mode==2)r.source_allocation_address=0;
  if(mode==3)r.source_allocation_address=0x20000001;
  if(mode==4)r.source_offset=1;
  if(mode==5){r.source_allocation_bytes=0;expected=EOVERFLOW;}
  if(mode==6){r.source_allocation_bytes=UINT64_MAX;expected=EOVERFLOW;}
  if(mode==7){r.source_offset=0x1fff4;expected=ENOSPC;}
  if(mode==8){r.source_offset=UINT64_MAX;expected=EINVAL;}
  if(mode==9){capacity=63;expected=ENOSPC;}
  if(mode==10)r.source_allocation_address=UINT64_C(0x100000000);
  if(mode==11){r.source_allocation_address=0xfffffff0;r.source_allocation_bytes=32;expected=EOVERFLOW;}
  uint8_t out[64],known[64],saved[64],savedknown[64];memset(out,0xa5,64);memset(known,0x5a,64);
  memcpy(saved,out,64);memcpy(savedknown,known,64);n=999;
  int actual=remesa_pds_structure_encode_psb_event_data(&r,out,known,capacity,&n);
  if(actual!=expected)fprintf(stderr,"mode%u actual%d expected%d\n",mode,actual,expected);
  C(actual==expected&&n==999&&
   !memcmp(out,saved,64)&&!memcmp(known,savedknown,64));
 }
 uint8_t out[64],saved[64];memset(out,0x47,64);memcpy(saved,out,64);n=77;
 C(remesa_pds_structure_encode_psb_event_data(&base,out,out,64,&n)==EINVAL&&n==77&&!memcmp(out,saved,64));
 C(remesa_pds_structure_encode_psb_event_data(&base,(void *)&base,mask,64,&n)==EINVAL);
 C(remesa_pds_structure_encode_psb_event_data(&base,out,mask,64,(void *)out)==EINVAL);
 printf("PASS psb392ab DATA64:3 original i386 store-slice oracles (40direct bytes/12symbolic references/12policy), retained physical DATA correspondence, unchanged Xpsb profile and transactional bounds/aliases; CODE84 unsupported/GPU0 (%uchecks)\n",checks);return 0;
}
