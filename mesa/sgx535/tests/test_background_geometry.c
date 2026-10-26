/* SPDX-License-Identifier: MIT */
/* Original bounded x86 caller/constructor data arrive only as an oracle. */
#include "remesa_render_target.h"
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static uint32_t le(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int main(void)
{
 unsigned count;assert(scanf("%u",&count)==1&&count==7);
 for(unsigned c=0;c<count;c++) {
  struct remesa_background_geometry_request r;uint32_t depth,oracle[6];
  assert(scanf("%u%u%"SCNx32,&r.width,&r.height,&depth)==3);memcpy(&r.depth,&depth,4);
  for(unsigned i=0;i<6;i++)assert(scanf("%"SCNx32,&oracle[i])==1);
  uint8_t out[32];memset(out,0xa5,sizeof(out));assert(!remesa_background_geometry_encode(&r,out));
  for(unsigned i=0;i<6;i++)assert(le(out+4*i)==oracle[i]);
  for(unsigned i=24;i<32;i++)assert(out[i]==0xa5);
 }
 struct remesa_background_geometry_request r={1280,800,1};uint8_t out[24],saved[24];
 assert(!remesa_background_geometry_encode(&r,out));
 assert(le(out+8)==0x90004000&&le(out+16)==0x40007200);
 memset(out,0xa5,24);memcpy(saved,out,24);
 for(unsigned i=0;i<6;i++) {
  struct remesa_background_geometry_request bad=r;
  switch(i){case 0:bad.width=0;break;case 1:bad.height=0;break;case 2:bad.width=2049;break;
   case 3:bad.height=2049;break;case 4:bad.depth=NAN;break;case 5:bad.depth=-1;break;}
  assert(remesa_background_geometry_encode(&bad,out)==(i==2||i==3?ENOTSUP:EINVAL));
  assert(!memcmp(out,saved,24));
 }
 assert(remesa_background_geometry_encode(NULL,out)==EINVAL);
 assert(remesa_background_geometry_encode(&r,(void *)&r)==EINVAL);
 r.width=r.height=2048;assert(!remesa_background_geometry_encode(&r,out));
 assert(le(out+8)==0xc0004000&&le(out+16)==0x4000c000);
 puts("PASS CPU_ONLY:7original background geometry oracles, nonsquare1280x800 field order, depth, extent/alias/transactional controls; no GPU");
 return 0;
}
