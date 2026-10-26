/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_DEPTH1280_CAPTURE_H
#define REMESA_DEPTH1280_CAPTURE_H
#include "remesa_gem_memory.h"
int remesa_depth1280_capture_visible(struct remesa_gem_memory *,struct remesa_memory_ticket);
int remesa_depth1280_capture_hashes(struct remesa_memory_ticket,u8 [32],u8 [32]);
int remesa_depth1280_snapshot_ioctl(struct drm_device *,void *,struct drm_file *);
#endif
