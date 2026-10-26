/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_GEM_PUBLISH_INPUTS_H
#define REMESA_GEM_PUBLISH_INPUTS_H
#include "remesa_gem_memory.h"
/* Kernel-only CPU cache publication before issue. Caller holds the endpoint
 * serialization AND memory mutex; never re-enters the endpoint from callbacks.
 * Snapshot/views must cover the complete held PREPARED job. No external CPU
 * alias may mutate images after validation/publication. This flushes every
 * object's pages and orders CPU writes, but does not flush device caches/TLB,
 * install an executor, advance job state on success, authorize execution or prove
 * completion. Existing PTE faults still quarantine and retain all job holds.
 * Backend source isolation and device maintenance remain separate contracts. */
int remesa_gem_job_publish_inputs_locked(struct remesa_gem_memory *,
 const struct remesa_memory_snapshot *,const struct remesa_memory_view *,unsigned);
#endif
