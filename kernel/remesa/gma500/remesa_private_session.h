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
/* Same backing lease, one monotonic source lease across distinct jobs. Caller
 * holds the outer DRM entry and keeps the token stable. No source rearm. */
int remesa_private_session_source_begin(struct remesa_gem_memory *,const void *);
int remesa_private_session_source_end(struct remesa_gem_memory *,const void *);
struct remesa_gem_private;
struct sgx535_gma500_owner;
struct sgx535_gma500_fixed_backend;
struct remesa_stream_observation;
struct remesa_memory_view;
/* Trusted kernel-only execution. Caller authenticates complete final inputs,
 * commands and permissions, prepares the loan/borrowed scene, and supplies a
 * fresh observation. Arguments survive any failure. This wraps the qualified
 * cold executor, keeping source ownership between successful jobs. No hashes,
 * readback or new user admission; not the cache-reuse/FAST implementation.
 * Any executor error poisons this session, with no retry or release. */
int remesa_private_session_execute(struct remesa_gem_memory *,const void *,
 struct remesa_gem_private *,struct sgx535_gma500_owner *,
 struct sgx535_gma500_fixed_backend *,struct remesa_stream_observation *,
 const struct remesa_memory_view *,unsigned,unsigned,unsigned);
#endif
