/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_CONSOLE_MAPPING_H
#define REMESA_CONSOLE_MAPPING_H
struct psb_mmu_pd;
/* Exact existing default-PD uncached RW stolen mapping. Caller authenticates
 * physical ownership and retains/serializes the PD and framebuffer backing.
 * CPU table observation only; no pin, lifetime loan, visibility or retirement.
 */
int psb_mmu_verify_stolen_rw_sequence(struct psb_mmu_pd *,u64,u64,u32);
#endif
