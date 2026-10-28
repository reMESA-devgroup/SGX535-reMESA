/* SPDX-License-Identifier: MIT */
#ifndef REMESA_TWOD_FILL_UAPI_H
#define REMESA_TWOD_FILL_UAPI_H
#include <linux/types.h>
#include <drm/drm.h>
#define REMESA_TWOD_FILL_VERSION 1u
#define REMESA_TWOD_FILL_PREPARED 1u
#define REMESA_TWOD_FILL_STARTED 2u
#define REMESA_TWOD_FILL_COMPLETE 4u
#define REMESA_TWOD_FILL_VISIBLE 8u
#define REMESA_TWOD_FILL_RETIRED 16u
#define REMESA_TWOD_FILL_ALL 31u
struct remesa_twod_fill_surface {__u32 id,width,height,stride;};
/* Privileged destination-only solid fill. All control/response fields start0.
 * Color is a packed native32-bit fill value; no source VA/ID, user packet or
 * injected receipt. Same staged completion contract as the normal copy ABI,
 * separately enabled and qualified. Experimental, not general GL admission. */
struct remesa_twod_fill_request {
 __u32 version,flags;
 __u64 context;
 struct remesa_twod_fill_surface destination;
 __u32 destination_x,destination_y,width,height,color,budget_ms,reserved[2];
 __u64 operation,cookie;
 __u32 stages,retained,core,revision;
 __s32 error;
 __u32 response_reserved;
};
#define DRM_REMESA_TWOD_FILL 8
#define DRM_IOCTL_REMESA_TWOD_FILL DRM_IOWR(DRM_COMMAND_BASE+DRM_REMESA_TWOD_FILL,struct remesa_twod_fill_request)
#endif
