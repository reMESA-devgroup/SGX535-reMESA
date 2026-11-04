/* SPDX-License-Identifier: MIT */
#ifndef REMESA_SCENE_CONTROL_WORDS_H
#define REMESA_SCENE_CONTROL_WORDS_H
#include "scene_layout.h"
/* Existing Series5 DEFINED surface DATA contribution; not a LOAD certificate. */
#define REMESA_SCENE_DEPTH_DEFINED_BITS 0x00200026u
/* Serialization shared by the existing Mesa producer and the native consumer.
 * Inputs MUST already be validated by their owners. No address admission,
 * float operations, MMIO, buffer initialization or execution occurs here. */
/* Exact caller/context contribution preserved from the qualified Series5
 * depth finalizer. The caller must first validate the complete surface and
 * actual backing presence; this helper alone admits no state or addresses. */
static inline remesa_scene_u32 remesa_scene_depth_finalize_word(
 remesa_scene_u32 word,int caller_flag,int context_depth_present)
{return word|((caller_flag&&context_depth_present)?0x004000d0u:0u);}
static inline void remesa_scene_raster_words(remesa_scene_u32 color_410,
 remesa_scene_u32 background,remesa_scene_u32 event,remesa_scene_u32 field0c,
 const remesa_scene_u32 depth[6],remesa_scene_u32 clear_bits,
 remesa_scene_u32 stencil,remesa_scene_u32 out[52])
{
 static const remesa_scene_u32 regs[26]={0x400,0x404,0x40c,0x410,0x414,0x418,0x41c,0x420,
  0x424,0x42c,0x480,0xcb0,0x484,0x488,0x48c,0x490,0x494,0x4c4,0x4bc,0x4b8,
  0x4c8,0x4dc,0x800,0xa5c,0xa60,0xa64};
 remesa_scene_u32 values[26]={0xffff80,0,0,color_410,0x100,1,0x0da24260,0,0xfbf4,0,
  0xf0000,0,0,0,0,0,0xff,((background>>4)&0x00ffffff)|0x02000000,
  0x200|stencil,clear_bits,0x88,0,0,event,(field0c>>2)&0x3f,0x4fff};
 unsigned i;
 for(i=0;i<6;i++)values[10+i]=depth[i];
 for(i=0;i<26;i++){out[i*2]=regs[i];out[i*2+1]=values[i];}
}
static inline void remesa_scene_ta_words(remesa_scene_u32 stream,remesa_scene_u32 out[14])
{
 const remesa_scene_u32 words[14]={0x204,0,0x218,0x400000,0x23c,0x82,
  0x240,0x358637bd,0x244,0x358637bd,0x250,0x88,0x238,stream};
 memcpy(out,words,sizeof(words));
}
#endif
