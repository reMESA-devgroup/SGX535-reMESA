/* SPDX-License-Identifier: MIT */
#include "remesa_usse_arithmetic.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;assert(x);}while(0)
int main(void)
{
 unsigned l,r,count=0;char hex[17];
 while(scanf("%u %u %16s",&l,&r,hex)==3) {
  struct remesa_usse_arithmetic_sources q={.op=REMESA_USSE_ARITHMETIC_MAXIMUM,.left={2,l},.right={2,r}};
  unsigned char bytes[16],oracle[8];size_t n=991;memset(bytes,0xa5,16);
  CHECK(strlen(hex)==16);
  for(unsigned i=0;i<8;i++){unsigned v;CHECK(sscanf(hex+2*i,"%2x",&v)==1);oracle[i]=v;}
  CHECK(!remesa_usse_arithmetic_sources_encode(&q,bytes,16,&n));
  CHECK(n==8&&!memcmp(bytes,oracle,8));
  for(unsigned i=8;i<16;i++)CHECK(bytes[i]==0xa5);
  count++;
 }
 CHECK(count==2);
 for(unsigned mode=0;mode<7;mode++) {
  struct remesa_usse_arithmetic_sources q={.op=3,.left={2,0},.right={2,4}};
  if(mode==0)q.left.base=128;
  if(mode==1)q.right.base=3;
  if(mode==2)q.right.cpu_class=3;
  if(mode==3)q.left.cpu_class=3;
  if(mode==4)q.reserved=1;
  if(mode==5)q.op=4;
  unsigned char bytes[16],saved[16];size_t n=991;memset(bytes,0x83,16);memcpy(saved,bytes,16);
  CHECK(remesa_usse_arithmetic_sources_encode(&q,bytes,mode==6?7:16,&n)==(mode==4?EINVAL:mode==6?ENOSPC:ENOTSUP));
  CHECK(n==991&&!memcmp(bytes,saved,16));
 }
 struct remesa_usse_arithmetic old={3,0,4,0};unsigned char bytes[8];size_t n=991;
 CHECK(remesa_usse_arithmetic_encode(&old,bytes,8,&n)==ENOTSUP&&n==991);
 puts("MAX CPU encoding: two original compiler source-order oracles; no setup/export/physical admission");
 printf("PASS USSE MAX CPU ONLY (%u checks)\n",checks);return 0;
}
