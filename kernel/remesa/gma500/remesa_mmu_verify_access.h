/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_MMU_VERIFY_ACCESS_H
#define REMESA_MMU_VERIFY_ACCESS_H
struct psb_mmu_pd;
struct page;
/* Exact existing GEM mapping profile: VALID+CACHED, readable and writable.
 * Checks real PDE/PT ownership and every complete PTE, not PFN alone.
 * Read-only; caller pins/serializes pages. No cache/TLB/MMIO or allocation.
 */
int psb_mmu_verify_cached_rw_pages(struct psb_mmu_pd *,u64,
 struct page **,u32);
#endif
