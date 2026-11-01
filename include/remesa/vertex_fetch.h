/* SPDX-License-Identifier: MIT */
#ifndef REMESA_VERTEX_FETCH_H
#define REMESA_VERTEX_FETCH_H
#include "ta_references.h"
struct remesa_vertex_fetch {remesa_scene_u32 source_address,stride_bytes,use_lower19,compiled;};
struct remesa_vertex_indices {remesa_scene_u32 minimum,maximum;};
/* Known fields of the existing single-descriptor DATA48/CODE16 CPU producer.
 * compiled selects4252d vs copy40355. Other DATA cells retain unknown masks.
 * Not a general PDS decoder or a certificate of shader/runtime semantics. */
static inline int remesa_vertex_fetch_pds(const void *input,size_t available,
 remesa_scene_u32 stride_dwords,remesa_scene_u32 compiled,struct remesa_vertex_fetch *out)
{
 remesa_ta_ref_uptr a=(remesa_ta_ref_uptr)input,b=(remesa_ta_ref_uptr)out,limit=~(remesa_ta_ref_uptr)0;
 const remesa_ta_ref_u8 *p=input;struct remesa_vertex_fetch value;remesa_scene_u32 dma,task;
 if(!input||!out||available<64||a>limit-64||b>limit-sizeof(*out)||b%_Alignof(struct remesa_vertex_fetch))return EINVAL;
 if(a<=b?b-a<64:a-b<sizeof(*out))return EINVAL;
 if(!stride_dwords||stride_dwords>16||compiled>1)return EINVAL;
 value.source_address=remesa_ta_ref_word(p);value.stride_bytes=stride_dwords*4;value.compiled=compiled;
 dma=0x80000000u|((stride_dwords-1)<<21)|(stride_dwords-1);task=remesa_ta_ref_word(p+16);
 if(remesa_ta_ref_word(p+4)!=dma||(task&~0x7ffffu)!=0x200000u||remesa_ta_ref_word(p+32)!=value.stride_bytes||
    remesa_ta_ref_word(p+48)!=(0x67000000u|0x800000u|0x70u)||
    remesa_ta_ref_word(p+52)!=(0x2f000000u|0x30000u|0x343u)||
    remesa_ta_ref_word(p+56)!=(0x03000205u|(4u<<17)|0x60u|0x180u)||
    remesa_ta_ref_word(p+60)!=0xaf000000u)return EINVAL;
 if(compiled&&(remesa_ta_ref_word(p+20)!=0xe0000000u||remesa_ta_ref_word(p+36)!=12))return EINVAL;
 if((value.source_address&3)||!remesa_ta_ref_span(value.source_address,value.stride_bytes))return ERANGE;
 value.use_lower19=task&0x7ffffu;*out=value;return 0;
}
/* Inspect only the active little-endian u16 triangle-list window. Vertex count
 * describes committed tightly packed records, never rounded BO capacity.
 * No vertex arithmetic/software rendering. Output is disjoint/transactional. */
static inline int remesa_vertex_indices_check(const void *input,size_t available,
 remesa_scene_u32 index_count,remesa_scene_u32 vertex_count,struct remesa_vertex_indices *out)
{
 remesa_ta_ref_uptr a=(remesa_ta_ref_uptr)input,b=(remesa_ta_ref_uptr)out,limit=~(remesa_ta_ref_uptr)0;
 const remesa_ta_ref_u8 *p=input;remesa_scene_u32 bytes=index_count*2,i;
 struct remesa_vertex_indices value={0xffffffffu,0};
 if(!input||!out||index_count<3||index_count>65535||index_count%3||!vertex_count||vertex_count>65536)return EINVAL;
 if(available<bytes||a>limit-bytes||b>limit-sizeof(*out)||b%_Alignof(struct remesa_vertex_indices))return EINVAL;
 if(a<=b?b-a<bytes:a-b<sizeof(*out))return EINVAL;
 for(i=0;i<index_count;i++){
  remesa_scene_u32 index=(remesa_scene_u32)p[i*2]|((remesa_scene_u32)p[i*2+1]<<8);
  if(index>=vertex_count)return ERANGE;
  if(index<value.minimum)value.minimum=index;
  if(index>value.maximum)value.maximum=index;
 }
 *out=value;return 0;
}
#endif
