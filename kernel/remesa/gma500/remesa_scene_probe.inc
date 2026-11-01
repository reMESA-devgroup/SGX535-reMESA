/* SPDX-License-Identifier: GPL-2.0-only */
/* Separate closed-profile one-shot LAB entry. Never used in FAST frames. */
#include "remesa_scene_probe.h"
#include "remesa_scene_packet.h"
#include "remesa_twod_reservation.h"
#include "remesa_scene_probe_request.h"
#include "remesa_scene_probe_profiles.h"
#include "remesa_private_session.h"
static DEFINE_MUTEX(scene_probe_lock);
static bool scene_probe_enabled;
module_param_named(remesa_scene_probe_enabled,scene_probe_enabled,bool,0600);
struct remesa_scene_probe {
 struct remesa_gem_memory *memory;
 struct remesa_scene_packet packet;
 struct remesa_gem_scene_job job;
 struct remesa_twod_reservation guard;
 struct sgx535_gma500_fixed_backend backend;
 struct remesa_stream_observation observation;
 bool source_claimed,execution_consumed,backing_owned;
};
static struct remesa_scene_probe *scene_probe_retained;
static bool scene_probe_consumed;
static struct remesa_scene_probe_trace scene_probe_trace;
/* scene_probe_lock serializes both snapshot publication and its read-only
 * query. No allocation, register read or new source sample is introduced. */
static void scene_probe_save_trace(const struct remesa_scene_probe *p,
 const struct remesa_scene_probe_request *r)
{
 struct remesa_scene_probe_trace *t=&scene_probe_trace;unsigned i;
 BUILD_BUG_ON(sizeof(*t)!=4256);
 BUILD_BUG_ON(sizeof(p->job.service_plan.load)!=sizeof(t->load_actions));
 BUILD_BUG_ON(sizeof(p->job.service_plan.ta)!=sizeof(t->ta_actions));
 BUILD_BUG_ON(sizeof(p->job.service_plan.raster)!=sizeof(t->raster_actions));
 memset(t,0,sizeof(*t));t->version=1;t->bytes=sizeof(*t);
 t->profile=r->profile;t->stages=r->stages;t->phase=r->phase;t->ledger=r->ledger;
 t->error=r->error;t->retained=r->retained;t->context=r->context;
 t->operation=r->operation;t->cookie=r->cookie;
 t->cpu_flush_ns=p->backend.cpu_flush_ns;t->ta_begin_ns=p->backend.ta_fire_begin_ns;
 t->ta_end_ns=p->backend.ta_fire_end_ns;t->raster_begin_ns=p->backend.raster_fire_begin_ns;
 t->raster_end_ns=p->backend.raster_fire_end_ns;
 t->event_count=p->observation.event_count;
 if(t->event_count>64)t->event_count=64;
 t->zero_samples=p->observation.zero_samples;
 for(i=0;i<t->event_count;i++){
  const struct remesa_stream_source_event *e=&p->observation.events[i];
  t->events[i]=(struct remesa_scene_trace_event){e->kind,e->sequence,e->status1,e->status2,e->ledger,e->phase,p->observation.event_ns[i]};
 }
 t->binding_count=p->job.count>17?17:p->job.count;
 for(i=0;i<t->binding_count;i++)t->bindings[i]=(struct remesa_scene_trace_binding){
  p->job.views[i].binding.id,p->packet.bindings[i].access,p->job.views[i].binding.va,p->job.views[i].binding.bytes};
 memcpy(t->ta,p->job.service_plan.commands.ta,sizeof(t->ta));
 memcpy(t->raster,p->job.service_plan.commands.raster,sizeof(t->raster));
 for(i=0;i<16;i++){t->use_banks[2*i]=p->job.service_plan.use.banks[i].offset;t->use_banks[2*i+1]=p->job.service_plan.use.banks[i].word;}
 t->use_mask=p->job.service_plan.use.mask;t->load_count=p->job.service_plan.load_count;
 t->ta_count=p->job.service_plan.ta_count;t->raster_count=p->job.service_plan.raster_count;
 memcpy(t->load_actions,p->job.service_plan.load,sizeof(t->load_actions));
 memcpy(t->ta_actions,p->job.service_plan.ta,sizeof(t->ta_actions));
 memcpy(t->raster_actions,p->job.service_plan.raster,sizeof(t->raster_actions));
}
int remesa_scene_trace_ioctl(struct drm_device *dev,void *data,struct drm_file *file)
{
 struct remesa_scene_trace_request *r=data;const u8 *bytes;unsigned i;int error=0;
 BUILD_BUG_ON(sizeof(*r)!=4272);BUILD_BUG_ON(offsetof(struct remesa_scene_probe_trace,events)!=1800);
 if(!dev||!r||!file||!capable(CAP_SYS_ADMIN))return -EPERM;
 if(!memory_enabled||!scene_probe_enabled)return -EOPNOTSUPP;
 if(r->version!=1||r->reserved||!r->context)return -EINVAL;
 bytes=(const u8 *)&r->trace;for(i=0;i<sizeof(r->trace);i++)if(bytes[i])return -EINVAL;
 mutex_lock(&scene_probe_lock);mutex_lock(&endpoint_lock);
 if(!endpoint||endpoint->dev!=dev||endpoint->file!=file||endpoint->private_device_lost||endpoint->context!=r->context)error=-EACCES;
 else if(!scene_probe_trace.version||scene_probe_trace.context!=r->context)error=-ENODATA;
 else r->trace=scene_probe_trace;
 mutex_unlock(&endpoint_lock);mutex_unlock(&scene_probe_lock);return error;
}
static u32 scene_probe_id(const struct remesa_scene_packet *original,const u32 *ids,u32 old)
{unsigned i;for(i=0;i<17;i++)if(original->bindings[i].id==old)return ids[i];return 0;}
static int scene_probe_packet(struct remesa_scene_probe *p,const struct remesa_scene_probe_profile *profile,
 const struct remesa_scene_probe_request *r)
{
 struct remesa_scene_packet *original;unsigned i;
 if(profile->packet_bytes!=sizeof(p->packet))return -EPROTO;
 /* Temporary source preserves captured IDs while translating only metadata.
  * No large packet or general context is allocated on the kernel stack. */
 original=kmemdup(profile->packet,sizeof(*original),GFP_KERNEL);if(!original)return -ENOMEM;
 if(!remesa_scene_packet_valid(original)||original->binding_count!=17){kfree(original);return -EPROTO;}
 p->packet=*original;p->packet.owner=r->context;
 for(i=0;i<17;i++)p->packet.bindings[i].id=r->ids[i];
 p->packet.actions.region_id=scene_probe_id(original,r->ids,original->actions.region_id);
 p->packet.actions.table_id=scene_probe_id(original,r->ids,original->actions.table_id);
 p->packet.actions.parameter_id=scene_probe_id(original,r->ids,original->actions.parameter_id);
 p->packet.controls.color_id=scene_probe_id(original,r->ids,original->controls.color_id);
 p->packet.controls.depth_id=scene_probe_id(original,r->ids,original->controls.depth_id);
 p->packet.controls.ta_id=scene_probe_id(original,r->ids,original->controls.ta_id);
 p->packet.controls.background_id=scene_probe_id(original,r->ids,original->controls.background_id);
 p->packet.controls.event_id=scene_probe_id(original,r->ids,original->controls.event_id);
 for(i=0;i<p->packet.use_count;i++)p->packet.uses[i].id=scene_probe_id(original,r->ids,original->uses[i].id);
 for(i=0;i<p->packet.window_count;i++){
  p->packet.windows[i].index_id=scene_probe_id(original,r->ids,original->windows[i].index_id);
  p->packet.windows[i].vertex_id=scene_probe_id(original,r->ids,original->windows[i].vertex_id);
 }
 kfree(original);return remesa_scene_packet_valid(&p->packet)?0:-EPROTO;
}
static int scene_probe_images(struct remesa_scene_probe *p,const struct remesa_scene_probe_profile *profile)
{
 unsigned i;u8 digest[32];int error;
 for(i=0;i<17;i++)if(profile->bindings[i].access==1){
  const struct remesa_memory_view *v=&p->job.views[i];
  if(!v->cpu||v->binding.bytes!=profile->bindings[i].bytes)return -EACCES;
  error=remesa_depth1280_sha256(v->cpu,v->binding.bytes,digest);if(error)return error;
  if(memcmp(digest,profile->bindings[i].digest,32))return -EKEYREJECTED;
 }
 return 0;
}
/* Caller owns endpoint_lock; backing lease survives every refused cleanup. */
static int scene_probe_cancel_locked(struct remesa_scene_probe *p)
{
 int error;
 if(p->execution_consumed||p->job.loan.execution_attempted)return -EBUSY;
 if(p->job.memory){error=remesa_gem_scene_job_cancel(&p->job);if(error)return error;}
 if(p->guard.held){error=remesa_twod_release(p->guard.device,&p->guard);if(error)return error;}
 return 0;
}
int remesa_scene_probe_ioctl(struct drm_device *dev,void *data,struct drm_file *file)
{
 struct remesa_scene_probe_request *r=data;const struct remesa_scene_probe_profile *profile;
 struct remesa_gem_memory *s;struct drm_psb_private *dev_priv;struct remesa_scene_probe *p=NULL;
 struct retained_job *job;unsigned i;int error,cleanup=0;
 BUILD_BUG_ON(sizeof(scene_probe_profiles)/sizeof(scene_probe_profiles[0])!=REMESA_SCENE_PROBE_PROFILE_COUNT);
 BUILD_BUG_ON(sizeof(*r)!=144);BUILD_BUG_ON(offsetof(struct remesa_scene_probe_request,operation)!=88);
 if(!dev||!r||!file||!capable(CAP_SYS_ADMIN))return -EPERM;
 if(!memory_enabled||!scene_probe_enabled)return -EOPNOTSUPP;
 if(!remesa_scene_probe_request_valid(r))return -EINVAL;
 mutex_lock(&scene_probe_lock);
 if(scene_probe_retained||scene_probe_consumed){error=-EBUSY;goto done;}
 p=kzalloc(sizeof(*p),GFP_KERNEL);if(!p){error=-ENOMEM;goto done;}
 error=remesa_private_session_acquire(dev,file,r->context,p,&p->memory);
 if(error){kfree(p);p=NULL;goto done;}
 p->backing_owned=true;scene_probe_retained=p;s=p->memory;
 profile=&scene_probe_profiles[r->profile-1];
 mutex_lock(&endpoint_lock);
 if(endpoint!=s||s->private_device_lost||s->file!=file||s->private_session!=p){error=-ENODEV;goto cancel_locked;}
 dev_priv=dev->dev_private;
 if(!dev_priv||!dev_priv->sgx_reg||!gma_power_is_on(dev)){error=-ENODEV;goto cancel_locked;}
 r->core=PSB_RSGX32(PSB_CR_CORE_ID);r->revision=PSB_RSGX32(PSB_CR_CORE_REVISION);r->pds_base=PSB_RSGX32(PSB_CR_PDS_EXEC_BASE);
 if(r->core!=0x01130000u||r->revision!=0x00010201u||r->pds_base!=0x20000000u){error=-ENODEV;goto cancel_locked;}
 mutex_lock(&s->lock);error=0;
 if(s->executor||s->stream_bound||s->ta_cache_domains||s->root.end+1!=0xe0000000ULL){error=-EBUSY;goto bindings_done;}
 for(i=0;i<8;i++)if(s->jobs[i].live){error=-EBUSY;goto bindings_done;}
 for(i=0;i<17;i++){
  u32 id=r->ids[i];struct backing *b=&s->bo[id&31];const struct remesa_scene_probe_binding *expected=&profile->bindings[i];
  if(!b->live||b->generation!=id>>5){error=-ENOENT;break;}
  if(b->va.start!=expected->address||b->gem.size!=expected->bytes||b->lease||b->holds||
     !b->pinned||!b->reserved||b->quarantine||atomic_read(&b->color_vmas)){error=-EACCES;break;}
  if(expected->access==3&&(!b->initialized||!b->zero_pristine||b->cached_job_view)){error=-ENODATA;break;}
 }
 bindings_done:mutex_unlock(&s->lock);if(error)goto cancel_locked;
 error=scene_probe_packet(p,profile,r);if(error)goto cancel_locked;
 error=remesa_twod_reserve(dev_priv,&p->guard);if(error)goto cancel_locked;
 mutex_unlock(&endpoint_lock);
 /* Existing opaque backing/source leases keep all endpoint/device dependencies
  * alive even before a job exists. No fabricated canonical session or owner. */
 error=remesa_private_session_source_begin(s,p);
 if(error)goto reacquire_cancel;
 p->source_claimed=true;scene_probe_consumed=true;
 mutex_lock(&endpoint_lock);
 if(endpoint!=s||s->private_device_lost||s->file!=file||s->private_session!=p){error=-ENODEV;cleanup=-ENODEV;goto unlock_cleanup;}
 error=remesa_gem_scene_packet_prepare(s,&p->packet,&p->guard,&p->job);if(error)goto cancel_locked;
 r->operation=p->job.snapshot.ticket.operation;r->stages=1;
 error=scene_probe_images(p,profile);if(error)goto cancel_locked;
 r->stages|=2;
 error=remesa_gem_scene_job_execute(&p->job,&p->backend,&p->observation,p,msecs_to_jiffies(r->budget_ms),100000);
 p->execution_consumed=p->job.loan.execution_attempted;
 mutex_lock(&s->lock);job=ticket_job(s,p->job.snapshot.ticket);r->cookie=job?job->cookie:0;mutex_unlock(&s->lock);
 if(p->job.loan.execution_started)r->stages|=4;
 r->phase=p->job.session.phase;r->ledger=p->job.session.observed_events;
 if(error)goto cancel_locked;
 if(!p->job.loan.execution_retired||p->job.state!=REMESA_SCENE_JOB_ACKNOWLEDGED){error=-EPROTO;goto cancel_locked;}
 /* Backing lease pins the endpoint through every failure, including after
  * job/guard release. Source lease ends only after those idle prerequisites. */
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
 if(!cleanup){if(!error)r->stages|=8;scene_probe_retained=NULL;}
 else if(!error)error=cleanup;
 r->error=error;r->retained=scene_probe_retained!=NULL;
 if(scene_probe_consumed)scene_probe_save_trace(p,r);
 if(!cleanup){kfree(p);p=NULL;}
 error=0;
 done:mutex_unlock(&scene_probe_lock);return error;
}
