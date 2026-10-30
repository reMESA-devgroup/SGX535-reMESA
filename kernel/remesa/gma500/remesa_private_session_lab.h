/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_PRIVATE_SESSION_LAB_H
#define REMESA_PRIVATE_SESSION_LAB_H
struct drm_device;struct drm_file;
int remesa_private_session_lab_ioctl(struct drm_device *,void *,struct drm_file *);
#endif
