/* SPDX-License-Identifier: MIT */
#include "remesa_usse_sample.h"
#include <errno.h>
#include <string.h>
static int span_valid(const void *p,size_t size)
{return p&&size<=UINTPTR_MAX-(uintptr_t)p;}
static int overlaps(const void *a,size_t na,const void *b,size_t nb)
{
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 if(!na||!nb)return 0;
 return x<=y?y-x<na:x-y<nb;
}
static void put32(uint8_t *p,uint32_t word)
{for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(word>>(8*i));}
int remesa_usse_sample_2d_body_encode(const struct remesa_usse_sample_2d_body *s,
 void *out,size_t capacity,size_t *written)
{
 if(!span_valid(s,sizeof(*s))||!span_valid(out,capacity)||
    !span_valid(written,sizeof(*written)))return EINVAL;
 if(overlaps(s,sizeof(*s),out,capacity)||
    overlaps(s,sizeof(*s),written,sizeof(*written))||
    overlaps(out,capacity,written,sizeof(*written)))return EINVAL;
 if(s->reserved)return EINVAL;
 if(s->destination>127||s->coordinates>127||s->sampler>127||s->drc>3)
  return ERANGE;
 if(capacity<REMESA_USSE_SAMPLE_2D_BODY_BYTES)return ENOSPC;
 /* Major28; scalar SA source bank3, PA coordinate bank1, r destination.
  * Dimension1=2D, ordinary LOD0, channel mask15; no caller-selected bits. */
 uint8_t bytes[REMESA_USSE_SAMPLE_2D_BODY_BYTES];
 put32(bytes,(3u<<30)|(s->destination<<21)|
             (s->coordinates<<14)|(s->sampler<<7));
 put32(bytes+4,(28u<<27)|(15u<<12)|(1u<<10)|(1u<<2)|s->drc);
 /* Major31/other2/subop1, DRC bits1:0, all other controls clear. */
 put32(bytes+8,0);
 put32(bytes+12,(31u<<27)|(2u<<20)|(1u<<24)|s->drc);
 memcpy(out,bytes,sizeof(bytes));*written=sizeof(bytes);return 0;
}
