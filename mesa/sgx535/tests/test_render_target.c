/* SPDX-License-Identifier: MIT */
/* CPU state encoding only. Expected words are independent original-producer
 * captures, not a decoder sharing the encoder's field construction. */
#include "remesa_render_target.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static uint32_t get32(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static const struct {uint32_t w,h,pitch,a,z,bg,raster,term;} cases[]={
 {32,32,32,0x000f8000,0x0001f01f,0x6c01f01f,0x00020002,0x00020002},
 {64,32,64,0x001f8000,0x0001f03f,0x6c03f01f,0x00040002,0x00040002},
 {1280,800,1280,0x027f8000,0x0031f4ff,0x6c4ff31f,0x00500032,0x00500032},
 {33,32,64,0x001f8000,0x0001f03f,0x6c02001f,0x00020002,0x00030002}
};
int main(int argc,char **argv)
{
 /* No device, window or original library is executed here. */
 int dump=argc==2&&!strcmp(argv[1],"--dump");
 for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
  struct remesa_render_target_request r={cases[i].w,cases[i].h,cases[i].pitch,
   UINT64_C(0x40001000),(uint64_t)cases[i].pitch*4*cases[i].h};
  struct {uint64_t before;struct remesa_render_target_state state;uint64_t after;} guarded;
  memset(&guarded,0xa5,sizeof(guarded));assert(!remesa_render_target_encode_linear_bgra8(&r,&guarded.state));
  assert(guarded.before==UINT64_C(0xa5a5a5a5a5a5a5a5)&&guarded.after==guarded.before);
  const struct remesa_render_target_state *s=&guarded.state;
  assert(get32(s->target_data)==cases[i].a&&get32(s->target_data+4)==0x40001000&&
   !get32(s->target_data+8)&&get32(s->target_data+12)==cases[i].z);
  assert(s->background_data_extent==cases[i].bg&&s->raster_410==cases[i].raster&&s->termination_extent==cases[i].term);
  assert(s->stride_bytes==cases[i].pitch*4&&s->image_bytes==r.allocation_bytes);
  if(dump)printf("T %u %u %u %08x %08x %08x %08x\n",r.width,r.height,r.pitch_pixels,
   get32(s->target_data),get32(s->target_data+4),get32(s->target_data+8),get32(s->target_data+12));
  struct remesa_render_target_state saved=*s;
  r.allocation_bytes--;assert(remesa_render_target_encode_linear_bgra8(&r,&guarded.state)==ENOSPC);
  assert(!memcmp(&saved,&guarded.state,sizeof(saved)));
 }
 struct remesa_render_target_request r={32,32,32,UINT64_C(0x40001000),4096};
 struct remesa_render_target_state s,saved;memset(&s,0xcc,sizeof(s));saved=s;
 r.pitch_pixels=48;assert(remesa_render_target_encode_linear_bgra8(&r,&s)==ENOTSUP);
 r.pitch_pixels=32;r.width=33;assert(remesa_render_target_encode_linear_bgra8(&r,&s)==EINVAL);
 r.width=32;r.gpu_va++;assert(remesa_render_target_encode_linear_bgra8(&r,&s)==EINVAL);
 r.gpu_va=UINT64_C(0x100000000);assert(remesa_render_target_encode_linear_bgra8(&r,&s)==EINVAL);
 r.gpu_va=UINT64_C(0xfffff000);r.allocation_bytes=8192;
 assert(remesa_render_target_encode_linear_bgra8(&r,&s)==EOVERFLOW);
 assert(!memcmp(&s,&saved,sizeof(s)));
 r.allocation_bytes=4096;assert(!remesa_render_target_encode_linear_bgra8(&r,&s));
 assert(get32(s.target_data+4)==0xfffff000);
 const unsigned sizes[][2]={{32,32},{64,32},{1280,800},{33,32},{64,64}};
 for(unsigned i=0;i<5;i++)for(unsigned option=0;option<2;option++) {
  struct remesa_scene_layout scene;
  assert(!remesa_scene_layout_encode(sizes[i][0],sizes[i][1],option,&scene));
  if(i==2)assert(scene.required_bytes==119584&&scene.allocation_pages==30&&scene.clear_page_count==16);
  else assert(scene.required_bytes==5152&&scene.allocation_pages==2&&scene.clear_page_count==1);
  assert(!scene.clear_first_page&&sizeof(scene.cookie)==15*4);
  if(dump){printf("S %u %u %u",sizes[i][0],sizes[i][1],option);
   for(unsigned j=0;j<15;j++)printf(" %08x",scene.cookie[j]);
   printf(" %u %u %u\n",scene.required_bytes,scene.clear_first_page,scene.clear_page_count);}
 }
 struct remesa_scene_layout scene,old;memset(&scene,0xcc,sizeof(scene));old=scene;
 assert(remesa_scene_layout_encode(0,32,0,&scene)==EINVAL);
 assert(remesa_scene_layout_encode(32,32,2,&scene)==EINVAL);
 assert(remesa_scene_layout_encode(2048,32,0,&scene)==ENOTSUP);
 assert(!memcmp(&scene,&old,sizeof(scene)));
 for(unsigned option=0;option<2;option++) {
  assert(!remesa_scene_layout_encode(2047,2047,option,&scene));
  assert(scene.required_bytes==262432&&scene.allocation_pages==65&&scene.clear_page_count==16);
  if(option)assert(scene.cookie[2]==0x3fcff0ff&&scene.cookie[3]==0x3fcff0ff&&scene.cookie[4]==65025);
  if(dump){printf("S 2047 2047 %u",option);for(unsigned j=0;j<15;j++)printf(" %08x",scene.cookie[j]);
   printf(" %u %u %u\n",scene.required_bytes,scene.clear_first_page,scene.clear_page_count);}
 }
 for(unsigned compare=0;compare<8;compare++)for(unsigned write=0;write<2;write++) {
  uint32_t bits=0;
  assert(!remesa_depth_control_encode(true,true,write,(enum remesa_depth_compare)compare,&bits));
  assert(!(bits&~REMESA_DEPTH_CONTROL_MASK));
  if(dump)printf("D 1 1 %u %u %08x\n",write,compare,bits);
 }
 uint32_t bits=123;
 assert(!remesa_depth_control_encode(false,true,true,REMESA_DEPTH_LESS,&bits)&&bits==0x01d00000);
 if(dump)printf("D 0 1 1 1 %08x\n",bits);
 assert(!remesa_depth_control_encode(true,false,true,REMESA_DEPTH_LESS,&bits)&&bits==0x01d00000);
 if(dump)printf("D 1 0 1 1 %08x\n",bits);
 bits=123;assert(remesa_depth_control_encode(true,true,true,(enum remesa_depth_compare)8,&bits)==EINVAL&&bits==123);
 struct remesa_depth_surface_state zs,zold;
 assert(!remesa_depth_surface_encode(NULL,&zs)&&zs.control_480==0xf0000&&!zs.address_count);
 for(unsigned cpp=2;cpp<=4;cpp+=2)for(unsigned p=0;p<3;p++)for(unsigned state=0;state<3;state++) {
  unsigned pitch=(unsigned[]){32,64,1280}[p];
  struct remesa_depth_surface_request zr={pitch,32,pitch,cpp,(enum remesa_depth_surface_status)state,
   UINT64_C(0x4abc0010),pitch*32*cpp};
  assert(!remesa_depth_surface_encode(&zr,&zs)&&zs.address_count==5);
  assert(zs.minimum_backing_bytes==zr.allocation_bytes);
  if(dump)printf("Z %u %u %u %08x\n",cpp,pitch,state,zs.control_480);
  for(unsigned j=0;j<5;j++) {
   assert(zs.addresses[j].reg==(j?0x480+j*4:0xcb0));
   assert(zs.addresses[j].address_mask==(j?0x0ffffff0:0xf0000000));
   assert(zs.addresses[j].word==(j?0x0abc0010:0x40000000));
  }
 }
 struct remesa_depth_surface_request zr={33,33,64,2,REMESA_DEPTH_SURFACE_DEFINED,
  UINT64_C(0x40004000),8192};
 assert(!remesa_depth_surface_encode(&zr,&zs)&&zs.minimum_backing_bytes==8192);
 zold=zs;zr.allocation_bytes--;assert(remesa_depth_surface_encode(&zr,&zs)==ENOSPC);
 assert(!memcmp(&zs,&zold,sizeof(zs)));
 zr.allocation_bytes++;zr.status=(enum remesa_depth_surface_status)3;
 assert(remesa_depth_surface_encode(&zr,&zs)==ENOTSUP);
 zr.status=REMESA_DEPTH_SURFACE_CLEAR;zr.cpp=3;assert(remesa_depth_surface_encode(&zr,&zs)==ENOTSUP);
 zr.cpp=2;zr.pitch_pixels=48;assert(remesa_depth_surface_encode(&zr,&zs)==ENOTSUP);
 zr.pitch_pixels=64;zr.gpu_va++;assert(remesa_depth_surface_encode(&zr,&zs)==EINVAL);
 zr.gpu_va=UINT64_C(0xfffff000);assert(remesa_depth_surface_encode(&zr,&zs)==EOVERFLOW);
 assert(!memcmp(&zs,&zold,sizeof(zs)));
 if(!dump)puts("PASS: independent target/scene encoders, four descriptor oracles, twelve scene variants including clamp boundary, padded logical width, floor/ceil distinction, bounds, VA wrap, transactional errors; CPU ONLY");
 return 0;
}
