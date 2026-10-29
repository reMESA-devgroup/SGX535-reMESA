/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_PRIVATE_GEM_LIFETIME_H
#define REMESA_PRIVATE_GEM_LIFETIME_H
#include <linux/types.h>
struct drm_device;struct drm_gem_object;
/* Standard GTT-backed GEM only. Private memory/legacy owner objects have
 * different containers and retain their separate lifetime protocols. */
void remesa_private_gem_track(struct drm_gem_object *);
bool remesa_private_gem_device_retained(struct drm_device *);
bool remesa_private_gem_rebind_blocked(void);
#endif
