/* SPDX-License-Identifier: MIT */
#ifndef REMESA_SCENE_USE_BODIES_H
#define REMESA_SCENE_USE_BODIES_H
#include "scene_auxiliary.h"
/* Exact selected bodies from the existing typed CPU constructors. These are
 * word predicates, not shader emission/templates or a general ISA validator.
 * No parameter/uniform source occurs in these profiles: prefix reads4 input
 * DWORDs; selected white-Lambert reads12. No new mapping admission follows. */
static inline int remesa_scene_vertex_body(const void *input,size_t available,remesa_scene_u32 compiled)
{
 const remesa_ta_ref_u8 *p=input;remesa_ta_ref_uptr a=(remesa_ta_ref_uptr)input;unsigned i;
 remesa_scene_u32 bytes=compiled?64:16;
 static const remesa_scene_u32 words[16]={
  (2u<<30)|(1u<<29),1u|(5u<<27)|(3u<<12)|(1u<<21)|(1u<<23)|(1u<<16),
  (2u<<30)|(2u<<28)|(2u<<21)|(4u<<7)|8u,(2u<<27)|(1u<<23)|(1u<<21)|(2u<<12)|1u,
  (1u<<24)|1u,(31u<<27)|(1u<<25)|(1u<<20),
  (1u<<30)|(1u<<29)|(5u<<21)|(4u<<7),1u|(5u<<27)|(2u<<12)|(1u<<21)|(1u<<23)|(1u<<16),
  (1u<<24)|(1u<<16)|(1u<<8),(31u<<27)|(1u<<25)|(1u<<20),
  (1u<<30)|(1u<<28)|(4u<<21)|(4u<<7)|48u,(3u<<27)|(1u<<23)|(1u<<21)|(3u<<12)|(1u<<16)|(1u<<9)|1u,
  (1u<<30)|(1u<<28)|(4u<<21)|(4u<<7)|52u,(3u<<27)|(1u<<23)|(1u<<21)|(3u<<12)|(1u<<16)|1u,
  (2u<<30)|(2u<<28)|(1u<<21),(31u<<27)|(3u<<24)|(1u<<21)|(1u<<14)|(1u<<17)|(1u<<16)|(1u<<12)|(1u<<18)};
 if(!input||compiled>1||available<bytes||a>~(remesa_ta_ref_uptr)0-bytes)return EINVAL;
 if(compiled){for(i=0;i<16;i++)if(remesa_ta_ref_word(p+i*4)!=words[i])return EINVAL;}
 else if(remesa_ta_ref_word(p)!=words[0]||remesa_ta_ref_word(p+4)!=words[1]||
         remesa_ta_ref_word(p+8)!=words[14]||remesa_ta_ref_word(p+12)!=words[15])return EINVAL;
 return 0;
}
static inline int remesa_scene_fragment_body(const void *input,size_t available,remesa_scene_u32 compiled)
{
 const remesa_ta_ref_u8 *p=input;remesa_ta_ref_uptr a=(remesa_ta_ref_uptr)input;
 remesa_scene_u32 color=0xff00ff00u,fixed=(31u<<27)|(4u<<24)|(1u<<21)|(1u<<23)|(1u<<18)|1u;
 if(!input||compiled>1||available<8||a>~(remesa_ta_ref_uptr)0-8)return EINVAL;
 if(compiled)return remesa_scene_background_aux(input,8); /* Same ordinary COLOR0 MOVE8. */
 return remesa_ta_ref_word(p)==(color&0x1fffffu)&&
  remesa_ta_ref_word(p+4)==(fixed|(((color>>21)&31u)<<4)|(((color>>26)&63u)<<12))?0:EINVAL;
}
#endif
