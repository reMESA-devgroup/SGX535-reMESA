/* SPDX-License-Identifier: GPL-2.0-only */
/* Include after the retained GEM memory service; no runtime ioctl added. */
#include "remesa_gem_private.h"
#include "remesa_mmu_verify_access.h"
#include "remesa_mmu_switch.h"
#include "remesa_twod_reservation.h"
static int private_block_validate(void *c,const void *p,unsigned n,
 const struct remesa_memory_binding *b,unsigned count)
{(void)c;(void)p;(void)n;(void)b;(void)count;return -EOPNOTSUPP;}
static int private_block_start(void *c,struct remesa_memory_ticket t,
 const void *p,unsigned n,u64 *cookie)
{(void)c;(void)t;(void)p;(void)n;(void)cookie;return -EOPNOTSUPP;}
static int private_block_poll(void *c,struct remesa_memory_ticket t,u64 cookie,
 unsigned budget,struct remesa_job_completion *out)
{(void)c;(void)t;(void)cookie;(void)budget;(void)out;return -EOPNOTSUPP;}
static int private_block_finish(void *c,struct remesa_memory_ticket t,u64 cookie)
{(void)c;(void)t;(void)cookie;return -EOPNOTSUPP;}
/* Reserve the existing attachment slot, without making any legacy issue path
 * accept this unbound-directory loan as an executable job. */
static const struct remesa_job_executor private_reserved_executor={
 .validate=private_block_validate,.start=private_block_start,.poll=private_block_poll,
 .visible=private_block_finish,.acknowledge=private_block_finish
};
int remesa_gem_private_prepare(struct remesa_gem_memory *s,
 struct remesa_memory_ticket ticket,const struct remesa_gem_private_rule *rules,
 unsigned count,struct remesa_twod_reservation *guard,struct remesa_gem_private *loan)
{
 struct remesa_mmu_allowed_binding bindings[16],swap;
 struct remesa_gem_private_rule saved_rules[16];
 struct retained_job *j;struct backing *b;unsigned i,k,gpu_count=0;int error=-ENOENT;
 if(!s||!rules||!guard||!loan||!count||count>16)return -EINVAL;
 if(loan->memory||loan->owner||loan->guard||loan->live||loan->ticket.context||
    loan->ticket.operation||loan->ticket.slot||loan->ticket.generation)return -EBUSY;
 mutex_lock(&s->lock);j=ticket_job(s,ticket);if(!j)goto done;
 if(j->quarantined){error=-EIO;goto done;}
 if(j->state!=REMESA_JOB_PREPARED||j->cookie||j->attached||
    s->executor||s->stream_bound||s->ta_cache_domains){error=-EBUSY;goto done;}
 for(i=0;i<8;i++)if(s->jobs[i].live&&s->jobs[i].state!=REMESA_JOB_PREPARED){error=-EBUSY;goto done;}
 if(j->count!=count){error=-EACCES;goto done;}
 if(!s->pd||!s->pd->driver){error=-ENODEV;goto done;}
 for(i=0;i<count;i++){
  if(rules[i].id!=j->ids[i]){error=-EACCES;goto done;}
  if(rules[i].access>3||rules[i].cached>1||(!rules[i].access&&rules[i].cached)){error=-EINVAL;goto done;}
  b=&s->bo[j->ids[i]&31];
  if(!b->live||b->generation!=j->ids[i]>>5||b->quarantine||!b->holds||
     b->lease||!b->pinned||!b->reserved||!b->pages||
     !b->gem.size||(b->gem.size&(PAGE_SIZE-1))||
     b->inserted!=b->gem.size/PAGE_SIZE){error=job_fault(s,j,-EIO);goto done;}
  if((rules[i].access&REMESA_MMU_GPU_READ)&&!b->initialized){error=-ENODATA;goto done;}
  /* Authenticate the real default mapping once during loan preparation.
   * The private auditor subsequently verifies exact narrowed permissions. */
  error=psb_mmu_verify_cached_rw_pages(s->pd,b->va.start,b->pages,b->inserted);
  if(error){error=job_fault(s,j,error);goto done;}
  if(rules[i].access)bindings[gpu_count++]=(struct remesa_mmu_allowed_binding){b->va.start,b->pages,
                 b->inserted,rules[i].access,rules[i].cached};
 }
 if(!gpu_count){error=-ENODATA;goto done;}
 memcpy(saved_rules,rules,count*sizeof(*rules));
 /* Job order remains unchanged; private auditor requires increasing VA. */
 for(i=1;i<gpu_count;i++)for(k=i;k&&bindings[k].address<bindings[k-1].address;k--){
  swap=bindings[k];bindings[k]=bindings[k-1];bindings[k-1]=swap;
 }
 *loan=(struct remesa_gem_private){.memory=s,.ticket=ticket,.live=true};
 memcpy(loan->rules,saved_rules,count*sizeof(*saved_rules));
 j->attached=true;s->executor=&private_reserved_executor;s->executor_context=loan;
 error=remesa_twod_mmu_guard_acquire(s->dev->dev_private,guard,loan);
 if(error)goto rollback;
 loan->guard=guard;
 error=remesa_mmu_private_prepare(s->pd->driver,bindings,gpu_count,&loan->owner);
 if(!error&&!loan->owner){error=job_fault(s,j,-EPROTO);goto done;}
 if(error){
  if(loan->owner){error=job_fault(s,j,error);goto done;}
  if(remesa_twod_mmu_guard_release(s->dev->dev_private,guard,loan)){
   error=job_fault(s,j,-EIO);goto done;
  }
  loan->guard=NULL;
  goto rollback;
 }
 goto done;
 rollback:j->attached=false;s->executor=NULL;s->executor_context=NULL;memset(loan,0,sizeof(*loan));
 done:mutex_unlock(&s->lock);return error;
}
int remesa_gem_private_release(struct remesa_gem_private *loan)
{
 struct remesa_gem_memory *s;struct retained_job *j;int error=-ENOENT;
 if(!loan||!loan->live||!loan->memory)return -EINVAL;
 s=loan->memory;mutex_lock(&s->lock);j=ticket_job(s,loan->ticket);
 if(!j||!j->attached||s->executor!=&private_reserved_executor||s->executor_context!=loan)
  goto done;
 /* No issue/retirement transition is defined for this preparation-only API. */
 if(j->state!=REMESA_JOB_PREPARED&&j->state!=REMESA_JOB_QUARANTINED){error=-EBUSY;goto done;}
 error=remesa_mmu_private_release(&loan->owner);if(error)goto done;
 if(!loan->guard){error=-EIO;goto done;}
 error=remesa_twod_mmu_guard_release(s->dev->dev_private,loan->guard,loan);
 if(error)goto done;
 j->attached=false;s->executor=NULL;s->executor_context=NULL;memset(loan,0,sizeof(*loan));
 done:mutex_unlock(&s->lock);return error;
}

int remesa_gem_private_validate_locked(struct remesa_gem_private *loan,
 const struct remesa_memory_view *views,unsigned count)
{
 struct remesa_gem_memory *s;struct retained_job *j;struct backing *b;
 struct remesa_mmu_allowed_binding expected[16],swap;unsigned i,k,gpu_count=0;
 if(!loan||!loan->live||!loan->memory||!loan->owner||!loan->guard||!views||
    !count||count>16)return -EINVAL;
 s=loan->memory;
 if(!s->pd||!s->pd->driver||!s->dev||!s->dev->dev_private)return -ENODEV;
#ifdef __KERNEL__
 lockdep_assert_held(&s->lock);
 lockdep_assert_held_write(&s->pd->driver->sem);
#endif
 j=ticket_job(s,loan->ticket);if(!j)return -ENOENT;
 if(j->quarantined)return -EIO;
 if(!j->attached||s->executor!=&private_reserved_executor||s->executor_context!=loan||
    (j->state!=REMESA_JOB_PREPARED&&j->state!=REMESA_JOB_ISSUED))return -EBUSY;
 if(j->count!=count)return -EACCES;
 if(!loan->guard->held||loan->guard->device!=s->dev->dev_private||
    loan->guard->mmu_owner!=loan)return -EACCES;
 for(i=0;i<count;i++){
  const struct remesa_memory_view *v=&views[i];
  if(v->binding.id!=j->ids[i]||loan->rules[i].id!=j->ids[i])return -EACCES;
  b=&s->bo[j->ids[i]&31];
  if(!b->live||b->generation!=j->ids[i]>>5||!b->holds||b->quarantine||b->lease||
     !b->pinned||!b->reserved||!b->pages||b->inserted!=b->gem.size/PAGE_SIZE)
   return job_fault(s,j,-EIO);
  if(v->binding.va!=b->va.start||v->binding.bytes!=b->gem.size||
     v->pages!=b->pages||v->page_count!=b->inserted)return -EACCES;
  if(v->cpu&&(v->cpu!=j->cpu_views[i]||v->cpu!=b->cached_job_view))return -EACCES;
  if(loan->rules[i].access)expected[gpu_count++]=(struct remesa_mmu_allowed_binding){b->va.start,b->pages,b->inserted,
                   loan->rules[i].access,loan->rules[i].cached};
 }
 if(!gpu_count)return -ENODATA;
 for(i=1;i<gpu_count;i++)for(k=i;k&&expected[k].address<expected[k-1].address;k--){
  swap=expected[k];expected[k]=expected[k-1];expected[k-1]=swap;
 }
 return remesa_mmu_private_validate_bound_locked(loan->owner,expected,gpu_count);
}
