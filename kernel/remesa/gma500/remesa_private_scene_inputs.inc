/* SPDX-License-Identifier: GPL-2.0-only */
/* Same memory TU. Reuse the existing input_seal==2 write exclusion. Neither
 * a content validator nor a GPU credential: trusted admission authenticates
 * bytes and derives the READ mask from the actual prepared private loan. */
#define REMESA_PRIVATE_SCENE_INPUT_SEALS 1
#include "remesa_private_session.h"
static int scene_input_objects_locked(struct remesa_gem_memory *s)
{
 unsigned i,k;u32 mask=s->private_scene_input_mask;
 if(!s->private_scene_binding_count){
  if(mask)return -EIO;
  for(i=0;i<32;i++)if(s->private_scene_input_ids[i])return -EIO;
  return 0;
 }
 if(s->private_scene_binding_count>32||!mask||
    (s->private_scene_binding_count<32&&(mask>>s->private_scene_binding_count)))return -EIO;
 for(i=0;i<32;i++){
  u32 id=s->private_scene_input_ids[i];struct backing *b;
  if(!(mask&((u32)1<<i))){if(id)return -EIO;continue;}
  if(!id)return -EIO;
  b=&s->bo[id&31];
  if(!b->live||b->generation!=id>>5||b->input_seal!=2||!b->initialized||
     !b->pinned||!b->reserved||!b->pages||!b->gem.size||
     b->inserted!=b->gem.size/PAGE_SIZE||b->lease||b->quarantine||
     atomic_read(&b->color_vmas))return -EACCES;
  for(k=0;k<i;k++)if(s->private_scene_input_ids[k]==id)return -EACCES;
 }
 return 0;
}
int remesa_private_session_seal_scene_inputs(struct remesa_gem_memory *s,
 const void *owner,struct remesa_memory_ticket t,u32 mask)
{
 struct retained_job *j;unsigned i,k;int error=-EACCES;
 if(!s||!owner||!mask)return -EINVAL;
 mutex_lock(&endpoint_lock);if(endpoint!=s)goto done;
 mutex_lock(&s->lock);
 if(s->private_session!=owner)goto unlock;
 if(s->private_device_lost||!s->file){error=-ENODEV;goto unlock;}
 if(s->private_source_poisoned){error=-EIO;goto unlock;}
 if(s->private_source_held||s->stream_bound||s->ta_cache_domains||
    s->private_work_owned||s->pds_write_guard||s->private_scene_binding_count){error=-EBUSY;goto unlock;}
 error=scene_input_objects_locked(s);if(error)goto unlock;
 for(i=0;i<5;i++)if(s->private_input_ids[i]){error=-EBUSY;goto unlock;}
 for(i=0;i<6;i++)if(s->private_work_ids[i]){error=-EBUSY;goto unlock;}
 error=remesa_private_session_dpm_locked(s,t);if(error)goto unlock;
 j=ticket_job(s,t);
 if(!j||j->state!=REMESA_JOB_PREPARED||j->cookie||j->quarantined||
    !j->count||j->count>32||(j->count<32&&(mask>>j->count))||
    (mask&(j->dpm_mask|j->region_mask))){error=-EACCES;goto unlock;}
 for(i=0;i<8;i++)if(s->jobs[i].live&& &s->jobs[i]!=j){error=-EBUSY;goto unlock;}
 for(i=0;i<j->count;i++)if(mask&((u32)1<<i)){
  u32 id=j->ids[i];struct backing *b=&s->bo[id&31];
  if(!id||!b->live||b->generation!=id>>5||b->holds!=1||b->lease||b->quarantine||
     b->input_seal||!b->initialized||!b->pinned||!b->reserved||!b->pages||
     !b->gem.size||b->inserted!=b->gem.size/PAGE_SIZE||atomic_read(&b->color_vmas)){
   error=-EACCES;goto unlock;
  }
  for(k=0;k<i;k++)if(j->ids[k]==id){error=-EACCES;goto unlock;}
 }
 /* Commit only after the entire set has passed under exclusive job holds. */
 for(i=0;i<j->count;i++)if(mask&((u32)1<<i)){
  s->private_scene_input_ids[i]=j->ids[i];s->bo[j->ids[i]&31].input_seal=2;
 }
 s->private_scene_input_mask=mask;s->private_scene_binding_count=j->count;error=0;
 unlock:mutex_unlock(&s->lock);
 done:mutex_unlock(&endpoint_lock);return error;
}
int remesa_private_session_scene_inputs_locked(struct remesa_gem_memory *s,
 struct remesa_memory_ticket t,u32 mask)
{
 struct retained_job *j;unsigned i;int error;
 if(!s||!s->private_session||s->private_device_lost||!s->file||s->private_source_poisoned)return -EACCES;
#ifdef __KERNEL__
 lockdep_assert_held(&s->lock);
#endif
 if(!mask||mask!=s->private_scene_input_mask)return -EACCES;
 error=scene_input_objects_locked(s);if(error)return error;
 j=ticket_job(s,t);
 if(!j||j->state!=REMESA_JOB_PREPARED||j->quarantined||j->cookie||
    j->count!=s->private_scene_binding_count)return -EBUSY;
 for(i=0;i<j->count;i++)if(mask&((u32)1<<i))
  if(j->ids[i]!=s->private_scene_input_ids[i]||s->bo[j->ids[i]&31].holds!=1)return -EACCES;
 return 0;
}
