/* SPDX-License-Identifier: MIT */
#ifndef REMESA_RESOURCE_LAYOUT_H
#define REMESA_RESOURCE_LAYOUT_H
#include "remesa_device.h"
#include <errno.h>
#include <limits.h>
struct remesa_resource_layout {
 uint32_t stride,pages;
 size_t bytes;
};
/* Separate BO backing sizes for a prospective drawable. This plans storage,
 * not SGX surface descriptors or depth-test/rendering support. Front and back
 * must be independently owned; a swap must not alias their live allocations. */
struct remesa_drawable_layout {
 uint32_t count;
 struct remesa_resource_layout planes[3]; /* front, optional back, optional Z */
 size_t allocation_bytes[3],total_bytes;
};
#define REMESA_TEXTURE_LEVELS 32u
struct remesa_texture_level {
 uint32_t width,height,stride;
 size_t offset,bytes;
};
struct remesa_texture_layout {
 uint32_t count,pages;
 size_t bytes;
 struct remesa_texture_level levels[REMESA_TEXTURE_LEVELS];
};
/* Checked host-storage layout only. Neither successful layout nor allocation
 * establishes SGX visibility, hardware dimensions, or sampling support. */
static inline int remesa_resource_layout(const struct remesa_resource_limits *limits,
 uint32_t width,uint32_t height,uint32_t cpp,struct remesa_resource_layout *out)
{
 if(!limits||!out)return EINVAL;
 uint32_t maxw=limits->max_width?limits->max_width:4096;
 uint32_t maxh=limits->max_height?limits->max_height:4096;
 uint32_t align=limits->row_alignment?limits->row_alignment:1;
 uint64_t budget=limits->max_resource_bytes?limits->max_resource_bytes:UINT64_C(64)*1024*1024;
 uint64_t row,total;
 if(!width||!height||!cpp||width>maxw||height>maxh||
    (align&(align-1)))return EINVAL;
 row=(uint64_t)width*cpp;
 if(row>UINT32_MAX||row+align-1>UINT32_MAX)return EOVERFLOW;
 row=(row+align-1)&~((uint64_t)align-1);
 total=row*height;
 /* Conservative <=4GiB storage admission keeps layouts portable to the
  * i386 target; pipe_transfer.layer_stride is uintptr_t in pinned Mesa. */
 if(total>SIZE_MAX||total>UINT32_MAX)return EOVERFLOW;
 if(total>budget)return ENOMEM;
 *out=(struct remesa_resource_layout){.stride=(uint32_t)row,
  .bytes=(size_t)total,.pages=(uint32_t)((total+4095)/4096)};
 return 0;
}
/* BGRA8 colors; depth_cpp describes backing only (0,2,4), not a GPU format.
 * Entire result is transactional, including page rounding and aggregate budget.
 * Reuse the resource owner/allocator for each plane, never grow a live BO. */
static inline int remesa_drawable_layout(const struct remesa_resource_limits *limits,
 uint32_t width,uint32_t height,bool double_buffer,uint32_t depth_cpp,
 size_t byte_budget,struct remesa_drawable_layout *out)
{
 struct remesa_drawable_layout result={0};
 if(!limits||!out||!byte_budget||(depth_cpp&&depth_cpp!=2&&depth_cpp!=4))return EINVAL;
 result.count=1+(unsigned)double_buffer+(depth_cpp!=0);
 for(unsigned i=0;i<result.count;i++) {
  uint32_t cpp=depth_cpp&&i==result.count-1?depth_cpp:4;
  int e=remesa_resource_layout(limits,width,height,cpp,&result.planes[i]);if(e)return e;
  uint64_t rounded=(uint64_t)result.planes[i].pages*4096;
  /* Keep allocation sizes and the combined plan portable to i386, even when
   * a sub-4GiB pixel extent rounds up to exactly 4GiB. */
  if(rounded>UINT32_MAX || rounded>UINT32_MAX-result.total_bytes)return EOVERFLOW;
  if(rounded>byte_budget-result.total_bytes)return ENOSPC;
  result.allocation_bytes[i]=(size_t)rounded;result.total_bytes+=(size_t)rounded;
 }
 *out=result;return 0;
}
/* Linear 2D host backing, with independently bounded rows at every mip level.
 * The whole chain shares one allocation and one writer/lease. This is not a
 * hardware texture descriptor, tiled layout or claim about GPU sampling. */
static inline int remesa_texture_layout(const struct remesa_resource_limits *limits,
 uint32_t width,uint32_t height,uint32_t cpp,uint32_t last_level,
 struct remesa_texture_layout *out)
{
 struct remesa_texture_layout result={0};uint32_t largest=width>height?width:height,max_level=0;
 if(!limits||!out||!width||!height||last_level>=REMESA_TEXTURE_LEVELS)return EINVAL;
 while(largest>1){largest>>=1;++max_level;}
 if(last_level>max_level)return EINVAL;
 uint64_t budget=limits->max_resource_bytes?limits->max_resource_bytes:UINT64_C(64)*1024*1024;
 for(uint32_t level=0;level<=last_level;level++) {
  struct remesa_resource_layout row;
  int error=remesa_resource_layout(limits,width,height,cpp,&row);if(error)return error;
  if(row.bytes>SIZE_MAX-result.bytes||row.bytes>UINT32_MAX-result.bytes)return EOVERFLOW;
  size_t total=result.bytes+row.bytes;
  if(total>budget)return ENOMEM;
  result.levels[level]=(struct remesa_texture_level){width,height,row.stride,result.bytes,row.bytes};
  result.bytes=total;
  if(width>1)width>>=1;
  if(height>1)height>>=1;
 }
 result.count=last_level+1;result.pages=(uint32_t)(((uint64_t)result.bytes+4095)/4096);
 *out=result;return 0;
}
#endif
