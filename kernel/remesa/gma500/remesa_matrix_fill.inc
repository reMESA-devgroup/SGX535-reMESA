/* SPDX-License-Identifier: GPL-2.0-only */
/* After the existing scene owner and matrix_submit_enabled declaration.
 * Backing continuity for ordinary glClear -> CPU scene construction -> draw.
 * Claims the concrete owned-clear source before any possible native issue;
 * the original first-owner guard is never repaired or rearmed. */
#include "remesa_gem_twod_job.h"
#include "remesa_twod_fill_uapi.h"
#include "remesa_owned_fill_source.h"
struct remesa_matrix_fill_owner {
 struct remesa_scene_probe base; /* stable common owner, offset zero */
 struct remesa_twod_retirement retirement;
 u32 color,width,height,stride,last_depth_pitch;
 bool active,failed,continuous;
 u64 last_draw_operation;
};
static struct remesa_matrix_fill_owner *matrix_fill_owner;
int remesa_matrix_fill_begin(struct drm_device *dev,struct drm_file *file,
 const struct remesa_twod_fill_request *r,const void **token)
{
 struct remesa_matrix_fill_owner *m;struct remesa_gem_memory *s;
 struct remesa_gem_twod_command command={0};struct remesa_memory_binding uses[2];
 unsigned count=0;int error=0,cleanup;bool fresh=false;
 if(!token)return -EINVAL;
 *token=NULL;if(!matrix_submit_enabled)return 0;
 if(!dev||!file||!r)return -EINVAL;
 if(r->color||r->destination_x||r->destination_y||!r->width||!r->height||r->width>=2048||r->height>=2048||
    r->width!=r->destination.width||r->height!=r->destination.height||
    r->destination.stride!=(u64)r->width*4)return -EOPNOTSUPP;
 mutex_lock(&scene_probe_lock);mutex_lock(&endpoint_lock);s=endpoint;
 if(!s||s->dev!=dev||s->file!=file||s->context!=r->context||s->private_device_lost){error=-EACCES;goto unlock;}
 if((scene_probe_consumed&&(!matrix_fill_owner||!matrix_fill_owner->continuous))||general_session||
    (scene_probe_retained&&(!matrix_fill_owner||scene_probe_retained!=&matrix_fill_owner->base))){error=-EBUSY;goto unlock;}
 error=twod_fill_command(s,r,&command,uses,&count);if(error)goto unlock;
 if(count!=1){error=-EPROTO;goto unlock;}
 m=matrix_fill_owner;
 if(m){
  if(m->base.memory!=s||s->private_session!=&m->base||m->active||m->failed||
     !s->private_matrix_constructing||m->color!=r->destination.id||m->width!=r->width||
     m->height!=r->height||m->stride!=r->destination.stride){error=-EBUSY;goto unlock;}
 }else{
  mutex_unlock(&endpoint_lock);
  m=kzalloc(sizeof(*m),GFP_KERNEL);if(!m){error=-ENOMEM;goto done;}
  error=remesa_private_session_acquire(dev,file,r->context,&m->base,&m->base.memory);
  if(error){kfree(m);goto done;}
  m->base.backing_owned=true;fresh=true;
  mutex_lock(&endpoint_lock);s=m->base.memory;
  if(endpoint!=s||s->file!=file||s->private_device_lost){error=-ENODEV;goto retain;}
  error=twod_fill_command(s,r,&command,uses,&count);if(error)goto retain;
  if(count!=1){error=-EPROTO;goto retain;}
  m->color=r->destination.id;m->width=r->width;m->height=r->height;m->stride=r->destination.stride;
 }
 mutex_lock(&s->lock);
 error=private_session_owner(s,&m->base);
 if(!error)error=private_session_idle(s);
 if(!error){
  if(fresh){
   error=remesa_owned_fill_claim(dev,&m->base,m->color);
   if(!error){s->private_source_held=true;m->base.source_claimed=true;}
  }else if(!s->private_source_held||!m->base.source_claimed)error=-EACCES;
  else error=remesa_owned_fill_ready(dev,&m->base);
 }
 if(error){mutex_unlock(&s->lock);if(fresh)goto refused;goto unlock;}
 s->private_matrix_constructing=false;s->private_matrix_color=m->color;
 mutex_unlock(&s->lock);
 m->active=true;matrix_fill_owner=m;scene_probe_retained=&m->base;*token=&m->base;
 goto unlock;
 refused:
 /* No source claim or native publication occurred. Release only through the
  * existing idle backing close; a loss/close failure retains stable storage. */
 mutex_unlock(&endpoint_lock);
 cleanup=remesa_private_session_release(s,&m->base);
 if(!cleanup){kfree(m);goto done;}
 mutex_lock(&endpoint_lock);
 retain:
 /* A lost file/device cannot be guessed safe or released as a new owner.
  * This interval contains no MMIO; retain the stable backing lease. */
 if(fresh){m->failed=true;matrix_fill_owner=m;scene_probe_retained=&m->base;*token=&m->base;error=-EOWNERDEAD;}
 unlock:mutex_unlock(&endpoint_lock);
 done:mutex_unlock(&scene_probe_lock);return error;
}
int remesa_matrix_fill_finish(struct remesa_gem_memory *s,const void *token,
 const struct remesa_twod_fill_request *r,const struct remesa_twod_retirement *proof)
{
 struct remesa_matrix_fill_owner *m=matrix_fill_owner;int error=0;
 if(!token)return 0;
 if(!m||token!=&m->base||m->base.memory!=s||!m->active||m->failed)return -EACCES;
 mutex_lock(&s->lock);
 if(s->private_session!=token||s->private_device_lost||!s->file||s->executor||
    s->context!=r->context||r->destination.id!=m->color||r->width!=m->width||r->height!=m->height||
    r->destination.stride!=m->stride||r->error||r->retained||r->stages!=31||
    !proof||!proof->native_retired||!proof->gem_retired||proof->dev!=s->dev||proof->pd!=s->pd||
    proof->ticket.context!=s->context||proof->ticket.operation!=r->operation||proof->cookie!=r->cookie||
    proof->ticket.operation<=m->retirement.ticket.operation||proof->destination_id!=m->color||
    proof->packet_dwords!=7||proof->core!=0x01130000||proof->revision!=0x10201){error=-EPROTO;goto done;}
 error=remesa_owned_fill_complete(token,proof);if(error)goto done;
 m->retirement=*proof;m->active=false;
 s->private_matrix_color=m->color;s->private_matrix_constructing=true;
 done:
 if(error){
  m->failed=true;s->private_matrix_constructing=false;
  if(s->private_source_held){s->private_source_poisoned=true;remesa_stream_source_poison(token);}
 }
 mutex_unlock(&s->lock);return error;
}
void remesa_matrix_fill_failed_locked(struct remesa_gem_memory *s,const void *token)
{
 struct remesa_matrix_fill_owner *m=matrix_fill_owner;
 if(!token||!m||token!=&m->base||m->base.memory!=s)return;
 mutex_lock(&s->lock);m->failed=true;s->private_matrix_constructing=false;mutex_unlock(&s->lock);
}
/* Endpoint lock NOT held. Only after the normal endpoint has proved there
 * is no possibly issued/retained 2D job. Failed release retains the owner. */
int remesa_matrix_fill_cancel_unissued(const void *token)
{
 struct remesa_matrix_fill_owner *m;struct remesa_gem_memory *s;int error=-EACCES;unsigned i;
 if(!token)return 0;
 mutex_lock(&scene_probe_lock);m=matrix_fill_owner;
 if(!m||token!=&m->base)goto done;
 s=m->base.memory;mutex_lock(&endpoint_lock);
 if(endpoint!=s){mutex_unlock(&endpoint_lock);error=-ENODEV;goto done;}
 mutex_lock(&s->lock);
 if(s->private_session!=token||s->private_device_lost||!s->file||s->executor||s->stream_bound){error=-EBUSY;goto unlock;}
 for(i=0;i<8;i++)if(s->jobs[i].live){error=-EBUSY;goto unlock;}
 if(m->retirement.gem_retired){
  error=remesa_owned_fill_ready(s->dev,token);
  if(!error){m->active=m->failed=false;s->private_matrix_constructing=true;}
 }else error=1;
 unlock:mutex_unlock(&s->lock);mutex_unlock(&endpoint_lock);
 if(error==1){
  if(m->base.source_claimed){
   error=remesa_private_session_source_end(s,&m->base);
   if(!error)m->base.source_claimed=false;
  }else error=0;
  if(!error)error=remesa_private_session_release(s,&m->base);
  if(!error){scene_probe_retained=NULL;matrix_fill_owner=NULL;kfree(m);}
 }
 done:mutex_unlock(&scene_probe_lock);return error;
}
