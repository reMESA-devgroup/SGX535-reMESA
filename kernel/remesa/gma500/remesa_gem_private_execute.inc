/* SPDX-License-Identifier: GPL-2.0-only */
/* Included after GEM/private loan; existing source lease remains caller-owned. */
#ifndef REMESA_GEM_PRIVATE_EXECUTION_TEST
#include "gma500_bo_owner.h"
#include "gma500_fixed_backend.h"
#include "remesa_stream_source.h"
#include "remesa_stream_contract_api.h"
#include <linux/jiffies.h>
#endif
int remesa_gem_private_execute(struct remesa_gem_private *loan,
 struct sgx535_gma500_owner *owner,struct sgx535_gma500_fixed_backend *backend,
 struct remesa_stream_observation *observation,const void *source_owner,
 const struct remesa_memory_view *views,unsigned count,unsigned ticks,unsigned samples)
{
 struct remesa_gem_memory *s;struct retained_job *j;
 struct remesa_memory_snapshot snapshot;unsigned i;int error;
 if(!loan||!loan->live||!loan->memory||!loan->owner||!owner||!backend||
    !observation||!source_owner||!views||count!=SGX535_BO_COUNT||!ticks||
    ticks>60*HZ||!samples||samples>300000||loan->ticket.operation>0xffffffffULL)
  return -EINVAL;
 s=loan->memory;mutex_lock(&s->lock);j=ticket_job(s,loan->ticket);
 if(!j){error=-ENOENT;goto unlock;}
 if(loan->execution_attempted||loan->host_published||j->state!=REMESA_JOB_PREPARED||
    j->cookie||j->quarantined||!j->attached||s->executor!=&private_reserved_executor||
    s->executor_context!=loan||backend->active||backend->private_loan){error=-EBUSY;goto unlock;}
 if(owner->dev!=s->dev||owner->mmu_end!=s->root.end+1||
    j->command_bytes!=sizeof(owner->commands)||
    memcmp(j->commands,&owner->commands,sizeof(owner->commands))){error=-EACCES;goto unlock;}
 /* Authenticate default mappings BEFORE the writer interval, never recurse
  * through a default-PD walker while the private root is bound. */
 snapshot=(struct remesa_memory_snapshot){s->dev,loan->ticket,s->root.end+1,
             j->commands,j->command_bytes,true,true};
 error=remesa_gem_job_check_snapshot_locked(s,&snapshot,views,count);if(error)goto unlock;
 error=remesa_stream_source_bind(source_owner,observation,backend,
                    loan->ticket.operation,loan->ticket.operation);if(error)goto unlock;
 loan->execution_attempted=true;
 down_write(&s->pd->driver->sem);
 error=remesa_mmu_private_bind_locked(loan->owner);if(error)goto quarantine;
 error=sgx535_gma500_fixed_backend_begin_private(backend,owner,loan,views,count);
 if(error)goto quarantine;
 /* Mirror GEM's possible-issue transition BEFORE the service callback. The
  * upload stage permits this state only for this authenticated retained loan. */
 loan->execution_started=true;j->state=REMESA_JOB_ISSUED;j->cookie=loan->ticket.operation;
 error=remesa_stream_sgx535_fixed_service_run_once(&owner->fixed_service,
   &sgx535_gma500_fixed_backend_ops,&sgx535_gma500_fixed_backend_status_source,
   backend,0x00010201u,(u32)loan->ticket.operation,ticks,samples);
 if(error)goto quarantine;
 if(owner->session.sequence!=loan->ticket.operation||
    owner->session.phase!=SGX535_PHASE_RETIRED||
    owner->session.observed_events!=REMESA_JOB_LIFECYCLE_ALL||
    !sgx535_fixed_service_may_release(&owner->fixed_service)){
  error=-EPROTO;goto quarantine;
 }
 j->state=REMESA_JOB_COMPLETE;
 /* Cold-job CPU ownership transfer only. Read-only input and CPU-only
  * backing cannot have GPU writes under the narrowed private permissions. */
 for(i=0;i<count;i++)if(loan->rules[i].access&REMESA_MMU_GPU_WRITE){
  struct backing *b=&s->bo[j->ids[i]&31];drm_clflush_pages(b->pages,b->inserted);
 }
 dma_rmb();j->state=REMESA_JOB_VISIBLE;
 error=remesa_stream_source_finish_nohash(source_owner,observation);if(error)goto quarantine;
 error=sgx535_gma500_fixed_backend_end(backend);if(error)goto quarantine;
 error=remesa_mmu_private_restore_locked(loan->owner);if(error)goto quarantine;
 error=remesa_stream_source_unbind(source_owner,observation);if(error)goto quarantine;
 if(owner->fixed_service.observer_context==backend){
  owner->fixed_service.observer_context=NULL;owner->fixed_service.observer=NULL;
 }
 j->state=REMESA_JOB_ACKNOWLEDGED;loan->execution_retired=true;
 up_write(&s->pd->driver->sem);error=0;goto unlock;
 quarantine:
 /* Even prefire failures after source bind retain this operation. Never
  * synthesize source close, retirement, a reset or a successful root restore. */
 (void)remesa_mmu_private_quarantine_locked(loan->owner);
 error=job_fault(s,j,error?error:-EIO);
 up_write(&s->pd->driver->sem);
 unlock:mutex_unlock(&s->lock);return error;
}
