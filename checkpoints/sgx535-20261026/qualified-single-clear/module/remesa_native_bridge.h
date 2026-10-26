/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_NATIVE_BRIDGE_H
#define REMESA_NATIVE_BRIDGE_H
#include "remesa/native_uapi.h"
struct drm_device;
struct drm_file;
struct sgx535_gma500_owner;
/* Supplied by the isolated qualified asset build, not generic clean sources.
 * expected contains semantic inputs only; construct must generate that exact
 * workload and keep all SGX program encodings in the isolated asset tree. */
struct remesa_qualified_asset {
    const struct remesa_native_draw *expected;
    int (*construct)(struct drm_device *, struct sgx535_gma500_owner *);
    /* Copy validated request geometry into retained owned CPU views before
     * backend publication. Must not issue any SGX work or change programs. */
    int (*publish)(struct sgx535_gma500_owner *, const struct remesa_native_draw *);
    u32 core_id;
    u32 core_revision;
    u32 timeout_ticks;
    u32 max_samples;
};
int remesa_native_triangle_profile_install(void);
int remesa_native_asset_install(const struct remesa_qualified_asset *asset);
int remesa_native_draw_ioctl(struct drm_device *, void *, struct drm_file *);
#endif
