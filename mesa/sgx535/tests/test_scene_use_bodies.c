/* SPDX-License-Identifier: MIT. Original constructor comparison only. */
#include "remesa/scene_use_bodies.h"
#include "remesa_usse.h"
#include "remesa_usse_arithmetic.h"
#include "remesa_usse_fragment_color.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void corrupt(unsigned char *bytes,size_t n,unsigned vertex,unsigned profile)
{
 for(size_t i=0;i<n;i++)for(unsigned bit=0;bit<8;bit++){
  bytes[i]^=1u<<bit;assert((vertex?remesa_scene_vertex_body(bytes,n,profile):remesa_scene_fragment_body(bytes,n,profile))==EINVAL);bytes[i]^=1u<<bit;
 }
}
int main(void)
{
 unsigned char image[144],oracle[64];size_t n;
 assert(fread(oracle,1,64,stdin)==64&&fgetc(stdin)==EOF);
 struct remesa_usse_program prefix={.schema=1,.stage=REMESA_IR_VERTEX,.count=8};
 for(unsigned i=0;i<4;i++){
  prefix.instructions[i]=(struct remesa_usse_instruction){.op=REMESA_USSE_INPUT,.dst=i,.src=REMESA_USSE_NO_VALUE,.channel=i};
  prefix.instructions[i+4]=(struct remesa_usse_instruction){.op=REMESA_USSE_EXPORT_POSITION,.dst=REMESA_USSE_NO_VALUE,.src=i,.channel=i};
 }
 struct remesa_usse_vertex_mapping mapping={0};
 for(unsigned i=0;i<4;i++){mapping.input[i]=(struct remesa_usse_cpu_register){2,i};mapping.output[i]=(struct remesa_usse_cpu_register){1,i};}
 memset(image,0xa5,144);assert(!remesa_usse_vertex_candidate(&prefix,&mapping,image,144,&n)&&n==16&&image[16]==0xa5);
 assert(!remesa_scene_vertex_body(image,16,0)&&!remesa_scene_vertex_body(image,144,0));corrupt(image,16,1,0);
 assert(remesa_scene_vertex_body(image,15,0)==EINVAL);
 struct remesa_usse_cpu_white_lambert body={0,4,8,0};
 memset(image,0xa5,144);assert(!remesa_usse_cpu_white_lambert_vertex_encode(&body,image,144,&n)&&n==64&&image[64]==0xa5);
 assert(!memcmp(image,oracle,64)&&!remesa_scene_vertex_body(oracle,64,1));
 assert(!remesa_scene_vertex_body(image,64,1)&&!remesa_scene_vertex_body(image,144,1));corrupt(image,64,1,1);
 assert(remesa_scene_vertex_body(image,63,1)==EINVAL);
 body.normal_base=8;body.light_base=4;assert(!remesa_usse_cpu_white_lambert_vertex_encode(&body,image,144,&n));assert(remesa_scene_vertex_body(image,64,1)==EINVAL);
 struct remesa_usse_program fs={.schema=1,.stage=REMESA_IR_FRAGMENT,.count=8};
 for(unsigned i=0;i<4;i++){
  fs.instructions[i]=(struct remesa_usse_instruction){.op=REMESA_USSE_CONSTANT,.dst=i,.src=REMESA_USSE_NO_VALUE,.bits=(i==1||i==3)?0x3f800000:0};
  fs.instructions[i+4]=(struct remesa_usse_instruction){.op=REMESA_USSE_EXPORT_COLOR,.dst=REMESA_USSE_NO_VALUE,.src=i,.channel=i};
 }
 memset(image,0xa5,144);assert(!remesa_usse_encode(&fs,image,144,&n)&&n==8&&image[8]==0xa5);
 assert(!remesa_scene_fragment_body(image,8,0)&&!remesa_scene_fragment_body(image,144,0));corrupt(image,8,0,0);
 assert(remesa_scene_fragment_body(image,7,0)==EINVAL);
 const struct remesa_usse_fragment_color_request color={1,0};struct remesa_usse_fragment_color_extent extent;
 memset(image,0xa5,144);assert(!remesa_usse_fragment_color_encode(&color,image,144,&extent)&&extent.logical_bytes==8&&image[8]==0xa5);
 assert(!remesa_scene_fragment_body(image,8,1)&&!remesa_scene_fragment_body(image,144,1));corrupt(image,8,0,1);
 assert(remesa_scene_fragment_body(image,7,1)==EINVAL);
 assert(remesa_scene_fragment_body(image,8,2)==EINVAL&&remesa_scene_vertex_body(image,144,2)==EINVAL);
 puts("PASS selected USE bodies: unchanged prefix16/green8/white-Lambert64/COLOR0-MOVE8 constructors, independent original-compiler USE64 oracle/all768 single-bit mutations rejected, exact effective bytes vs capacity, other mapping and short input refused; GPU0");return 0;
}
