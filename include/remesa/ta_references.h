/* SPDX-License-Identifier: MIT */
#ifndef REMESA_TA_REFERENCES_H
#define REMESA_TA_REFERENCES_H
#include "scene_layout.h"
#ifdef __KERNEL__
typedef u8 remesa_ta_ref_u8;
typedef u64 remesa_ta_ref_u64;
typedef unsigned long remesa_ta_ref_uptr;
#else
typedef uint8_t remesa_ta_ref_u8;
typedef uint64_t remesa_ta_ref_u64;
typedef uintptr_t remesa_ta_ref_uptr;
#endif
#define REMESA_TA_REFERENCE_MAX_DRAWS 16u
#define REMESA_TA_REFERENCE_MAX (3u*(1u+REMESA_TA_REFERENCE_MAX_DRAWS)+1u)
enum remesa_ta_reference_kind {
 REMESA_TA_REF_PREFIX_STATE=1,REMESA_TA_REF_PREFIX_INDEX,REMESA_TA_REF_PREFIX_VERTEX,
 REMESA_TA_REF_DRAW_STATE,REMESA_TA_REF_DRAW_INDEX,REMESA_TA_REF_DRAW_VERTEX,
 REMESA_TA_REF_CLOSE_STATE
};
struct remesa_ta_reference {
 remesa_scene_u32 address,minimum_bytes,kind,index_count,draw;
};
struct remesa_ta_reference_counts {remesa_scene_u32 draws,references;};
typedef int (*remesa_ta_reference_visitor)(void *,const struct remesa_ta_reference *);
static inline remesa_scene_u32 remesa_ta_ref_word(const remesa_ta_ref_u8 *p)
{return (remesa_scene_u32)p[0]|((remesa_scene_u32)p[1]<<8)|((remesa_scene_u32)p[2]<<16)|((remesa_scene_u32)p[3]<<24);}
static inline int remesa_ta_ref_span(remesa_scene_u32 address,remesa_scene_u32 bytes)
{return address&&bytes&&(remesa_ta_ref_u64)address+bytes<=0x100000000ULL;}
static inline int remesa_ta_ref_group(const remesa_ta_ref_u8 *p,unsigned draw,
 struct remesa_ta_reference refs[3])
{
 remesa_scene_u32 state=remesa_ta_ref_word(p),control=remesa_ta_ref_word(p+4);
 remesa_scene_u32 header=remesa_ta_ref_word(p+8),index=remesa_ta_ref_word(p+12);
 remesa_scene_u32 zero=remesa_ta_ref_word(p+16),pds=remesa_ta_ref_word(p+20);
 remesa_scene_u32 descriptor=remesa_ta_ref_word(p+24),count=header&65535u;
 if((state&0xf0000000u)!=0x40000000u||control!=(draw?0x0c02a204u:0x0c02a203u)||
    (header&0xffff0000u)!=0x81400000u||zero||(pds&0xf0000000u)||
    descriptor!=(draw?0x10000303u:0x02000103u)||count<3||count%3||(!draw&&count!=3))return EINVAL;
 refs[0]=(struct remesa_ta_reference){(state&0x0fffffffu)<<4,60,
  draw?REMESA_TA_REF_DRAW_STATE:REMESA_TA_REF_PREFIX_STATE,0,draw};
 refs[1]=(struct remesa_ta_reference){index,count*2,
  draw?REMESA_TA_REF_DRAW_INDEX:REMESA_TA_REF_PREFIX_INDEX,count,draw};
 refs[2]=(struct remesa_ta_reference){pds<<4,64,
  draw?REMESA_TA_REF_DRAW_VERTEX:REMESA_TA_REF_PREFIX_VERTEX,0,draw};
 if((index&1)||!remesa_ta_ref_span(refs[0].address,60)||
    !remesa_ta_ref_span(index,count*2)||!remesa_ta_ref_span(refs[2].address,64))return ERANGE;
 if((remesa_ta_ref_u64)index<((remesa_ta_ref_u64)refs[2].address+64)&&
    (remesa_ta_ref_u64)refs[2].address<(remesa_ta_ref_u64)index+count*2)return EINVAL;
 return 0;
}
/* Exact grammar of the current owned linked-TA producer, not a general ISA
 * decoder. Validates all syntax before invoking any visitor, then emits ordered
 * references. minimum_bytes is only a lower bound visible from this grammar,
 * NEVER the complete PDS object/fetch length. The visitor must authenticate
 * actual backing, committed extents, graph relationships and permissions.
 * Caller keeps input immutable throughout; visitor side effects are caller-owned.
 * No allocation, MMIO, source authority or execution admission. Positive errno.
 * Counts are disjoint/aligned and unchanged on failure, including visitor error.
 */
static inline int remesa_ta_references_walk(const void *input,size_t bytes,
 remesa_ta_reference_visitor visitor,void *context,struct remesa_ta_reference_counts *out)
{
 remesa_ta_ref_uptr a=(remesa_ta_ref_uptr)input,b=(remesa_ta_ref_uptr)out;
 remesa_ta_ref_uptr limit=~(remesa_ta_ref_uptr)0;
 const remesa_ta_ref_u8 *p=input;struct remesa_ta_reference refs[3],close;
 struct remesa_ta_reference_counts value;unsigned pass,draw,k;int error;
 if(!input||!out||a>limit-bytes||b>limit-sizeof(*out)||b%_Alignof(struct remesa_ta_reference_counts))return EINVAL;
 if(a<=b?b-a<bytes:a-b<sizeof(*out))return EINVAL;
 if(bytes<68||bytes>40+28*REMESA_TA_REFERENCE_MAX_DRAWS||(bytes-40)%28)return EINVAL;
 value.draws=(bytes-40)/28;value.references=3*(value.draws+1)+1;
 for(pass=0;pass<(visitor?2u:1u);pass++){
  for(draw=0;draw<=value.draws;draw++){
   error=remesa_ta_ref_group(p+draw*28,draw,refs);if(error)return error;
   if(pass)for(k=0;k<3;k++){error=visitor(context,&refs[k]);if(error)return error;}
  }
  {
   const remesa_ta_ref_u8 *tail=p+(value.draws+1)*28;
   remesa_scene_u32 state=remesa_ta_ref_word(tail);
   if((state&0xf0000000u)!=0x60000000u||remesa_ta_ref_word(tail+4)!=0x0c02a201u||
      remesa_ta_ref_word(tail+8)!=0xc0000000u)return EINVAL;
   close=(struct remesa_ta_reference){(state&0x0fffffffu)<<4,60,REMESA_TA_REF_CLOSE_STATE,0,value.draws+1};
   if(!remesa_ta_ref_span(close.address,60))return ERANGE;
   if(pass){error=visitor(context,&close);if(error)return error;}
  }
 }
 *out=value;return 0;
}
#endif
