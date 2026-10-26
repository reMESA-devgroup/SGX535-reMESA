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
 struct remesa_usse_cpu_normalize_scale r={0};char hex[97];unsigned count=0;
 while(scanf("%u %u %96s",&r.normal_base,&r.value_base,hex)==3) {
  CHECK(strlen(hex)==96);unsigned char expected[48],out[56];memset(out,0x86,56);size_t n=991;
  for(unsigned i=0;i<48;i++){unsigned v;CHECK(sscanf(hex+2*i,"%2x",&v)==1);expected[i]=v;}
  CHECK(!remesa_usse_cpu_normalize_scale_vertex_encode(&r,out,56,&n));CHECK(n==48&&!memcmp(out,expected,48));
  for(unsigned i=48;i<56;i++)CHECK(out[i]==0x86);
  unsigned char before[56];memcpy(before,out,56);n=991;
  CHECK(remesa_usse_cpu_normalize_scale_vertex_encode(&r,out,47,&n)==ENOSPC&&n==991&&!memcmp(out,before,56));
  count++;
 }
 CHECK(count==3);unsigned char out[48],before[48];memset(out,0x85,48);memcpy(before,out,48);size_t n=991;
 r=(struct remesa_usse_cpu_normalize_scale){0,1,0};CHECK(remesa_usse_cpu_normalize_scale_vertex_encode(&r,out,48,&n)==ENOTSUP&&n==991&&!memcmp(out,before,48));
 r=(struct remesa_usse_cpu_normalize_scale){0,4,0};CHECK(remesa_usse_cpu_normalize_scale_vertex_encode(&r,&r,sizeof(r),&n)==EINVAL);
 CHECK(remesa_usse_cpu_normalize_scale_vertex_encode(&r,out,SIZE_MAX,&n)==EINVAL);
 CHECK(remesa_usse_cpu_normalize_scale_vertex_encode(&r,out,48,(void *)out)==EINVAL);
 printf("PASS three complete48-byte original linked USE allocations; independent body+typed tail, no auxiliary records, rollback/bounds; CPU ONLY (%u checks)\n",checks);return 0;
}
