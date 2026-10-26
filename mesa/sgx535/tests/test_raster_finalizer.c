/* SPDX-License-Identifier: MIT */
/* CPU-only complete1280 prefix. stdin: original0/1 finalizer depth-pair
 * oracles (64bytes each), followed by the actually submitted208byte prefix. */
#include "remesa_render_target.h"
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
static unsigned checks;
#define CHECK(x) do{checks++;assert(x);}while(0)
static uint32_t word(const uint8_t*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void depth_pairs(const uint8_t in[208],uint8_t out[64]){
 memcpy(out,in+80,48);memcpy(out+48,in+144,16);
}
int main(void){
 uint8_t reference[128],submitted[208];CHECK(fread(reference,1,128,stdin)==128);CHECK(fread(submitted,1,208,stdin)==208);CHECK(fgetc(stdin)==EOF);
 struct remesa_raster_control_request r={.color={1280,800,1280,UINT64_C(0x40001000),4096000},.has_depth=true,
 .depth={.depth={1280,800,1280,4,REMESA_DEPTH_SURFACE_UNDEFINED,UINT64_C(0x403e9000),4096000},.color_width=1280,.color_height=800,.clear_depth=1},
 .background={UINT64_C(0x30000000),4096,0,68},.event={UINT64_C(0x20000000),131072,0x44c0,148},.event_field0c=16};
 uint8_t a[208],b[208],pairs[64];CHECK(!remesa_raster_control_encode(&r,a));CHECK(!memcmp(a,submitted,208));depth_pairs(a,pairs);CHECK(!memcmp(pairs,reference,64));
 r.depth.caller_finalize_flag=true;struct remesa_raster_control_request unchanged=r;CHECK(!remesa_raster_control_encode(&r,b));CHECK(!memcmp(&r,&unchanged,sizeof(r)));CHECK(r.depth.depth.status==REMESA_DEPTH_SURFACE_UNDEFINED);
 depth_pairs(b,pairs);CHECK(!memcmp(pairs,reference+64,64));CHECK(word(a+84)==0x54f00);CHECK(word(b+84)==0x454fd0);
 for(unsigned i=0;i<208;i++)if(i<84||i>=88)CHECK(a[i]==b[i]);
 r.depth.caller_finalize_flag=false;CHECK(!remesa_raster_control_encode(&r,b));CHECK(!memcmp(a,b,208));
 for(unsigned bad=0;bad<5;bad++){
  struct remesa_raster_control_request q=r;int expected=ENOTSUP;
  if(bad==0){q.has_depth=false;q.depth.caller_finalize_flag=true;}
  if(bad==1)q.event_field0c=32;
  if(bad==2){q.depth.depth.status=(enum remesa_depth_surface_status)42;}
  if(bad==3){q.depth.depth.allocation_bytes=4096;expected=ENOSPC;}
  if(bad==4){q.depth.clear_depth=NAN;expected=EINVAL;}
  memset(b,0xa5,208);uint8_t before[208];memcpy(before,b,208);CHECK(remesa_raster_control_encode(&q,b)==expected);CHECK(!memcmp(b,before,208));
 }
 printf("PASS full1280 finalizer prefix: original x86 0/1 oracles; actual submitted baseline unchanged; one DWORD delta; no state promotion; transaction guards; CPU_ONLY (%u checks)\n",checks);
}
