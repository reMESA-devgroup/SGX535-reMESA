/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SGX535_GMA500_FIXED_UAPI_H
#define SGX535_GMA500_FIXED_UAPI_H

#include <linux/types.h>
#include <drm/drm.h>

/* Private index zero is vacant in the retained antiX gma500 driver. This
 * request has no address, handle, pointer, register or command input. */
struct sgx535_fixed_ioctl {
    __u32 abi_version;
    __u32 operation;
    __u32 flags;
    __u32 reserved;
    __s32 operation_errno;
    __u32 outcome;
    __u32 phase;
    __u32 observed_events;
    __u32 color_observed;
    __u32 color_fnv1a;
    __u32 color_nonzero_pixels;
    __u32 color_row_nonzero[32];
    __u8 color_bytes[4096];
};

#define DRM_IOCTL_PSB_FIXED_TRIANGLE \
    DRM_IOWR(DRM_COMMAND_BASE, struct sgx535_fixed_ioctl)

#endif
