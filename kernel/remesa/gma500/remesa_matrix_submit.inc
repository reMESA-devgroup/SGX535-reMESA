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
 if(r->flags!=REMESA_MATRIX_CLOSE_SESSION&&(!memory_enabled||!matrix_submit_enabled))return -EOPNOTSUPP;
 if(!remesa_matrix_submit_request_valid(r))return -EINVAL;
 mutex_lock(&scene_probe_lock);
 if(r->flags==REMESA_MATRIX_CLOSE_SESSION){
  struct remesa_matrix_fill_owner *m=matrix_fill_owner;
  if(!m||!m->continuous||scene_probe_retained!=&m->base){error=-ENOENT;goto done;}
  p=&m->base;s=p->memory;mutex_lock(&endpoint_lock);
  if(endpoint!=s||s->dev!=dev||s->file!=file||s->context!=r->context||s->private_device_lost||
     s->private_session!=p||m->active||m->failed||p->guard.held||p->backend.active||
     p->execution_consumed){error=-EACCES;mutex_unlock(&endpoint_lock);goto done;}
  mutex_lock(&s->lock);error=private_session_owner(s,p);
  if(!error)error=private_session_idle(s);
  mutex_unlock(&s->lock);mutex_unlock(&endpoint_lock);
  if(error)goto done;
  error=remesa_private_session_source_end(s,p);
  if(!error){p->source_claimed=false;error=remesa_private_session_release(s,p);}
  if(error){
   mutex_lock(&endpoint_lock);
   if(endpoint==s)remesa_matrix_fill_failed_locked(s,p);else m->failed=true;
   mutex_unlock(&endpoint_lock);r->error=error;r->retained=1;error=0;goto done;
  }
  scene_probe_retained=NULL;matrix_fill_owner=NULL;kfree(p);r->stages=8;error=0;goto done;
 }
 if((scene_probe_consumed&&(!matrix_fill_owner||!matrix_fill_owner->continuous))||general_session||
    (scene_probe_retained&&(!matrix_fill_owner||scene_probe_retained!=&matrix_fill_owner->base))){error=-EBUSY;goto done;}
 if(matrix_fill_owner){
  struct remesa_matrix_fill_owner *m=matrix_fill_owner;p=&m->base;s=p->memory;
  mutex_lock(&endpoint_lock);
  if(endpoint!=s||s->dev!=dev||s->file!=file||s->context!=r->context||s->private_device_lost||
     s->private_session!=p||!s->private_source_held||s->private_source_poisoned||!p->source_claimed){error=-EACCES;mutex_unlock(&endpoint_lock);goto done;}
  if(r->flags&REMESA_MATRIX_PRESERVE_DEPTH){
   /* Content authority is kernel-owned retirement history, not DEFINED DATA
    * or a userspace flag. CPU writes/remaps to this Z have stayed excluded. */
   if(!m->continuous||!m->last_draw_operation||!s->private_matrix_depth||
      s->private_matrix_depth!=r->packet.controls.depth_id||
      m->last_depth_pitch!=r->packet.controls.depth_pitch||
      r->packet.controls.color_pitch!=m->stride/4||
      m->retirement.ticket.operation>=m->last_draw_operation){error=-ESTALE;mutex_unlock(&endpoint_lock);goto done;}
   mutex_lock(&s->lock);error=private_session_owner(s,p);
   if(!error)error=private_session_idle(s);
   mutex_unlock(&s->lock);
   if(error){mutex_unlock(&endpoint_lock);goto done;}
  }else if(m->retirement.ticket.operation<=m->last_draw_operation){error=-ESTALE;mutex_unlock(&endpoint_lock);goto done;}
  if(m->active||m->failed||!m->retirement.gem_retired||m->color!=r->packet.controls.color_id||
     m->width!=r->packet.controls.width||m->height!=r->packet.controls.height){error=-EBUSY;mutex_unlock(&endpoint_lock);goto done;}
  mutex_lock(&s->lock);s->private_matrix_constructing=false;mutex_unlock(&s->lock);
  mutex_unlock(&endpoint_lock);
 }else{
  if(r->flags&REMESA_MATRIX_KEEP_SESSION){error=-EOPNOTSUPP;goto done;}
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
 /* A paired clear already owns the source acquired before its publication.
  * Reclaiming the original first-owner source here would reject that history. */
 if(p->source_claimed)error=remesa_owned_fill_ready(dev,p);
 else{
  error=remesa_private_session_source_begin(s,p);
  if(!error)p->source_claimed=true;
 }
 if(error)goto reacquire_cancel;
 scene_probe_consumed=true;
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
 if(r->flags&REMESA_MATRIX_KEEP_SESSION){
  struct remesa_matrix_fill_owner *m=matrix_fill_owner;
  /* Every operation below has ALREADY retired, restored the root, released
   * its private loan/native reservation and dropped the real GEM holds.
   * Keep only the existing source/backing lease; never reset boot lineage. */
  mutex_lock(&s->lock);error=private_session_owner(s,p);
  if(!error)error=private_session_idle(s);
  if(!error)error=remesa_owned_fill_ready(dev,p);
  if(!error&&(!m||&m->base!=p))error=-EPROTO;
  if(!error){
   memset(&p->job,0,sizeof(p->job));memset(&p->backend,0,sizeof(p->backend));
   memset(&p->observation,0,sizeof(p->observation));p->execution_consumed=false;
   m->last_draw_operation=r->operation;m->continuous=true;
   s->private_matrix_depth=p->packet.controls.depth_id;m->last_depth_pitch=p->packet.controls.depth_pitch;
   s->private_matrix_constructing=true;r->stages|=REMESA_MATRIX_SESSION_READY;
  }
  mutex_unlock(&s->lock);
  if(error){
   if(m && &m->base==p)m->failed=true;
   mutex_lock(&s->lock);s->private_source_poisoned=true;mutex_unlock(&s->lock);
   remesa_stream_source_poison(p);goto cancel_locked;
  }
  mutex_unlock(&endpoint_lock);r->retained=0;error=0;goto done;
 }
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
