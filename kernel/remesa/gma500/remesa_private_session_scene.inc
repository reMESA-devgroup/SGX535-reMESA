/* SPDX-License-Identifier: GPL-2.0-only */
/* Same TU after source-session helpers and general scene executor. */
#include "remesa_private_session.h"
#ifndef REMESA_PRIVATE_SESSION_SCENE_TEST
#include "remesa_gem_scene_job.h"
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
 mutex_unlock(&s->lock);if(error)goto done;
 error=remesa_gem_scene_job_execute(job,backend,observation,token,ticks,samples);
 if(error){
  mutex_lock(&s->lock);s->private_source_poisoned=true;mutex_unlock(&s->lock);
  remesa_stream_source_poison(token);
  job->state=REMESA_SCENE_JOB_HELD;
  (void)remesa_gem_job_quarantine(s,job->loan.ticket);
 }
 done:mutex_unlock(&endpoint_lock);return error;
}
