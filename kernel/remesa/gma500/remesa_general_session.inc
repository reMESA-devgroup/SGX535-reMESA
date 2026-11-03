/* SPDX-License-Identifier: GPL-2.0-only */
/* Included after closed scene catalogues/helpers. Same scene_probe_lock. */
#include "remesa_general_session_uapi.h"
#include "remesa_parameter_window.h"
struct remesa_general_lab_session {
 struct remesa_scene_probe p;
 struct scene_probe_selection selected;
 u64 serial,completed;unsigned maximum,profile;
 bool poisoned;
 struct remesa_parameter_patch_policy parameter_policy;
 bool parameter_policy_bound;
};
static struct remesa_general_lab_session *general_session;
static int general_identity(struct drm_device *dev,struct remesa_general_session_request *r)
{
 struct drm_psb_private *dev_priv=dev->dev_private;
 if(!dev_priv||!dev_priv->sgx_reg||!gma_power_is_on(dev))return -ENODEV;
 r->core=PSB_RSGX32(PSB_CR_CORE_ID);r->revision=PSB_RSGX32(PSB_CR_CORE_REVISION);
 r->pds_base=PSB_RSGX32(PSB_CR_PDS_EXEC_BASE);
 return r->core==0x01130000&&r->revision==0x10201&&r->pds_base==0x20000000?0:-ENODEV;
}
static void general_poison(struct remesa_general_lab_session *s)
{
 struct remesa_gem_memory *m=s->p.memory;
 s->poisoned=true;
 mutex_lock(&m->lock);m->private_source_poisoned=true;m->private_dpm_ready=false;mutex_unlock(&m->lock);
 if(s->p.source_claimed)remesa_stream_source_poison(&s->p);
 if(s->p.job.memory)(void)remesa_gem_job_quarantine(m,s->p.job.snapshot.ticket);
}
static int general_open(struct drm_device *dev,struct drm_file *file,struct remesa_general_session_request *r)
{
 struct remesa_general_lab_session *s;struct remesa_scene_probe *p;
 const struct remesa_gears_scene_probe_profile *profile;
 struct remesa_scene_probe_request normalized={.context=r->context};
 unsigned i;int error,cleanup=0;
 if(general_session||scene_probe_retained||scene_probe_consumed)return -EBUSY;
 s=kzalloc(sizeof(*s),GFP_KERNEL);if(!s)return -ENOMEM;p=&s->p;
 error=remesa_private_session_acquire(dev,file,r->context,p,&p->memory);
 if(error){kfree(s);return error;}
 p->backing_owned=true;general_session=s;scene_probe_consumed=true;
 s->serial=1;s->maximum=r->max_jobs;s->profile=r->profile;
 /* Reuse the exact existing one-shot catalogues, including their complete
  * image/placement authentication. No new program bytes or mutable inputs. */
 profile=r->profile<=4?&gears_homogeneous_probe_profiles[r->profile-3]:
  &gears_clamped_api_probe_profiles[r->profile-5];
 s->selected=(struct scene_probe_selection){profile->packet,profile->packet_bytes,18,profile->bindings};
 mutex_lock(&endpoint_lock);
 if(endpoint!=p->memory||p->memory->private_device_lost||p->memory->file!=file){error=-ENODEV;goto unlock;}
 error=general_identity(dev,r);if(error)goto unlock;
 mutex_lock(&p->memory->lock);
 if(p->memory->root.end+1!=0xe0000000ULL){error=-EACCES;goto metadata_done;}
 for(i=0;i<18;i++){
  struct backing *b=&p->memory->bo[r->ids[i]&31];const struct remesa_scene_probe_binding *v=&profile->bindings[i];
  if(!b->live||b->generation!=r->ids[i]>>5||b->va.start!=v->address||b->gem.size!=v->bytes||
     b->lease||b->holds||!b->pinned||!b->reserved||b->quarantine||atomic_read(&b->color_vmas)){
   error=-EACCES;break;
  }
  if(v->access==3&&(!b->initialized||!b->zero_pristine||b->cached_job_view)){error=-ENODATA;break;}
 }
 metadata_done:mutex_unlock(&p->memory->lock);if(error)goto unlock;
 error=scene_probe_packet(p,&s->selected,&normalized,r->ids);if(error)goto unlock;
 error=remesa_twod_reserve(dev->dev_private,&p->guard);if(error)goto unlock;
 error=remesa_gem_scene_packet_prepare(p->memory,&p->packet,&p->guard,&p->job);if(error)goto unlock;
 error=scene_probe_images(p,&s->selected);if(error)goto unlock;
 mutex_unlock(&endpoint_lock);
 error=remesa_private_session_seal_dpm(p->memory,p,p->job.snapshot.ticket,p->packet.actions.table_id,p->packet.actions.parameter_id);
 if(!error){
  u32 mask=0;
  for(i=0;i<p->job.count;i++)if(p->job.loan.rules[i].access==REMESA_MMU_GPU_READ)mask|=(u32)1<<i;
  error=remesa_private_session_seal_scene_inputs(p->memory,p,p->job.snapshot.ticket,mask);
 }
 if(!error){
  mutex_lock(&endpoint_lock);error=remesa_gem_scene_job_cancel(&p->job);mutex_unlock(&endpoint_lock);
 }
 if(!error){error=remesa_private_session_source_begin(p->memory,p);if(!error)p->source_claimed=true;}
 mutex_lock(&endpoint_lock);
 unlock:
 if(!error&&p->guard.held)error=remesa_twod_release(dev->dev_private,&p->guard);
 if(error){
  if(p->job.memory)cleanup=remesa_gem_scene_job_cancel(&p->job);
  if(!cleanup&&p->guard.held)cleanup=remesa_twod_release(dev->dev_private,&p->guard);
 }
 mutex_unlock(&endpoint_lock);
 if(!error){r->session=s->serial;r->state=1;r->stages=16;return 0;}
 if(!cleanup&&p->source_claimed){cleanup=remesa_private_session_source_end(p->memory,p);if(!cleanup)p->source_claimed=false;}
 if(!cleanup)cleanup=remesa_private_session_release(p->memory,p);
 r->error=error;
 if(cleanup){general_poison(s);r->session=s->serial;r->state=2;}
 else{general_session=NULL;kfree(s);}
 return 0;
}
static int general_submit(struct remesa_general_lab_session *s,struct drm_device *dev,
 struct remesa_general_session_request *r)
{
 struct remesa_scene_probe *p=&s->p;int error;
 if(r->sequence!=s->completed+1||s->completed>=s->maximum)return -ESTALE;
 mutex_lock(&endpoint_lock);error=general_identity(dev,r);
 if(!error)error=remesa_twod_reserve(dev->dev_private,&p->guard);
 mutex_unlock(&endpoint_lock);if(error)goto failed;
 error=remesa_private_session_scene_prepare(p->memory,p,&p->packet,&p->guard,&p->job);
 if(error)goto failed;
 r->operation=p->job.snapshot.ticket.operation;r->stages=1;
 /* Qualification-only exact input authentication under real job holds. No
  * per-frame COLOR/Z hash/readback, and no production FAST change. */
 mutex_lock(&endpoint_lock);error=scene_probe_images(p,&s->selected);mutex_unlock(&endpoint_lock);
 if(error)goto failed;
 r->stages|=2;
 error=remesa_private_session_scene_execute(p->memory,p,&p->job,&p->backend,&p->observation,msecs_to_jiffies(r->budget_ms),100000);
 r->cookie=p->job.loan.execution_started?r->operation:0;
 if(p->job.loan.execution_started)r->stages|=4;
 r->phase=p->job.session.phase;r->ledger=p->job.session.observed_events;
 if(error)goto failed;
 if(!p->job.loan.execution_retired||p->job.state!=REMESA_SCENE_JOB_ACKNOWLEDGED){error=-EPROTO;goto failed;}
 /* Retain only after the existing full executor validated PARAM/TA and the
  * first job retired. No views survive the subsequent loan/job release. */
 if(!s->parameter_policy_bound){
  error=remesa_private_session_bind_scene_parameters(p->memory,p,&p->job,1,0x44800000U,&s->parameter_policy);
  if(error)goto failed;
  s->parameter_policy_bound=true;
 }
 mutex_lock(&endpoint_lock);
 error=remesa_gem_private_release(&p->job.loan);
 if(!error)error=remesa_twod_release(dev->dev_private,&p->guard);
 if(!error)error=remesa_gem_job_retire(p->memory,p->job.snapshot.ticket);
 mutex_unlock(&endpoint_lock);if(error)goto failed;
 /* LAB snapshot is queried separately between submissions. Preserve the
  * existing complete18-binding trace contract for independent supervision. */
 {
  struct remesa_scene_probe_request trace={.version=2,.profile=s->profile,.context=r->context,
   .operation=r->operation,.cookie=r->cookie,.core=r->core,.revision=r->revision,.pds_base=r->pds_base,
   .phase=r->phase,.ledger=r->ledger,.stages=15};
  scene_probe_save_trace(p,&trace,18);
 }
 memset(&p->job,0,sizeof(p->job));memset(&p->backend,0,sizeof(p->backend));memset(&p->observation,0,sizeof(p->observation));
 s->completed++;r->stages|=8;r->state=1;return 0;
 failed:general_poison(s);r->error=error;r->state=2;return 0;
}
int remesa_general_session_ioctl(struct drm_device *dev,void *data,struct drm_file *file)
{
 struct remesa_general_session_request *r=data;struct remesa_general_lab_session *s;
 struct remesa_gem_memory *m;int error;
 BUILD_BUG_ON(sizeof(*r)!=176);BUILD_BUG_ON(offsetof(struct remesa_general_session_request,operation)!=120);
 if(!dev||!r||!file||!capable(CAP_SYS_ADMIN))return -EPERM;
 if(!remesa_general_session_request_valid(r))return -EINVAL;
 if(r->action!=3&&(!memory_enabled||!scene_probe_enabled))return -EOPNOTSUPP;
 mutex_lock(&scene_probe_lock);
 if(r->action==1){error=general_open(dev,file,r);goto done;}
 s=general_session;if(!s||s->serial!=r->session){error=-ENOENT;goto done;}
 m=s->p.memory;mutex_lock(&endpoint_lock);
 error=endpoint!=m||m->dev!=dev||m->file!=file||m->context!=r->context||m->private_device_lost?-EACCES:0;
 mutex_unlock(&endpoint_lock);if(error)goto done;
 if(s->poisoned){r->error=-EIO;r->state=2;error=0;goto done;}
 if(r->action==2){error=general_submit(s,dev,r);goto done;}
 if(r->sequence!=s->completed){error=-ESTALE;goto done;}
 error=remesa_private_session_source_end(m,&s->p);
 if(!error){s->p.source_claimed=false;error=remesa_private_session_release(m,&s->p);}
 if(error){general_poison(s);r->error=error;r->state=2;error=0;goto done;}
 general_session=NULL;kfree(s);r->state=3;r->stages=32;error=0;
 done:mutex_unlock(&scene_probe_lock);return error;
}
