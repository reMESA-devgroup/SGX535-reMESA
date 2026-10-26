/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_GEM_VISIBLE_READBACK_H
#define REMESA_GEM_VISIBLE_READBACK_H
/* Include after remesa_gem_memory.h. Kernel-only, chunk <=4096, caller-owned
 * output disjoint from service/GEM storage. Requires VISIBLE/ACKNOWLEDGED job;
 * all job holds and mappings remain installed. Not a userspace API. */
int remesa_gem_job_copy_visible(struct remesa_gem_memory *,
 struct remesa_memory_ticket,u32 id,u64 offset,u32 bytes,void *out);
#endif
