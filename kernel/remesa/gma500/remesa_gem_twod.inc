/* SPDX-License-Identifier: GPL-2.0-only */
/* Include in the existing GEM memory service translation unit. */
#include "remesa_gem_twod.h"
#include "remesa_mmu_verify_access.h"
#ifdef __KERNEL__
#include <linux/sort.h>
#endif
static int remesa_twod_pfn_compare(const void *a,const void *b)
{
 u32 x=*(const u32 *)a,y=*(const u32 *)b;
 return (x>y)-(x<y);
}
int remesa_gem_twod_validate_locked(struct remesa_gem_memory *s,
 struct remesa_memory_ticket ticket,struct remesa_twod_copy_plan *out)
{
 struct retained_job *j;struct remesa_gem_twod_command command;
 struct remesa_twod_copy_plan plan;struct remesa_memory_binding uses[16];
 struct backing *source,*destination,*b;u32 *pfns;
 unsigned i,k,n=0,total=0,count;int error;
 if(!s||!out)return -EINVAL;
#ifndef REMESA_MEMORY_TEST
 lockdep_assert_held(&s->lock);
#endif
 j=ticket_job(s,ticket);if(!j)return -ENOENT;
 if(j->quarantined)return -EIO;
 if(j->state!=REMESA_JOB_PREPARED||j->cookie||j->attached||
    s->executor||s->stream_bound||s->ta_cache_domains)return -EBUSY;
 for(i=0;i<8;i++)if(s->jobs[i].live&&s->jobs[i].state!=REMESA_JOB_PREPARED)return -EBUSY;
 if(j->command_bytes!=sizeof(command))return -EINVAL;
 memcpy(&command,j->commands,sizeof(command));
 if(command.schema!=REMESA_GEM_TWOD_COPY_SCHEMA||command.reserved)return -EINVAL;
 count=command.copy.source.allocation_id==command.copy.destination.allocation_id?1:2;
 if(j->count!=count||command.copy.source.allocation_id!=j->ids[0]||
    command.copy.destination.allocation_id!=j->ids[count-1])return -EACCES;
 /* One existing PTE/hold authentication pass, no extra virtual-to-PFN walk. */
 for(i=0;i<count;i++){
  b=&s->bo[j->ids[i]&31];
  if(!b->live||b->generation!=j->ids[i]>>5||b->quarantine||!b->holds||
     b->lease||!b->pinned||!b->reserved||b->inserted!=b->gem.size/PAGE_SIZE)
   return job_fault(s,j,-EIO);
  error=psb_mmu_verify_cached_rw_pages(s->pd,b->va.start,b->pages,b->inserted);
  if(error)return job_fault(s,j,-EIO);
  uses[i]=(struct remesa_memory_binding){j->ids[i],b->va.start,b->gem.size};
 }
 source=&s->bo[j->ids[0]&31];destination=&s->bo[j->ids[count-1]&31];
 if(command.copy.source.gpu_address!=uses[0].va||command.copy.source.bytes!=uses[0].bytes||
    command.copy.destination.gpu_address!=uses[count-1].va||
    command.copy.destination.bytes!=uses[count-1].bytes)return -EACCES;
 if(!source->initialized)return -ENODATA;
 error=remesa_twod_copy_prepare(&command.copy,&plan);if(error)return -error;
 for(i=0;i<count;i++){
  b=i?destination:source;
  if(b->inserted>2u*MAX_BYTES/PAGE_SIZE-total)return -E2BIG;
  total+=b->inserted;
 }
 pfns=kvcalloc(total,sizeof(*pfns),GFP_KERNEL);if(!pfns)return -ENOMEM;
 for(i=0;i<count;i++){
  b=i?destination:source;
  for(k=0;k<b->inserted;k++){
   unsigned long pfn=page_to_pfn(b->pages[k]);
   if(pfn>0xfffffUL){error=-ERANGE;goto done;}
   pfns[n++]=(u32)pfn;
  }
 }
 /* Tests a distinct property: pinned-frame aliases, not the PTEs again.
  * Single-BO overlap contributes its pages once. No framebuffer scan/hash. */
 sort(pfns,n,sizeof(*pfns),remesa_twod_pfn_compare,NULL);
 for(i=1;i<n;i++)if(pfns[i-1]==pfns[i]){error=-EACCES;goto done;}
 *out=plan;error=0;
 done:kvfree(pfns);return error;
}
