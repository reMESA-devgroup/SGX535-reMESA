/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_PRIVATE_PROBE_H
#define REMESA_PRIVATE_PROBE_H
struct drm_device;struct drm_file;
int remesa_private_probe_ioctl(struct drm_device *,void *,struct drm_file *);
#endif
