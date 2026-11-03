/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_MATRIX_SUBMIT_H
#define REMESA_MATRIX_SUBMIT_H
struct drm_device;struct drm_file;
int remesa_matrix_submit_ioctl(struct drm_device *,void *,struct drm_file *);
#endif
