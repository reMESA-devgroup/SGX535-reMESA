/* SPDX-License-Identifier: MIT */
#ifndef REMESA_COPY_H
#define REMESA_COPY_H
#include <stdint.h>
/* Internal serialized byte-preserving four-component copy; not an ioctl ABI.
 * No format conversion or caller-supplied GPU address/code. */
struct remesa_copy_surface {void *bo;uint32_t width,height,stride;};
struct remesa_copy_region {
 struct remesa_copy_surface source,destination;
 uint32_t source_x,source_y,destination_x,destination_y,width,height;
};
struct remesa_gpu_copy {
 struct {uint32_t id,width,height,stride;} source,destination;
 uint32_t source_x,source_y,destination_x,destination_y,width,height;
};
/* Zero attests synchronous authenticated GPU completion, publication and
 * retirement. Ordinary errors guarantee NOT issued. EOWNERDEAD (or a malformed
 * negative return) means uncertain effects: retain both buffers, never retry
 * or silently replace the operation with a CPU copy. Test callbacks are not
 * physical GPU evidence. */
typedef int (*remesa_copy_region_fn)(void *,const struct remesa_copy_region *);
typedef int (*remesa_gpu_copy_fn)(void *,const struct remesa_gpu_copy *);
/* Native fill values are packed bytes in surface storage order. */
struct remesa_fill_region {
 struct remesa_copy_surface destination;
 uint32_t destination_x,destination_y,width,height,color;
};
struct remesa_gpu_fill {
 struct {uint32_t id,width,height,stride;} destination;
 uint32_t destination_x,destination_y,width,height,color;
};
/* Same synchronous receipt/error contract as copy; retain the destination on
 * uncertainty. These internal callbacks do not grant hardware admission. */
typedef int (*remesa_fill_region_fn)(void *,const struct remesa_fill_region *);
typedef int (*remesa_gpu_fill_fn)(void *,const struct remesa_gpu_fill *);
#endif
