/* SPDX-License-Identifier: GPL-2.0-only */
/* Same private endpoint TU, after the qualified probe and session helpers.
 * All callers are covered by the existing outer DRM entry wrapper. */
#include "remesa_private_session_lab.h"
#include "remesa_private_session_lab_request.h"
#include "remesa_canonical_inputs.h"
#include "remesa_pds_write_guard.h"
struct private_lab_session {
 struct remesa_private_probe p;
 u32 ids[11],maximum;
 u64 serial,completed;
 bool acquired,source_started,poisoned;
};
static DEFINE_MUTEX(private_lab_lock);
static struct private_lab_session *private_lab_active;
static bool private_lab_attempted;
static int private_lab_guard(u64 offset,const void *data,unsigned n)
{return remesa_canonical_pds_page(private_probe_profile.batches,private_probe_profile.count,
 remesa_canonical_inputs[0],offset,data,n);}
static int private_lab_identity(struct drm_device *dev,struct remesa_private_session_lab_request *r)
{
 struct drm_psb_private *dev_priv=dev->dev_private;
 if(!dev_priv||!dev_priv->sgx_reg||!gma_power_is_on(dev))return -ENODEV;
 r->core=PSB_RSGX32(PSB_CR_CORE_ID);r->revision=PSB_RSGX32(PSB_CR_CORE_REVISION);
 return r->core==0x01130000u&&r->revision==0x00010201u?0:-ENODEV;
}
static int private_lab_bindings(struct private_lab_session *s,struct remesa_memory_binding uses[11])
{
 struct remesa_gem_memory *m=s->p.memory;unsigned i;int error=0;
 mutex_lock(&m->lock);
 if(m->private_device_lost||!m->file||m->private_session!=&s->p||
    m->root.end+1!=0xe0000000ULL){error=-EACCES;goto done;}
 for(i=0;i<11;i++){
  u32 id=s->ids[i];struct backing *b=&m->bo[id&31];
  if(!id||!b->live||b->generation!=id>>5||
     b->va.start!=private_probe_profile.bindings[i].va||
     b->gem.size!=private_probe_profile.bindings[i].bytes){error=-EACCES;break;}
  uses[i]=(struct remesa_memory_binding){id,b->va.start,b->gem.size};
 }
 done:mutex_unlock(&m->lock);return error;
}
static int private_lab_prepare(struct private_lab_session *s)
{
 struct remesa_memory_binding uses[11];int error=private_lab_bindings(s,uses);
 if(error)return error;
 error=remesa_gem_job_prepare(s->p.memory,uses,11,&private_probe_profile.commands,
          sizeof(private_probe_profile.commands),&s->p.snapshot.ticket);
 if(error)return error;
 error=remesa_gem_job_snapshot(s->p.memory,s->p.snapshot.ticket,&s->p.snapshot,s->p.views,11);
 return error==11?0:error<0?error:-EPROTO;
}
static int private_lab_authenticate(struct remesa_private_probe *p)
{
 unsigned i;int error;void *masked;
 for(i=0;i<5;i++){
  if(p->views[i].binding.bytes!=remesa_canonical_input_sizes[i])return -EKEYREJECTED;
  if(!i){
   masked=vmalloc(p->views[i].binding.bytes);if(!masked)return -ENOMEM;
   memcpy(masked,p->views[i].cpu,p->views[i].binding.bytes);
   error=remesa_stream_mask_sources(private_probe_profile.batches,private_probe_profile.count,
                 masked,p->views[i].binding.bytes,p->views[i].binding.va);
   if(!error&&memcmp(masked,remesa_canonical_inputs[i],p->views[i].binding.bytes))error=-EKEYREJECTED;
   vfree(masked);if(error)return error;
  }else if(memcmp(p->views[i].cpu,remesa_canonical_inputs[i],p->views[i].binding.bytes))return -EKEYREJECTED;
 }
 error=remesa_stream_validate_geometry(private_probe_profile.batches,private_probe_profile.count,
              p->views[2].cpu,p->views[2].binding.bytes);
 if(!error)p->images_authenticated=true;
 return error;
}
static void private_lab_reset_job(struct private_lab_session *s)
{
 struct remesa_gem_memory *m=s->p.memory;bool auth=s->p.images_authenticated;
 memset(&s->p,0,sizeof(s->p));s->p.memory=m;s->p.images_authenticated=auth;
}
static void private_lab_poison(struct private_lab_session *s)
{
 s->poisoned=true;
 if(s->source_started){
  remesa_stream_source_poison(&s->p);
  mutex_lock(&s->p.memory->lock);s->p.memory->private_source_poisoned=true;mutex_unlock(&s->p.memory->lock);
 }
 if(s->p.snapshot.ticket.operation)
  (void)remesa_gem_job_quarantine(s->p.memory,s->p.snapshot.ticket);
}
static int private_lab_open(struct drm_device *dev,struct drm_file *file,
 struct remesa_private_session_lab_request *r)
{
 struct private_lab_session *s;int error,cleanup;
 if(private_lab_attempted||private_lab_active||private_probe_consumed||private_probe_retained)return -EBUSY;
 s=kzalloc(sizeof(*s),GFP_KERNEL);if(!s)return -ENOMEM;
 error=remesa_private_session_acquire(dev,file,r->context,&s->p,&s->p.memory);
 if(error){kfree(s);return error;}
 s->acquired=true;s->serial=1;s->maximum=r->max_jobs;memcpy(s->ids,r->ids,sizeof(s->ids));
 private_lab_attempted=true;private_lab_active=s;
 error=private_lab_identity(dev,r);if(error)goto failed;
 error=private_lab_prepare(s);if(error)goto failed;
 error=private_lab_authenticate(&s->p);if(error)goto failed;
 error=remesa_private_session_seal_inputs(s->p.memory,&s->p,s->p.snapshot.ticket,private_lab_guard);
 if(error)goto failed;
 error=remesa_gem_job_cancel(s->p.memory,s->p.snapshot.ticket);if(error)goto failed;
 private_lab_reset_job(s);
 error=remesa_private_session_source_begin(s->p.memory,&s->p);if(error)goto failed;
 s->source_started=true;private_probe_consumed=true;
 r->session=s->serial;r->state=1;r->stages=16;return 0;
 failed:
 /* OPEN has not submitted hardware. Cancel only known pre-issue metadata;
  * if any cleanup refuses, keep stable owner storage and backing until recovery. */
 cleanup=0;
 if(s->p.snapshot.ticket.operation)cleanup=remesa_gem_job_cancel(s->p.memory,s->p.snapshot.ticket);
 if(!cleanup)cleanup=remesa_private_session_release(s->p.memory,&s->p);
 r->error=error;
 if(cleanup){private_lab_poison(s);r->session=s->serial;r->state=2;}
 else{private_lab_active=NULL;kfree(s);}
 return 0;
}
static int private_lab_submit(struct private_lab_session *s,struct drm_device *dev,
 struct remesa_private_session_lab_request *r)
{
 struct remesa_private_probe *p=&s->p;struct remesa_gem_private_rule rules[11];
 struct retained_job *job;unsigned i;int error;
 if(r->sequence!=s->completed+1||s->completed>=s->maximum)return -ESTALE;
 error=private_lab_identity(dev,r);if(error)goto failed;
 private_lab_reset_job(s);
 error=private_lab_prepare(s);if(error)goto failed;
 r->operation=p->snapshot.ticket.operation;r->stages=1;
 mutex_lock(&p->memory->lock);error=remesa_private_session_inputs_locked(p->memory,p->snapshot.ticket);mutex_unlock(&p->memory->lock);
 if(error)goto failed;
 /* LAB-only cold initialization matches the physically qualified probe. It
  * is not introduced into production FAST or presented as a reuse benchmark. */
 for(i=5;i<10;i++)memset(p->views[i].cpu,0,p->views[i].binding.bytes);
 for(i=0;i<p->views[10].binding.bytes/sizeof(u32);i++)((u32*)p->views[10].cpu)[i]=0x00ffffff;
 error=remesa_stream_remesa_borrowed_owner_init(&p->borrower,&p->owner,dev,p->snapshot.ticket,
        p->views,11,p->snapshot.mmu_end,&private_probe_profile.commands,private_probe_verify,p);
 if(error)goto failed;
 error=remesa_twod_reserve(dev->dev_private,&p->guard);if(error)goto failed;
 for(i=0;i<11;i++){
  unsigned access=i<4?REMESA_MMU_GPU_READ:3u;
  if(p->owner.descriptors[i].domain==SGX535_DOMAIN_LOCAL)access=0;
  if(i<5&&(access&REMESA_MMU_GPU_WRITE)){error=-EACCES;goto failed;}
  rules[i]=(struct remesa_gem_private_rule){s->ids[i],access,access?1u:0u};
 }
 error=remesa_gem_private_prepare(p->memory,p->snapshot.ticket,rules,11,&p->guard,&p->loan);
 if(error)goto failed;
 r->stages|=2;
 error=remesa_private_session_execute(p->memory,p,&p->loan,&p->owner,&p->backend,
                    &p->observation,p->views,11,msecs_to_jiffies(r->budget_ms),100000);
 mutex_lock(&p->memory->lock);job=ticket_job(p->memory,p->snapshot.ticket);r->cookie=job?job->cookie:0;mutex_unlock(&p->memory->lock);
 if(p->loan.execution_started)r->stages|=4;
 r->phase=p->owner.session.phase;r->ledger=p->owner.session.observed_events;
 if(error)goto failed;
 if(!p->loan.execution_retired){error=-EPROTO;goto failed;}
 error=remesa_stream_remesa_borrowed_owner_deinit(&p->borrower);if(error)goto failed;
 error=remesa_gem_private_release(&p->loan);if(error)goto failed;
 error=remesa_twod_release(dev->dev_private,&p->guard);if(error)goto failed;
 error=remesa_gem_job_retire(p->memory,p->snapshot.ticket);if(error)goto failed;
 r->stages|=8;r->state=1;s->completed++;private_lab_reset_job(s);return 0;
 failed:private_lab_poison(s);r->error=error;r->state=2;return 0;
}
int remesa_private_session_lab_ioctl(struct drm_device *dev,void *data,struct drm_file *file)
{
 struct remesa_private_session_lab_request *r=data;struct private_lab_session *s;
 struct remesa_gem_memory *m;int error;
 BUILD_BUG_ON(sizeof(*r)!=136);
 if(!dev||!r||!file||!capable(CAP_SYS_ADMIN))return -EPERM;
 if(!remesa_private_session_lab_request_valid(r))return -EINVAL;
 if(r->action!=3&&(!memory_enabled||!private_probe_enabled))return -EOPNOTSUPP;
 mutex_lock(&private_lab_lock);
 if(r->action==1){error=private_lab_open(dev,file,r);goto done;}
 s=private_lab_active;if(!s||s->serial!=r->session){error=-ENOENT;goto done;}
 m=s->p.memory;mutex_lock(&endpoint_lock);
 error=!m||endpoint!=m||m->dev!=dev||m->file!=file||m->context!=r->context?-EACCES:0;
 mutex_unlock(&endpoint_lock);if(error)goto done;
 if(s->poisoned){r->error=-EIO;r->state=2;error=0;goto done;}
 if(r->action==2){error=private_lab_submit(s,dev,r);goto done;}
 if(r->sequence!=s->completed){error=-ESTALE;goto done;}
 if(s->source_started){
  error=remesa_private_session_source_end(m,&s->p);
  if(error){
   mutex_lock(&m->lock);s->poisoned=m->private_source_poisoned;mutex_unlock(&m->lock);
   r->error=error;r->state=s->poisoned?2:1;error=0;goto done;
  }
  s->source_started=false;
 }
 error=remesa_private_session_release(m,&s->p);
 if(error){r->error=error;r->state=1;error=0;goto done;}
 private_lab_active=NULL;kfree(s);r->state=3;r->stages=32;error=0;
 done:mutex_unlock(&private_lab_lock);return error;
}
