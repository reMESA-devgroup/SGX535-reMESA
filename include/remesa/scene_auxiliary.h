/* SPDX-License-Identifier: MIT */
#ifndef REMESA_SCENE_AUXILIARY_H
#define REMESA_SCENE_AUXILIARY_H
#include "ta_references.h"
/* Exact existing CPU auxiliary constructors, not a general ISA validator.
 * Only committed8/16/8 bytes; reserved alignment gaps are not instructions. */
static inline int remesa_scene_background_aux(const void *input,size_t available)
{
 const remesa_ta_ref_u8 *p=input;remesa_ta_ref_uptr a=(remesa_ta_ref_uptr)input;
 if(!input||available<8||a>~(remesa_ta_ref_uptr)0-8)return EINVAL;
 return remesa_ta_ref_word(p)==((2u<<30)|(1u<<29))&&
  remesa_ta_ref_word(p+4)==(1u|(5u<<27)|(1u<<12)|(1u<<18)|(1u<<23)|(1u<<16))?0:EINVAL;
}
static inline int remesa_scene_event_transfer_aux(const void *input,size_t available)
{
 const remesa_ta_ref_u8 *p=input;remesa_ta_ref_uptr a=(remesa_ta_ref_uptr)input;
 remesa_scene_u32 t=0x204u,control=(31u<<27)|(3u<<24)|(1u<<21)|(1u<<2);
 if(!input||available<16||a>~(remesa_ta_ref_uptr)0-16)return EINVAL;
 return remesa_ta_ref_word(p)==((2u<<30)|(1u<<26)|(1u<<21)|(2u<<14)|(3u<<7))&&
  remesa_ta_ref_word(p+4)==control&&
  remesa_ta_ref_word(p+8)==((2u<<30)|(1u<<21)|(1u<<7)|((t<<22)&0x0fc00000u))&&
  remesa_ta_ref_word(p+12)==(control|(1u<<18)|((t<<10)&0x00c00000u)|((t>>3)&0x1f8u))?0:EINVAL;
}
static inline int remesa_scene_event_end_aux(const void *input,size_t available)
{
 const remesa_ta_ref_u8 *p=input;remesa_ta_ref_uptr a=(remesa_ta_ref_uptr)input;
 if(!input||available<8||a>~(remesa_ta_ref_uptr)0-8)return EINVAL;
 return !remesa_ta_ref_word(p)&&remesa_ta_ref_word(p+4)==((31u<<27)|(3u<<20)|(1u<<15)|(1u<<18))?0:EINVAL;
}
#endif
