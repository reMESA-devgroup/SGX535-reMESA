/* SPDX-License-Identifier: GPL-2.0-only */
/* Included after the endpoint/backing lease and existing private executor. */
#include "remesa_private_session.h"
#ifndef REMESA_PRIVATE_SESSION_SOURCE_TEST
#include "remesa_stream_source.h"
#endif
/* Caller holds endpoint then memory lock. A lease pins s even after FD loss;
 * orphaned metadata is never authority to touch hardware. */
static int private_session_owner(struct remesa_gem_memory *s,const void *owner)
{
 if(!owner||s->private_session!=owner)return -EACCES;
 if(s->private_device_lost||!s->file)return -ENODEV;
 if(s->private_source_poisoned)return -EIO;
 return 0;
}
static int private_session_idle(struct remesa_gem_memory *s)
{
 unsigned i;
 if(s->stream_bound||s->executor)return -EBUSY;
 for(i=0;i<8;i++)if(s->jobs[i].live)return -EBUSY;
 for(i=0;i<OBJECTS;i++)if(s->bo[i].lease||s->bo[i].holds||s->bo[i].quarantine||
                         atomic_read(&s->bo[i].color_vmas))return -EBUSY;
 return 0;
}
int remesa_private_session_source_begin(struct remesa_gem_memory *s,const void *owner)
{
 int error=-EACCES;
 if(!s||!owner)return -EINVAL;
 mutex_lock(&endpoint_lock);if(endpoint!=s)goto done;
 mutex_lock(&s->lock);
 error=private_session_owner(s,owner);if(error)goto unlock;
 if(s->private_source_held){error=-EBUSY;goto unlock;}
 error=private_session_idle(s);if(error)goto unlock;
 error=remesa_stream_source_claim(s->dev,owner);
 if(!error)s->private_source_held=true;
 unlock:mutex_unlock(&s->lock);
 done:mutex_unlock(&endpoint_lock);return error;
}
int remesa_private_session_source_end(struct remesa_gem_memory *s,const void *owner)
{
 int error=-EACCES;
 if(!s||!owner)return -EINVAL;
 mutex_lock(&endpoint_lock);if(endpoint!=s)goto done;
 mutex_lock(&s->lock);
 error=private_session_owner(s,owner);if(error)goto unlock;
 if(!s->private_source_held){error=-ENOENT;goto unlock;}
 error=private_session_idle(s);if(error)goto unlock;
 error=remesa_stream_source_settled(owner);
 if(!error)error=remesa_stream_source_release(owner);
 if(error){
  s->private_source_poisoned=true;remesa_stream_source_poison(owner);
 }else s->private_source_held=false;
 unlock:mutex_unlock(&s->lock);
 done:mutex_unlock(&endpoint_lock);return error;
}
int remesa_private_session_execute(struct remesa_gem_memory *s,const void *token,
 struct remesa_gem_private *loan,struct sgx535_gma500_owner *owner,
 struct sgx535_gma500_fixed_backend *backend,struct remesa_stream_observation *observation,
 const struct remesa_memory_view *views,unsigned count,unsigned ticks,unsigned samples)
{
 int error=-EACCES;
 if(!s||!token||!loan||!owner||!backend||!observation||!views)return -EINVAL;
 mutex_lock(&endpoint_lock);if(endpoint!=s)goto done;
 mutex_lock(&s->lock);
 error=private_session_owner(s,token);
 if(!error&&(!s->private_source_held||loan->memory!=s))error=-EACCES;
 mutex_unlock(&s->lock);if(error)goto done;
 /* The original executor retains the complete bind, permission, publication,
  * lifecycle, root-restore, source-close and settled checks. Its source unbind
  * closes this observation; it does not release the session's source lease. */
 error=remesa_gem_private_execute(loan,owner,backend,observation,token,
                                  views,count,ticks,samples);
 if(error){
  mutex_lock(&s->lock);s->private_source_poisoned=true;mutex_unlock(&s->lock);
  remesa_stream_source_poison(token);
  (void)remesa_gem_job_quarantine(s,loan->ticket);
 }
 done:mutex_unlock(&endpoint_lock);return error;
}
