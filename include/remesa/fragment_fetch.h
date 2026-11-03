/* SPDX-License-Identifier: MIT */
#ifndef REMESA_FRAGMENT_FETCH_H
#define REMESA_FRAGMENT_FETCH_H
#include "ta_references.h"
struct remesa_fragment_binding {remesa_scene_u32 primary,secondary,primary_bytes;};
struct remesa_fragment_task {remesa_scene_u32 use_lower19,committed_bytes;};
/* Selected state group40 reference triple, after its header. input_color
 * selects the current compiled COLOR0 profile; false is the no-R profile
 * used by scene Z-clear (some other constant profiles encode the same triple).
 * Window is an explicit CPU contract, NOT observation of PDS_EXEC_BASE. */
static inline int remesa_fragment_binding_read(const void *input,size_t available,
 remesa_scene_u32 window,remesa_scene_u32 input_color,struct remesa_fragment_binding *out)
{
 remesa_ta_ref_uptr a=(remesa_ta_ref_uptr)input,b=(remesa_ta_ref_uptr)out,limit=~(remesa_ta_ref_uptr)0;
 const remesa_ta_ref_u8 *p=input;struct remesa_fragment_binding value;
 remesa_scene_u32 secondary,primary,control,span,blocks,batch=4,chunks=32,n;
 if(!input||!out||available<12||a>limit-12||b>limit-sizeof(*out)||b%_Alignof(struct remesa_fragment_binding))return EINVAL;
 if(a<=b?b-a<12:a-b<sizeof(*out))return EINVAL;
 if((window&0x0fffffffu)||input_color>1)return EINVAL;
 secondary=remesa_ta_ref_word(p);primary=remesa_ta_ref_word(p+8);
 span=(input_color+3u)&~3u;blocks=(0x4dfu&0xffffffa0u)/3;
 if(span&&blocks/(span*4)<batch)batch=blocks/(span*4);
 if(!batch)batch=1;
 if(span){chunks=blocks/(span*4);if(chunks>32)chunks=32;if(!chunks)chunks=1;}
 n=chunks+3;if(n>32)n=32;if(n>batch*8)chunks=batch*8-3;
 control=input_color|((batch-1)<<16)|((chunks<<25)&0x3e000000u);
 if((secondary&0xff000000u)||(primary&0xff000000u)!=0x0c000000u||remesa_ta_ref_word(p+4)!=control)return EINVAL;
 value.secondary=window|((secondary&0xffffffu)<<4);value.primary=window|((primary&0xffffffu)<<4);
 value.primary_bytes=input_color?60:56;
 if((value.secondary&31)||(value.primary&31)||!remesa_ta_ref_span(value.secondary,4)||
    !remesa_ta_ref_span(value.primary,value.primary_bytes)||
    (remesa_ta_ref_u64)value.secondary+4>(remesa_ta_ref_u64)window+0x10000000u||
    (remesa_ta_ref_u64)value.primary+value.primary_bytes>(remesa_ta_ref_u64)window+0x10000000u)return ERANGE;
 if((remesa_ta_ref_u64)value.primary<((remesa_ta_ref_u64)value.secondary+4)&&
    (remesa_ta_ref_u64)value.secondary<(remesa_ta_ref_u64)value.primary+value.primary_bytes)return EINVAL;
 *out=value;return 0;
}
/* Existing CPU primary profile DATA48/CODE8 or12. Unknown DATA remains masked.
 * Known fields and CODE only, not USE shader-body/runtime semantics. */
static inline int remesa_fragment_primary(const void *input,size_t available,
 remesa_scene_u32 input_color,struct remesa_fragment_task *out)
{
 remesa_ta_ref_uptr a=(remesa_ta_ref_uptr)input,b=(remesa_ta_ref_uptr)out,limit=~(remesa_ta_ref_uptr)0;
 const remesa_ta_ref_u8 *p=input;struct remesa_fragment_task value;remesa_scene_u32 bytes=input_color?60:56,word;
 if(!input||!out||input_color>1||available<bytes||a>limit-bytes||b>limit-sizeof(*out)||b%_Alignof(struct remesa_fragment_task))return EINVAL;
 if(a<=b?b-a<bytes:a-b<sizeof(*out))return EINVAL;
 word=remesa_ta_ref_word(p);
 if((word&~0x7ffffu)!=(input_color?0x80000u:0)||remesa_ta_ref_word(p+4)||remesa_ta_ref_word(p+32)!=0x20u||
    remesa_ta_ref_word(p+48)!=(0x07000205u|0x40u|0x100u))return EINVAL;
 if(input_color){
  if(remesa_ta_ref_word(p+36)!=(0x01000000u|(10u<<12)|(3u<<22)|0x0c000000u|15u|0x02000000u)||
     remesa_ta_ref_word(p+52)!=(0x070400a2u|(3u<<11))||remesa_ta_ref_word(p+56)!=0xaf000000u)return EINVAL;
 }else if(remesa_ta_ref_word(p+52)!=0xaf000000u)return EINVAL;
 value.use_lower19=word&0x7ffffu;value.committed_bytes=bytes;*out=value;return 0;
}
static inline int remesa_fragment_secondary(const void *input,size_t available)
{
 remesa_ta_ref_uptr a=(remesa_ta_ref_uptr)input;
 if(!input||available<4||a>~(remesa_ta_ref_uptr)0-4)return EINVAL;
 return remesa_ta_ref_word(input)==0xaf000000u?0:EINVAL;
}
/* Complete zero-initialized constructor DATA plus the existing CODE checks.
 * This certifies emitted bytes only, not shader semantics or execution. */
static inline int remesa_fragment_primary_image(const void *input,size_t available,
 remesa_scene_u32 input_color,struct remesa_fragment_task *out)
{
 const remesa_ta_ref_u8 *p=input;unsigned i,bytes=input_color?60:56;
 if(!input||available<bytes||(remesa_ta_ref_uptr)input>~(remesa_ta_ref_uptr)0-bytes)return EINVAL;
 for(i=2;i<12;i++)if(i!=8&&!(input_color&&i==9))
  if(remesa_ta_ref_word(p+4*i))return EINVAL;
 return remesa_fragment_primary(input,available,input_color,out);
}
#endif
