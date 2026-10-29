/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_GEM_PRIVATE_H
#define REMESA_GEM_PRIVATE_H
#include "remesa_gem_memory.h"
#include "remesa_mmu_private.h"
struct remesa_gem_private_rule {u32 id,access,cached;};
/* Trusted exclusive kernel storage; zero initialize, retain at a stable address
 * until release succeeds. No userspace pointers or registration. The caller
 * supplies complete already-validated job resource closure and device exclusion
 * required by private_prepare. Session retains device/module/power; this loan
 * additionally retains its prepared job, mappings and pages. It does NOT claim
 * whole-device/MMU-writer exclusion or grant root publication. */
struct remesa_gem_private {
 struct remesa_gem_memory *memory;
 struct remesa_memory_ticket ticket;
 struct remesa_mmu_private *owner;
 bool live;
};
/* Exact job IDs/order required; rules narrow private PTE access/cache policy.
 * No PFN, VA, page pointer or extra resource is accepted from the caller.
 * Builds only an UNBOUND PD using actual held GEM pages. Does not flush data,
 * bind a root, register an executor, submit, wait, or generate retirement.
 * Ordinary failures leave the loan zero and job prepared/unattached. An
 * exceptional prepare failure that retains an owner preserves the live loan
 * and quarantines its backing; release may continue to refuse that owner.
 * Caller must retain a live loan even when prepare returns an error. */
int remesa_gem_private_prepare(struct remesa_gem_memory *,
 struct remesa_memory_ticket,const struct remesa_gem_private_rule *,unsigned,
 struct remesa_gem_private *);
/* Release only the unbound private tree and then detach the loan. The original
 * job stays prepared with all original holds, ready for explicit cancellation
 * or a qualified executor. Failure retains attachment and every GEM hold. */
int remesa_gem_private_release(struct remesa_gem_private *);
#endif
