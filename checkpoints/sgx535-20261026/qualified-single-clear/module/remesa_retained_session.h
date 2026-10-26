/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_RETAINED_SESSION_H
#define REMESA_RETAINED_SESSION_H
#include "remesa_gem_memory.h"
struct remesa_retained_session;
struct remesa_retained_snapshots;
int remesa_retained_session_snapshots(struct remesa_retained_session *,struct remesa_retained_snapshots *);
/* Qualified kernel profile validator, not userspace approval. It must check
 * final program/state images and relocations for this exact fixed workload.
 * Runs under endpoint mutex: no hardware actions or endpoint re-entry. */
typedef int (*remesa_retained_images)(void *,const struct remesa_memory_view *,
                                     unsigned,const void *,unsigned);
int remesa_retained_session_create(struct remesa_gem_memory *,struct remesa_memory_ticket,
 remesa_retained_images,void *,u32,u32,u32,struct remesa_retained_session **);
/* Detaches only never-issued or acknowledged sessions. The endpoint still
 * owns the job until cancellation/retirement. Failure never frees live state. */
int remesa_retained_session_detach(struct remesa_retained_session *);
#endif
