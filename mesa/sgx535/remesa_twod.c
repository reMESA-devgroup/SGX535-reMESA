/* SPDX-License-Identifier: MIT; independently authored bounded FIFO packing. */
#include "remesa_twod.h"
#ifdef __KERNEL__
#include <linux/errno.h>
#include <linux/string.h>
#else
#include <errno.h>
#include <stddef.h>
#include <string.h>
#endif
static int span(const void *p,size_t n)
{return p&&n<=~(uintptr_t)0-(uintptr_t)p;}
static int surface(const struct remesa_twod_surface *s,uint32_t base)
{
 uint64_t limit=(uint64_t)base+0x10000000u;
 if(!s->allocation_id||!s->width||s->width>4096||!s->height||s->height>4096||
    s->format!=REMESA_TWOD_ARGB8888)return EOPNOTSUPP;
 if((s->gpu_address&3)||s->gpu_address<base||s->gpu_address>=limit||
    !s->bytes||s->bytes>limit-s->gpu_address)return EINVAL;
 if((s->stride&3)||s->stride<s->width*4u||s->stride>0x7fffu)return EOPNOTSUPP;
 if((uint64_t)(s->height-1)*s->stride+(uint64_t)s->width*4>s->bytes)return ENOSPC;
 return 0;
}
static void put32(unsigned char *p,uint32_t x)
{unsigned n;for(n=0;n<4;n++)p[n]=(unsigned char)(x>>(8*n));}
int remesa_twod_copy_prepare(const struct remesa_twod_copy *request,
 struct remesa_twod_copy_plan *out)
{
 struct remesa_twod_copy r;struct remesa_twod_copy_plan plan={0};
 uintptr_t a,b;unsigned reverse_x,reverse_y,i;int error;
 uint32_t direction,sx,sy,dx,dy,words[10];
 if((uintptr_t)request%_Alignof(struct remesa_twod_copy)||
    (uintptr_t)out%_Alignof(struct remesa_twod_copy_plan)||
    !span(request,sizeof(*request))||!span(out,sizeof(*out)))return EINVAL;
 a=(uintptr_t)request;b=(uintptr_t)out;
 if(a<=b?b-a<sizeof(*request):a-b<sizeof(*out))return EINVAL;
 r=*request;
 if(r.reserved||(r.request_base&0x0fffffffu)||!r.width||!r.height)return EINVAL;
 if(r.width>4095||r.height>4095||r.width==8||r.height==8)return EOPNOTSUPP;
 error=surface(&r.source,r.request_base);if(error)return error;
 error=surface(&r.destination,r.request_base);if(error)return error;
 if(r.source_x>=r.source.width||r.source_y>=r.source.height||
    r.destination_x>=r.destination.width||r.destination_y>=r.destination.height||
    r.width>r.source.width-r.source_x||r.height>r.source.height-r.source_y||
    r.width>r.destination.width-r.destination_x||r.height>r.destination.height-r.destination_y)return EINVAL;
 if(r.source.allocation_id==r.destination.allocation_id){
  if(r.source.gpu_address!=r.destination.gpu_address||r.source.bytes!=r.destination.bytes||
     r.source.width!=r.destination.width||r.source.height!=r.destination.height||
     r.source.stride!=r.destination.stride||r.source.format!=r.destination.format)return EINVAL;
 }else if(r.source.gpu_address<r.destination.gpu_address+r.destination.bytes&&
          r.destination.gpu_address<r.source.gpu_address+r.source.bytes)return EINVAL;
 plan.source_id=r.source.allocation_id;plan.destination_id=r.destination.allocation_id;
 plan.request_base=r.request_base;plan.dwords=10;
 plan.source_first=r.source.gpu_address+(uint64_t)r.source_y*r.source.stride+(uint64_t)r.source_x*4;
 plan.source_end=plan.source_first+(uint64_t)(r.height-1)*r.source.stride+(uint64_t)r.width*4;
 plan.destination_first=r.destination.gpu_address+(uint64_t)r.destination_y*r.destination.stride+(uint64_t)r.destination_x*4;
 plan.destination_end=plan.destination_first+(uint64_t)(r.height-1)*r.destination.stride+(uint64_t)r.width*4;
 reverse_x=r.source_x<r.destination_x;reverse_y=r.source_y<r.destination_y;
 direction=reverse_x?(reverse_y?1u:2u):(reverse_y?3u:0u);
 sx=r.source_x+(reverse_x?r.width-1:0);sy=r.source_y+(reverse_y?r.height-1:0);
 dx=r.destination_x+(reverse_x?r.width-1:0);dy=r.destination_y+(reverse_y?r.height-1:0);
 words[0]=0x70000000u;words[1]=0xa0060000u|r.destination.stride;
 words[2]=(uint32_t)(r.destination.gpu_address-r.request_base);
 words[3]=0x90060000u|r.source.stride;
 words[4]=(uint32_t)(r.source.gpu_address-r.request_base);
 words[5]=0x30000000u|(sx<<12)|sy;words[6]=0x8001ccccu|(direction<<23);
 words[7]=(dx<<12)|dy;words[8]=(r.width<<12)|r.height;words[9]=0xf0000000u;
 for(i=0;i<10;i++)put32(plan.command+4*i,words[i]);
 *out=plan;return 0;
}
#ifdef REMESA_TWOD_VENDOR_STREAM
int remesa_twod_copy_prepare_vendor(const struct remesa_twod_copy *request,
 struct remesa_twod_copy_plan *out)
{
 struct remesa_twod_copy_plan plan;uintptr_t a,b;int error;
 if((uintptr_t)request%_Alignof(struct remesa_twod_copy)||
    (uintptr_t)out%_Alignof(struct remesa_twod_copy_plan)||
    !span(request,sizeof(*request))||!span(out,sizeof(*out)))return EINVAL;
 a=(uintptr_t)request;b=(uintptr_t)out;
 if(a<=b?b-a<sizeof(*request):a-b<sizeof(*out))return EINVAL;
 error=remesa_twod_copy_prepare(request,&plan);if(error)return error;
 memmove(plan.command,plan.command+4,36);
 memset(plan.command+36,0,4); /* Reserved tail, never submitted as an opcode. */
 plan.dwords=9;*out=plan;return 0;
}
#endif
