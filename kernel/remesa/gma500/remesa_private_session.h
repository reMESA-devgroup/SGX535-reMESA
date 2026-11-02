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
struct remesa_memory_ticket;
/* General scene pool write exclusion, independent of the retained11-role seal.
 * Seal the existing cold-claimed table/parameter of a PREPARED admission job,
 * cancel that job, then source_begin. Both IDs include generations. WRITE,
 * write-LEASE and user SYNC stay denied until idle source close/backing release.
 * READ retains ordinary job/lease exclusion. No clear, mapping, hash, cache
 * exemption or warm-reuse credential is created. Caller retains the outer DRM
 * guard; seal locks endpoint then memory, dpm_locked requires memory lock. */
int remesa_private_session_seal_dpm(struct remesa_gem_memory *,const void *,
 struct remesa_memory_ticket,u32 table_id,u32 parameter_id);
int remesa_private_session_dpm_locked(struct remesa_gem_memory *,struct remesa_memory_ticket);
/* Authenticate all five final input images/geometry under this PREPARED job's
 * holds before calling seal_inputs. The trusted pure callback permits only
 * typed PDS updates; CODE/geometry/other inputs become immutable to userspace.
 * No hashing is done here. Seal before source_begin, once per backing lease.
 * inputs_locked is a metadata reuse check with s->lock already held; it is
 * not image authentication and never admits a different input ID/generation. */
int remesa_private_session_seal_inputs(struct remesa_gem_memory *,const void *,
 struct remesa_memory_ticket,int (*)(u64,const void *,unsigned));
int remesa_private_session_inputs_locked(struct remesa_gem_memory *,
 struct remesa_memory_ticket);
/* Seal GPU-written roles5..10 under the SAME authenticated admission job,
 * after seal_inputs and before source_begin. User WRITE/write-LEASE/SYNC are
 * denied until idle acknowledged source close and backing release; READ and
 * INFO keep their existing job/CPU lease guards. This is exclusive ownership,
 * not initialization, visibility or permission to omit cold clears/flushes.
 * Subsequent prepared jobs validate these six IDs/generations too. */
int remesa_private_session_seal_work(struct remesa_gem_memory *,const void *,
 struct remesa_memory_ticket);
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
struct remesa_gem_scene_job;
/* Same file/backing/source lease, separately validated general scene job.
 * Does not reinterpret the retained eleven-role seals or ABI. Caller retains
 * DRM entry, authenticates final inputs and prepares a fresh actual scene job.
 * Success keeps source ownership; caller still releases/retire the job.
 * Any entered executor error poisons the source, quarantines the ticket and
 * retains HELD job/arguments. No rearm, cold-clear exemption or new ioctl. */
int remesa_private_session_scene_execute(struct remesa_gem_memory *,const void *,
 struct remesa_gem_scene_job *,struct sgx535_gma500_fixed_backend *,
 struct remesa_stream_observation *,unsigned,unsigned);
#endif
