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
 struct remesa_usse_cpu_arithmetic r={0};unsigned count=0;char hex[17];
 while(scanf("%u %u %u %u",&r.kind,&r.flags,&r.destination.cpu_class,&r.destination.base)==4) {
  for(unsigned i=0;i<3;i++)CHECK(scanf("%u %u %u",&r.source[i].cpu_class,&r.source[i].base,&r.modifier[i])==3);
  CHECK(scanf("%16s",hex)==1&&strlen(hex)==16);unsigned char expected[8],out[16];memset(out,0x86,16);size_t n=991;
  for(unsigned i=0;i<8;i++){unsigned v;CHECK(sscanf(hex+2*i,"%2x",&v)==1);expected[i]=v;}
  CHECK(!remesa_usse_cpu_arithmetic_encode(&r,out,16,&n));CHECK(n==8&&!memcmp(out,expected,8));
  for(unsigned i=8;i<16;i++)CHECK(out[i]==0x86);
  count++;
 }
 CHECK(count==16);
 const struct remesa_usse_cpu_arithmetic valid={.kind=0,.flags=0x41,.destination={1,0},.source={{0,0},{2,0},{6,48}}};
 for(unsigned mode=0;mode<13;mode++) {
  r=valid;int error=ENOTSUP;
  if(mode==0)r.kind=3;
  if(mode==1)r.flags=0x81;
  if(mode==2)r.destination.cpu_class=3;
  if(mode==3)r.destination.base=128;
  if(mode==4)r.source[0].cpu_class=1;
  if(mode==5)r.source[1].cpu_class=3;
  if(mode==6)r.source[2].base=49;
  if(mode==7)r.source[0].base=125;
  if(mode==8)r.modifier[2]=1;
  if(mode==9){r.reserved=1;error=EINVAL;}
  if(mode==10)error=ENOSPC;
  if(mode==11){r=(struct remesa_usse_cpu_arithmetic){.kind=5,.flags=0x201,.source={{0,0},{0,1}},.modifier={2}};error=EINVAL;}
  if(mode==12){r=(struct remesa_usse_cpu_arithmetic){.kind=5,.flags=0x201,.source={{0,128}},.modifier={2}};}
  unsigned char out[16],saved[16];memset(out,0x49,16);memcpy(saved,out,16);size_t n=991;
  CHECK(remesa_usse_cpu_arithmetic_encode(&r,out,mode==10?7:16,&n)==error);CHECK(n==991&&!memcmp(out,saved,16));
 }
 r=valid;unsigned char out[8];size_t n=991;
 CHECK(remesa_usse_cpu_arithmetic_encode(&r,&r,sizeof(r),&n)==EINVAL);
 CHECK(remesa_usse_cpu_arithmetic_encode(NULL,out,8,&n)==EINVAL);
 CHECK(remesa_usse_cpu_arithmetic_encode(&r,out,SIZE_MAX,&n)==EINVAL);
 CHECK(remesa_usse_cpu_arithmetic_encode(&r,out,8,(void *)out)==EINVAL);
 printf("PASS sixteen original arithmetic bodies including normalize temporaries and ADD/MUL constants; class0 temporaries, class2 input, class6 constants, source/destination/flags packing and transactional refusal; NO GPU (%u checks)\n",checks);
 return 0;
}
