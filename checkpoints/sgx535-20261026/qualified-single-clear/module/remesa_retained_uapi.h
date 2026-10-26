/* SPDX-License-Identifier: MIT */
#ifndef REMESA_RETAINED_UAPI_H
#define REMESA_RETAINED_UAPI_H
#include <linux/types.h>
#include <drm/drm.h>
#define REMESA_RETAINED_VERSION 2u
#define REMESA_RETAINED_ROLES 11u
#define REMESA_RETAINED_COMMAND_BYTES 264u
struct remesa_retained_binding {__u32 id,reserved;__u64 va,bytes;};
/* One isolated laboratory attempt, not a permanent render ABI. All response
 * bytes must initially be zero. Private1280 profile: complete job capture occurs before ACK in the same
 * syscall; ioctl4 reads immutable per-file snapshot after successful return. No completion injection is accepted. */
struct remesa_retained_request {
 __u32 version,flags;
 __u64 context;
 struct remesa_retained_binding bindings[REMESA_RETAINED_ROLES];
 __u8 commands[REMESA_RETAINED_COMMAND_BYTES];
 __u32 reserved[2];
 __u64 operation,serial;
 __u32 slot,generation,state;
 __s32 error;
 __u32 admitted,attached;
 __u32 observer_request_bytes,observer_response_bytes;
 __u8 observer_request[640],observer_response[4352];
};
#define DRM_REMESA_RETAINED 3
#define DRM_IOCTL_REMESA_RETAINED DRM_IOWR(DRM_COMMAND_BASE+DRM_REMESA_RETAINED,struct remesa_retained_request)
#endif
