/* SPDX-License-Identifier: GPL-2.0-only */
/* Include in GEM service translation unit; no ioctl/registration or MMIO. */
#include "remesa_gem_publish_inputs.h"
int remesa_gem_job_publish_inputs_locked(struct remesa_gem_memory *s,
 const struct remesa_memory_snapshot *snapshot,
 const struct remesa_memory_view *views,unsigned count)
{
 struct retained_job *j;unsigned i;int e;
 if(!s||!snapshot||!views)return -EINVAL;
#ifndef REMESA_MEMORY_TEST
 lockdep_assert_held(&s->lock);
#endif
 j=ticket_job(s,snapshot->ticket);if(!j)return -ENOENT;
 if(j->quarantined)return -EIO;
 if(j->state!=REMESA_JOB_PREPARED||j->cookie)return -EBUSY;
 /* CPU publication must not race an operation already possible on this
  * exclusive endpoint, including one whose start returned an error. */
 for(i=0;i<8;i++)if(s->jobs[i].live&&s->jobs[i].state!=REMESA_JOB_PREPARED)
  return -EBUSY;
 e=remesa_gem_job_check_snapshot_locked(s,snapshot,views,count);
 if(e)return e;
 /* All checks precede the first maintenance operation. In particular, a
  * late-role PTE mismatch cannot partially publish an earlier role. */
 for(i=0;i<count;i++)drm_clflush_pages(views[i].pages,views[i].page_count);
 dma_wmb();return 0;
}
