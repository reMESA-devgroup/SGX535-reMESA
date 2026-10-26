/* SPDX-License-Identifier: MIT */
#include "remesa_usse_arithmetic.h"
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;assert(x);}while(0)
static void reject(struct remesa_usse_arithmetic r,size_t capacity,int error)
{
 unsigned char out[16],before[16];memset(out,0xa7,sizeof(out));memcpy(before,out,sizeof(out));size_t n=991;
 CHECK(remesa_usse_arithmetic_encode(&r,out,capacity,&n)==error);
 CHECK(n==991&&!memcmp(out,before,sizeof(out)));
}
int main(void)
{
 for(unsigned op=1;op<=2;op++)for(unsigned i=0;i<2;i++) {
  struct remesa_usse_arithmetic r={op,i?4:0,i?0:4,0};unsigned char out[16];memset(out,0xa7,sizeof(out));size_t n=991;
  CHECK(!remesa_usse_arithmetic_encode(&r,out,sizeof(out),&n));CHECK(n==8);
  for(unsigned c=8;c<16;c++)CHECK(out[c]==0xa7);
  printf("oracle op=%u left=%u right=%u bytes=",op,r.left_base,r.right_base);
  for(unsigned c=0;c<8;c++)printf("%02x",out[c]);
  puts("");
 }
 struct remesa_usse_arithmetic r={1,0,4,0};reject(r,7,ENOSPC);
 r.op=0;reject(r,16,ENOTSUP);r.op=3;reject(r,16,ENOTSUP);r.op=1;
 r.left_base=128;reject(r,16,ENOTSUP);r.left_base=1;reject(r,16,ENOTSUP);r.left_base=0;
 r.right_base=UINT32_MAX;reject(r,16,ENOTSUP);r.right_base=126;reject(r,16,ENOTSUP);r.right_base=4;
 r.reserved=1;reject(r,16,EINVAL);r.reserved=0;
 unsigned char out[16];size_t n=991;
 CHECK(remesa_usse_arithmetic_encode(NULL,out,16,&n)==EINVAL);
 CHECK(remesa_usse_arithmetic_encode(&r,NULL,16,&n)==EINVAL);
 CHECK(remesa_usse_arithmetic_encode(&r,out,16,NULL)==EINVAL);
 CHECK(remesa_usse_arithmetic_encode(&r,&r,sizeof(r),&n)==EINVAL);
 CHECK(remesa_usse_arithmetic_encode(&r,out,16,(size_t *)out)==EINVAL);
 CHECK(remesa_usse_arithmetic_encode(&r,out,SIZE_MAX,&n)==EINVAL);
 CHECK(remesa_usse_arithmetic_encode((const void *)(UINTPTR_MAX-1),out,16,&n)==EINVAL);
 _Alignas(struct remesa_usse_arithmetic) unsigned char request_space[sizeof(r)+1];
 memcpy(request_space+1,&r,sizeof(r));memset(out,0xa7,sizeof(out));n=991;
 CHECK(remesa_usse_arithmetic_encode((const void *)(request_space+1),out,16,&n)==EINVAL);
 CHECK(n==991&&out[0]==0xa7);
 _Alignas(size_t) unsigned char count_space[sizeof(size_t)+1];
 memset(count_space,0x7b,sizeof(count_space));
 CHECK(remesa_usse_arithmetic_encode(&r,out,16,(void *)(count_space+1))==EINVAL);
 CHECK(count_space[0]==0x7b&&count_space[sizeof(size_t)]==0x7b&&out[0]==0xa7);
 r.left_base=124;r.right_base=124;CHECK(!remesa_usse_arithmetic_encode(&r,out,16,&n));
 printf("USSE_ARITHMETIC_CPU_PASS checks=%u GPU=NO\n",checks);return 0;
}
