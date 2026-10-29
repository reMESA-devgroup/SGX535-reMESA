/* SPDX-License-Identifier: MIT */
#ifndef REMESA_PRIVATE_PROBE_UAPI_H
#define REMESA_PRIVATE_PROBE_UAPI_H
#include <linux/types.h>
#include <drm/drm.h>
#define REMESA_PRIVATE_PROBE_VERSION 1u
#define REMESA_PRIVATE_PROBE_PREPARED 1u
#define REMESA_PRIVATE_PROBE_ISOLATED 2u
#define REMESA_PRIVATE_PROBE_STARTED 4u
#define REMESA_PRIVATE_PROBE_RETIRED 8u
#define REMESA_PRIVATE_PROBE_ALL 15u
/* Isolated privileged qualification only; defaults disabled. The kernel fixes
 * images/commands/VA/size to the retained profile. IDs resolve to this file's
 * existing GEM objects; no VA, PFN, raw packet or injected event is accepted.
 * All flags/reserved/response fields enter zero. ioctl0 is a structured result,
 * not GPU evidence. Independent physical readback/guards remain mandatory.
 * The attempt consumes the existing source lease's no-rearm lifetime. */
struct remesa_private_probe_request {
 __u32 version,flags;
 __u64 context;
 __u32 ids[11];
 __u32 budget_ms;
 __u64 operation,cookie;
 __u32 core,revision,phase,ledger;
 __s32 error;
 __u32 retained,stages,reserved;
};
#define DRM_REMESA_PRIVATE_PROBE 9
#define DRM_IOCTL_REMESA_PRIVATE_PROBE DRM_IOWR(DRM_COMMAND_BASE+DRM_REMESA_PRIVATE_PROBE,struct remesa_private_probe_request)
#endif
