/* SPDX-License-Identifier: MIT */
#ifndef REMESA_DEPTH_CPU_H
#define REMESA_DEPTH_CPU_H
#include "remesa_fixed_state.h"
#include "pipe/p_state.h"
#include <errno.h>
#include <math.h>
#include <string.h>
/* CPU linear Z24X8 clear only. Not a GPU depth descriptor or draw admission.
 * Depth occupies low24 bits, X is zero. .5->00800000,1->00ffffff agree
 * with the independently authenticated physical LAB depth readback. */
static inline int remesa_prepare_depth24_clear(unsigned format,uint32_t w,uint32_t h,
 uint32_t stride,size_t bytes,double depth,uint32_t x,uint32_t y,uint32_t rw,uint32_t rh,
 struct remesa_color_clear *out)
{
 if(!out||!isfinite(depth))return EINVAL;
 if(format!=PIPE_FORMAT_Z24X8_UNORM)return ENOTSUP;
 struct remesa_color_clear r;union pipe_color_union zero={0};
 int e=remesa_prepare_color_clear(PIPE_FORMAT_B8G8R8A8_UNORM,w,h,stride,bytes,&zero,x,y,rw,rh,&r);
 if(e)return e;
 uint32_t z=depth<=0?0:depth>=1?0xffffffu:(uint32_t)(depth*16777215.0+0.5);
 for(unsigned i=0;i<4;i++)r.pixel[i]=(unsigned char)(z>>(i*8));
 *out=r;return 0;
}
#endif
