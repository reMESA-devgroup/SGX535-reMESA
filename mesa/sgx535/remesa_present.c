/* SPDX-License-Identifier: MIT */
#include "remesa_present.h"
#include "remesa_screen.h"
#include "remesa_device.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>
int remesa_present_snapshot(struct pipe_context *ctx,struct pipe_resource *res,void *dst,size_t stride,size_t capacity)
{
 struct pipe_transfer *transfer=NULL;struct pipe_box box={0};void *src;size_t row,required;
 if(!ctx || !res || res->screen!=ctx->screen || !dst || !ctx->texture_map || !ctx->texture_unmap ||
    (res->target!=PIPE_TEXTURE_2D&&res->target!=PIPE_TEXTURE_RECT) ||
    res->format!=PIPE_FORMAT_B8G8R8A8_UNORM ||
    !res->width0 || !res->height0 || res->width0>INT32_MAX ||
    res->depth0!=1 || res->array_size!=1 || res->last_level || res->nr_samples>1||res->nr_storage_samples>1) return EINVAL;
 row=(size_t)res->width0*4;
 if(row/4!=res->width0 || stride<row || (res->height0>1 && stride>(SIZE_MAX-row)/(res->height0-1))) return EINVAL;
 required=(res->height0-1)*stride+row;if(capacity<required) return ENOSPC;
 box.width=res->width0;box.height=res->height0;box.depth=1;
 int error=remesa_resource_wait_writer(ctx,res,0);if(error) return error;
 /* Mapping is the resource's writer-fence/publication gate. Never retry here. */
 src=ctx->texture_map(ctx,res,0,PIPE_MAP_READ|PIPE_MAP_DONTBLOCK,&box,&transfer);
 if(!src){error=remesa_context_error(ctx);return error?error:EIO;}
 if(!transfer || transfer->resource!=res || transfer->stride<row ||
    (res->height0>1 && transfer->stride>(SIZE_MAX-row)/(res->height0-1))) {
  if(transfer) ctx->texture_unmap(ctx,transfer);
  return EPROTO;
 }
 for(unsigned y=0;y<res->height0;y++)
  memcpy((unsigned char *)dst+y*stride,(const unsigned char *)src+y*transfer->stride,row);
 ctx->texture_unmap(ctx,transfer);return remesa_context_error(ctx);
}
