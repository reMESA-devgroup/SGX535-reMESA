/* SPDX-License-Identifier: GPL-2.0-only */
/* Include after remesa_gem_twod.inc in the GEM service translation unit. */
#include "remesa_gem_twod_job.h"
#ifdef __KERNEL__
#include "psb_reg.h"
#if REMESA_TWOD_EVENT_COMPLETE != _PSB_CE_TWOD_COMPLETE || \
    REMESA_TWOD_FIFO_EMPTY != _PSB_C2_SOCIF_EMPTY || \
    REMESA_TWOD_BLIT_BUSY != _PSB_C2B_STATUS_BUSY
#error "2D completion constants disagree with native gma500 registers"
#endif
#endif
/* Reserve the existing endpoint executor slot without teaching the legacy
 * 3D entry points to accept a 2D receipt. Accidental cross-engine calls fail
 * closed, never dereference NULL callbacks or manufacture lifecycle bits. */
static int twod_block_validate(void *c,const void *p,unsigned bytes,
 const struct remesa_memory_binding *b,unsigned n)
{(void)c;(void)p;(void)bytes;(void)b;(void)n;return -EOPNOTSUPP;}
static int twod_block_start(void *c,struct remesa_memory_ticket t,const void *p,
 unsigned bytes,u64 *cookie)
{(void)c;(void)t;(void)p;(void)bytes;(void)cookie;return -EOPNOTSUPP;}
static int twod_block_poll(void *c,struct remesa_memory_ticket t,u64 cookie,
 unsigned budget,struct remesa_job_completion *out)
{(void)c;(void)t;(void)cookie;(void)budget;(void)out;return -EOPNOTSUPP;}
static int twod_block_finish(void *c,struct remesa_memory_ticket t,u64 cookie)
{(void)c;(void)t;(void)cookie;return -EOPNOTSUPP;}
static const struct remesa_job_executor twod_reserved_executor={
 .validate=twod_block_validate,.start=twod_block_start,.poll=twod_block_poll,
 .visible=twod_block_finish,.acknowledge=twod_block_finish
};
static int twod_ticket_equal(struct remesa_memory_ticket a,struct remesa_memory_ticket b)
{return a.context==b.context&&a.operation==b.operation&&a.slot==b.slot&&a.generation==b.generation;}
static struct retained_job *twod_owned_job(struct remesa_twod_job *r)
{
 struct remesa_gem_memory *s=r->memory;
 struct retained_job *j=ticket_job(s,r->launch.ticket);
 if(!j||!j->attached||s->executor!=&twod_reserved_executor||s->executor_context!=r)
  return NULL;
 return j;
}
static int twod_check_bindings(struct remesa_gem_memory *s,struct retained_job *j)
{
 unsigned i;int error;
 for(i=0;i<j->count;i++){
  struct backing *b=&s->bo[j->ids[i]&31];
  if(!b->live||b->generation!=j->ids[i]>>5||b->quarantine||!b->holds||
     b->lease||!b->pinned||!b->reserved||b->inserted!=b->gem.size/PAGE_SIZE)
   return job_fault(s,j,-EIO);
  error=psb_mmu_verify_cached_rw_pages(s->pd,b->va.start,b->pages,b->inserted);
  if(error)return job_fault(s,j,-EIO);
 }
 return 0;
}
int remesa_gem_twod_issue(struct remesa_gem_memory *s,struct remesa_memory_ticket t,
 const struct remesa_twod_native_ops *ops,void *context,struct remesa_twod_job *r)
{
 struct remesa_twod_job next={0};struct retained_job *j;unsigned i;int error;
 if(!s||!ops||!ops->start||!ops->poll||!ops->visible||!ops->retire||!context||!r)
  return -EINVAL;
 if(r->live)return -EBUSY;
 mutex_lock(&s->lock);
 error=remesa_gem_twod_validate_locked(s,t,&next.launch.plan);if(error)goto done;
 j=ticket_job(s,t);
 next.memory=s;next.ops=ops;next.context=context;next.live=true;
 next.launch.dev=s->dev;next.launch.pd=s->pd;next.launch.ticket=t;
 next.launch.count=j->count;
 for(i=0;i<j->count;i++){
  struct backing *b=&s->bo[j->ids[i]&31];
  next.launch.views[i].binding=(struct remesa_memory_binding){j->ids[i],b->va.start,b->gem.size};
  next.launch.views[i].pages=b->pages;next.launch.views[i].page_count=b->inserted;
  /* No CPU view allocation, clearing, hashing or readback. Flush destination
   * too: dirty CPU cache lines must not overwrite later GPU writes. */
  drm_clflush_pages(b->pages,b->inserted);
 }
 dma_wmb();
 *r=next;r->launch.retirement=&r->retirement;
 j->attached=true;s->executor=&twod_reserved_executor;s->executor_context=r;
 /* The following callback may emit commands even when it returns an error. */
 j->state=REMESA_JOB_ISSUED;
 error=ops->start(context,&r->launch,&r->cookie);j->cookie=r->cookie;
 if(error||!r->cookie)error=job_fault(s,j,error?error:-EPROTO);
 done:mutex_unlock(&s->lock);return error;
}
int remesa_gem_twod_wait(struct remesa_twod_job *r,unsigned budget)
{
 struct remesa_gem_memory *s;struct retained_job *j;
 struct remesa_twod_completion c={0};int error=-ENOENT;
 if(!r||!r->live||!r->memory||!budget||budget>10000)return -EINVAL;
 s=r->memory;mutex_lock(&s->lock);j=twod_owned_job(r);if(!j)goto done;
 if(j->quarantined){error=-EIO;goto done;}
 if(j->state!=REMESA_JOB_ISSUED){error=-EBUSY;goto done;}
 error=r->ops->poll(r->context,&r->launch,r->cookie,budget,&c);
 if(error){error=job_fault(s,j,error);goto done;}
 if(!twod_ticket_equal(c.ticket,r->launch.ticket)||c.cookie!=j->cookie||
    c.cookie!=r->cookie||c.event!=REMESA_TWOD_EVENT_COMPLETE||c.status2||
    c.fifo_free!=REMESA_TWOD_FIFO_EMPTY||(c.blit_status&REMESA_TWOD_BLIT_BUSY)){
  error=job_fault(s,j,-EPROTO);goto done;
 }
 j->state=REMESA_JOB_COMPLETE;
 done:mutex_unlock(&s->lock);return error;
}
int remesa_gem_twod_publish(struct remesa_twod_job *r)
{
 struct remesa_gem_memory *s;struct retained_job *j;struct backing *b;int error=-ENOENT;
 if(!r||!r->live||!r->memory)return -EINVAL;
 s=r->memory;mutex_lock(&s->lock);j=twod_owned_job(r);if(!j)goto done;
 if(j->quarantined){error=-EIO;goto done;}
 if(j->state!=REMESA_JOB_COMPLETE){error=-EBUSY;goto done;}
 error=twod_check_bindings(s,j);if(error)goto done;
 error=r->ops->visible(r->context,&r->launch,r->cookie);
 if(error){error=job_fault(s,j,error);goto done;}
 b=&s->bo[j->ids[j->count-1]&31];
#ifdef REMESA_TWOD_DEFER_CPU_INVALIDATE
 /* Device visibility has completed above. CPU access is still excluded by
  * job holds. Invalidate before the next CPU lease/view or page release,
  * rather than after every GPU-only operation. Prefire flush stays mandatory. */
 if(READ_ONCE(twod_defer_cpu_invalidation))b->twod_cpu_invalidate_pending=true;
 else{
  drm_clflush_pages(b->pages,b->inserted);
  b->twod_cpu_invalidate_pending=false;
 }
 dma_rmb();
#else
 drm_clflush_pages(b->pages,b->inserted);dma_rmb();
#endif
 j->state=REMESA_JOB_VISIBLE;
 done:mutex_unlock(&s->lock);return error;
}
int remesa_gem_twod_retire(struct remesa_twod_job *r)
{
 struct remesa_gem_memory *s;struct retained_job *j;unsigned i;int error=-ENOENT;
 struct remesa_twod_retirement proof={0};
 if(!r||!r->live||!r->memory)return -EINVAL;
 s=r->memory;mutex_lock(&s->lock);j=twod_owned_job(r);if(!j)goto done;
 if(j->quarantined){error=-EIO;goto done;}
 if(j->state!=REMESA_JOB_VISIBLE){error=-EBUSY;goto done;}
 error=twod_check_bindings(s,j);if(error)goto done;
 error=r->ops->retire(r->context,&r->launch,r->cookie);
 if(error){r->retirement.gem_retired=false;error=job_fault(s,j,error);goto done;}
 /* Optional native output is evidence only if bound to this exact held job.
  * Old adapters without a native record produce no new certificate. */
 if(r->retirement.native_retired){
  proof=r->retirement;
  if(proof.gem_retired||proof.dev!=s->dev||proof.pd!=s->pd||
     !twod_ticket_equal(proof.ticket,r->launch.ticket)||proof.cookie!=r->cookie||
     proof.destination_id!=r->launch.plan.destination_id||
     proof.destination_first!=r->launch.plan.destination_first||
     proof.destination_end!=r->launch.plan.destination_end||
     proof.request_base!=r->launch.plan.request_base||proof.packet_dwords!=r->launch.plan.dwords||
     !proof.default_root||(proof.default_root&4095U)||
     proof.core!=0x01130000U||proof.revision!=0x10201U||
     proof.status1!=REMESA_TWOD_EVENT_COMPLETE||proof.status2){
   r->retirement.gem_retired=false;error=job_fault(s,j,-EPROTO);goto done;
  }
 }
 j->state=REMESA_JOB_ACKNOWLEDGED;j->attached=false;
 s->executor=NULL;s->executor_context=NULL;
 for(i=0;i<j->count;i++)s->bo[j->ids[i]&31].holds--;
 drop_job_views(j);kfree(j->commands);j->commands=NULL;j->live=false;
 memset(r,0,sizeof(*r));
 if(proof.native_retired){proof.gem_retired=true;r->retirement=proof;}
 done:mutex_unlock(&s->lock);return error;
}
