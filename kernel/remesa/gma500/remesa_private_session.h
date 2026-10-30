/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_PRIVATE_SESSION_H
#define REMESA_PRIVATE_SESSION_H
struct drm_device;
struct drm_file;
struct remesa_gem_memory;
/* Kernel-only backing lifetime, not an execution or source credential. The
 * owner token must have a stable address until release succeeds. Acquisition
 * freezes the existing endpoint topology even between jobs. CPU leases remain
 * explicit and jobs retain their usual holds. The endpoint already retains
 * its device/module/power references. No new ioctl or parameter is installed.
 * Caller must separately retain/settle/release its source lease and execution
 * state BEFORE releasing this backing lease; no completion is inferred here.
 * Caller holds its outer DRM entry guard and serializes kernel users of the
 * returned pointer; do not dereference it after successful release. An orphaned
 * lease retains backing but never permits hardware access. File/device loss
 * makes the lease unreleasable until reboot. */
int remesa_private_session_acquire(struct drm_device *,struct drm_file *,u64,
 const void *owner,struct remesa_gem_memory **);
int remesa_private_session_release(struct remesa_gem_memory *,const void *owner);
#endif
