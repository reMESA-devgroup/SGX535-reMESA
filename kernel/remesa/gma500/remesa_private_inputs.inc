/* SPDX-License-Identifier: GPL-2.0-only */
/* Same private endpoint TU; trusted caller authenticates images under holds. */
#include "remesa_private_session.h"
int remesa_private_session_seal_inputs(struct remesa_gem_memory *s,const void *owner,
 struct remesa_memory_ticket ticket,int (*guard)(u64,const void *,unsigned))
{
 struct retained_job *j;unsigned i,k;int error=-EACCES;
 if(!s||!owner||!guard)return -EINVAL;
 mutex_lock(&endpoint_lock);if(endpoint!=s)goto done;
 mutex_lock(&s->lock);
 error=private_session_owner(s,owner);if(error)goto unlock;
 if(s->stream_bound||s->executor||s->private_source_held||s->ta_cache_domains||
    s->pds_write_guard||s->private_input_ids[0]||s->private_dpm_ids[0]||s->private_dpm_ids[1]){error=-EBUSY;goto unlock;}
 j=ticket_job(s,ticket);
 if(!j||j->state!=REMESA_JOB_PREPARED||j->quarantined||j->cookie||j->attached||j->count!=11){
  error=-EBUSY;goto unlock;
 }
 for(i=0;i<5;i++){
  unsigned id=j->ids[i];struct backing *b=&s->bo[id&31];
  if(!id||!b->live||b->generation!=id>>5||b->holds!=1||b->lease||
     !b->pinned||!b->reserved||!b->pages||!b->gem.size||
     b->inserted!=b->gem.size/PAGE_SIZE||b->quarantine||b->input_seal){error=-EBUSY;goto unlock;}
  for(k=0;k<i;k++)if(j->ids[k]==id){error=-EACCES;goto unlock;}
 }
 for(i=0;i<5;i++){
  s->private_input_ids[i]=j->ids[i];s->bo[j->ids[i]&31].input_seal=i?2:1;
 }
 s->pds_write_guard=guard;error=0;
 unlock:mutex_unlock(&s->lock);
 done:mutex_unlock(&endpoint_lock);return error;
}
int remesa_private_session_inputs_locked(struct remesa_gem_memory *s,
 struct remesa_memory_ticket ticket)
{
 struct retained_job *j;unsigned i;
 if(!s)return -EINVAL;
#ifdef __KERNEL__
 lockdep_assert_held(&s->lock);
#endif
 if(!s->private_session||s->private_device_lost||!s->file||s->private_source_poisoned||
    !s->private_input_ids[0]||!s->pds_write_guard)return -EACCES;
 j=ticket_job(s,ticket);
 if(!j||j->state!=REMESA_JOB_PREPARED||j->quarantined||j->cookie||j->count!=11)return -EBUSY;
 for(i=0;i<5;i++){
  unsigned id=j->ids[i];struct backing *b=&s->bo[id&31];
  if(id!=s->private_input_ids[i]||!b->live||b->generation!=id>>5||
     b->input_seal!=(i?2u:1u)||!b->holds||b->lease||b->quarantine||
     !b->pinned||!b->reserved||!b->pages||b->inserted!=b->gem.size/PAGE_SIZE)return -EACCES;
 }
 if(s->private_work_owned)for(i=0;i<6;i++){
  unsigned id=j->ids[i+5];struct backing *b=&s->bo[id&31];
  if(!id||id!=s->private_work_ids[i]||!b->live||b->generation!=id>>5||
     b->input_seal||!b->holds||b->lease||b->quarantine||!b->pinned||
     !b->reserved||!b->pages||b->inserted!=b->gem.size/PAGE_SIZE)return -EACCES;
 }
 return 0;
}
int remesa_private_session_seal_work(struct remesa_gem_memory *s,const void *owner,
 struct remesa_memory_ticket ticket)
{
 struct retained_job *j;unsigned i,k;int error=-EACCES;
 if(!s||!owner)return -EINVAL;
 mutex_lock(&endpoint_lock);if(endpoint!=s)goto done;
 mutex_lock(&s->lock);
 error=private_session_owner(s,owner);if(error)goto unlock;
 if(s->stream_bound||s->executor||s->private_source_held||s->ta_cache_domains||
    s->ta_pools_sealed||s->private_work_owned){error=-EBUSY;goto unlock;}
 for(i=0;i<6;i++)if(s->private_work_ids[i]){error=-EBUSY;goto unlock;}
 error=remesa_private_session_inputs_locked(s,ticket);if(error)goto unlock;
 j=ticket_job(s,ticket);
 if(j->attached){error=-EBUSY;goto unlock;}
 for(i=5;i<11;i++){
  unsigned id=j->ids[i];struct backing *b=&s->bo[id&31];
  if(!id||!b->live||b->generation!=id>>5||b->holds!=1||b->lease||
     !b->pinned||!b->reserved||!b->pages||!b->gem.size||
     b->inserted!=b->gem.size/PAGE_SIZE||b->quarantine||b->input_seal||
     atomic_read(&b->color_vmas)){error=-EBUSY;goto unlock;}
  for(k=0;k<i;k++)if(j->ids[k]==id){error=-EACCES;goto unlock;}
 }
 for(i=0;i<6;i++)s->private_work_ids[i]=j->ids[i+5];
 s->private_work_owned=true;error=0;
 unlock:mutex_unlock(&s->lock);
 done:mutex_unlock(&endpoint_lock);return error;
}
