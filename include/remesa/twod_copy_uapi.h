/* SPDX-License-Identifier: MIT */
#ifndef REMESA_TWOD_COPY_UAPI_H
#define REMESA_TWOD_COPY_UAPI_H
#include <linux/types.h>
#include <drm/drm.h>
#define REMESA_TWOD_COPY_VERSION 1u
#define REMESA_TWOD_COPY_PREPARED 1u
#define REMESA_TWOD_COPY_STARTED 2u
#define REMESA_TWOD_COPY_COMPLETE 4u
#define REMESA_TWOD_COPY_VISIBLE 8u
#define REMESA_TWOD_COPY_RETIRED 16u
#define REMESA_TWOD_COPY_ALL 31u
struct remesa_twod_copy_surface {__u32 id,width,height,stride;};
/* Privileged typed native copy, separate from the LAB interface.
 * No diagnostic timing, hashes, dumps or pixel readbacks. Default disabled;
 * per-file context and CAP_SYS_ADMIN admission remain mandatory.
 * Experimental versioned interface, not unprivileged/general GL admission.
 * IDs resolve to existing file-bound GEM objects; no VA/PFN/raw packet or
 * injected completion accepted. All flags/reserved/response fields start0.
 * After prepare, ioctl0 returns a STRUCTURED outcome including failures.
 * Only error0 + stagesALL + retained0 indicates successful retirement.
 * Actual GPU execution still needs independent physical readback/guards.
 */
struct remesa_twod_copy_request {
 __u32 version,flags;
 __u64 context;
 struct remesa_twod_copy_surface source,destination;
 __u32 source_x,source_y,destination_x,destination_y;
 __u32 width,height,budget_ms,reserved;
 __u64 operation,cookie;
 __u32 stages,retained,core,revision;
 __s32 error;
 __u32 response_reserved;
};
#define DRM_REMESA_TWOD_COPY 7
#define DRM_IOCTL_REMESA_TWOD_COPY DRM_IOWR(DRM_COMMAND_BASE+DRM_REMESA_TWOD_COPY,struct remesa_twod_copy_request)
#endif
