/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual general job transaction; source lease remains caller-owned. */
#ifndef REMESA_SCENE_EXECUTION_TEST
#include "gma500_fixed_backend.h"
#include "remesa_stream_source.h"
#include <linux/jiffies.h>
#endif
int remesa_gem_scene_job_execute(struct remesa_gem_scene_job *c,
 struct sgx535_gma500_fixed_backend *b,struct remesa_stream_observation *observation,
 const void *source_owner,unsigned ticks,unsigned samples)
{
 struct remesa_gem_memory *s;struct remesa_gem_private *loan;struct retained_job *j;
 unsigned i;int error;
 if(!c||!c->memory||!b||!observation||!source_owner||!ticks||ticks>60*HZ||
    !samples||samples>300000)return -EINVAL;
 s=c->memory;loan=&c->loan;
 mutex_lock(&s->lock);
 if(c->state!=REMESA_SCENE_JOB_PREPARED||!loan->live||!loan->owner||
    loan->execution_attempted||loan->execution_started||loan->execution_retired||loan->host_published||
    c->service.session||c->service.started||b->active||b->owner||b->general_scene||b->private_loan||
    !loan->ticket.operation||loan->ticket.operation>0xffffffffULL){error=-EBUSY;goto unlock;}
 error=remesa_gem_scene_job_check_snapshot_locked(c);if(error)goto unlock;
 j=ticket_job(s,c->snapshot.ticket);if(!j){error=-ENOENT;goto unlock;}
 error=remesa_stream_source_bind(source_owner,observation,b,loan->ticket.operation,loan->ticket.operation);
 if(error)goto unlock;
 loan->execution_attempted=true;
 down_write(&s->pd->driver->sem);
 error=remesa_mmu_private_bind_locked(loan->owner);if(error)goto quarantine;
 error=remesa_gem_scene_job_bind_service_locked(c);if(error)goto quarantine;
 error=remesa_scene_backend_begin(b,c);if(error)goto quarantine;
 /* Possible execution is recorded in the real retained job BEFORE any
  * publication callback; only this exact loan may publish an ISSUED job. */
 loan->execution_started=true;j->state=REMESA_JOB_ISSUED;j->cookie=loan->ticket.operation;
 error=remesa_scene_service_run_once(&c->service,&remesa_scene_backend_ops,
  &sgx535_gma500_fixed_backend_status_source,b,0x10201u,(u32)loan->ticket.operation,ticks,samples);
 if(error)goto quarantine;
 if(c->session.sequence!=loan->ticket.operation||c->session.phase!=SGX535_PHASE_RETIRED||
    c->session.observed_events!=REMESA_JOB_LIFECYCLE_ALL||!remesa_scene_service_may_release(&c->service)){
  error=-EPROTO;goto quarantine;
 }
 j->state=REMESA_JOB_COMPLETE;
 for(i=0;i<c->count;i++)if(loan->rules[i].access&REMESA_MMU_GPU_WRITE){
  struct backing *backing=&s->bo[j->ids[i]&31];drm_clflush_pages(backing->pages,backing->inserted);
 }
 dma_rmb();j->state=REMESA_JOB_VISIBLE;
 error=remesa_stream_source_finish_nohash(source_owner,observation);if(error)goto quarantine;
 error=sgx535_gma500_fixed_backend_end(b);if(error)goto quarantine;
 error=remesa_mmu_private_restore_locked(loan->owner);if(error)goto quarantine;
 error=remesa_stream_source_unbind(source_owner,observation);if(error)goto quarantine;
 error=remesa_stream_source_settled(source_owner);if(error)goto quarantine;
 if(c->service.observer_context==b){c->service.observer_context=NULL;c->service.observer=NULL;}
 j->state=REMESA_JOB_ACKNOWLEDGED;loan->execution_retired=true;c->state=REMESA_SCENE_JOB_ACKNOWLEDGED;
 up_write(&s->pd->driver->sem);error=0;goto unlock;
 quarantine:
 /* Even a prefire error after source bind consumes the attempt. No fabricated
  * completion, source closure, root restoration, retry or automatic release. */
 c->state=REMESA_SCENE_JOB_HELD;(void)remesa_mmu_private_quarantine_locked(loan->owner);
 error=job_fault(s,j,error?error:-EIO);up_write(&s->pd->driver->sem);
 unlock:mutex_unlock(&s->lock);return error;
}
