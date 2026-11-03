/* SPDX-License-Identifier: GPL-2.0-only */
/* After existing scene/general-session owners. Same source-attempt mutex and
 * retained storage; no catalogue selector or LAB image authentication reused. */
#include "remesa_matrix_submit.h"
#include "remesa_matrix_submit_uapi.h"
static bool matrix_submit_enabled;
module_param_named(remesa_matrix_submit_enabled,matrix_submit_enabled,bool,0600);
#include "remesa_matrix_fill.inc"
int remesa_matrix_submit_ioctl(struct drm_device *dev,void *data,struct drm_file *file)
{
 struct remesa_matrix_submit_request *r=data;struct remesa_scene_probe *p=NULL;
 struct remesa_gem_memory *s;struct drm_psb_private *dev_priv;unsigned i;int error,cleanup=0;
 BUILD_BUG_ON(sizeof(*r)!=2344);BUILD_BUG_ON(offsetof(struct remesa_matrix_submit_request,operation)!=2288);
 if(!dev||!r||!file||!capable(CAP_SYS_ADMIN))return -EPERM;
 if(!memory_enabled||!matrix_submit_enabled)return -EOPNOTSUPP;
 if(!remesa_matrix_submit_request_valid(r))return -EINVAL;
 mutex_lock(&scene_probe_lock);
 if(scene_probe_consumed||general_session||
    (scene_probe_retained&&(!matrix_fill_owner||scene_probe_retained!=&matrix_fill_owner->base))){error=-EBUSY;goto done;}
 if(matrix_fill_owner){
  struct remesa_matrix_fill_owner *m=matrix_fill_owner;p=&m->base;s=p->memory;
  mutex_lock(&endpoint_lock);
  if(endpoint!=s||s->dev!=dev||s->file!=file||s->context!=r->context||s->private_device_lost||
     s->private_session!=p){error=-EACCES;mutex_unlock(&endpoint_lock);goto done;}
  if(m->active||m->failed||!m->retirement.gem_retired||m->color!=r->packet.controls.color_id||
     m->width!=r->packet.controls.width||m->height!=r->packet.controls.height){error=-EBUSY;mutex_unlock(&endpoint_lock);goto done;}
  mutex_lock(&s->lock);s->private_matrix_constructing=false;mutex_unlock(&s->lock);
  mutex_unlock(&endpoint_lock);
 }else{
  p=kzalloc(sizeof(*p),GFP_KERNEL);if(!p){error=-ENOMEM;goto done;}
  error=remesa_private_session_acquire(dev,file,r->context,p,&p->memory);
  if(error){kfree(p);p=NULL;goto done;}
  p->backing_owned=true;scene_probe_retained=p;s=p->memory;
 }
 p->packet=r->packet;
 mutex_lock(&endpoint_lock);
 if(endpoint!=s||s->private_device_lost||s->file!=file||s->private_session!=p){error=-ENODEV;goto cancel_locked;}
 dev_priv=dev->dev_private;
 if(!dev_priv||!dev_priv->sgx_reg||!gma_power_is_on(dev)){error=-ENODEV;goto cancel_locked;}
 r->core=PSB_RSGX32(PSB_CR_CORE_ID);r->revision=PSB_RSGX32(PSB_CR_CORE_REVISION);
 r->pds_base=PSB_RSGX32(PSB_CR_PDS_EXEC_BASE);
 if(r->core!=0x01130000U||r->revision!=0x10201U||r->pds_base!=0x20000000U){error=-ENODEV;goto cancel_locked;}
 mutex_lock(&s->lock);error=0;
 if(s->executor||s->stream_bound||s->ta_cache_domains||s->root.end+1!=0xe0000000ULL){error=-EBUSY;goto metadata_done;}
 for(i=0;i<8;i++)if(s->jobs[i].live){error=-EBUSY;goto metadata_done;}
 for(i=0;i<p->packet.binding_count;i++){
  u32 id=p->packet.bindings[i].id;struct backing *b=&s->bo[id&31];
  if(!b->live||b->generation!=id>>5){error=-ENOENT;break;}
  if(b->lease||b->holds||!b->pinned||!b->reserved||b->quarantine||atomic_read(&b->color_vmas)){
   error=-EACCES;break;
  }
  /* A background load must never expose uninitialized backing. Actual
   * glClear is the frontend's native fill with authenticated retirement;
   * initialized is a memory-safety condition, not a pixel/hash certificate. */
  if(p->packet.bindings[i].access==3&&(!b->initialized||b->cached_job_view)){error=-ENODATA;break;}
 }
 metadata_done:mutex_unlock(&s->lock);if(error)goto cancel_locked;
 error=remesa_twod_reserve(dev_priv,&p->guard);if(error)goto cancel_locked;
 mutex_unlock(&endpoint_lock);
 error=remesa_private_session_source_begin(s,p);
 if(error)goto reacquire_cancel;
 p->source_claimed=true;scene_probe_consumed=true;
 mutex_lock(&endpoint_lock);
 if(endpoint!=s||s->private_device_lost||s->file!=file||s->private_session!=p){error=-ENODEV;cleanup=-ENODEV;goto unlock_cleanup;}
 error=remesa_gem_scene_packet_prepare(s,&p->packet,&p->guard,&p->job);if(error)goto cancel_locked;
 r->operation=p->job.snapshot.ticket.operation;r->stages=1;
 if(p->job.count!=p->packet.binding_count){error=-EPROTO;goto cancel_locked;}
 /* This entry, unlike the trusted/LAB executor, requires the complete
  * selected PDS/source-value contract BEFORE publication or possible issue. */
 error=remesa_gem_scene_job_execute_matrix(&p->job,&p->backend,&p->observation,p,
  msecs_to_jiffies(r->budget_ms),100000);
 p->execution_consumed=p->job.loan.execution_attempted;
 r->cookie=p->job.loan.execution_started?r->operation:0;
 if(p->job.loan.execution_started)r->stages|=4;
 r->phase=p->job.session.phase;r->ledger=p->job.session.observed_events;
 if(error)goto cancel_locked;
 if(!p->job.loan.execution_retired||p->job.state!=REMESA_SCENE_JOB_ACKNOWLEDGED){error=-EPROTO;goto cancel_locked;}
 r->stages|=2;
 error=remesa_gem_private_release(&p->job.loan);if(error)goto cancel_locked;
 error=remesa_twod_release(dev_priv,&p->guard);if(error)goto cancel_locked;
 error=remesa_gem_job_retire(s,p->job.snapshot.ticket);if(error)goto cancel_locked;
 goto unlock_cleanup;
 reacquire_cancel:mutex_lock(&endpoint_lock);
 cancel_locked:
 if(s->private_device_lost||s->file!=file)cleanup=-ENODEV;
 else cleanup=scene_probe_cancel_locked(p);
 unlock_cleanup:mutex_unlock(&endpoint_lock);
 if(!cleanup){
  if(p->source_claimed){cleanup=remesa_private_session_source_end(s,p);if(!cleanup)p->source_claimed=false;}
  if(!cleanup){cleanup=remesa_private_session_release(s,p);if(!cleanup)p->backing_owned=false;}
  if(cleanup&&!error)error=cleanup;
 }
 if(!cleanup){scene_probe_retained=NULL;matrix_fill_owner=NULL;r->stages|=8;}
 else if(!error)error=cleanup;
 r->error=error;r->retained=scene_probe_retained!=NULL;
 if(!cleanup){kfree(p);p=NULL;}
 error=0;
 done:mutex_unlock(&scene_probe_lock);return error;
}
