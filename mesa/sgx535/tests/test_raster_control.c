/* SPDX-License-Identifier: MIT */
/* Original whole-x86 constructor/relocation/commit oracles arrive on stdin. */
#include "remesa_render_target.h"
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
static unsigned checks;
#define CHECK(x) do{assert(x);++checks;}while(0)
static uint32_t word(const uint8_t *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int main(void)
{
 unsigned count;CHECK(scanf("%u",&count)==1&&count==8);
 struct remesa_raster_control_request r={0};
 for(unsigned k=0;k<count;k++) {
  unsigned w,h,cpp,pitch;uint32_t z,st;uint64_t zva,bg,ev;uint32_t field,reference[52];
  CHECK(scanf("%u%u%u%u%"SCNx32"%"SCNx32"%"SCNx64"%"SCNx64"%"SCNx64"%"SCNx32,&w,&h,&cpp,&pitch,&z,&st,&zva,&bg,&ev,&field)==10);
  for(unsigned i=0;i<52;i++)CHECK(scanf("%"SCNx32,&reference[i])==1);
  float clear;memcpy(&clear,&z,4);
  r=(struct remesa_raster_control_request){.color={w,h,pitch,0x40001000,(uint64_t)pitch*h*4},
   .has_depth=cpp!=0,.depth={.depth={w,h,pitch,cpp,REMESA_DEPTH_SURFACE_UNDEFINED,zva,(uint64_t)pitch*((h+31)&~31u)*cpp},
   .color_width=w,.color_height=h,.clear_stencil=st,.clear_depth=clear},
   .background={bg,4096,0,68},.event={ev,4096,0,128},.event_field0c=field};
  uint8_t b[216];memset(b,0xa5,sizeof(b));CHECK(!remesa_raster_control_encode(&r,b));
  for(unsigned i=0;i<52;i++)CHECK(word(b+4*i)==reference[i]);
  for(unsigned i=208;i<sizeof(b);i++)CHECK(b[i]==0xa5);
  if(r.has_depth) {
   uint8_t finalized[208],again[208];r.depth.caller_finalize_flag=true;
   CHECK(!remesa_raster_control_encode(&r,finalized));
   /* Original2673d ->2a39a prefix, independently replayed: the saved
    * register480 value alone receives OR4000d0 when depth is present. */
   for(unsigned i=0;i<52;i++)CHECK(word(finalized+4*i)==
      (reference[i]|(i==21?UINT32_C(0x004000d0):0u)));
   r.depth.caller_finalize_flag=false;
   CHECK(!remesa_raster_control_encode(&r,again));CHECK(!memcmp(again,b,208));
  }
 }
 uint8_t b[208],saved[208];
 for(unsigned mode=0;mode<10;mode++) {
  struct remesa_raster_control_request bad=r;int e=EINVAL;
  switch(mode) {
   case 0:bad.event_field0c=32;e=ENOTSUP;break;
   case 1:bad.has_depth=false;bad.depth.caller_finalize_flag=true;e=ENOTSUP;break;
   case 2:bad.background.object_bytes=67;break;
   case 3:bad.background.byte_offset=4096;break;
   case 4:bad.event.bo_gpu_va=0x30000000;break;
   case 5:bad.event.byte_offset=1;break;
   case 6:bad.color.allocation_bytes=1;e=ENOSPC;break;
   case 7:bad.depth.clear_depth=NAN;break;
   case 8:bad.depth.color_width++;break;
   case 9:bad.depth.clear_stencil=256;break;
  }
  memset(b,0xcc,sizeof(b));memcpy(saved,b,sizeof(b));
  CHECK(remesa_raster_control_encode(&bad,b)==e&&!memcmp(b,saved,sizeof(b)));
 }
 CHECK(remesa_raster_control_encode((void *)b,b)==EINVAL);
 CHECK(remesa_raster_control_encode(&r,(void *)&r)==EINVAL);
 CHECK(remesa_raster_control_encode((void *)(UINTPTR_MAX-1),b)==EINVAL);
 printf("PASS full raster26 pairs:8 original x86 constructor/relocation/commit oracles, opt-in depth-only finalizer, equivalent no-depth negative, bounds/metadata/alias/transaction guards; CPU ONLY (%u checks)\n",checks);
 return 0;
}
