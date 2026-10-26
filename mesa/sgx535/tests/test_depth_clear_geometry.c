/* SPDX-License-Identifier: MIT */
#include "remesa_render_target.h"
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static uint32_t word(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static float value(const uint8_t *p)
{uint32_t w=word(p);float f;memcpy(&f,&w,4);return f;}
int main(int argc,char **argv)
{
 if(argc==2&&!strcmp(argv[1],"--dump-controls")) {
  for(unsigned mask=0;mask<16;mask++) {
   uint32_t value;assert(!remesa_isp_opaque_color_mask_encode(mask,&value));
   printf("%u %08x\n",mask,value);
  }
  return 0;
 }
 bool dump=argc==2&&!strcmp(argv[1],"--dump");
 const struct remesa_depth_clear_request cases[]={
  {32,32,0,0,32,32,1,{0,0,0,0}},
  {32,32,3,5,10,14,.5f,{1,.25f,.5f,1}},
  {32,32,0,0,32,32,0,{1,0,1,1}},
  {64,32,0,0,64,32,.25f,{0,1,0,1}},
  {1280,800,0,0,1280,800,1,{0,0,1,1}},
  {1280,800,11,23,1279,799,.75f,{1,.5f,.25f,0}}
 };
 for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
  struct remesa_depth_clear_geometry result;
  memset(&result,0xa5,sizeof(result));
  assert(!remesa_depth_clear_geometry_encode(&cases[i],&result));
  assert(result.depth_control==UINT32_C(0x01c00000));
  const uint8_t original_indices[12]={0,0,1,0,2,0,0,0,2,0,3,0};
  assert(!memcmp(result.indices,original_indices,sizeof(original_indices)));
  const uint32_t x[]={cases[i].x0,cases[i].x1,cases[i].x1,cases[i].x0};
  const uint32_t y[]={cases[i].y0,cases[i].y0,cases[i].y1,cases[i].y1};
  for(unsigned v=0;v<4;v++) {
   const uint8_t *p=result.vertices+v*32;
   assert(value(p)==x[v]&&value(p+4)==y[v]);
   assert(value(p+8)==cases[i].depth&&word(p+12)==UINT32_C(0x3f800000));
   for(unsigned c=0;c<4;c++)assert(value(p+16+c*4)==cases[i].color[c]);
  }
  if(dump){printf("%u ",i);for(unsigned n=0;n<128;n++)printf("%02x",result.vertices[n]);puts("");}
 }
 struct remesa_depth_clear_request bad=cases[0];
 struct remesa_depth_clear_geometry out,old;memset(&out,0xa5,sizeof(out));old=out;
#define REJECT(ERROR) do {assert(remesa_depth_clear_geometry_encode(&bad,&out)==(ERROR));assert(!memcmp(&out,&old,sizeof(out)));bad=cases[0];} while(0)
 bad.x0=32;REJECT(EINVAL);bad.x1=33;REJECT(EINVAL);
 bad.y0=33;REJECT(EINVAL);bad.width=0;REJECT(EINVAL);
 bad.width=2049;REJECT(ENOTSUP);bad.height=2049;REJECT(ENOTSUP);
 bad.depth=NAN;REJECT(EINVAL);bad.depth=INFINITY;REJECT(EINVAL);
 bad.depth=-.25f;REJECT(EINVAL);bad.depth=1.25f;REJECT(EINVAL);
 bad.color[2]=NAN;REJECT(EINVAL);
 assert(remesa_depth_clear_geometry_encode(NULL,&out)==EINVAL);
 assert(remesa_depth_clear_geometry_encode(&cases[0],NULL)==EINVAL);
 struct remesa_depth_surface_request surface={32,32,32,4,REMESA_DEPTH_SURFACE_DEFINED,UINT64_C(0x41000000),4096};
 struct remesa_depth_surface_state prefix,final,saved;
 assert(!remesa_depth_surface_encode(&surface,&prefix));
 for(unsigned f=0;f<2;f++)for(unsigned present=0;present<2;present++) {
  assert(!remesa_depth_surface_finalize(&prefix,f,present,&final));
  assert(final.control_480==(prefix.control_480|((f&&present)?UINT32_C(0x004000d0):0)));
  saved=prefix;saved.control_480=final.control_480;
  assert(!memcmp(&saved,&final,sizeof(saved)));
  assert(!remesa_depth_surface_finalize(&final,f,present,&final));
  assert(!memcmp(&saved,&final,sizeof(saved)));
 }
 saved=prefix;prefix.address_count=1;final=saved;
 assert(remesa_depth_surface_finalize(&prefix,true,true,&final)==EINVAL);
 assert(!memcmp(&final,&saved,sizeof(final)));
 for(unsigned mutation=0;mutation<8;mutation++) {
  prefix=saved;
  switch(mutation) {
   case 0:prefix.addresses[2].word^=16;break;
   case 1:prefix.addresses[0].word=0;break;
   case 2:prefix.addresses[1].reg=0;break;
   case 3:prefix.addresses[3].address_mask=0;break;
   case 4:prefix.control_480^=1;break;
   case 5:prefix.control_480^=256;break;
   case 6:prefix.minimum_backing_bytes=0;break;
   case 7:prefix.minimum_backing_bytes=UINT32_MAX;break;
  }
  assert(remesa_depth_surface_finalize(&prefix,true,true,&final)==EINVAL);
  assert(!memcmp(&final,&saved,sizeof(final)));
 }
 assert(!remesa_depth_surface_encode(NULL,&prefix));
 assert(remesa_depth_surface_finalize(&prefix,true,true,&final)==ENODATA);
 assert(!memcmp(&final,&saved,sizeof(final)));
 assert(!remesa_depth_surface_finalize(&prefix,true,false,&final));
 assert(final.control_480==prefix.control_480);
 const unsigned pitches[]={32,64,1280};
 for(unsigned cpp=2;cpp<=4;cpp+=2)for(unsigned p=0;p<3;p++)for(unsigned status=0;status<3;status++) {
  surface.width=surface.pitch_pixels=pitches[p];surface.cpp=cpp;
  surface.status=status;surface.allocation_bytes=(uint64_t)pitches[p]*32*cpp;
  assert(!remesa_depth_surface_encode(&surface,&prefix));
  assert(!remesa_depth_surface_finalize(&prefix,true,true,&final));
  assert(!remesa_depth_surface_finalize(&final,true,true,&final));
 }
 uint32_t color=UINT32_C(0xdeadbeef);
 for(unsigned mask=0;mask<16;mask++) {
  assert(!remesa_isp_opaque_color_mask_encode(mask,&color));
  assert(color==(mask==15?0:mask?UINT32_C(0x02000000):UINT32_C(0x02200000)));
 }
 color=UINT32_C(0xdeadbeef);
 assert(remesa_isp_opaque_color_mask_encode(16,&color)==EINVAL&&color==UINT32_C(0xdeadbeef));
 assert(remesa_isp_opaque_color_mask_encode(0,NULL)==EINVAL);
 if(!dump)puts("PASS: six depth-clear vertex payloads; bounded normalized Z, finite colors, transactional rejection and conditional finalization; CPU ONLY");
 return 0;
}
