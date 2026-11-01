/* SPDX-License-Identifier: MIT */
#ifndef REMESA_EVENT_FETCH_H
#define REMESA_EVENT_FETCH_H
#include "ta_references.h"
struct remesa_event_fetch {remesa_scene_u32 source_address,end_lower19,transfer_lower19;};
/* Preserved fixed392ab CPU DATA64/CODE84. Numeric equality is not general
 * event ISA semantics, measured fetch extent or permission to execute. */
static inline int remesa_event_fetch_read(const void *input,size_t available,struct remesa_event_fetch *out)
{
 const remesa_ta_ref_u8 *p=input;remesa_ta_ref_uptr a=(remesa_ta_ref_uptr)input,b=(remesa_ta_ref_uptr)out,limit=~(remesa_ta_ref_uptr)0;
 struct remesa_event_fetch value;remesa_scene_u32 i,task;
 static const remesa_scene_u32 code[21]={0xcf820030u,0x87600000u,0x90000005u,0x070003e5u,0xaf000000u,
  0xcf820830u,0x87600000u,0x90000014u,0x07042363u,0x170086e0u,0xf7800b31u,0xcf621031u,
  0xc762c070u,0xf7800b32u,0xcf641432u,0xc764c070u,0xf7811933u,0xcf661833u,0xc766c070u,0x070b0345u,0xaf000000u};
 static const remesa_scene_u32 tail[7]={8,0,2,0x10000000u,0x7f800u,0x07f00000u,0x08000000u};
 if(!input||!out||available<148||a>limit-148||b>limit-sizeof(*out)||b%_Alignof(struct remesa_event_fetch))return EINVAL;
 if(a<=b?b-a<148:a-b<sizeof(*out))return EINVAL;
 value.end_lower19=remesa_ta_ref_word(p);task=remesa_ta_ref_word(p+16);
 if(value.end_lower19>0x7ffffu||(task&~0x7ffffu)!=0x200000u||remesa_ta_ref_word(p+4)||
    remesa_ta_ref_word(p+12)!=0x80000003u||remesa_ta_ref_word(p+20))return EINVAL;
 for(i=0;i<7;i++)if(remesa_ta_ref_word(p+32+i*4)!=tail[i])return EINVAL;
 for(i=0;i<21;i++)if(remesa_ta_ref_word(p+64+i*4)!=code[i])return EINVAL;
 /* DATA6/7/15 remain unknown under the original producer's masks. */
 value.transfer_lower19=task&0x7ffffu;value.source_address=remesa_ta_ref_word(p+8);
 if((value.source_address&3)||!remesa_ta_ref_span(value.source_address,16))return ERANGE;
 *out=value;return 0;
}
/* Original BGRA8 event target DATA16. Pitch, not logical width, is the low
 * extent dimension. Caller already checked full COLOR backing/ownership. */
static inline int remesa_event_target_check(const void *input,size_t available,
 remesa_scene_u32 pitch,remesa_scene_u32 height,remesa_scene_u32 color)
{
 const remesa_ta_ref_u8 *p=input;remesa_ta_ref_uptr a=(remesa_ta_ref_uptr)input;
 if(!input||available<16||a>~(remesa_ta_ref_uptr)0-16||!pitch||pitch>2048||(pitch&31)||!height||height>2048)return EINVAL;
 if((color&3)||!remesa_ta_ref_span(color,4))return ERANGE;
 return remesa_ta_ref_word(p)==((((pitch>>1)-1)<<16)|0x8000u)&&remesa_ta_ref_word(p+4)==color&&
  !remesa_ta_ref_word(p+8)&&remesa_ta_ref_word(p+12)==(((height-1)<<12)|(pitch-1))?0:EINVAL;
}
#endif
