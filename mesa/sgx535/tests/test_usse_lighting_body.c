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
 struct remesa_usse_lighting_body r={0};char hex[17];unsigned count=0;
 while(scanf("%u %u %u %16s",&r.op,&r.left_base,&r.right_base,hex)==4) {
  CHECK(strlen(hex)==16);unsigned char expected[8],out[16];memset(out,0xa7,16);size_t n=991;
  for(unsigned i=0;i<8;i++){unsigned v;CHECK(sscanf(hex+2*i,"%2x",&v)==1);expected[i]=v;}
  CHECK(!remesa_usse_lighting_body_encode(&r,out,16,&n)&&n==8&&!memcmp(out,expected,8));
  for(unsigned i=8;i<16;i++)CHECK(out[i]==0xa7);
  count++;
 }
 CHECK(count==6);
 for(unsigned mode=0;mode<8;mode++) {
  r=(struct remesa_usse_lighting_body){1,0,4,0};
  if(mode==0)r.op=3;
  if(mode==1)r.left_base=128;
  if(mode==2)r.right_base=3;
  if(mode==3){r.op=2;r.left_base=4;r.right_base=0;}
  if(mode==4){r.op=2;r.left_base=0;r.right_base=1;}
  if(mode==5)r.reserved=1;
  unsigned char out[16],saved[16];memset(out,0x85,16);memcpy(saved,out,16);size_t n=991;
  int error=mode==5?EINVAL:mode==6?ENOSPC:mode==7?EINVAL:ENOTSUP;
  CHECK(remesa_usse_lighting_body_encode(&r,mode==7?(void *)&r:out,mode==6?7:16,&n)==error);
  CHECK(n==991&&!memcmp(out,saved,16));
 }
 unsigned char out[8];size_t n=991;
 CHECK(remesa_usse_lighting_body_encode(NULL,out,8,&n)==EINVAL);
 CHECK(remesa_usse_lighting_body_encode(&r,NULL,8,&n)==EINVAL);
 CHECK(remesa_usse_lighting_body_encode(&r,out,8,(void *)out)==EINVAL);
 printf("PASS DOT3/RSQ CPU bodies6 independent original compiler oracles; symbolic sources, bounds, unknown/alias/transactional refusal; NO GPU (%u checks)\n",checks);
 return 0;
}
