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
 unsigned pos,col,count=0;char hex[81];
 while(scanf("%u %u %80s",&pos,&col,hex)==3) {
  struct remesa_usse_cpu_position_color r={pos,col,0};
  unsigned char out[48],oracle[40];size_t n=999;memset(out,0xa5,sizeof(out));
  CHECK(strlen(hex)==80);
  for(unsigned i=0;i<40;i++){unsigned v;CHECK(sscanf(hex+2*i,"%2x",&v)==1);oracle[i]=(unsigned char)v;}
  CHECK(!remesa_usse_cpu_position_color_vertex_encode(&r,out,48,&n));
  CHECK(n==40&&!memcmp(out,oracle,40));
  for(unsigned i=40;i<48;i++)CHECK(out[i]==0xa5);
  /* Independently compare both clamp bodies with original compiler slots. */
  struct remesa_usse_cpu_arithmetic q={.kind=13,.flags=0x41,.destination={1,4},
   .source={{0,0},{2,col},{6,48}}};
  CHECK(!remesa_usse_cpu_arithmetic_encode(&q,out,48,&n));CHECK(n==8&&!memcmp(out,oracle+16,8));
  q.kind=12;q.source[1]=(struct remesa_usse_arithmetic_source){1,4};q.source[2].base=52;
  CHECK(!remesa_usse_cpu_arithmetic_encode(&q,out,48,&n));CHECK(n==8&&!memcmp(out,oracle+24,8));
  count++;
 }
 CHECK(count==3);
 for(unsigned mode=0;mode<7;mode++) {
  struct remesa_usse_cpu_position_color r={0,4,0};unsigned char out[48],saved[48];
  memset(out,0xa5,48);memcpy(saved,out,48);size_t n=999;
  if(mode==0)r.position_base=8;if(mode==1)r.color_base=128;
  if(mode==2)r.reserved=1;if(mode==3)r.position_base=r.color_base=4;
  int e=mode==2?EINVAL:mode==4?ENOSPC:mode>=5?EINVAL:ENOTSUP;
  void *o=mode==5?(void *)&r:out;size_t *written=mode==6?(void *)out:&n;
  CHECK(remesa_usse_cpu_position_color_vertex_encode(&r,o,mode==4?39:48,written)==e);
  CHECK(n==999&&!memcmp(out,saved,48));
 }
 for(unsigned mode=0;mode<9;mode++) {
  struct remesa_usse_cpu_arithmetic q={.kind=12,.flags=0x41,.destination={1,4},.source={{0,0},{1,4},{6,52}}};
  if(mode==0)q.source[1].cpu_class=2;if(mode==1)q.source[2].base=48;
  if(mode==2)q.destination.base=0;if(mode==3)q.flags=0x31;
  if(mode==4)q.modifier[1]=1;if(mode==5)q.source[0].base=1;
  if(mode==6)q.source[2].cpu_class=2;if(mode==7)q.reserved=1;
  unsigned char out[8],saved[8];memset(out,0xa5,8);memcpy(saved,out,8);size_t n=999;
  CHECK(remesa_usse_cpu_arithmetic_encode(&q,out,mode==8?7:8,&n)==(mode==5||mode==7?EINVAL:mode==8?ENOSPC:ENOTSUP));
  CHECK(n==999&&!memcmp(out,saved,8));
 }
 printf("PASS3 original complete40-byte POSITION/COLOR0 USE oracles; typed clamp12/13, component/MOVE/tail composition, source remapping and transactional negatives; CPU ONLY (%u checks)\n",checks);
 return 0;
}
