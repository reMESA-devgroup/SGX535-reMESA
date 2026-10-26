/* SPDX-License-Identifier: MIT */
#ifndef REMESA_SCENE_LAYOUT_H
#define REMESA_SCENE_LAYOUT_H
#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/string.h>
typedef u32 remesa_scene_u32;
#else
#include <stdint.h>
#include <errno.h>
#include <string.h>
typedef uint32_t remesa_scene_u32;
#endif
/* Shared CPU construction math, not hardware admission or a wire format.
 * Original Xpsb3a40 writes only cookies0..14. Unwritten cookie15 is absent.
 * Reused by Mesa encoding and the kernel-buildable variable action planner. */
struct remesa_scene_layout {
 remesa_scene_u32 cookie[15];
 remesa_scene_u32 required_bytes,allocation_pages,clear_first_page,clear_page_count;
};
static inline int remesa_scene_layout_build(remesa_scene_u32 width,remesa_scene_u32 height,remesa_scene_u32 option64,
 struct remesa_scene_layout *out)
{
 remesa_scene_u32 tx,ty,ax,ay,dx,dy,sx,sy,n=1,offset,tail;
 struct remesa_scene_layout result;
 if(!out||!width||!height||option64>1)return EINVAL;
 if(width>=2048||height>=2048)return EOPNOTSUPP;
 tx=(width+15)/16;ty=(height+15)/16;
 ax=(((tx+1)/2)+3)&~3u;ay=(((ty+1)/2)+3)&~3u;
 dx=option64?(2*tx+3)&~3u:ax;dy=option64?(2*ty+3)&~3u:ay;
 if(dx>255)dx=255;
 if(dy>255)dy=255;
 sx=2*ax;sy=2*ay;
 while(n<sx||n<sy)n*=2;
 offset=(4*n*n+4095)&~4095u;tail=offset+12*sx*sy;
 result=(struct remesa_scene_layout){.cookie={
  0,ax*ay,(dx<<22)|(dx<<12)|dx,(dy<<22)|(dy<<12)|dy,dx*dy,
  ((ty-1)<<12)|(tx-1),((height-1)<<12)|(width-1),offset,0,
  tail,tail+0x50,tail+0xe0,0,0,0},
  .required_bytes=tail+0x120,.allocation_pages=(tail+0x120+4095)/4096,
  .clear_first_page=0,.clear_page_count=(offset+4095)/4096};
 memcpy(out,&result,sizeof(result));return 0;
}

#endif
