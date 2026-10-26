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
 struct remesa_usse_cpu_normalize_scale r={0};char hex[81];unsigned count=0;
 while(scanf("%u %u %80s",&r.normal_base,&r.value_base,hex)==3) {
  CHECK(strlen(hex)==80);unsigned char expected[40],out[48];memset(out,0x86,48);size_t n=991;
  for(unsigned i=0;i<40;i++){unsigned v;CHECK(sscanf(hex+2*i,"%2x",&v)==1);expected[i]=v;}
  CHECK(!remesa_usse_cpu_normalize_scale_encode(&r,out,48,&n));CHECK(n==40&&!memcmp(out,expected,40));
  for(unsigned i=40;i<48;i++)CHECK(out[i]==0x86);
  count++;
 }
 CHECK(count==3);
 for(unsigned mode=0;mode<8;mode++) {
  r=(struct remesa_usse_cpu_normalize_scale){0,4,0};int error=ENOTSUP;
  if(mode==0)r.normal_base=UINT32_MAX;
  if(mode==1)r.value_base=125;
  if(mode==2)r.value_base=1;
  if(mode==3){r.normal_base=2;r.value_base=0;}
  if(mode==4){r.reserved=1;error=EINVAL;}
  if(mode==5)error=ENOSPC;
  if(mode==6)error=EINVAL;
  if(mode==7)error=EINVAL;
  unsigned char out[48],before[48];memset(out,0x48,48);memcpy(before,out,48);size_t n=991;
  CHECK(remesa_usse_cpu_normalize_scale_encode(&r,mode==6?(void *)&r:out,mode==5?39:mode==7?SIZE_MAX:48,&n)==error);
  CHECK(n==991&&!memcmp(out,before,48));
 }
 r=(struct remesa_usse_cpu_normalize_scale){0,4,0};unsigned char out[40];size_t n=991;
 CHECK(remesa_usse_cpu_normalize_scale_encode(NULL,out,40,&n)==EINVAL);
 CHECK(remesa_usse_cpu_normalize_scale_encode(&r,NULL,40,&n)==EINVAL);
 CHECK(remesa_usse_cpu_normalize_scale_encode(&r,out,40,(void *)out)==EINVAL);
 CHECK(remesa_usse_cpu_normalize_scale_encode(&r,out,40,NULL)==EINVAL);
 printf("PASS independent full40-byte CPU normalization/scale CODE;3 original IR/compiler oracles with live shared versus dead reusable inputs; bounds/alias/rollback; NO GPU (%u checks)\n",checks);
 return 0;
}
