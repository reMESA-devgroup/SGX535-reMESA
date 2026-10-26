/* SPDX-License-Identifier: MIT */
/* Whole original x86 constructor/relocation results are external oracles. */
#include "remesa_ta_scene.h"
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
 unsigned count;assert(scanf("%u",&count)==1&&count==8);
 for(unsigned c=0;c<count;c++) {
  struct remesa_background_object_request r={0};uint32_t depth,oracle[17];uint64_t bo,p,s;
  assert(scanf("%u%u%"SCNx32"%"SCNx64"%"SCNx64"%"SCNx64,&r.width,&r.height,&depth,&bo,&p,&s)==6);
  memcpy(&r.depth,&depth,4);r.address_window_base=bo&~UINT64_C(0xfffffff);
  r.primary=(struct remesa_ta_fragment_pds_reference){bo,0x20000,p,64,12,1,0};
  r.secondary=(struct remesa_ta_fragment_pds_reference){bo,0x20000,s,4,0,0,0};
  for(unsigned i=0;i<17;i++)assert(scanf("%"SCNx32,&oracle[i])==1);
  uint8_t out[76];memset(out,0xa5,sizeof(out));assert(!remesa_background_object_encode(&r,out));
  for(unsigned i=0;i<17;i++)assert(le(out+4*i)==oracle[i]);
  for(unsigned i=68;i<76;i++)assert(out[i]==0xa5);
 }
 struct remesa_background_object_request r={.address_window_base=0x40000000,
 .primary={0x40000000,0x20000,0x200,64,12,1,0},.secondary={0x40000000,0x20000,0x400,4,0,0,0},
 .width=1280,.height=800,.depth=1};
 for(unsigned mode=0;mode<12;mode++) {
  struct remesa_background_object_request bad=r;int expected=EINVAL;
  switch(mode){case 0:bad.primary.field10=2;expected=ENOTSUP;break;
  case 1:bad.primary.data_dwords=8;expected=ENOTSUP;break;
  case 2:bad.secondary.field10=1;expected=ENOTSUP;break;
  case 3:bad.primary.object_bytes=56;expected=ENOTSUP;break;
  case 4:bad.primary.byte_offset++;break;
  case 5:bad.secondary.byte_offset=bad.primary.byte_offset;break;
  case 6:bad.secondary.bo_gpu_va=0x50000000;break;
  case 7:bad.primary.byte_offset=0x20000;break;
  case 8:bad.address_window_base=0x40000001;break;
  case 9:bad.width=0;break;case 10:bad.depth=NAN;break;
  case 11:bad.secondary.field14=1;expected=ENOTSUP;break;}
  uint8_t out[68],saved[68];memset(out,0xa5,68);memcpy(saved,out,68);
  assert(remesa_background_object_encode(&bad,out)==expected&&!memcmp(saved,out,68));
 }
 assert(remesa_background_object_encode(&r,(void *)&r)==EINVAL);
 puts("PASS CPU_ONLY:8whole original background object+relocation oracles,17DWORDs each; symbolic PDS refs, nonsquare extents, bounds/profile/rollback rejection; no GPU");return 0;
}
