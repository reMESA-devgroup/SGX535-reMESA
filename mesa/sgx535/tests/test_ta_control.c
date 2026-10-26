/* SPDX-License-Identifier: MIT */
#include "remesa_render_target.h"
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
static uint32_t get(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int main(void)
{
 unsigned n;assert(scanf("%u",&n)==1&&n==3);
 for(unsigned k=0;k<n;k++) {
  uint64_t va;uint32_t words[14];assert(scanf("%"SCNx64,&va)==1);
  for(unsigned i=0;i<14;i++)assert(scanf("%"SCNx32,&words[i])==1);
  struct remesa_raster_object_reference r={va&~UINT64_C(4095),4096,va&4095,124};
  uint8_t b[64];memset(b,0xa5,sizeof(b));assert(!remesa_ta_control_encode(&r,b));
  for(unsigned i=0;i<14;i++)assert(get(b+4*i)==words[i]);
  for(unsigned i=56;i<64;i++)assert(b[i]==0xa5);
 }
 struct remesa_raster_object_reference r={0x20028000,4096,4,124};uint8_t b[56],saved[56];
 memset(b,0xcc,56);memcpy(saved,b,56);
 r.byte_offset=4096;assert(remesa_ta_control_encode(&r,b)==EINVAL&&!memcmp(b,saved,56));
 r.byte_offset=1;assert(remesa_ta_control_encode(&r,b)==EINVAL&&!memcmp(b,saved,56));
 r.byte_offset=4;r.bo_bytes=UINT64_MAX;assert(remesa_ta_control_encode(&r,b)==EINVAL&&!memcmp(b,saved,56));
 assert(remesa_ta_control_encode((void *)b,b)==EINVAL);
 puts("PASS TA7 pairs:3 whole original constructor/relocation/commit oracles, stream offset/bounds/alias guards; CPU ONLY");return 0;
}
