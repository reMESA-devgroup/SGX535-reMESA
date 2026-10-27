/* SPDX-License-Identifier: MIT */
/* Independent original CPU producer records are supplied on stdin only. */
#include "remesa_usse.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define C(x) do{checks++;if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static int encode(unsigned transfer,unsigned mode,void *out,size_t cap,size_t *n)
{return transfer?remesa_usse_psb_event_transfer_encode(mode,out,cap,n):remesa_usse_psb_event_end_encode(out,cap,n);}
int main(void)
{
 char kind[16],hex[40];unsigned mode,rows=0,seen=0;
 while(scanf("%15s %u %39s",kind,&mode,hex)==3){
  unsigned transfer=!strcmp(kind,"transfer");C(transfer||!strcmp(kind,"end"));
  unsigned size=transfer?16:8;C(strlen(hex)==size*2);uint8_t original[16];
  for(unsigned i=0;i<size;i++){unsigned v;C(sscanf(hex+2*i,"%2x",&v)==1);original[i]=v;}
  C(!transfer?mode==0:(mode==0||mode==1||mode==3));
  unsigned bit=transfer?(1u<<(mode+1)):1;C(!(seen&bit));seen|=bit;
  for(unsigned offset=0;offset<16;offset++){
   uint8_t out[64],saved[64];memset(out,0xa5,64);memcpy(saved,out,64);size_t n=999;
   C(!encode(transfer,mode,out+offset,64-offset,&n)&&n==size);
   C(!memcmp(out+offset,original,size));C(!memcmp(out,saved,offset));
   C(!memcmp(out+offset+size,saved+offset+size,64-offset-size));
  }
  for(unsigned cap=0;cap<size;cap++){
   uint8_t out[32],saved[32];memset(out,0x47,32);memcpy(saved,out,32);size_t n=777;
   C(encode(transfer,mode,out,cap,&n)==ENOSPC&&n==777&&!memcmp(out,saved,32));
  }
  rows++;
 }
 C(feof(stdin)&&rows==4&&seen==23);
 for(unsigned t=0;t<2;t++){
  union {size_t alignment;uint8_t out[64];} u;memset(&u,0x5a,sizeof(u));uint8_t saved[64];memcpy(saved,u.out,64);size_t n=123;
  C(encode(t,0,NULL,32,&n)==EINVAL&&n==123);
  C(encode(t,0,u.out,32,NULL)==EINVAL&&!memcmp(u.out,saved,64));
  C(encode(t,0,u.out,32,(void *)u.out)==EINVAL&&!memcmp(u.out,saved,64));
  C(encode(t,0,u.out,32,(void *)(u.out+1))==EINVAL&&!memcmp(u.out,saved,64));
  C(encode(t,0,(void *)(UINTPTR_MAX-3),32,&n)==EINVAL&&n==123);
  C(encode(t,0,u.out,SIZE_MAX,&n)==EINVAL&&n==123&&!memcmp(u.out,saved,64));
 }
 for(unsigned mode=0;mode<256;mode++)if(mode!=0&&mode!=1&&mode!=3){
  uint8_t out[32],saved[32];memset(out,0x23,32);memcpy(saved,out,32);size_t n=999;
  C(remesa_usse_psb_event_transfer_encode(mode,out,32,&n)==ENOTSUP&&n==999&&!memcmp(out,saved,32));
 }
 uint8_t out[32],saved[32];memset(out,0x23,32);memcpy(saved,out,32);size_t n=999;
 C(remesa_usse_psb_event_transfer_encode(UINT32_MAX,out,32,&n)==ENOTSUP&&n==999&&!memcmp(out,saved,32));
 /* Existing EE remains a separate producer with distinct emitted fields. */
 struct remesa_usse_cpu_control_ee ee={0};uint8_t end[8];size_t ne=0;
 C(!remesa_usse_cpu_control_ee_encode(&ee,out,32,&n)&&n==8);
 C(!remesa_usse_psb_event_end_encode(end,8,&ne)&&ne==8&&memcmp(out,end,8));
 printf("PASS PSB event auxiliaries:4 original CPU oracles, transactional capacities/aliases/unaligned-byte-output, admitted caller modes only, selectorEE distinct; GPU0 (%uchecks)\n",checks);return 0;
}
