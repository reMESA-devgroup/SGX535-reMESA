/* SPDX-License-Identifier: GPL-2.0-only */
/* Same TU as the private endpoint. Lock order: endpoint_lock then s->lock. */
#include "remesa_private_session.h"
int remesa_private_session_acquire(struct drm_device *dev,struct drm_file *file,
 u64 context,const void *owner,struct remesa_gem_memory **out)
{
 struct remesa_gem_memory *s;unsigned i;int error=-EACCES;
 if(!out)return -EINVAL;
 *out=NULL;
 if(!dev||!file||!context||!owner)return -EINVAL;
 mutex_lock(&endpoint_lock);s=endpoint;
 if(!s||s->dev!=dev||s->file!=file||s->context!=context)goto done;
 mutex_lock(&s->lock);
 if(s->private_device_lost){error=-ENODEV;goto unlock;}
 if(s->private_session||s->stream_bound||s->executor||s->pds_write_guard){error=-EBUSY;goto unlock;}
 if(s->private_work_owned){error=-EBUSY;goto unlock;}
 for(i=0;i<2;i++)if(s->private_dpm_ids[i]){error=-EBUSY;goto unlock;}
 for(i=0;i<6;i++)if(s->private_work_ids[i]){error=-EBUSY;goto unlock;}
#if !defined(REMESA_MEMORY_TEST) || defined(REMESA_RETAINED_IOCTL_TEST)
 if(retained_entry.initialized||retained_entry.attempted){error=-EBUSY;goto unlock;}
#endif
 for(i=0;i<8;i++)if(s->jobs[i].live){error=-EBUSY;goto unlock;}
 for(i=0;i<OBJECTS;i++)if(s->bo[i].lease||s->bo[i].holds||s->bo[i].quarantine||
                         s->bo[i].input_seal||atomic_read(&s->bo[i].color_vmas)){
  error=-EBUSY;goto unlock;
 }
 s->private_session=owner;*out=s;error=0;
 unlock:mutex_unlock(&s->lock);
 done:mutex_unlock(&endpoint_lock);return error;
}
int remesa_private_session_release(struct remesa_gem_memory *s,const void *owner)
{
 unsigned i;int error=-EACCES;
 if(!s||!owner)return -EINVAL;
 mutex_lock(&endpoint_lock);
 /* Compare identity before dereferencing an arbitrary/stale endpoint. */
 if(endpoint!=s)goto done;
 mutex_lock(&s->lock);
 if(s->private_device_lost||!s->file){error=-ENODEV;goto unlock;}
 if(s->private_session!=owner)goto unlock;
 if(s->stream_bound||s->executor||s->private_source_held||s->private_source_poisoned){error=-EBUSY;goto unlock;}
 for(i=0;i<8;i++)if(s->jobs[i].live){error=-EBUSY;goto unlock;}
 for(i=0;i<OBJECTS;i++)if(s->bo[i].lease||s->bo[i].holds||s->bo[i].quarantine||
                         atomic_read(&s->bo[i].color_vmas)){
  error=-EBUSY;goto unlock;
 }
 /* Validate the complete seal before clearing any part of it. Source close,
  * job retirement and all CPU leases were checked above. */
 if(s->private_dpm_ids[0]||s->private_dpm_ids[1]){
  if(s->private_work_owned||s->pds_write_guard){error=-EIO;goto unlock;}
  for(i=0;i<5;i++)if(s->private_input_ids[i]){error=-EIO;goto unlock;}
  for(i=0;i<6;i++)if(s->private_work_ids[i]){error=-EIO;goto unlock;}
  if(!s->private_dpm_ids[0]||!s->private_dpm_ids[1]||
     s->private_dpm_ids[0]==s->private_dpm_ids[1]){error=-EIO;goto unlock;}
  for(i=0;i<2;i++){
   unsigned id=s->private_dpm_ids[i];struct backing *b=&s->bo[id&31];
   if(!b->live||b->generation!=id>>5||b->input_seal||!b->pinned||!b->reserved||
      !b->pages||b->gem.size!=33554432ULL||b->inserted!=8192){error=-EIO;goto unlock;}
  }
 }
 if(s->private_work_owned){
  if(!s->private_input_ids[0]||!s->pds_write_guard){error=-EIO;goto unlock;}
  for(i=0;i<6;i++){
   unsigned id=s->private_work_ids[i],k;struct backing *b=&s->bo[id&31];
   if(!id||!b->live||b->generation!=id>>5||b->input_seal||!b->pinned||
      !b->reserved||!b->pages||!b->gem.size||b->inserted!=b->gem.size/PAGE_SIZE){error=-EIO;goto unlock;}
   for(k=0;k<i;k++)if(s->private_work_ids[k]==id){error=-EIO;goto unlock;}
  }
 }else for(i=0;i<6;i++)if(s->private_work_ids[i]){error=-EIO;goto unlock;}
 if(s->private_input_ids[0]){
  if(!s->pds_write_guard){error=-EIO;goto unlock;}
  for(i=0;i<5;i++){
   unsigned id=s->private_input_ids[i];struct backing *b=&s->bo[id&31];
   if(!id||!b->live||b->generation!=id>>5||b->input_seal!=(i?2u:1u)){
    error=-EIO;goto unlock;
   }
  }
  for(i=0;i<5;i++)s->bo[s->private_input_ids[i]&31].input_seal=0;
  memset(s->private_input_ids,0,sizeof(s->private_input_ids));s->pds_write_guard=NULL;
 }else if(s->pds_write_guard){error=-EIO;goto unlock;}
 memset(s->private_work_ids,0,sizeof(s->private_work_ids));s->private_work_owned=false;
#ifdef REMESA_PRIVATE_DPM_CACHE
 /* Source closed and every job/lease gone. Restore CPU coherence before
  * dropping domain classification, including pools never read back. */
 for(i=0;i<2;i++)if(s->private_dpm_ids[i])ta_for_cpu(&s->bo[s->private_dpm_ids[i]&31]);
#endif
 memset(s->private_dpm_ids,0,sizeof(s->private_dpm_ids));
 s->private_session=NULL;error=0;
 unlock:mutex_unlock(&s->lock);
 done:mutex_unlock(&endpoint_lock);return error;
}
