/* SPDX-License-Identifier: MIT */
#ifndef REMESA_GEARS_SCENE_PROBE_UAPI_H
#define REMESA_GEARS_SCENE_PROBE_UAPI_H
#include "scene_probe_uapi.h"
/* Separate closed LAB ABI. Never reinterpret the old17-binding request or
 * its reserved words. Selectors1/2 describe exact1280x800/640x640 catalogues.
 * Matching kernel registration/catalogues and physical qualification required.
 * No caller packet, VA, CODE, pointer or execution-evidence injection. */
#define REMESA_GEARS_SCENE_PROFILE_COUNT 2u
struct remesa_gears_scene_probe_request {
 __u32 version,profile;
 __u64 context;
 __u32 ids[18],budget_ms,reserved_input;
 __u64 operation,cookie;
 __u32 core,revision,pds_base,phase,ledger;
 __s32 error;
 __u32 retained,stages,reserved[2];
};
#define DRM_REMESA_GEARS_SCENE_PROBE 13
#define DRM_IOCTL_REMESA_GEARS_SCENE_PROBE DRM_IOWR(DRM_COMMAND_BASE+DRM_REMESA_GEARS_SCENE_PROBE,struct remesa_gears_scene_probe_request)
/* Shared prefix layout only. prefix.version=2 and prefix.bytes=4280; the
 * complete binding list is prefix.bindings[0..16] followed by last_binding.
 * The old trace ioctl refuses18-binding data rather than truncating it. */
struct remesa_gears_scene_trace {
 struct remesa_scene_probe_trace prefix;
 struct remesa_scene_trace_binding last_binding;
};
struct remesa_gears_scene_trace_request {
 __u32 version,reserved;
 __u64 context;
 struct remesa_gears_scene_trace trace;
};
#define DRM_REMESA_GEARS_SCENE_TRACE 14
#define DRM_IOCTL_REMESA_GEARS_SCENE_TRACE DRM_IOWR(DRM_COMMAND_BASE+DRM_REMESA_GEARS_SCENE_TRACE,struct remesa_gears_scene_trace_request)
#endif
