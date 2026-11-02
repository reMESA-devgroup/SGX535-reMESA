/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_SCENE_PROBE_H
#define REMESA_SCENE_PROBE_H
struct drm_device;struct drm_file;
int remesa_scene_probe_ioctl(struct drm_device *,void *,struct drm_file *);
int remesa_scene_trace_ioctl(struct drm_device *,void *,struct drm_file *);
int remesa_gears_scene_probe_ioctl(struct drm_device *,void *,struct drm_file *);
int remesa_gears_scene_trace_ioctl(struct drm_device *,void *,struct drm_file *);
int remesa_general_session_ioctl(struct drm_device *,void *,struct drm_file *);
#endif
