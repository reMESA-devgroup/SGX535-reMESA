/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_GEM_VISIBLE_IOCTL_H
#define REMESA_GEM_VISIBLE_IOCTL_H
struct drm_device;
struct drm_file;
/* Deliberately NOT installed in the active DRM ioctl table. Include the
 * implementation after the GEM endpoint and held-readback implementation. */
int remesa_gem_visible_readback_ioctl(struct drm_device *,void *,struct drm_file *);
#endif
