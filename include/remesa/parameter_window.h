/* SPDX-License-Identifier: MIT */
#ifndef REMESA_PARAMETER_WINDOW_H
#define REMESA_PARAMETER_WINDOW_H
/* Internal metadata of the existing owned PARAM source, not an ioctl ABI,
 * executable validator or permission to trust a userspace-supplied window. */
#ifdef __KERNEL__
#include <linux/types.h>
typedef u32 remesa_parameter_word;typedef u64 remesa_parameter_address;
#else
#include <stdint.h>
typedef uint32_t remesa_parameter_word;typedef uint64_t remesa_parameter_address;
#endif
struct remesa_parameter_window {
 remesa_parameter_address owner,generation,gpu_va;
 remesa_parameter_word id,allocation_bytes,source_offset;
 remesa_parameter_word coefficient_dwords,effective_source_dwords,transfer_dwords;
 remesa_parameter_word use_offset,use_bytes,pds_offset,pds_bytes,ta_offset,ta_bytes;
};
/* Bounds of producer metadata only. Actual PARAM/DMA encoding, program role,
 * complete image authentication and native ownership remain admission work. */
static inline int remesa_parameter_window_valid(const struct remesa_parameter_window *w)
{
 remesa_parameter_address source_end,use_end,pds_end,ta_end;
 remesa_parameter_word effective;
 if(!w||!w->owner||!w->generation||!w->id||!w->gpu_va||w->gpu_va>0xffffffffULL||
    !w->allocation_bytes||w->allocation_bytes>0x100000000ULL-w->gpu_va||
    (w->gpu_va&3)||(w->source_offset&3)||!w->coefficient_dwords||
    w->coefficient_dwords>127||w->transfer_dwords>128)return 0;
 effective=w->coefficient_dwords+1;
 if(w->effective_source_dwords!=effective||
    (w->transfer_dwords!=effective&&w->transfer_dwords!=((effective+3u)&~3u))||
    w->use_bytes!=8||w->pds_bytes!=60||w->ta_bytes!=8||
    (w->use_offset&15)||(w->pds_offset&31)||(w->ta_offset&3))return 0;
 source_end=(remesa_parameter_address)w->source_offset+w->transfer_dwords*4u;
 use_end=(remesa_parameter_address)w->use_offset+w->use_bytes;
 pds_end=(remesa_parameter_address)w->pds_offset+w->pds_bytes;
 ta_end=(remesa_parameter_address)w->ta_offset+w->ta_bytes;
 return source_end<=w->use_offset&&use_end<=w->pds_offset&&
        pds_end<=w->ta_offset&&ta_end<=w->allocation_bytes;
}
/* A DWORD subset is allowed; the self-address, transfer padding, reserved
 * gaps and all executable/command bytes are outside the coefficient window. */
static inline int remesa_parameter_window_patch_valid(const struct remesa_parameter_window *w,
 remesa_parameter_word id,remesa_parameter_address offset,unsigned bytes)
{
 remesa_parameter_address start,end;
 if(!remesa_parameter_window_valid(w)||id!=w->id||!bytes||(bytes&3)||(offset&3))return 0;
 start=(remesa_parameter_address)w->source_offset+4;
 end=start+w->coefficient_dwords*4u;
 return offset>=start&&offset<=end&&bytes<=end-offset;
}
/* Optional rule of the existing bounded-float profiles, not a universal GL
 * uniform restriction. A future program policy may admit different data. */
struct remesa_parameter_patch_policy {
 struct remesa_parameter_window window;
 remesa_parameter_word maximum_abs_bits;
};
static inline int remesa_parameter_patch_bounded_valid(const struct remesa_parameter_patch_policy *p,
 remesa_parameter_word id,remesa_parameter_address offset,const void *data,unsigned bytes)
{
 const unsigned char *b=data;unsigned i;
 if(!p||!data||p->maximum_abs_bits>0x7f7fffffu||
    !remesa_parameter_window_patch_valid(&p->window,id,offset,bytes))return 0;
 for(i=0;i<bytes;i+=4){
  remesa_parameter_word word=b[i]|((remesa_parameter_word)b[i+1]<<8)|
   ((remesa_parameter_word)b[i+2]<<16)|((remesa_parameter_word)b[i+3]<<24);
  if((word&0x7fffffffu)>p->maximum_abs_bits)return 0;
 }
 return 1;
}
#endif
