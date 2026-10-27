/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_MMU_DEFAULT_ROOT_H
#define REMESA_MMU_DEFAULT_ROOT_H
struct drm_device;
struct psb_mmu_pd;
/* Read-only CPU authentication, not hardware binding or an execution grant.
 * Caller retains device/PD and freezes bindings. Output unchanged on failure.
 */
int psb_mmu_default_root(struct drm_device *,struct psb_mmu_pd *,u32 *);
#endif
