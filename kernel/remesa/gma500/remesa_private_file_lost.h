/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_PRIVATE_FILE_LOST_H
#define REMESA_PRIVATE_FILE_LOST_H
struct drm_device;struct drm_file;
/* Internal post-unplug orphaning, never a release/retirement receipt.
 * Stream bridge holds stream_lock while calling the trusted helper; it drops
 * that lock before endpoint_lock -> memory lock. No hardware callback runs. */
void remesa_private_stream_file_lost(struct drm_device *,struct drm_file *);
void remesa_private_trusted_file_lost(struct drm_device *,struct drm_file *);
void remesa_private_memory_file_lost(struct drm_device *,struct drm_file *);
#endif
