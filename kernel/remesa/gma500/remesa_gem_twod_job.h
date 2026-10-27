/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_GEM_TWOD_JOB_H
#define REMESA_GEM_TWOD_JOB_H
#include "remesa_gem_twod.h"
#define REMESA_TWOD_EVENT_COMPLETE (1u << 27)
#define REMESA_TWOD_FIFO_EMPTY 0x80u
#define REMESA_TWOD_BLIT_BUSY (1u << 24)
struct psb_mmu_pd;
/* Trusted kernel views, held throughout this operation. Not a user ABI. */
struct remesa_twod_launch {
 struct drm_device *dev;
 struct psb_mmu_pd *pd;
 struct remesa_memory_ticket ticket;
 struct remesa_twod_copy_plan plan;
 struct remesa_memory_view views[2];
 unsigned count;
};
struct remesa_twod_completion {
 struct remesa_memory_ticket ticket;
 u64 cookie;
 u32 event,fifo_free,blit_status,status2;
};
/* Kernel-only adapter. No registration or userspace function pointers.
 * All callbacks run under the endpoint mutex and must not reenter it.
 * start authenticates hardware/root/exclusive ownership and establishes a
 * stale-event boundary before emitting the supplied reconstructed packet.
 * start may issue even on failure; every failure retains/quarantines backing.
 * poll must capture actual owned status before IRQ acknowledgment and
 * associate it with the exact launch/cookie. event excludes only the master
 * interrupt bit31; do not strip unexpected events or status2 faults. BIF
 * fault state must also be checked by the adapter. Idle alone is insufficient.
 * visible completes device-side coherency; CPU clflush is done by the service.
 * retire proves no further DMA, restores borrowed state and releases its IRQ
 * ownership. Failure MUST retain native ownership, including partial restore.
 * Neither FENCE opcode nor 3D lifecycle mask7 is a 2D completion receipt.
 */
struct remesa_twod_native_ops {
 int (*start)(void *,const struct remesa_twod_launch *,u64 *);
 int (*poll)(void *,const struct remesa_twod_launch *,u64,unsigned,
             struct remesa_twod_completion *);
 int (*visible)(void *,const struct remesa_twod_launch *,u64);
 int (*retire)(void *,const struct remesa_twod_launch *,u64);
};
/* Caller zero-initializes and retains this exclusive, disjoint kernel storage
 * plus immutable ops and adapter/context until successful retire. Do not free
 * on uncertainty.
 * memory and launch pointers are borrowed only while live. */
struct remesa_twod_job {
 struct remesa_gem_memory *memory;
 const struct remesa_twod_native_ops *ops;
 void *context;
 struct remesa_twod_launch launch;
 u64 cookie;
 bool live;
};
int remesa_gem_twod_issue(struct remesa_gem_memory *,struct remesa_memory_ticket,
 const struct remesa_twod_native_ops *,void *,struct remesa_twod_job *);
int remesa_gem_twod_wait(struct remesa_twod_job *,unsigned);
int remesa_gem_twod_publish(struct remesa_twod_job *);
int remesa_gem_twod_retire(struct remesa_twod_job *);
#endif
