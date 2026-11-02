/* SPDX-License-Identifier: GPL-2.0-only */
/* Same endpoint TU. Pool exclusion only: no warm admission or cache shortcut. */
#include "remesa_private_session.h"
static int private_dpm_job_locked(struct remesa_gem_memory *s,
 struct remesa_memory_ticket t,u32 table,u32 parameter,bool cold)
{
 struct retained_job *j;struct backing *b;u32 ids[2]={table,parameter},mask=0;
 unsigned role,i;
 if(!table||!parameter||table==parameter)return -EACCES;
 j=ticket_job(s,t);
 if(!j||j->state!=REMESA_JOB_PREPARED||j->quarantined||j->cookie)return -EBUSY;
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
 return j->dpm_mask==mask?0:-EACCES;
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
 return private_dpm_job_locked(s,t,s->private_dpm_ids[0],s->private_dpm_ids[1],false);
}
