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
#define REMESA_SCENE_PROBE_PROFILE_COUNT 6u
/* Selectors1/2 preserve original profiles;3/4 add explicit initialization;
 *5/6 additionally finalize actual Z through the existing caller contract. */
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
/* LAB snapshot only: no MMIO, command input or borrowed kernel pointers.
 * Query while the submitting memory context remains open; all trace bytes
 * enter zero. Saved plans describe intended writes, not MMIO readbacks. */
struct remesa_scene_trace_event {
 __u32 kind,sequence,status1,status2,ledger,phase;
 __u64 ns;
};
struct remesa_scene_trace_binding {__u32 id,access;__u64 address,bytes;};
struct remesa_scene_probe_trace {
 __u32 version,bytes,profile,stages,phase,ledger;
 __s32 error;
 __u32 retained,event_count,zero_samples,binding_count,load_count,ta_count,raster_count,use_mask,reserved;
 __u64 context,operation,cookie;
 __u64 cpu_flush_ns,ta_begin_ns,ta_end_ns,raster_begin_ns,raster_end_ns;
 __u32 ta[14],raster[52],use_banks[32];
 __u32 load_actions[29][4],ta_actions[31][4],raster_actions[20][4];
 struct remesa_scene_trace_event events[64];
 struct remesa_scene_trace_binding bindings[17];
};
struct remesa_scene_trace_request {
 __u32 version,reserved;
 __u64 context;
 struct remesa_scene_probe_trace trace;
};
#define DRM_REMESA_SCENE_TRACE 12
#define DRM_IOCTL_REMESA_SCENE_TRACE DRM_IOWR(DRM_COMMAND_BASE+DRM_REMESA_SCENE_TRACE,struct remesa_scene_trace_request)
#endif
