/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_GEM_TWOD_H
#define REMESA_GEM_TWOD_H
#include "remesa_gem_memory.h"
#include "remesa_twod.h"
#define REMESA_GEM_TWOD_COPY_SCHEMA 0x31443252u
#ifdef REMESA_TWOD_VENDOR_STREAM
#define REMESA_GEM_TWOD_VENDOR_SCHEMA 0x31443256u
#endif
/* Kernel job-command image, not a registered ioctl or public stable ABI.
 * Only the high-level copy is retained; GPU words are reconstructed in kernel.
 */
struct remesa_gem_twod_command {
 u32 schema,reserved;
 struct remesa_twod_copy copy;
};
/* Caller already holds memory->lock; output is exclusive trusted kernel
 * storage disjoint from the endpoint/command/backing metadata. Authenticates
 * the exact retained job, its complete bindings/PFNs, source/destination IDs,
 * VA/length and absence of physical aliases before producing FIFO words.
 * Same-allocation overlap owns one binding, not two duplicate job holds.
 * No MMIO/cache maintenance or state transition; output unchanged on failure.
 * Mapping-authentication failure uses the existing job quarantine contract.
 * This is preflight only: legacy completion mask7 describes 3D TA/render/free
 * and MUST NOT be fabricated for a future 2D-only executor.
 */
int remesa_gem_twod_validate_locked(struct remesa_gem_memory *,
 struct remesa_memory_ticket,struct remesa_twod_copy_plan *);
#endif
