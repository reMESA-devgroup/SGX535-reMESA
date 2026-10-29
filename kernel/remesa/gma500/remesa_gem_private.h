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
 bool live,host_published;
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
 * job stays prepared with all original holds, ready for explicit cancellation
 * or a qualified executor. Failure retains attachment and every GEM hold. */
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
 * This is a cold-upload path, not a cache-epoch/reuse protocol. No prior MEM_SYNC
 * receipt is inferred; streaming reuse still needs its own ownership protocol.
 * Does NOT publish commands, issue work or authorize visibility/retirement. */
int remesa_gem_private_publish_locked(struct remesa_gem_private *,
 const struct remesa_memory_view *,unsigned count);
#endif
