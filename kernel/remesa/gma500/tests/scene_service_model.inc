/* SPDX-License-Identifier: GPL-2.0-only. Hardware callbacks modeled, GPU0. */
static unsigned scene_service_model_runs;
struct scene_service_model {
 const struct remesa_scene_service_plan *plan;struct remesa_scene_service *service;
 unsigned fail_stage,source_fault,calls,samples,begins,terminals,before,after;bool asserted;
};
static const struct sgx535_fixed_ops scene_model_ops;
static const struct sgx535_fixed_status_source scene_model_status;
static int scene_model_run(void *context,enum sgx535_fixed_stage stage,const void *payload,size_t count,
 struct sgx535_fixed_observation *observation)
{
 struct scene_service_model *m=context;const struct remesa_scene_service_plan *p=m->plan;
 struct sgx535_frozen_reg_action ta[8],raster[29];struct sgx535_frozen_reg_write init[12];
 CHECK((unsigned)stage==++m->calls&&stage>=1&&stage<=19);
 CHECK(!sgx535_frozen_ta_schedule_plan(&p->commands,ta,8)&&!sgx535_frozen_raster_schedule_plan(&p->commands,raster,29));
 switch(stage){
 case SGX535_FIXED_PUBLISH_CPU:case SGX535_FIXED_PUBLISH_TRANSLATIONS:case SGX535_FIXED_DEVICE_MAINTAIN:
 case SGX535_FIXED_XHW_INIT:case SGX535_FIXED_SCENE_VALIDATE:case SGX535_FIXED_STATUS_BASELINE:CHECK(!payload&&!count);break;
 case SGX535_FIXED_INIT_WRITES:CHECK(count==12&&!sgx535_frozen_rev121_init_writes(0x10201,init,12)&&!memcmp(payload,init,sizeof init));break;
 case SGX535_FIXED_TA_INFO:case SGX535_FIXED_TA_LOAD:CHECK(count==29&&!memcmp(payload,p->load,sizeof p->load));break;
 case SGX535_FIXED_SCENE_INFO:CHECK(count==1&&payload==p);break;
 case SGX535_FIXED_USE_RESERVE:case SGX535_FIXED_USE_PROGRAM:case SGX535_FIXED_USE_RELEASE:
  CHECK(count==1&&!memcmp(payload,&p->use,sizeof p->use));
  if(stage==SGX535_FIXED_USE_RELEASE)CHECK(m->service->session->phase==SGX535_PHASE_SCENE_COMPLETED);
  break;
 case SGX535_FIXED_TA_SCHEDULE:CHECK(count==8&&!memcmp(payload,ta,sizeof ta));break;
 case SGX535_FIXED_TA_FIRE:case SGX535_FIXED_RASTER_FIRE:{
  const struct remesa_scene_fire_payload *fire=payload;
  CHECK(count==1&&fire->action_count==(stage==SGX535_FIXED_TA_FIRE?31u:20u));
  CHECK(!memcmp(fire->actions,stage==SGX535_FIXED_TA_FIRE?p->ta:p->raster,fire->action_count*sizeof(fire->actions[0])));break;}
 case SGX535_FIXED_ISP_RESET_ASSERT:CHECK(count==1&&!memcmp(payload,raster,sizeof raster[0])&&!m->asserted);m->asserted=true;break;
 case SGX535_FIXED_ISP_RESET_CLEAR:CHECK(count==1&&!memcmp(payload,raster+1,sizeof raster[0])&&m->asserted);if(m->fail_stage!=(unsigned)stage)m->asserted=false;break;
 case SGX535_FIXED_RASTER_SCHEDULE:CHECK(count==27&&!memcmp(payload,raster+2,27*sizeof raster[0])&&!m->asserted);break;
 default:CHECK(0);
 }
 if(m->fail_stage==(unsigned)stage)return -EIO;
 if(stage==SGX535_FIXED_TA_LOAD){observation->load_flags=0x1f;observation->status2=15;observation->initend=0x400000;}
 return 0;
}
static int scene_model_sample(void *context,sgx535_u32 sequence,sgx535_u32 *a,sgx535_u32 *d,int *owned)
{
 struct scene_service_model *m=context;CHECK(sequence==91);*d=0;*owned=m->source_fault==21?0:1;
 *a=m->samples++?((1u<<18)|1u):(1u<<13);
 if(m->source_fault==20)return -EIO;
 if(m->source_fault==22){*a=0;*d=16;}
 if(m->source_fault==23)*a=0;
 if(m->source_fault==24)*a=(1u<<18)|1u;
 if(m->source_fault==25)*a=1u<<13;
 return 0;
}
static const struct sgx535_fixed_ops scene_model_ops={scene_model_run};
static const struct sgx535_fixed_status_source scene_model_status={scene_model_sample};
static void scene_model_begin(void *context,struct remesa_scene_service *s,const struct sgx535_fixed_ops *o,
 const struct sgx535_fixed_status_source *source,void *run)
{struct scene_service_model *m=context;CHECK(s==m->service&&o==&scene_model_ops&&source==&scene_model_status&&run==m);m->begins++;}
static void scene_model_before(void *context,struct remesa_scene_service *s,u32 q,u32 a,u32 d,int owned)
{struct scene_service_model *m=context;(void)a;(void)d;(void)owned;CHECK(s==m->service&&q==91);m->before++;}
static void scene_model_after(void *context,struct remesa_scene_service *s)
{struct scene_service_model *m=context;CHECK(s==m->service);m->after++;}
static void scene_model_terminal(void *context,struct remesa_scene_service *s,int result)
{struct scene_service_model *m=context;CHECK(s==m->service);if(!result)CHECK(s->session->phase==SGX535_PHASE_RETIRED);m->terminals++;}
static const struct remesa_scene_service_observer scene_model_observer={scene_model_begin,scene_model_before,scene_model_after,scene_model_terminal};
static void scene_service_cases(const struct remesa_gem_scene_job *job)
{
 for(unsigned fault=0;fault<=25;fault++){
  struct sgx535_frozen_session session=job->session;
  struct remesa_scene_service service={0};
  struct scene_service_model model={.plan=&job->service_plan,.service=&service,
   .fail_stage=fault<=19?fault:0,.source_fault=fault>=20?fault:0};
  CHECK(!remesa_scene_service_bind(&service,&session,&job->service_plan));
  service.observer=&scene_model_observer;service.observer_context=&model;
  int error=remesa_scene_service_run_once(&service,&scene_model_ops,&scene_model_status,&model,0x10201,91,10,2);
  scene_service_model_runs++;
  CHECK(model.begins==1&&model.terminals==1&&model.before==model.after);
  if(!fault)CHECK(!error&&session.phase==SGX535_PHASE_RETIRED&&session.observed_events==7&&model.calls==19&&!model.asserted&&remesa_scene_service_may_release(&service));
  else{
   CHECK(error);
   if(fault<=2)CHECK(session.phase==SGX535_PHASE_ABORTED_BEFORE_SUBMIT&&remesa_scene_service_may_release(&service));
   else CHECK(session.phase==SGX535_PHASE_HELD_AFTER_FAILURE&&!remesa_scene_service_may_release(&service));
  }
 }
}
