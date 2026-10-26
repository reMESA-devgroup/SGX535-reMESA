/* SPDX-License-Identifier: MIT
 * Independent original producer DATA oracles arrive through stdin, not a
 * program-byte template. CPU construction only; no device access. */
#include "remesa_pds_structure.h"
#include <assert.h>
#include <stdbool.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do {assert(x);checks++;}while(0)
static uint32_t get32(const uint8_t *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int main(void)
{
 unsigned count;CHECK(scanf("%u",&count)==1&&count==7);
 for(unsigned k=0;k<count;k++) {
  struct remesa_pds_background_request r={.use_lower19=0x1203};uint32_t oracle[12];
  CHECK(scanf("%u%u%u%"SCNx64,&r.surface_tag,&r.width,&r.height,&r.color_address)==4);
  for(unsigned c=0;c<12;c++)CHECK(scanf("%"SCNx32,&oracle[c])==1);
  uint8_t b[72],m[72];memset(b,0xa5,sizeof(b));memset(m,0x5a,sizeof(m));size_t n=99;
  CHECK(!remesa_pds_structure_encode_background(&r,b,m,64,&n)&&n==64);
  CHECK(get32(b)==(0x100000u|r.use_lower19));
  for(unsigned c=1;c<12;c++) {
   bool known=c<4||c==8||c==9||c==10;
   CHECK(get32(m+4*c)==(known?UINT32_MAX:0));
   CHECK(get32(b+4*c)==(known?oracle[c]:0));
  }
  CHECK(get32(m)==UINT32_MAX);
  for(unsigned i=48;i<64;i++)CHECK(m[i]==0xff);
  for(unsigned i=64;i<72;i++)CHECK(b[i]==0xa5&&m[i]==0x5a);
 }
 struct remesa_pds_background_request r={.color_address=0x40001000,.use_lower19=0x1203,.surface_tag=4,.width=32,.height=32};
 uint8_t b[64],m[64],before[64],prior[64],oracle[64];size_t n=99;
 for(unsigned i=0;i<64;i++){unsigned v;CHECK(scanf("%x",&v)==1&&v<256);oracle[i]=(uint8_t)v;}
 CHECK(!remesa_pds_structure_encode_background(&r,b,m,sizeof(b),&n)&&n==64);
 CHECK(!memcmp(b,oracle,64)); /* Entire selected retained64-byte oracle. */
 for(unsigned mode=0;mode<9;mode++) {
  struct remesa_pds_background_request bad=r;size_t cap=64;int expected=ERANGE;
  switch(mode){case 0:bad.surface_tag=3;expected=ENOTSUP;break;
   case 1:bad.width=0;break;case 2:bad.width=4097;break;case 3:bad.height=0;break;
   case 4:bad.height=4097;break;case 5:bad.use_lower19=0x80000;break;
   case 6:bad.color_address=UINT64_C(0x100000000);break;
   case 7:cap=63;expected=ENOSPC;break;case 8:bad.surface_tag=0;expected=ENOTSUP;break;}
  memset(b,0xa5,sizeof(b));memset(m,0x5a,sizeof(m));memcpy(before,b,64);memcpy(prior,m,64);n=99;
  CHECK(remesa_pds_structure_encode_background(&bad,b,m,cap,&n)==expected);
  CHECK(n==99&&!memcmp(b,before,64)&&!memcmp(m,prior,64));
 }
 CHECK(remesa_pds_structure_encode_background(&r,b,b,64,&n)==EINVAL);
 CHECK(remesa_pds_structure_encode_background(&r,b,m,64,(void *)b)==EINVAL);
 CHECK(remesa_pds_structure_encode_background((void *)b,b,m,64,&n)==EINVAL);
 CHECK(remesa_pds_structure_encode_background(&r,(void *)&r,m,64,&n)==EINVAL);
 CHECK(remesa_pds_structure_encode_background(&r,b,m,64,(void *)&r)==EINVAL);
 CHECK(remesa_pds_structure_encode_background((void *)(UINTPTR_MAX-1),b,m,64,&n)==EINVAL);
 CHECK(remesa_pds_structure_encode_background(&r,(void *)(UINTPTR_MAX-1),m,64,&n)==EINVAL);
 CHECK(remesa_pds_structure_encode_background(&r,b,m,64,(void *)(UINTPTR_MAX-1))==EINVAL);
 CHECK(remesa_pds_structure_encode_background(NULL,b,m,64,&n)==EINVAL);
 r.width=r.height=4096;r.color_address=UINT32_MAX;r.use_lower19=0x7ffff;
 CHECK(!remesa_pds_structure_encode_background(&r,b,m,64,&n));
 CHECK(get32(b+12)==0x6cffffff&&get32(b+40)==UINT32_MAX&&get32(b)==0x17ffff);
 printf("PASS %u dedicated CPU background DATA+CODE checks; no GPU admission\n",checks);return 0;
}
