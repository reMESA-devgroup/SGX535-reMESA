/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_GEM_PRIVATE_H
#define REMESA_GEM_PRIVATE_H
#include "remesa_gem_memory.h"
#include "remesa_mmu_private.h"
struct remesa_twod_reservation;
/* CPU_ONLY keeps the GEM/job hold but creates no GPU PTE; cached must be0.
 * READ/WRITE/RW retain the existing MMU access bits1/2/3. */
#define REMESA_GEM_PRIVATE_CPU_ONLY 0u
struct remesa_gem_private_rule {u32 id,access,cached;};
/* Trusted exclusive kernel storage; zero initialize, retain at a stable address
 * until release succeeds. No userspace pointers or registration. The caller
 * supplies complete already-validated job resource closure and device exclusion
 * required by private_prepare. Session retains device/module/power; this loan
 * additionally retains its prepared job, mappings and pages. The supplied
 * reservation must already be held by this device. Its guard token prevents
 * producer-reservation release/issue until the MMU owner is released. This
 * does NOT exclude default MMU writers or grant root publication. */
struct remesa_gem_private {
 struct remesa_gem_memory *memory;
 struct remesa_memory_ticket ticket;
 struct remesa_mmu_private *owner;
 struct remesa_twod_reservation *guard;
 struct remesa_gem_private_rule rules[16];
 bool live,host_published,execution_attempted,execution_started,execution_retired;
};
/* Exact job IDs/order required; rules narrow private PTE access/cache policy.
 * CPU-only sidecars stay retained outside the private GPU allowlist. At least
 * one GPU-visible binding is required; all-CPU jobs return ENODATA.
 * No PFN, VA, page pointer or extra resource is accepted from the caller.
 * Claims the supplied idle/unissued GPU reservation before MMU preparation.
 * Builds only an UNBOUND PD using actual held GEM pages. Does not flush data,
 * bind a root, register an executor, submit, wait, or generate retirement.
 * Ordinary failures leave the loan zero and job prepared/unattached. An
 * exceptional prepare failure that retains an owner preserves the live loan
 * and quarantines its backing; release may continue to refuse that owner.
 * Caller must retain a live loan even when prepare returns an error. */
int remesa_gem_private_prepare(struct remesa_gem_memory *,
 struct remesa_memory_ticket,const struct remesa_gem_private_rule *,unsigned,
 struct remesa_twod_reservation *,struct remesa_gem_private *);
/* Release the unbound private tree, unclaim its guard, then detach the loan. The original
 * job retains its original holds: PREPARED for cancellation, or ACKNOWLEDGED
 * after successful execution/restore/source close for explicit job retirement.
 * A consumed execution attempt without that final receipt refuses release.
 * Failure retains attachment and every GEM hold. */
int remesa_gem_private_release(struct remesa_gem_private *);
/* Pre-issue resource/translation check for the exact retained loan and job
 * views. Caller already holds endpoint mutex THEN MMU write semaphore.
 * No default-PD getter/PTE walk, allocation, data hashing or lock acquisition.
 * CPU views, when present, must be the existing job-owned cached mappings.
 * Publication/commands and completion remain the qualified executor's job. */
int remesa_gem_private_validate_locked(struct remesa_gem_private *,
 const struct remesa_memory_view *,unsigned count);
/* Final CPU upload publication, after private bind and before any issue.
 * Same locks/exclusion as validate_locked. Caller has finished ALL writes
 * through the borrowed kernel views and must exclude further CPU access until
 * GPU retirement/CPU ownership transfer. No alias is unmapped or freed here.
 * Flushes GPU-visible backing only, including destinations whose old CPU cache
 * lines must not survive GPU writes; CPU_ONLY sidecars are untouched. One
 * publication per loan: repetition returns EALREADY rather than flushing again.
 * The same-call executor's authenticated possible-issue bookkeeping may already
 * be ISSUED with its exact cookie; arbitrary ISSUED jobs are still refused.
 * This is a cold-upload path, not a cache-epoch/reuse protocol. No prior MEM_SYNC
 * receipt is inferred; streaming reuse still needs its own ownership protocol.
 * Does NOT publish commands, issue work or authorize visibility/retirement. */
int remesa_gem_private_publish_locked(struct remesa_gem_private *,
 const struct remesa_memory_view *,unsigned count);
struct sgx535_gma500_owner;
struct sgx535_gma500_fixed_backend;
struct remesa_stream_observation;
/* Trusted kernel-only cold job. Caller already owns the existing stream source
 * lease (source_owner), has independently validated final images/commands and
 * initialized a borrowed scene, and retains all arguments at stable addresses.
 * Acquires endpoint mutex THEN one MMU writer interval for the complete job.
 * Uses existing fixed service/source attribution, no hashes or pixel readback.
 * A successful source bind consumes this attempt. Failure thereafter retains
 * the loan, source observation, backend and all resources; no retry/reset/free.
 * Caller must retain them even if this call fails. No live ioctl/admission is
 * added; LAB qualification cannot be inferred from this nohash path. */
int remesa_gem_private_execute(struct remesa_gem_private *,
 struct sgx535_gma500_owner *,struct sgx535_gma500_fixed_backend *,
 struct remesa_stream_observation *,const void *source_owner,
 const struct remesa_memory_view *,unsigned count,unsigned ticks,unsigned samples);
#endif
