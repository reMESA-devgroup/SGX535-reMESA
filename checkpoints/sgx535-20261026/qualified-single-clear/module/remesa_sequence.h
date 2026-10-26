/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_SEQUENCE_H
#define REMESA_SEQUENCE_H
#ifndef REMESA_MEMORY_TEST
#include <linux/mutex.h>
#endif
#include "remesa_gem_memory.h"
#define REMESA_SEQUENCE_DEPTH 8u
/* Kernel-only, externally owned context. No ioctl or executor installation.
 * A fence authenticates ownership/state; hardware authenticity remains the
 * GEM executor's responsibility. Initialize once per endpoint; the owner must
 * not call raw GEM job APIs in parallel or close the endpoint while attached.
 * Caller must keep this object and the endpoint alive on EBUSY.
 */
enum remesa_sequence_state {
 REMESA_SEQUENCE_FREE, REMESA_SEQUENCE_PREPARED, REMESA_SEQUENCE_ISSUED,
 REMESA_SEQUENCE_COMPLETE, REMESA_SEQUENCE_VISIBLE,
 REMESA_SEQUENCE_ACKNOWLEDGED, REMESA_SEQUENCE_RETIRED,
 REMESA_SEQUENCE_CANCELLED, REMESA_SEQUENCE_QUARANTINED
};
struct remesa_sequence_fence {
 struct remesa_memory_ticket ticket;
 u64 serial;
 u32 slot,generation;
};
struct remesa_sequence_record {
 struct remesa_sequence_fence fence;
 enum remesa_sequence_state state;
 int error;
};
struct remesa_sequence {
 struct mutex lock;
 struct remesa_gem_memory *memory;
 struct remesa_sequence_record records[REMESA_SEQUENCE_DEPTH];
 u64 serial;
 bool closing,failed;
};
int remesa_sequence_init(struct remesa_sequence *,struct remesa_gem_memory *);
int remesa_sequence_prepare(struct remesa_sequence *,const struct remesa_memory_binding *,
                            unsigned,const void *,unsigned,struct remesa_sequence_fence *);
int remesa_sequence_issue(struct remesa_sequence *,struct remesa_sequence_fence);
/* Kernel-only synchronous orchestration, not a new ioctl or execution mode.
 * Caller must run this in one task/syscall where observer policy requires it,
 * retaining the outer response and source epoch on errors. Possible issuance
 * failure never triggers cancellation, detach, or a GPU retry. The optional
 * detach callback runs after authenticated visibility/acknowledgement, under
 * the sequence mutex; it may access the endpoint but must not re-enter this
 * sequence. Failed detach retains the job and blocks new admission. The caller
 * may finish safe teardown/retirement, but may not execute that fence again.
 * Production executor admission remains independently disabled. */
int remesa_sequence_execute(struct remesa_sequence *,struct remesa_sequence_fence,
 unsigned,int (*)(void *,struct remesa_memory_ticket),void *);
int remesa_sequence_wait(struct remesa_sequence *,struct remesa_sequence_fence,unsigned);
int remesa_sequence_publish(struct remesa_sequence *,struct remesa_sequence_fence);
int remesa_sequence_acknowledge(struct remesa_sequence *,struct remesa_sequence_fence);
int remesa_sequence_retire(struct remesa_sequence *,struct remesa_sequence_fence);
int remesa_sequence_cancel(struct remesa_sequence *,struct remesa_sequence_fence);
int remesa_sequence_query(struct remesa_sequence *,struct remesa_sequence_fence,
                          enum remesa_sequence_state *,int *);
/* Cancels never-issued work only. Pending/uncertain hardware retains ownership
 * and returns EBUSY. Does not close GEM endpoint or implement hardware reset. */
int remesa_sequence_close(struct remesa_sequence *);
#endif
