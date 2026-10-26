/* SPDX-License-Identifier: GPL-2.0-only */
/* Compiled in the GEM service translation unit. No ioctl or executor
 * registration. Kernel caller supplies disjoint output storage and serializes
 * its own consumer. A successful read is not independent evidence of hardware
 * execution when the executor is injected. Existing publication/retirement
 * authority, PTE checks, holds and quarantine remain mandatory. */
#include "remesa_gem_visible_readback.h"
int remesa_gem_job_copy_visible(struct remesa_gem_memory *s,
 struct remesa_memory_ticket ticket,u32 id,u64 offset,u32 bytes,void *out)
{
 struct retained_job *j;struct remesa_memory_binding uses[16];
 struct backing *b;void *mapping;unsigned i;int e=-ENOENT;
 if(!s||!out||!bytes||bytes>REMESA_MEMORY_CHUNK)return -EINVAL;
 mutex_lock(&s->lock);j=ticket_job(s,ticket);if(!j)goto done;
 if(j->quarantined){e=-EIO;goto done;}
 if(j->state!=REMESA_JOB_VISIBLE&&j->state!=REMESA_JOB_ACKNOWLEDGED){e=-EBUSY;goto done;}
 for(i=0;i<j->count;i++)if(j->ids[i]==id)break;
 if(i==j->count){e=-ENOENT;goto done;}
 b=&s->bo[id&31];
 if(offset>b->gem.size||bytes>b->gem.size-offset){e=-EINVAL;goto done;}
 e=verify_job_bindings(s,j,uses);if(e){e=job_fault(s,j,e);goto done;}
 /* The complete BO remains pinned and held, even between chunks. The lock
  * excludes teardown/state changes and every CPU-write ioctl rejects holds. */
 mapping=j->cpu_views[i];
 if(!mapping) {
  mapping=vmap(b->pages,b->gem.size/PAGE_SIZE,VM_MAP,PAGE_KERNEL);
  if(!mapping){e=-ENOMEM;goto done;}
 }
 memcpy(out,(u8 *)mapping+(size_t)offset,bytes);
 if(!j->cpu_views[i])vunmap(mapping);
 e=0;
done:mutex_unlock(&s->lock);return e;
}
