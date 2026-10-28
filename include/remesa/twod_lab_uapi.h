/* SPDX-License-Identifier: MIT */
#ifndef REMESA_TWOD_LAB_UAPI_H
#define REMESA_TWOD_LAB_UAPI_H
#include <linux/types.h>
#include <drm/drm.h>
#define REMESA_TWOD_LAB_VERSION 1u
#define REMESA_TWOD_LAB_PREPARED 1u
#define REMESA_TWOD_LAB_STARTED 2u
#define REMESA_TWOD_LAB_COMPLETE 4u
#define REMESA_TWOD_LAB_VISIBLE 8u
#define REMESA_TWOD_LAB_RETIRED 16u
#define REMESA_TWOD_LAB_ALL 31u
struct remesa_twod_lab_surface {__u32 id,width,height,stride;};
/* Isolated root-only LAB copy, not a stable general rendering ABI.
 * IDs resolve to existing file-bound GEM objects; no VA/PFN/raw packet or
 * injected completion accepted. All flags/reserved/response fields start0.
 * After prepare, ioctl0 returns a STRUCTURED outcome including failures.
 * Only error0 + stagesALL + retained0 indicates successful retirement.
 * Actual GPU execution still needs independent physical readback/guards.
 */
struct remesa_twod_lab_request {
 __u32 version,flags;
 __u64 context;
 struct remesa_twod_lab_surface source,destination;
 __u32 source_x,source_y,destination_x,destination_y;
 __u32 width,height,budget_ms,reserved;
 __u64 operation,cookie;
 __u32 stages,retained,core,revision;
 __s32 error;
 __u32 response_reserved;
 __u64 phase_ns[4]; /* issue, wait, publish, retire: LAB only */
};
#define DRM_REMESA_TWOD_LAB 6
#define DRM_IOCTL_REMESA_TWOD_LAB DRM_IOWR(DRM_COMMAND_BASE+DRM_REMESA_TWOD_LAB,struct remesa_twod_lab_request)
#endif
