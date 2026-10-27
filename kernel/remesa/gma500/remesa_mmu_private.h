/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_MMU_PRIVATE_H
#define REMESA_MMU_PRIVATE_H
#include "remesa_mmu_confinement.h"
struct psb_mmu_driver;
struct remesa_mmu_private;
/* Trusted kernel callers only. Caller serializes all inputs/device activity
 * and keeps driver/default PD alive until release. *out must initially be NULL.
 * Copies bindings/page arrays and holds every data page; builds a new unbound
 * trap-on-invalid PD with exact permissions, then runs the complete auditor.
 * No directory is published/bound and no job or GPU permission is created.
 * On ordinary failure *out remains NULL. If an impossible/externally bound PD
 * prevents rollback, returns -EUCLEAN and preserves the owning handle in *out:
 * caller must retain it, not drop page references or assume cleanup happened.
 * No API here exposes or binds the owned directory.
 */
int remesa_mmu_private_prepare(struct psb_mmu_driver *,
 const struct remesa_mmu_allowed_binding *,u32,struct remesa_mmu_private **out);
/* Idempotent for *owner==NULL. Caller still holds device exclusion. Refuses a
 * bound PD with -EBUSY and retains the entire owner. Page refs are released
 * only AFTER the original unbound PD destructor has destroyed all tables.
 * This is not retirement/default-restoration proof for a future bound job.
 */
int remesa_mmu_private_release(struct remesa_mmu_private **owner);
#endif
