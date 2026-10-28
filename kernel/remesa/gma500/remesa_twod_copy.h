/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_TWOD_COPY_KERNEL_H
#define REMESA_TWOD_COPY_KERNEL_H
struct drm_device;struct drm_file;
int remesa_twod_copy_ioctl(struct drm_device *,void *,struct drm_file *);
#endif
