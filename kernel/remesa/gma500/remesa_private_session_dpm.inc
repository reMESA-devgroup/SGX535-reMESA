/* SPDX-License-Identifier: GPL-2.0-only */
/* Same endpoint TU. Distinct cold origin, pool exclusion and acknowledged
 * general-session warm epoch; never infer retirement from cache residency. */
#include "remesa_private_session.h"
static int private_dpm_objects_locked(struct remesa_gem_memory *s,
 struct retained_job *j,u32 table,u32 parameter,bool cold,u32 *out_mask)
{
 struct backing *b;u32 ids[2]={table,parameter},mask=0;
 unsigned role,i;
 if(!table||!parameter||table==parameter)return -EACCES;
 if(!j||j->quarantined)return -EBUSY;
 for(role=0;role<2;role++){
  for(i=0;i<j->count&&j->ids[i]!=ids[role];i++);
  if(i==j->count)return -EACCES;
  b=&s->bo[ids[role]&31];
  if(!b->live||b->generation!=ids[role]>>5||b->holds!=1||b->lease||b->quarantine||
     !b->pinned||!b->reserved||!b->pages||b->inserted!=8192||b->gem.size!=33554432ULL||
     b->cached_job_view||j->cpu_views[i]||atomic_read(&b->color_vmas)||b->input_seal)
   return -EACCES;
  if(!b->initialized||(cold&&!b->zero_pristine))return -ENODATA;
  if(!role){
   if(b->va.start<0x40000000ULL||b->va.start>0x100000000ULL-b->gem.size||(b->va.start&4095))return -ERANGE;
  }else if(b->va.start<0x30000000ULL||b->va.start>0x40000000ULL-b->gem.size||
           (b->va.start&0xfffff))return -ERANGE;
  mask|=(u32)1<<i;
 }
 *out_mask=mask;return 0;
}
static int private_dpm_job_locked(struct remesa_gem_memory *s,
 struct remesa_memory_ticket t,u32 table,u32 parameter,bool cold)
{
 struct retained_job *j=ticket_job(s,t);u32 mask;int error;
 if(!j||j->state!=REMESA_JOB_PREPARED||j->cookie)return -EBUSY;
 error=private_dpm_objects_locked(s,j,table,parameter,cold,&mask);
 return error?error:j->dpm_mask==mask?0:-EACCES;
}
int remesa_private_session_seal_dpm(struct remesa_gem_memory *s,const void *owner,
 struct remesa_memory_ticket t,u32 table,u32 parameter)
{
 unsigned i;int error=-EACCES;
 if(!s||!owner)return -EINVAL;
 mutex_lock(&endpoint_lock);if(endpoint!=s)goto done;
 mutex_lock(&s->lock);
 if(s->private_session!=owner)goto unlock;
 if(s->private_device_lost||!s->file){error=-ENODEV;goto unlock;}
 if(s->private_source_poisoned){error=-EIO;goto unlock;}
 if(s->private_source_held||s->stream_bound||s->ta_cache_domains||
    s->private_dpm_ids[0]||s->private_dpm_ids[1]||s->private_work_owned||s->pds_write_guard){error=-EBUSY;goto unlock;}
 for(i=0;i<5;i++)if(s->private_input_ids[i]){error=-EBUSY;goto unlock;}
 for(i=0;i<6;i++)if(s->private_work_ids[i]){error=-EBUSY;goto unlock;}
 error=private_dpm_job_locked(s,t,table,parameter,true);if(error)goto unlock;
 /* Both validated under the same holds before either exclusion is published.
  * Cancel the admission job before source_begin's existing idle check. */
 s->private_dpm_ids[0]=table;s->private_dpm_ids[1]=parameter;
 s->private_dpm_retired_operation=0;s->private_dpm_ready=false;
 unlock:mutex_unlock(&s->lock);
 done:mutex_unlock(&endpoint_lock);return error;
}
int remesa_private_session_dpm_locked(struct remesa_gem_memory *s,
 struct remesa_memory_ticket t)
{
 if(!s||!s->private_session)return -EACCES;
#ifdef __KERNEL__
 lockdep_assert_held(&s->lock);
#endif
 if(s->private_device_lost||!s->file)return -ENODEV;
 if(s->private_source_poisoned)return -EIO;
 {
  struct retained_job *j=ticket_job(s,t);
  int error=private_dpm_job_locked(s,t,s->private_dpm_ids[0],s->private_dpm_ids[1],false);
  if(!error&&j->dpm_previous_operation)error=remesa_private_session_warm_check_locked(s,t);
  return error;
 }
}
/* Caller holds memory lock and the same outer endpoint/source lease. */
static int private_dpm_source_locked(struct remesa_gem_memory *s,const void *owner)
{
 if(!owner||s->private_session!=owner||!s->private_source_held)return -EACCES;
 if(s->private_device_lost||!s->file)return -ENODEV;
 if(s->private_source_poisoned)return -EIO;
 if(!s->private_dpm_ids[0]||!s->private_dpm_ids[1])return -EACCES;
 return 0;
}
int remesa_private_session_warm_check_locked(struct remesa_gem_memory *s,
 struct remesa_memory_ticket t)
{
 struct retained_job *j=ticket_job(s,t);u32 mask;int error;
 if(!j||!j->dpm_previous_operation||
    (j->state!=REMESA_JOB_PREPARED&&j->state!=REMESA_JOB_ISSUED))return -EACCES;
 error=private_dpm_source_locked(s,j->private_scene_owner);if(error)return error;
 if(j->state==REMESA_JOB_PREPARED&&!s->private_dpm_ready)return -ENODATA;
 if(j->dpm_previous_operation!=s->private_dpm_retired_operation||
    j->dpm_previous_operation>=j->operation)return -ESTALE;
 error=private_dpm_objects_locked(s,j,s->private_dpm_ids[0],s->private_dpm_ids[1],false,&mask);
 return error?error:j->dpm_mask==mask?0:-EACCES;
}
int remesa_private_session_claim_warm_dpm(struct remesa_gem_memory *s,const void *owner,
 struct remesa_memory_ticket t,u32 table,u32 parameter)
{
 struct retained_job *j;u32 mask;unsigned i;int error;
 if(!s||!owner)return -EINVAL;
 mutex_lock(&s->lock);
 error=private_dpm_source_locked(s,owner);if(error)goto done;
 j=ticket_job(s,t);
 if(!j||j->state!=REMESA_JOB_PREPARED||j->cookie||j->attached||j->quarantined||
    j->dpm_mask||j->dpm_previous_operation||j->private_scene_owner!=owner){error=-EBUSY;goto done;}
 if(table!=s->private_dpm_ids[0]||parameter!=s->private_dpm_ids[1]){error=-EACCES;goto done;}
 if(!s->private_dpm_ready||!s->private_dpm_retired_operation){error=-ENODATA;goto done;}
 if(s->private_dpm_retired_operation>=j->operation){error=-ESTALE;goto done;}
 for(i=0;i<8;i++)if(s->jobs[i].live&& &s->jobs[i]!=j){error=-EBUSY;goto done;}
 error=private_dpm_objects_locked(s,j,table,parameter,false,&mask);if(error)goto done;
 if(s->bo[table&31].zero_pristine||s->bo[parameter&31].zero_pristine){error=-ENODATA;goto done;}
 j->dpm_mask=mask;j->dpm_previous_operation=s->private_dpm_retired_operation;error=0;
 done:mutex_unlock(&s->lock);return error;
}
int remesa_private_session_dpm_retired_locked(struct remesa_gem_memory *s,const void *owner,
 struct remesa_memory_ticket t)
{
 struct retained_job *j=ticket_job(s,t);u32 mask;int error;
 error=private_dpm_source_locked(s,owner);if(error)return error;
 if(!j||j->state!=REMESA_JOB_ACKNOWLEDGED||j->cookie!=j->operation||!j->attached)return -EACCES;
 /* Existing cold callers do not become warm-authority producers merely
  * because they use the same source adapter and an ownership seal. */
 if(!j->private_scene_owner)return 0;
 if(j->private_scene_owner!=owner||s->private_dpm_ready||
    j->operation<=s->private_dpm_retired_operation)return -EACCES;
 if(j->dpm_previous_operation&&j->dpm_previous_operation!=s->private_dpm_retired_operation)return -ESTALE;
 error=private_dpm_objects_locked(s,j,s->private_dpm_ids[0],s->private_dpm_ids[1],false,&mask);if(error)return error;
 if(j->dpm_mask!=mask)return -EACCES;
 s->private_dpm_retired_operation=j->operation;s->private_dpm_ready=true;return 0;
}
