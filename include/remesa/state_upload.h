/* SPDX-License-Identifier: MIT */
#ifndef REMESA_STATE_UPLOAD_H
#define REMESA_STATE_UPLOAD_H
#include "ta_references.h"
struct remesa_state_upload {remesa_scene_u32 source_address,source_bytes,use_lower19;};
/* Selected ISP state producer28ef1..28fdb: DATA48/CODE12. Inspect only its
 * known DATA0..2 and complete three-word CODE; DATA3..11 remain unknown.
 * Numeric CPU producer contract, not general PDS/USSE semantics/admission.
 * Available capacity may exceed60, but only the committed60 bytes are read.
 * Positive errno, disjoint aligned output unchanged on error. */
static inline int remesa_state_upload_pds(const void *input,size_t available,
 remesa_scene_u32 dwords,struct remesa_state_upload *out)
{
 remesa_ta_ref_uptr a=(remesa_ta_ref_uptr)input,b=(remesa_ta_ref_uptr)out,limit=~(remesa_ta_ref_uptr)0;
 const remesa_ta_ref_u8 *p=input;struct remesa_state_upload value;remesa_scene_u32 dma,task,pair;
 if(!input||!out||available<60||a>limit-60||b>limit-sizeof(*out)||b%_Alignof(struct remesa_state_upload))return EINVAL;
 if(a<=b?b-a<60:a-b<sizeof(*out))return EINVAL;
 if(!dwords||dwords>16)return EINVAL;
 value.source_address=remesa_ta_ref_word(p);value.source_bytes=dwords*4;
 dma=0x80000000u|((dwords-1)<<21)|(dwords-1);task=remesa_ta_ref_word(p+8);
 pair=0x07000205u|(2u<<17)|(1u<<13)|0x40u|0x100u;
 if(remesa_ta_ref_word(p+4)!=dma||(task&~0x7ffffu)!=0x200000u||
    remesa_ta_ref_word(p+48)!=(0x07000000u|0x30000u|0x223u)||
    remesa_ta_ref_word(p+52)!=pair||remesa_ta_ref_word(p+56)!=0xaf000000u)return EINVAL;
 if((value.source_address&3)||!remesa_ta_ref_span(value.source_address,value.source_bytes))return ERANGE;
 value.use_lower19=task&0x7ffffu;*out=value;return 0;
}
/* Exact existing two-record CPU state auxiliary: ordinary MOVE plus the
 * independently encoded three-source final record. Count drives both records;
 * no synthetic CODE or instruction is added. Only16 committed bytes are read. */
static inline int remesa_state_upload_auxiliary(const void *input,size_t available,remesa_scene_u32 dwords)
{
 const remesa_ta_ref_u8 *p=input;remesa_ta_ref_uptr a=(remesa_ta_ref_uptr)input;
 if(!input||available<16||a>~(remesa_ta_ref_uptr)0-16||!dwords||dwords>16)return EINVAL;
 if(remesa_ta_ref_word(p)!=((2u<<30)|(1u<<29))||
    remesa_ta_ref_word(p+4)!=(1u|(5u<<27)|((dwords-1)<<12)|(1u<<21)|(1u<<23)|(1u<<16))||
    remesa_ta_ref_word(p+8)!=((dwords<<7)|(2u<<30)|(2u<<28)|(1u<<21))||
    remesa_ta_ref_word(p+12)!=((31u<<27)|(3u<<24)|(1u<<21)|(1u<<14)|(1u<<17)|(1u<<16)|(1u<<18)))return EINVAL;
 return 0;
}
#endif
