/* SPDX-License-Identifier: MIT */
#ifndef REMESA_SCENE_PROBE_UAPI_H
#define REMESA_SCENE_PROBE_UAPI_H
#include <linux/types.h>
#include <drm/drm.h>
/* Disabled-by-default, root-only, single consumed attempt per boot. Closed
 * Mesa-generated 1280x800/640x640 qualification profiles, not a production
 * render ABI. No caller VA, packet, CODE, pointer or injected event accepted.
 * Responses/reserved enter zero. ioctl0 is a structured result; independent
 * physical completion/readback/guard evidence is still required. */
struct remesa_scene_probe_request {
 __u32 version,profile;
 __u64 context;
 __u32 ids[17],budget_ms;
 __u64 operation,cookie;
 __u32 core,revision,pds_base,phase,ledger;
 __s32 error;
 __u32 retained,stages,reserved[2];
};
#define DRM_REMESA_SCENE_PROBE 11
#define DRM_IOCTL_REMESA_SCENE_PROBE DRM_IOWR(DRM_COMMAND_BASE+DRM_REMESA_SCENE_PROBE,struct remesa_scene_probe_request)
#endif
