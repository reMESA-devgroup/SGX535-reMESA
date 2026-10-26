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
 unsigned op,side,oracles=0;char hex[17];
 while(scanf("%u %u %16s",&op,&side,hex)==3) {
  CHECK(side<2&&strlen(hex)==16);
  struct remesa_usse_arithmetic_sources r={.op=op,.left={side?3:2,side?1:0},.right={side?2:3,side?0:1}};
  unsigned char out[16],expected[8];memset(out,0xa7,sizeof(out));size_t n=991;
  for(unsigned i=0;i<8;i++){unsigned v;CHECK(sscanf(hex+2*i,"%2x",&v)==1);expected[i]=v;}
  CHECK(!remesa_usse_arithmetic_sources_encode(&r,out,16,&n));
  CHECK(n==8&&!memcmp(out,expected,8));
  for(unsigned i=8;i<16;i++)CHECK(out[i]==0xa7);
  oracles++;
 }
 CHECK(oracles==8);
 struct remesa_usse_arithmetic_sources r={.op=1,.left={2,0},.right={3,1}};
 for(unsigned mode=0;mode<8;mode++) {
  struct remesa_usse_arithmetic_sources invalid=r;
  if(mode==0)invalid.left.base=4;
  if(mode==1)invalid.right.base=0;
  if(mode==2)invalid.right.base=5;
  if(mode==3)invalid.left.cpu_class=3;
  if(mode==4)invalid.right.cpu_class=6;
  if(mode==5)invalid.op=4;
  if(mode==6)invalid.reserved=1;
  unsigned char out[16],saved[16];memset(out,0xb6,16);memcpy(saved,out,16);size_t n=991;
  CHECK(remesa_usse_arithmetic_sources_encode(&invalid,out,mode==7?7:16,&n)==(mode==6?EINVAL:mode==7?ENOSPC:ENOTSUP));
  CHECK(n==991&&!memcmp(out,saved,16));
 }
 unsigned char out[16];size_t n=991;
 CHECK(remesa_usse_arithmetic_sources_encode(&r,&r,sizeof(r),&n)==EINVAL);
 CHECK(remesa_usse_arithmetic_sources_encode(&r,out,16,(void *)out)==EINVAL);
 CHECK(remesa_usse_arithmetic_sources_encode(NULL,out,16,&n)==EINVAL);
 CHECK(remesa_usse_arithmetic_sources_encode(&r,NULL,16,&n)==EINVAL);
 CHECK(remesa_usse_arithmetic_sources_encode(&r,out,SIZE_MAX,&n)==EINVAL);
 for(op=1;op<=2;op++) {
  r=(struct remesa_usse_arithmetic_sources){.op=op,.left={2,4},.right={2,0}};
  struct remesa_usse_arithmetic old={op,4,0,0};unsigned char previous[8];size_t m;
  CHECK(!remesa_usse_arithmetic_encode(&old,previous,8,&m));
  CHECK(!remesa_usse_arithmetic_sources_encode(&r,out,16,&n)&&n==m&&!memcmp(out,previous,8));
 }
 printf("PASS coefficient ADD/MUL8 original compiler oracles, source canonicalization, restricted indices, rejection/rollback and old API equivalence; CPU ONLY (%u checks)\n",checks);
 return 0;
}
