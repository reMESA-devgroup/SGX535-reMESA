/* SPDX-License-Identifier: MIT. Consumed CODE vs reserved capacity; GPU0. */
#include "remesa/scene_use_bodies.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void read_exact(const char *name,void *bytes,size_t size)
{
 FILE *f=fopen(name,"rb");assert(f&&fread(bytes,1,size,f)==size&&fgetc(f)==EOF&&!fclose(f));
}
int main(int argc,char **argv)
{
 unsigned char original[320],code[344],clamp[24],arena[384],saved[384];
 assert(argc==4);read_exact(argv[1],original,320);read_exact(argv[2],code,344);read_exact(argv[3],clamp,24);
 assert(!memcmp(code,original,312)&&!memcmp(code+312,clamp,24)&&!memcmp(code+336,original+312,8));
 assert(!remesa_scene_gears_attribute_vertex_body(original,sizeof original));
 assert(remesa_scene_gears_attribute_vertex_body(code,sizeof code)==EINVAL);
 assert(remesa_scene_gears_attribute_clamped_vertex_body(original,sizeof original)==EINVAL);
 memset(arena,0xa5,sizeof arena);memcpy(arena+16,code,344);memcpy(saved,arena,sizeof saved);
 assert(!remesa_scene_gears_attribute_clamped_vertex_body(arena+16,344));
 assert(!remesa_scene_gears_attribute_clamped_vertex_body(arena+16,352));
 assert(remesa_scene_gears_attribute_clamped_vertex_body(arena+16,343)==EINVAL);
 assert(!memcmp(arena,saved,sizeof arena));
 unsigned mutations=0;
 for(unsigned byte=0;byte<344;byte++)for(unsigned bit=0;bit<8;bit++){
  arena[16+byte]^=1u<<bit;
  assert(remesa_scene_gears_attribute_clamped_vertex_body(arena+16,352)==EINVAL);
  arena[16+byte]^=1u<<bit;mutations++;
 }
 assert(!memcmp(arena,saved,sizeof arena));
 /* Padding belongs to allocation policy; it is not an extra instruction. */
 arena[16+344]^=1;assert(!remesa_scene_gears_attribute_clamped_vertex_body(arena+16,352));
 assert(remesa_scene_gears_attribute_clamped_vertex_body(NULL,344)==EINVAL);
 assert(remesa_scene_gears_attribute_clamped_vertex_body((void *)((remesa_ta_ref_uptr)-1-100),344)==EINVAL);
 memcpy(arena+16,original,320);memset(arena+16+320,0,24);
 assert(remesa_scene_gears_attribute_clamped_vertex_body(arena+16,344)==EINVAL);
 printf("PASS distinct original320/clamped344 predicates, %u single-bit refusals, capacity padding, short/wrapped inputs and immutable bytes; GPU0\n",mutations);
 return 0;
}
