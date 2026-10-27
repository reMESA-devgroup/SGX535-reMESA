/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_MMU_CONFINEMENT_H
#define REMESA_MMU_CONFINEMENT_H
/* Kernel-only trusted, pinned inputs. Caller owns an UNBOUND private PD,
 * holds device exclusion/GPU idle and serializes its mappings/page arrays.
 * Caller must not hold driver->sem. Workspace is exclusive kernel storage,
 * disjoint from ALL inputs/pages/metadata and never GPU-mapped. It is reusable
 * and may be clobbered on failure. No allocation or table modification occurs.
 * This is not cache publication, context binding, a job hold or admission.
 * In particular NEVER call psb_mmu_set_pd_context(pd,-1) as an unbind helper:
 * the existing setter computes a hardware register offset even for negative IDs.
 */
#define REMESA_MMU_AUDIT_BINDINGS 32u
#define REMESA_MMU_GPU_READ 1u
#define REMESA_MMU_GPU_WRITE 2u
#define REMESA_MMU_AUDIT_OVERHEAD_WORDS (1024u+2u*(1024u+3u))
struct page;
struct psb_mmu_pd;
struct remesa_mmu_allowed_binding {
 u64 address;
 struct page **pages;
 u32 page_count,access,cached;
};
/* Allowlist must be VA-sorted/disjoint, access R/W/RW, cached0/1. Closed
 * 4KiB/32-bit profile: every valid leaf must match exactly, no physical aliases
 * (including private/default page-table frames), no nonzero invalid entries.
 * Requires default PD distinct from private, private hw_context=-1 and trap
 * invalid_pde/pte=0. Checks PFNs, permissions and the ENTIRE private tree.
 * Workspace words >= sum(page_count)+OVERHEAD; max sum=2^20 pages.
 * Empty allowlist can audit an empty tree, never authorize an empty job.
 * Failure does not change PD/PT/PTE, flags, context, TLB/cache or hardware.
 */
int psb_mmu_audit_unbound_pd(struct psb_mmu_pd *,
 const struct remesa_mmu_allowed_binding *,u32 count,u32 *workspace,u32 words);
#endif
