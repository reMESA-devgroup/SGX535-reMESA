/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_STREAM_TRUSTED_H
#define REMESA_STREAM_TRUSTED_H
#include "stream_lifecycle.h"
#include "frozen_kernel_contract.h"
struct drm_device;struct drm_file;
/* A compiled kernel asset, never accepted from an ioctl. Hashes authorize ALL
 * bytes in GEN/USE/DATA/PDS/CONTROL. This initial strict grammar admits an
 * exact frame image only. Future animated uniforms need a separately reviewed
 * typed encoder/validator; arbitrary user-chosen digests cannot authorize it. */
struct remesa_stream_profile {
 unsigned char digest[32],images[5][32];
 struct remesa_stream_binding bindings[11]; /* ids ignored; VA/bytes exact */
 struct remesa_stream_batch batches[64]; /* generation ignored; rest exact */
 unsigned count;
 struct sgx535_frozen_command_pairs commands;
};
int remesa_stream_profile_install(const struct remesa_stream_profile *);
int remesa_stream_trusted_install(void);
struct sgx535_gma500_owner;
int remesa_stream_trusted_prefire_cache(struct sgx535_gma500_owner *,bool *);
int remesa_stream_trusted_owner(struct drm_device *,struct drm_file *);
#endif
