/* SPDX-License-Identifier: MIT */
#include "remesa_render_target.h"
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void reject(const struct remesa_depth_prefix_request *r,int error)
{
 uint8_t b[64],old[64];memset(b,0xa5,sizeof(b));memcpy(old,b,sizeof(b));
 assert(remesa_depth_prefix_encode(r,b)==error&&!memcmp(b,old,sizeof(b)));
}
int main(void)
{
 uint8_t oracle[10][64];assert(fread(oracle,1,sizeof(oracle),stdin)==sizeof(oracle)&&fgetc(stdin)==EOF);
 for(unsigned i=0;i<10;i++) {
  unsigned status=i<6?i/2:i%2+1,flag=i<6?i%2:1;
  unsigned cpp=i<6?4:i<8?2:4,pitch=i<8?32:64;
  float z=i<6?(flag?1:.25f):i<8?.75f:0;
  unsigned stencil=i<6?(flag?0x5a:0):i<8?0:255;
  const uint64_t vas[]={0x40010000,0x50020000,0x70030000};
  const struct remesa_depth_prefix_request r={
   .depth={32,32,pitch,cpp,(enum remesa_depth_surface_status)status,vas[i%3],pitch*32*cpp},
   .color_width=32,.color_height=32,.clear_stencil=stencil,.clear_depth=z,
   .caller_finalize_flag=flag!=0};
  uint8_t b[68];memset(b,0xa5,sizeof(b));assert(!remesa_depth_prefix_encode(&r,b));
  assert(!memcmp(b,oracle[i],64));for(unsigned j=64;j<68;j++)assert(b[j]==0xa5);
 }
 struct remesa_depth_prefix_request r={.depth={32,32,32,4,REMESA_DEPTH_SURFACE_UNDEFINED,0x40010000,4096},.color_width=32,.color_height=32,.clear_depth=1};
 r.color_width=31;reject(&r,EINVAL);r.color_width=32;r.color_height=0;reject(&r,EINVAL);r.color_height=32;
 r.clear_depth=NAN;reject(&r,EINVAL);r.clear_depth=INFINITY;reject(&r,EINVAL);r.clear_depth=-.1f;reject(&r,EINVAL);r.clear_depth=1.1f;reject(&r,EINVAL);r.clear_depth=1;
 r.clear_stencil=256;reject(&r,ERANGE);r.clear_stencil=0;
 r.depth.allocation_bytes=4095;reject(&r,ENOSPC);r.depth.allocation_bytes=4096;
 r.depth.gpu_va=0x40010004;reject(&r,EINVAL);r.depth.gpu_va=0xfffff000;r.depth.allocation_bytes=4097;reject(&r,EOVERFLOW);r.depth.gpu_va=0x40010000;r.depth.allocation_bytes=4096;
 r.depth.cpp=3;reject(&r,ENOTSUP);r.depth.cpp=4;r.depth.status=(enum remesa_depth_surface_status)3;reject(&r,ENOTSUP);r.depth.status=REMESA_DEPTH_SURFACE_UNDEFINED;
 reject(NULL,EINVAL);assert(remesa_depth_prefix_encode(&r,NULL)==EINVAL);
 union {struct remesa_depth_prefix_request request;uint8_t storage[sizeof(r)+64];} alias;
 alias.request=r;assert(remesa_depth_prefix_encode(&alias.request,alias.storage)==EINVAL);
 uint8_t out[64];assert(remesa_depth_prefix_encode((void *)(alias.storage+1),out)==EINVAL);
 assert(remesa_depth_prefix_encode(&r,(void *)UINTPTR_MAX)==EINVAL);
 puts("PASS:10 whole original Zattachment/relocation/finalization oracles;8 register pairs; range/format/clear/dimension/alias/transaction tests; CPU ONLY");return 0;
}
