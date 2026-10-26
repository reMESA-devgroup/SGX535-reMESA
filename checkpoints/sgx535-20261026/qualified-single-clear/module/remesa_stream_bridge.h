/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_STREAM_BRIDGE_H
#define REMESA_STREAM_BRIDGE_H
#include "stream_lifecycle.h"
struct drm_device; struct drm_file;
/* Internal module installation only. Absent trusted backend => ENOTSUPP. */
int remesa_stream_backend_install(const struct remesa_stream_ops *,void *);
int remesa_stream_ioctl(struct drm_device *,void *,struct drm_file *);
void remesa_stream_postclose(struct drm_device *,struct drm_file *);
#endif
