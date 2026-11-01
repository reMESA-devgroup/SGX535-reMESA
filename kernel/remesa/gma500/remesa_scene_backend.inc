/* SPDX-License-Identifier: GPL-2.0-only */
/* Native plan consumer. Included after the real fixed IO/action primitives.
 * Caller owns endpoint/MMU writer, device/power, immutable inputs and source
 * lease. This is not an ioctl, source claim or general register-list API. */
#ifdef REMESA_GENERAL_SCENE
static int scene_backend_run(void *context,enum sgx535_fixed_stage stage,
 const void *payload,size_t count,struct sgx535_fixed_observation *observation)
{
 struct sgx535_gma500_fixed_backend *b=context;
 struct remesa_gem_scene_job *c;
 const struct remesa_scene_service_plan *p;
 struct sgx535_frozen_reg_action schedule[29];
 struct sgx535_frozen_reg_write init[12];
 const struct remesa_scene_fire_payload *fire;
 struct drm_device *dev;struct drm_psb_private *priv;
 unsigned long flags;unsigned i;int error=-EINVAL;u64 mark;
 if(!b||!b->active||!b->power_held||b->owner||!b->general_scene||
    !observation||stage!=b->last_stage+1u)return -EINVAL;
 c=b->general_scene;p=&c->service_plan;dev=c->snapshot.dev;priv=dev?dev->dev_private:NULL;
 if(!dev||!priv||!priv->sgx_reg||!gma_power_is_on(dev))return -ENODEV;
 if(b->private_loan!=&c->loan||!c->loan.live||!c->loan.execution_attempted||
    !c->loan.execution_started||c->service.session!=&c->session||c->service.plan!=p)return -EACCES;
 switch(stage){
 case SGX535_FIXED_PUBLISH_CPU:
  if(payload||count)break;
  mark=ktime_get_ns();error=remesa_gem_private_publish_locked(&c->loan,c->views,c->count);
  b->cpu_flush_ns=ktime_get_ns()-mark;break;
 case SGX535_FIXED_PUBLISH_TRANSLATIONS:case SGX535_FIXED_DEVICE_MAINTAIN:
  if(payload||count||!c->loan.host_published)break;
  error=remesa_gem_private_validate_locked(&c->loan,c->views,c->count);
  if(!error&&stage==SGX535_FIXED_DEVICE_MAINTAIN)error=remesa_mmu_private_maintain_locked(c->loan.owner);
  break;
 case SGX535_FIXED_INIT_WRITES:
  if(!payload||count!=12||remesa_stream_sgx535_frozen_rev121_init_writes(0x10201,init,12)||memcmp(payload,init,sizeof(init)))break;
  for(i=0;i<12;i++){error=fixed_write(b,init[i].offset,init[i].value);if(error)break;}break;
 case SGX535_FIXED_XHW_INIT:
  if(payload||count||b->xhw_internal)break;
  b->xhw_internal=true;error=0;break;
 case SGX535_FIXED_TA_INFO:
  if(!b->xhw_internal||!payload||count!=29||memcmp(payload,p->load,sizeof(p->load)))break;
  error=0;break;
 case SGX535_FIXED_SCENE_INFO:
  if(!b->xhw_internal||count!=1||payload!=p)break;
  error=0;break;
 case SGX535_FIXED_TA_LOAD:
  if(!b->xhw_internal||!payload||count!=29||memcmp(payload,p->load,sizeof(p->load)))break;
  flags=sgx535_gma500_fixed_irq_lock();b->prefire_load_active=true;sgx535_gma500_fixed_irq_unlock(flags);
  error=fixed_actions(b,payload,p->load,29);
  flags=sgx535_gma500_fixed_irq_lock();b->prefire_load_active=false;sgx535_gma500_fixed_irq_unlock(flags);
  if(!error){observation->load_flags=0x1f;observation->status2=15;observation->initend=0x400000;}
  break;
 case SGX535_FIXED_SCENE_VALIDATE:
  if(payload||count||!c->loan.host_published)break;
  /* Full instruction/reference closure already validated before binding this
   * immutable plan. Retain live backing/PTE authority before scheduling. */
  error=remesa_gem_private_validate_locked(&c->loan,c->views,c->count);break;
 case SGX535_FIXED_USE_RESERVE:case SGX535_FIXED_USE_PROGRAM:case SGX535_FIXED_USE_RELEASE:
  if(!payload||count!=1||memcmp(payload,&p->use,sizeof(p->use)))break;
  if(stage==SGX535_FIXED_USE_RESERVE){if(b->use_reserved)break;b->use_reserved=true;error=0;break;}
  if(!b->use_reserved)break;
  if(stage==SGX535_FIXED_USE_RELEASE){
   if(c->session.phase!=SGX535_PHASE_SCENE_COMPLETED)break;
   b->use_reserved=false;error=0;break;
  }
  error=0;
  for(i=3;i<16;i++)if(p->use.mask&(1u<<i)){
   error=fixed_write(b,p->use.banks[i].offset,p->use.banks[i].word);if(error)break;
  }
  break;
 case SGX535_FIXED_STATUS_BASELINE:
  if(payload||count)break;
  flags=sgx535_gma500_fixed_irq_lock();error=fixed_read(b,PSB_CR_EVENT_STATUS,&observation->status1);
  if(!error)error=fixed_read(b,PSB_CR_EVENT_STATUS2,&observation->status2);
  sgx535_gma500_fixed_irq_unlock(flags);break;
 case SGX535_FIXED_TA_SCHEDULE:
  if(count!=8||remesa_stream_sgx535_frozen_ta_schedule_plan(&p->commands,schedule,8))break;
  error=fixed_actions(b,payload,schedule,8);break;
 case SGX535_FIXED_TA_FIRE:case SGX535_FIXED_RASTER_FIRE:
  if(!payload||count!=1)break;
  fire=payload;i=stage==SGX535_FIXED_TA_FIRE?31:20;
  if(fire->action_count!=i||memcmp(fire->actions,stage==SGX535_FIXED_TA_FIRE?p->ta:p->raster,i*sizeof(fire->actions[0])))break;
  if(stage==SGX535_FIXED_TA_FIRE){
   flags=sgx535_gma500_fixed_irq_lock();b->fire_possible=true;b->deadline=jiffies+c->session.timeout_ticks;sgx535_gma500_fixed_irq_unlock(flags);
   remesa_route_issued(b);
  }
  mark=ktime_get_ns();if(stage==SGX535_FIXED_TA_FIRE)b->ta_fire_begin_ns=mark;else b->raster_fire_begin_ns=mark;
  error=fixed_actions(b,fire->actions,stage==SGX535_FIXED_TA_FIRE?p->ta:p->raster,i);
  if(stage==SGX535_FIXED_TA_FIRE)b->ta_fire_end_ns=ktime_get_ns();else b->raster_fire_end_ns=ktime_get_ns();break;
 case SGX535_FIXED_ISP_RESET_ASSERT:case SGX535_FIXED_ISP_RESET_CLEAR:case SGX535_FIXED_RASTER_SCHEDULE:
  if(remesa_stream_sgx535_frozen_raster_schedule_plan(&p->commands,schedule,29))break;
  if(stage==SGX535_FIXED_ISP_RESET_ASSERT){
   if(count!=1||b->isp_reset_asserted)break;
   b->isp_reset_asserted=true;error=fixed_actions(b,payload,schedule,1);
  }else if(stage==SGX535_FIXED_ISP_RESET_CLEAR){
   if(count!=1||!b->isp_reset_asserted)break;
   error=fixed_actions(b,payload,schedule+1,1);if(!error)b->isp_reset_asserted=false;
  }else{
   if(count!=27||b->isp_reset_asserted)break;
   error=fixed_actions(b,payload,schedule+2,27);
  }
  break;
 default:break;
 }
 if(!error)b->last_stage=stage;
 return error;
}
const struct sgx535_fixed_ops remesa_scene_backend_ops={scene_backend_run};
static void scene_backend_observer_begin(void *b,struct remesa_scene_service *s,
 const struct sgx535_fixed_ops *ops,const struct sgx535_fixed_status_source *source,void *context)
{remesa_route_scene_service(b,s,ops==&remesa_scene_backend_ops&&source==&sgx535_gma500_fixed_backend_status_source&&context==b);}
static void scene_backend_observer_before(void *b,struct remesa_scene_service *s,u32 q,u32 a,u32 d,int owned)
{remesa_route_scene_before(b,s,q,a,d,owned);}
static void scene_backend_observer_after(void *b,struct remesa_scene_service *s)
{remesa_route_scene_after(b,s);}
static void scene_backend_observer_terminal(void *b,struct remesa_scene_service *s,int result)
{remesa_route_scene_terminal(b,s,result);}
static const struct remesa_scene_service_observer scene_backend_observer={scene_backend_observer_begin,
 scene_backend_observer_before,scene_backend_observer_after,scene_backend_observer_terminal};
int remesa_scene_backend_begin(struct sgx535_gma500_fixed_backend *b,struct remesa_gem_scene_job *c)
{
 struct drm_device *dev;struct drm_psb_private *priv;struct remesa_twod_reservation *guard;
 struct sgx535_source_facts facts={0};unsigned long flags;int error;
 if(!b||b->active||b->owner||b->general_scene||b->private_loan||!c||
    c->state!=REMESA_SCENE_JOB_PREPARED||!c->loan.live||c->loan.host_published||
    !c->loan.execution_attempted||c->loan.execution_started||c->loan.execution_retired||
    c->loan.memory!=c->memory||c->service.session!=&c->session||c->service.plan!=&c->service_plan||
    c->session.phase!=SGX535_PHASE_HOST_IMAGE_FINAL||c->service.started)return -EINVAL;
 dev=c->snapshot.dev;priv=dev?dev->dev_private:NULL;guard=c->loan.guard;
 if(!dev||!priv||!priv->sgx_reg||!gma_power_is_on(dev)||
    !guard||!guard->held||guard->device!=dev->dev_private||guard->mmu_owner!=&c->loan)return -ENODEV;
 error=remesa_gem_private_validate_locked(&c->loan,c->views,c->count);if(error)return error;
 /* Same two read-only boundary/IRQ handoff intervals as the fixed backend.
  * No status drain, wake, reset, rearm or release on a failed boundary. */
 flags=sgx535_gma500_fixed_irq_lock();
 if(fixed_irq_owner||remesa_twod_irq_handoff_ready_locked(guard,&c->loan,dev)||!remesa_route_source_begin(b,dev)){
  sgx535_gma500_fixed_irq_unlock(flags);return -EBUSY;
 }
 sgx535_gma500_source_snapshot(dev->dev_private,&facts);
 facts.pending|=b->pending_status1|b->pending_status2;facts.busy|=b->fire_possible;facts.coverage_complete=0;
 if(!remesa_route_source_boundary(b,&facts)){sgx535_gma500_fixed_irq_unlock(flags);return -EAGAIN;}
 sgx535_gma500_fixed_irq_unlock(flags);
 b->general_scene=c;b->private_loan=&c->loan;b->active=true;b->power_held=true;b->last_stage=0;
 flags=sgx535_gma500_fixed_irq_lock();
 error=(fixed_irq_owner||remesa_twod_irq_handoff_ready_locked(guard,&c->loan,dev))?-EBUSY:
  remesa_twod_irq_handoff_fixed_locked(guard,&c->loan,b,dev);
 if(error){
  b->active=false;b->power_held=false;b->private_loan=NULL;b->general_scene=NULL;
  sgx535_gma500_fixed_irq_unlock(flags);return error;
 }
 sgx535_gma500_fixed_irq_unlock(flags);
 remesa_route_admit(b);c->service.observer=&scene_backend_observer;c->service.observer_context=b;return 0;
}
#endif
