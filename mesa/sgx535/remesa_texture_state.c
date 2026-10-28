/* SPDX-License-Identifier: MIT */
#include "remesa_texture_state.h"
#include <errno.h>
#include <string.h>

static void put32(uint8_t *p, uint32_t value)
{
   for (unsigned i=0; i<4; i++) p[i]=(uint8_t)(value>>(8*i));
}

int remesa_texture_cpu_strided_state_encode(
   const struct remesa_texture_cpu_strided_state *s, void *out, size_t capacity,
   size_t *written)
{
   if (!s || !out || !written) return EINVAL;
   if (!s->width || !s->height || !s->stride_bytes ||
       (s->stride_bytes&3u) || !s->image_address || (s->image_address&3u))
      return EINVAL;
   if (s->width>4096 || s->height>4096 || s->stride_bytes/4u>4096)
      return ERANGE;
   if ((uint64_t)s->width*4u>s->stride_bytes) return EINVAL;
   if ((s->format_id!=20 && s->format_id!=21) || s->filter_id>2)
      return ENOTSUP;
   const uint64_t end_limit=UINT64_C(1)<<32;
   const uint64_t span=(uint64_t)s->stride_bytes*s->height;
   if (s->image_address>UINT32_MAX || span>end_limit-s->image_address)
      return ERANGE;
   if (capacity<REMESA_TEXTURE_CPU_STATE_BYTES) return ENOSPC;
   const uint32_t pitch=s->stride_bytes/4u-1u;
   static const uint32_t filter_words[]={0,0x1400,0x2800};
   const uint32_t word0=UINT32_C(0x80000000)|filter_words[s->filter_id]|
      (pitch&0x1ffu)|((pitch&0xe00u)<<9);
   const uint32_t word1=UINT32_C(0x60000000)|
      (s->format_id==20?UINT32_C(0x0c000000):UINT32_C(0x0d000000))|
      ((s->width-1u)<<12)|(s->height-1u);
   uint8_t bytes[REMESA_TEXTURE_CPU_STATE_BYTES];
   put32(bytes,word0); put32(bytes+4,word1);
   put32(bytes+8,(uint32_t)s->image_address);
   memcpy(out,bytes,sizeof(bytes));
   *written=sizeof(bytes);
   return 0;
}
