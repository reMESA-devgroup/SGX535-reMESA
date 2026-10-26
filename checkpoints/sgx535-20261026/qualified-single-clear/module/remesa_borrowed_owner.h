/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_BORROWED_OWNER_H
#define REMESA_BORROWED_OWNER_H
#include "remesa_gem_memory.h"
struct sgx535_gma500_owner;
struct sgx535_frozen_command_pairs;
/* This binding owns only bookkeeping, never pages, GEM, VA nodes, power,
 * exclusion or module/device references. Keep endpoint/job alive until deinit.
 * It does not authorize backend admission or repeated hardware execution. */
struct remesa_borrowed_owner {
 struct sgx535_gma500_owner *owner;
 struct remesa_memory_ticket ticket;
 bool live;
};
/* The verifier must attest the exact endpoint device, context/ticket, held
 * power/exclusion, MMU ceiling, immutable final images and PTE/GEM PFNs. It
 * must not perform hardware operations or reenter a locked endpoint API.
 * Commands are extracted from held CONTROL bytes and compared, not generated
 * here. Caller supplies final independently qualified images/relocations. */
typedef int (*remesa_borrowed_verify)(void *,struct drm_device *,
 struct remesa_memory_ticket,const struct remesa_memory_view *,unsigned,u64);
int remesa_borrowed_owner_init(struct remesa_borrowed_owner *,
 struct sgx535_gma500_owner *,struct drm_device *,struct remesa_memory_ticket,
 const struct remesa_memory_view *,unsigned,u64,
 const struct sgx535_frozen_command_pairs *,remesa_borrowed_verify,void *);
/* Refuses pending/uncertain work, active CPU aliases or attached observer.
 * Never owner_release: borrowed references and reservations belong to lender. */
int remesa_borrowed_owner_deinit(struct remesa_borrowed_owner *);
#endif
