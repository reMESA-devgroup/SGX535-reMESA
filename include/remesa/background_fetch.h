/* SPDX-License-Identifier: MIT */
#ifndef REMESA_BACKGROUND_FETCH_H
#define REMESA_BACKGROUND_FETCH_H
#include "fragment_fetch.h"
struct remesa_background_binding {remesa_scene_u32 primary,secondary;};
struct remesa_background_task {remesa_scene_u32 color_address,use_lower19;};
/* Selected existing background68 constructor, depth1, no kernel floating point.
 * Explicit address window; no claim about active PDS_EXEC_BASE or ISA semantics. */
static inline int remesa_background_binding_read(const void *input,size_t available,
 remesa_scene_u32 width,remesa_scene_u32 height,remesa_scene_u32 window,struct remesa_background_binding *out)
{
 const remesa_ta_ref_u8 *p=input;remesa_ta_ref_uptr a=(remesa_ta_ref_uptr)input,b=(remesa_ta_ref_uptr)out,limit=~(remesa_ta_ref_uptr)0;
 struct remesa_background_binding value;remesa_scene_u32 primary,secondary,i,packed[3];
 if(!input||!out||available<68||a>limit-68||b>limit-sizeof(*out)||b%_Alignof(struct remesa_background_binding))return EINVAL;
 if(a<=b?b-a<68:a-b<sizeof(*out))return EINVAL;
 if(!width||width>2048||!height||height>2048||(window&0x0fffffffu))return EINVAL;
 secondary=remesa_ta_ref_word(p);primary=remesa_ta_ref_word(p+8);
 if((secondary&0xff000000u)||(primary&0xff000000u)!=0x0c000000u||remesa_ta_ref_word(p+4)!=0x30001u)return EINVAL;
 for(i=0;i<6;i++)if(remesa_ta_ref_word(p+12+i*4)!=((i==2||i==5)?0x3f800000u:0))return EINVAL;
 if(remesa_ta_ref_word(p+36)!=2||remesa_ta_ref_word(p+40))return EINVAL;
 packed[0]=(1024u<<20)|(1024u<<4);packed[1]=((1024u+width)<<20)|(1024u<<4);packed[2]=(1024u<<20)|((1024u+height)<<4);
 for(i=0;i<3;i++)if(remesa_ta_ref_word(p+44+i*8)!=packed[i]||remesa_ta_ref_word(p+48+i*8)!=0x3f800000u)return EINVAL;
 value.primary=window|((primary&0xffffffu)<<4);value.secondary=window|((secondary&0xffffffu)<<4);
 if((value.primary&31)||(value.secondary&31)||!remesa_ta_ref_span(value.primary,64)||!remesa_ta_ref_span(value.secondary,4)||
    (remesa_ta_ref_u64)value.primary+64>(remesa_ta_ref_u64)window+0x10000000u||
    (remesa_ta_ref_u64)value.secondary+4>(remesa_ta_ref_u64)window+0x10000000u)return ERANGE;
 if((remesa_ta_ref_u64)value.primary<(remesa_ta_ref_u64)value.secondary+4&&
    (remesa_ta_ref_u64)value.secondary<(remesa_ta_ref_u64)value.primary+64)return EINVAL;
 *out=value;return 0;
}
/* Existing BGRA8/tag4 DATA48/CODE16 primary. Unknown DATA4..7/11 stays masked.
 * Dimensions/address are producer inputs, not texture-layout/runtime proof. */
static inline int remesa_background_primary(const void *input,size_t available,
 remesa_scene_u32 width,remesa_scene_u32 height,struct remesa_background_task *out)
{
 const remesa_ta_ref_u8 *p=input;remesa_ta_ref_uptr a=(remesa_ta_ref_uptr)input,b=(remesa_ta_ref_uptr)out,limit=~(remesa_ta_ref_uptr)0;
 struct remesa_background_task value;remesa_scene_u32 word;
 if(!input||!out||available<64||a>limit-64||b>limit-sizeof(*out)||b%_Alignof(struct remesa_background_task))return EINVAL;
 if(a<=b?b-a<64:a-b<sizeof(*out))return EINVAL;
 if(!width||width>2048||!height||height>2048)return EINVAL;
 word=remesa_ta_ref_word(p);
 if((word&~0x7ffffu)!=0x100000u||remesa_ta_ref_word(p+4)||remesa_ta_ref_word(p+8)!=0x001e0092u||
    remesa_ta_ref_word(p+12)!=(0x6c000000u|((width-1)<<12)|(height-1))||
    remesa_ta_ref_word(p+32)!=0x20u||remesa_ta_ref_word(p+36)!=0xf800u||
    remesa_ta_ref_word(p+48)!=0x07000345u||remesa_ta_ref_word(p+52)!=0x070418a2u||
    remesa_ta_ref_word(p+56)!=0x07042364u||remesa_ta_ref_word(p+60)!=0xaf000000u)return EINVAL;
 value.color_address=remesa_ta_ref_word(p+40);value.use_lower19=word&0x7ffffu;
 if((value.color_address&3)||!remesa_ta_ref_span(value.color_address,4))return ERANGE;
 *out=value;return 0;
}
/* Complete emitted BGRA8 primary; sparse unknown-field handling above is
 * unchanged. Output is transactional through the existing decoder. */
static inline int remesa_background_primary_image(const void *input,size_t available,
 remesa_scene_u32 width,remesa_scene_u32 height,struct remesa_background_task *out)
{
 const remesa_ta_ref_u8 *p=input;unsigned i;
 if(!input||available<64||(remesa_ta_ref_uptr)input>~(remesa_ta_ref_uptr)0-64)return EINVAL;
 for(i=4;i<8;i++)if(remesa_ta_ref_word(p+4*i))return EINVAL;
 if(remesa_ta_ref_word(p+44))return EINVAL;
 return remesa_background_primary(input,available,width,height,out);
}
#endif
