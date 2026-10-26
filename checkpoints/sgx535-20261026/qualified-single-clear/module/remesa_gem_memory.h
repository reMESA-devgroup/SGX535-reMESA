/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_GEM_MEMORY_H
#define REMESA_GEM_MEMORY_H
#include "remesa/memory_uapi.h"
struct drm_device;
struct drm_file;
struct page;
struct file;
struct vm_area_struct;
int remesa_gem_color_mmap(struct file *,struct vm_area_struct *);
struct remesa_gem_memory;
/* Integration must establish first-owner/source isolation before open; open
 * takes the SAME device exclusion as the known-good scene owner. The caller
 * serializes open/close and never dereferences after successful close.
 * No ioctl registration, submission or completion injection is provided. */
int remesa_gem_memory_open(struct drm_device *,struct drm_file *,u64,
                           struct remesa_gem_memory **);
int remesa_gem_memory_request(struct remesa_gem_memory *,struct drm_file *,
                              struct remesa_memory_request *);
int remesa_gem_memory_close(struct remesa_gem_memory *);
/* Kernel-only command ownership. No user ioctl exposes these entry points.
 * Commands are copied. Hardware validation belongs to a kernel-only executor.
 * There is no installed production executor or completion-injection ioctl. */
struct remesa_memory_binding {u32 id;u64 va,bytes;};
struct remesa_memory_ticket {u64 context,operation;u32 slot,generation;};
/* Borrowed kernel views: valid while this job owns its holds. Not mmap/ABI.
 * Consumer must serialize endpoint/job teardown and finish image writes before
 * dispatch validation; never mutate published GPU images while issued. */
struct remesa_memory_view {
 struct remesa_memory_binding binding;
 struct page **pages;
 unsigned page_count;
 void *cpu;
};
/* Kernel authority snapshot, never a userspace credential. Lifetime follows
 * the held job. Snapshot/check serialize with endpoint teardown. */
struct remesa_memory_snapshot {
 struct drm_device *dev;
 struct remesa_memory_ticket ticket;
 u64 mmu_end;
 const void *commands;
 unsigned command_bytes;
 bool exclusive_held,power_held;
};
int remesa_gem_job_snapshot(struct remesa_gem_memory *,struct remesa_memory_ticket,
                            struct remesa_memory_snapshot *,struct remesa_memory_view *,unsigned);
int remesa_gem_job_check_snapshot(struct remesa_gem_memory *,
 const struct remesa_memory_snapshot *,const struct remesa_memory_view *,unsigned);
/* Only for executor callbacks ALREADY holding the endpoint mutex. */
int remesa_gem_job_check_snapshot_locked(struct remesa_gem_memory *,
 const struct remesa_memory_snapshot *,const struct remesa_memory_view *,unsigned);
int remesa_gem_job_views(struct remesa_gem_memory *,struct remesa_memory_ticket,
                         struct remesa_memory_view *,unsigned);
int remesa_gem_job_prepare(struct remesa_gem_memory *,const struct remesa_memory_binding *,
                           unsigned,const void *,unsigned,struct remesa_memory_ticket *);
int remesa_gem_job_cancel(struct remesa_gem_memory *,struct remesa_memory_ticket);
int remesa_gem_job_quarantine(struct remesa_gem_memory *,struct remesa_memory_ticket);
/* Kernel-internal executor contract, NOT an ABI. All callbacks are bounded,
 * serialized under the endpoint mutex and must not re-enter this service.
 * validate cannot perform device actions. start may issue even when returning
 * an error. poll must associate/consume actual exclusive IRQ events, not infer
 * completion from ioctl success or an unowned register snapshot. visible must
 * finish device cache/TLB synchronization; host clflush alone is insufficient.
 * acknowledge must establish safe device retirement before backing is released.
 * No production registration method exists; host tests inject an executor only.
 */
enum remesa_job_state {
 REMESA_JOB_PREPARED, REMESA_JOB_ISSUED, REMESA_JOB_COMPLETE,
 REMESA_JOB_VISIBLE, REMESA_JOB_ACKNOWLEDGED, REMESA_JOB_QUARANTINED
};
#define REMESA_JOB_LIFECYCLE_ALL 7u /* TA, end-render, 3D memory free */
struct remesa_job_completion {
 struct remesa_memory_ticket ticket;
 u64 cookie;
 u32 lifecycle;
};
struct remesa_job_executor {
 int (*validate)(void *,const void *,unsigned,
                 const struct remesa_memory_binding *,unsigned);
 int (*start)(void *,struct remesa_memory_ticket,const void *,unsigned,u64 *);
 int (*poll)(void *,struct remesa_memory_ticket,u64,unsigned,
             struct remesa_job_completion *);
 int (*visible)(void *,struct remesa_memory_ticket,u64);
 int (*acknowledge)(void *,struct remesa_memory_ticket,u64);
};
/* Always EOPNOTSUPP in production until a separately qualified adapter exists. */
int remesa_gem_job_issue(struct remesa_gem_memory *,struct remesa_memory_ticket);
int remesa_gem_job_wait(struct remesa_gem_memory *,struct remesa_memory_ticket,unsigned);
int remesa_gem_job_publish(struct remesa_gem_memory *,struct remesa_memory_ticket);
int remesa_gem_job_acknowledge(struct remesa_gem_memory *,struct remesa_memory_ticket);
int remesa_gem_job_retire(struct remesa_gem_memory *,struct remesa_memory_ticket);
int remesa_gem_job_state(struct remesa_gem_memory *,struct remesa_memory_ticket,
                         enum remesa_job_state *);
/* Offline integration hooks. Registration requires explicit separate review. */
int remesa_retained_submit_ioctl(struct drm_device *,void *,struct drm_file *);
int remesa_gem_memory_ioctl(struct drm_device *,void *,struct drm_file *);
void remesa_gem_memory_postclose(struct drm_device *,struct drm_file *);
/* Kernel-only lease and executor installation; no userspace executor pointer. */
int remesa_gem_stream_claim(struct drm_device *,struct drm_file *,u64,struct remesa_gem_memory **);
/* OPEN8-only exact TA domain policy. No ioctl/VA/PFN or generic export ABI. */
int remesa_gem_stream_enable_ta_domains(struct remesa_gem_memory *);
/* Before first3 kernel TA clears, or reuse policy after3; caller is unlocked. */
int remesa_gem_job_ta_policy(struct remesa_gem_memory *,struct remesa_memory_ticket,bool);
/* Only executor start, whose endpoint mutex is already held. */
int remesa_gem_job_prefire_cache_locked(struct remesa_gem_memory *,struct remesa_memory_ticket);
int remesa_gem_stream_seal_ta_pools(struct remesa_gem_memory *);
/* Called outside endpoint lock after one full authentication under job holds. */
int remesa_gem_stream_seal_inputs(struct remesa_gem_memory *,struct remesa_memory_ticket,
 int (*)(u64,const void *,unsigned));
int remesa_gem_stream_release(struct remesa_gem_memory *);
int remesa_gem_stream_attach(struct remesa_gem_memory *,struct remesa_memory_ticket,const struct remesa_job_executor *,void *);
int remesa_gem_stream_detach(struct remesa_gem_memory *,struct remesa_memory_ticket,void *);
#endif
