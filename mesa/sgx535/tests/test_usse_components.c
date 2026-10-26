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
 unsigned count=0;
 for(;;) {
  struct remesa_usse_component_control r={0};char hex[17];
  int first=scanf("%u",&r.operand[0].format);if(first==EOF)break;CHECK(first==1);
  for(unsigned i=0;i<4;i++) {
   struct remesa_usse_component_operand *s=&r.operand[i];
   if(i)CHECK(scanf("%u",&s->format)==1);
   CHECK(scanf("%d %u %u %u %u",&s->value,&s->selector[0],&s->selector[1],&s->selector[2],&s->selector[3])==5);
  }
  CHECK(scanf("%16s",hex)==1&&strlen(hex)==16);
  unsigned char out[16],expected[8];memset(out,0xa7,16);size_t n=991;
  for(unsigned i=0;i<8;i++){unsigned v;CHECK(sscanf(hex+2*i,"%2x",&v)==1);expected[i]=v;}
  CHECK(!remesa_usse_component_control_encode(&r,out,16,&n));CHECK(n==8&&!memcmp(out,expected,8));
  for(unsigned i=8;i<16;i++)CHECK(out[i]==0xa7);
  count++;
 }
 CHECK(count==11);
 struct remesa_usse_component_control r={0};
 for(unsigned i=0;i<4;i++)r.operand[i].format=8;
 for(unsigned mode=0;mode<8;mode++) {
  struct remesa_usse_component_control bad=r;
  if(mode==0)bad.operand[0].value=-129;
  if(mode==1)bad.operand[0].value=128;
  if(mode==2)bad.operand[1].format=9;
  if(mode==3)bad.operand[0].selector[0]=1;
  if(mode==4){bad.operand[2].format=15;bad.operand[2].selector[3]=4;}
  if(mode==5){bad.operand[2].format=15;bad.operand[2].value=1;}
  if(mode==6)bad.reserved=1;
  unsigned char out[16],before[16];memset(out,0x9c,16);memcpy(before,out,16);size_t n=991;
  CHECK(remesa_usse_component_control_encode(&bad,out,mode==7?7:16,&n)==(mode==3||mode==5||mode==6?EINVAL:mode==7?ENOSPC:ENOTSUP));
  CHECK(n==991&&!memcmp(out,before,16));
 }
 unsigned char out[16];size_t n=991;
 CHECK(remesa_usse_component_control_encode(NULL,out,16,&n)==EINVAL);
 CHECK(remesa_usse_component_control_encode(&r,NULL,16,&n)==EINVAL);
 CHECK(remesa_usse_component_control_encode(&r,out,SIZE_MAX,&n)==EINVAL);
 CHECK(remesa_usse_component_control_encode(&r,&r,sizeof(r),&n)==EINVAL);
 CHECK(remesa_usse_component_control_encode(&r,out,16,(void *)out)==EINVAL);
 r.operand[0].value=-128;r.operand[1].value=127;r.operand[2].value=-1;
 CHECK(!remesa_usse_component_control_encode(&r,out,16,&n));
 CHECK(out[0]==0&&out[1]==255&&out[2]==127&&out[3]==128); /* Explicit byte order, no signed shifts. */
 printf("PASS B5 selected CPU control11 original writer oracles; format bits, signed bounds, transactional errors, endian/alias/unknown refusal; NO GPU (%u checks)\n",checks);
 return 0;
}
