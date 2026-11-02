/* SPDX-License-Identifier: GPL-2.0-only */
/* Same TU after source-session helpers and general scene executor. */
#include "remesa_private_session.h"
#ifndef REMESA_PRIVATE_SESSION_SCENE_TEST
#include "remesa_gem_scene_job.h"
#endif
#ifndef REMESA_PRIVATE_SESSION_SCENE_TEST
int remesa_private_session_scene_prepare(struct remesa_gem_memory *s,const void *token,
 const struct remesa_scene_packet *packet,struct remesa_twod_reservation *guard,
 struct remesa_gem_scene_job *job)
{
 unsigned i;int error=-EACCES;
 if(!s||!token||!packet||!guard||!job)return -EINVAL;
 mutex_lock(&endpoint_lock);if(endpoint!=s)goto done;
 mutex_lock(&s->lock);
 error=private_session_owner(s,token);
 if(!error&&(!s->private_source_held||!s->private_dpm_ids[0]||!s->private_dpm_ids[1]||
    packet->actions.table_id!=s->private_dpm_ids[0]||packet->actions.parameter_id!=s->private_dpm_ids[1]))error=-EACCES;
 if(!error)for(i=0;i<8;i++)if(s->jobs[i].live){error=-EBUSY;break;}
 mutex_unlock(&s->lock);if(error)goto done;
 error=scene_packet_prepare_owned(s,packet,guard,job,token);
 if(error&&job->state==REMESA_SCENE_JOB_HELD){
  mutex_lock(&s->lock);s->private_source_poisoned=true;s->private_dpm_ready=false;mutex_unlock(&s->lock);
  remesa_stream_source_poison(token);
 }
 done:mutex_unlock(&endpoint_lock);return error;
}
#endif
int remesa_private_session_scene_execute(struct remesa_gem_memory *s,const void *token,
 struct remesa_gem_scene_job *job,struct sgx535_gma500_fixed_backend *backend,
 struct remesa_stream_observation *observation,unsigned ticks,unsigned samples)
{
 unsigned i;int error=-EACCES;
 if(!s||!token||!job||!backend||!observation)return -EINVAL;
 mutex_lock(&endpoint_lock);if(endpoint!=s)goto done;
 mutex_lock(&s->lock);
 error=private_session_owner(s,token);
 if(!error&&(!s->private_source_held||job->memory!=s||job->loan.memory!=s))error=-EACCES;
 /* The five-input/six-work retained seals authenticate a different job
  * grammar. Never skip those checks by passing that lease to this adapter. */
 if(!error){
  if(s->private_work_owned||s->pds_write_guard)error=-EOPNOTSUPP;
  for(i=0;i<5;i++)if(s->private_input_ids[i])error=-EOPNOTSUPP;
  for(i=0;i<6;i++)if(s->private_work_ids[i])error=-EOPNOTSUPP;
 }
 if(!error&&(s->private_dpm_ids[0]||s->private_dpm_ids[1]))
  error=remesa_private_session_dpm_locked(s,job->loan.ticket);
#ifdef REMESA_PRIVATE_SCENE_INPUT_SEALS
 if(!error&&s->private_scene_binding_count){
  u32 mask=0;
  if(!job->count||job->count>REMESA_GEM_JOB_BINDINGS)error=-EACCES;
  else{
   for(i=0;i<job->count;i++)if(job->loan.rules[i].access==REMESA_MMU_GPU_READ)mask|=(u32)1<<i;
   error=remesa_private_session_scene_inputs_locked(s,job->loan.ticket,mask);
  }
 }
#endif
#ifdef REMESA_PRIVATE_DPM_CACHE
 if(!error&&(s->private_dpm_ids[0]||s->private_dpm_ids[1])){
  struct retained_job *j=ticket_job(s,job->loan.ticket);
  if(!j->dpm_previous_operation&&(!s->bo[s->private_dpm_ids[0]&31].zero_pristine||
     !s->bo[s->private_dpm_ids[1]&31].zero_pristine))error=-ENODATA;
 }
#endif
 mutex_unlock(&s->lock);if(error)goto done;
 error=remesa_gem_scene_job_execute(job,backend,observation,token,ticks,samples);
#ifdef REMESA_PRIVATE_DPM_CACHE
 if(!error&&(s->private_dpm_ids[0]||s->private_dpm_ids[1])){
  mutex_lock(&s->lock);error=remesa_private_session_dpm_retired_locked(s,token,job->loan.ticket);mutex_unlock(&s->lock);
 }
#endif
 if(error){
  mutex_lock(&s->lock);s->private_source_poisoned=true;s->private_dpm_ready=false;mutex_unlock(&s->lock);
  remesa_stream_source_poison(token);
  job->state=REMESA_SCENE_JOB_HELD;
  (void)remesa_gem_job_quarantine(s,job->loan.ticket);
 }
 done:mutex_unlock(&endpoint_lock);return error;
}
