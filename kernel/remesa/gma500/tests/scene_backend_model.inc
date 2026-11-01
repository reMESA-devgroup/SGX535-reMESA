/* SPDX-License-Identifier: GPL-2.0-only. Native consumer, modeled IO/GEM hooks. */
#define REMESA_GENERAL_SCENE 1
#include "frozen_source_guard.h"
struct sgx535_gma500_owner {struct drm_device *dev;struct sgx535_frozen_session session;struct sgx535_fixed_service fixed_service;};
#include "scene_backend_types.inc"
#include "../remesa_scene_backend.h"
static struct sgx535_gma500_fixed_backend *fixed_irq_owner;
static unsigned long jiffies;
#define PSB_CR_EVENT_STATUS 0x12c
#define PSB_CR_EVENT_STATUS2 0x130
static struct {struct remesa_gem_scene_job *c;struct scene_service_model protocol;unsigned stage,fault,writes,actions,issues,admitted,locks;bool fail_io;} backend_model;
static unsigned backend_model_runs;
static unsigned long sgx535_gma500_fixed_irq_lock(void){CHECK(!backend_model.locks);backend_model.locks++;return 0;}
static void sgx535_gma500_fixed_irq_unlock(unsigned long flags){(void)flags;CHECK(backend_model.locks==1);backend_model.locks--;}
static int gma_power_is_on(struct drm_device *dev){return dev&&dev->dev_private&&dev->dev_private->sgx_reg;}
static u64 ktime_get_ns(void){static u64 ticks;return ++ticks;}
static int backend_validate(struct remesa_gem_private *loan,const struct remesa_memory_view *views,unsigned count)
{CHECK(loan==&backend_model.c->loan&&views==backend_model.c->views&&count==17);return backend_model.fault==30?-EACCES:0;}
static int backend_publish(struct remesa_gem_private *loan,const struct remesa_memory_view *views,unsigned count)
{CHECK(!backend_validate(loan,views,count)&&!loan->host_published);if(backend_model.fault==1)return -EIO;loan->host_published=true;return 0;}
static int backend_maintain(struct remesa_mmu_private *owner)
{CHECK(owner==backend_model.c->loan.owner);return backend_model.fault==3?-EIO:0;}
static int fixed_write(void *b,u32 offset,u32 value)
{CHECK(b&&offset<0x8000&&!(offset&3));(void)value;backend_model.writes++;return backend_model.fault==backend_model.stage?-EIO:0;}
static int fixed_read(void *b,u32 offset,u32 *value)
{CHECK(b&&(offset==PSB_CR_EVENT_STATUS||offset==PSB_CR_EVENT_STATUS2));*value=0;return backend_model.fault==backend_model.stage?-EIO:0;}
static int fixed_actions(struct sgx535_gma500_fixed_backend *b,const struct sgx535_frozen_reg_action *a,const struct sgx535_frozen_reg_action *e,size_t n)
{
 CHECK(b&&a&&e&&!memcmp(a,e,n*sizeof(*e)));backend_model.actions++;
 if(backend_model.stage==SGX535_FIXED_TA_LOAD)CHECK(b->prefire_load_active);
 if(backend_model.stage==SGX535_FIXED_TA_FIRE)CHECK(b->fire_possible&&b->deadline==jiffies+backend_model.c->session.timeout_ticks&&backend_model.issues==1);
 if(backend_model.stage==SGX535_FIXED_ISP_RESET_ASSERT)CHECK(b->isp_reset_asserted);
 return backend_model.fault==backend_model.stage?-EIO:0;
}
static void remesa_route_issued(struct sgx535_gma500_fixed_backend *b)
{CHECK(b->general_scene==backend_model.c&&b->fire_possible);backend_model.issues++;}
static void remesa_route_admit(struct sgx535_gma500_fixed_backend *b)
{CHECK(b->general_scene==backend_model.c&&b->active&&fixed_irq_owner==b);backend_model.admitted++;}
static int remesa_route_source_begin(struct sgx535_gma500_fixed_backend *b,struct drm_device *d)
{CHECK(b&&d==backend_model.c->snapshot.dev&&backend_model.locks);return backend_model.fault!=31;}
static int remesa_route_source_boundary(struct sgx535_gma500_fixed_backend *b,const struct sgx535_source_facts *f)
{CHECK(b&&f&&!f->coverage_complete&&backend_model.locks);return backend_model.fault!=32;}
static void sgx535_gma500_source_snapshot(struct drm_psb_private *p,struct sgx535_source_facts *f)
{CHECK(p==backend_model.c->snapshot.dev->dev_private);memset(f,0,sizeof(*f));}
static int remesa_twod_irq_handoff_ready_locked(struct remesa_twod_reservation *g,const void *token,struct drm_device *d)
{CHECK(g==backend_model.c->loan.guard&&token==&backend_model.c->loan&&d==backend_model.c->snapshot.dev&&backend_model.locks);return backend_model.fault==33?-EBUSY:0;}
static int remesa_twod_irq_handoff_fixed_locked(struct remesa_twod_reservation *g,const void *token,struct sgx535_gma500_fixed_backend *b,struct drm_device *d)
{CHECK(!remesa_twod_irq_handoff_ready_locked(g,token,d));if(backend_model.fault==34)return -EIO;fixed_irq_owner=b;return 0;}
static void remesa_route_scene_service(struct sgx535_gma500_fixed_backend *b,struct remesa_scene_service *s,int matched)
{CHECK(b->general_scene==backend_model.c&&s==&backend_model.c->service&&matched);}
static void remesa_route_scene_before(struct sgx535_gma500_fixed_backend *b,struct remesa_scene_service *s,u32 q,u32 a,u32 d,int owned)
{CHECK(b&&s&&q==91&&owned);(void)a;(void)d;}
static void remesa_route_scene_after(struct sgx535_gma500_fixed_backend *b,struct remesa_scene_service *s){CHECK(b&&s);}
static void remesa_route_scene_terminal(struct sgx535_gma500_fixed_backend *b,struct remesa_scene_service *s,int result){CHECK(b&&s);(void)result;}
static int backend_sample(void *context,u32 q,u32 *a,u32 *d,int *owned)
{CHECK(context&&q==91);return scene_model_sample(&backend_model.protocol,q,a,d,owned);}
static const struct sgx535_fixed_status_source sgx535_gma500_fixed_backend_status_source={backend_sample};
#define remesa_gem_private_validate_locked backend_validate
#define remesa_gem_private_publish_locked backend_publish
#define remesa_mmu_private_maintain_locked backend_maintain
#define remesa_stream_sgx535_frozen_rev121_init_writes sgx535_frozen_rev121_init_writes
#define remesa_stream_sgx535_frozen_ta_schedule_plan sgx535_frozen_ta_schedule_plan
#define remesa_stream_sgx535_frozen_raster_schedule_plan sgx535_frozen_raster_schedule_plan
#include "../remesa_scene_backend.inc"
#undef remesa_gem_private_validate_locked
#undef remesa_gem_private_publish_locked
#undef remesa_mmu_private_maintain_locked
/* Validate each payload against the existing original-plan oracle, then run
 * the actual native consumer. This wrapper is only a CPU test, never evidence
 * of real source attribution; observer identity is tested separately above. */
static int backend_checked_run(void *context,enum sgx535_fixed_stage stage,const void *p,size_t n,struct sgx535_fixed_observation *o)
{
 struct sgx535_fixed_observation expected={0};backend_model.stage=stage;
 CHECK(!scene_model_run(&backend_model.protocol,stage,p,n,&expected));
 return remesa_scene_backend_ops.run(context,stage,p,n,o);
}
static const struct sgx535_fixed_ops backend_checked_ops={backend_checked_run};
static void scene_backend_cases(const struct remesa_gem_scene_job *job)
{
 const unsigned faults[]={0,1,3,4,8,11,12,13,14,15,16,17,18,30,31,32,33,34};
 for(unsigned k=0;k<sizeof(faults)/sizeof(faults[0]);k++){
  struct remesa_gem_scene_job *c=malloc(sizeof(*c));CHECK(c);*c=*job;
  struct remesa_twod_reservation guard=*job->loan.guard;c->loan.guard=&guard;guard.mmu_owner=&c->loan;
  c->loan.execution_attempted=true;c->service=(struct remesa_scene_service){0};
  CHECK(!remesa_scene_service_bind(&c->service,&c->session,&c->service_plan));
  struct sgx535_gma500_fixed_backend b={0};fixed_irq_owner=NULL;
  backend_model=(__typeof__(backend_model)){.c=c,.fault=faults[k]};
  backend_model.protocol=(struct scene_service_model){.plan=&c->service_plan,.service=&c->service};
  void *saved=c->snapshot.dev->dev_private->sgx_reg;c->snapshot.dev->dev_private->sgx_reg=(void*)1;
  int error=remesa_scene_backend_begin(&b,c);backend_model_runs++;
  if(faults[k]>=30){CHECK(error&&!b.active&&!b.general_scene&&!b.private_loan&&!fixed_irq_owner);}
  else{
   CHECK(!error&&b.general_scene==c&&!b.owner&&b.private_loan==&c->loan&&backend_model.admitted==1);
   CHECK(remesa_backend_device(&b)==c->snapshot.dev&&remesa_backend_session(&b)==&c->session&&remesa_backend_owner(&b)==c&&remesa_backend_service(&b)==&c->service);
   CHECK(remesa_scene_backend_begin(&b,c)==-EINVAL);
   /* The checked wrapper differs from the hardware ops identity. Test the
    * actual observer's positive identity separately, then suppress it here. */
   c->service.observer->begin(&b,&c->service,&remesa_scene_backend_ops,&sgx535_gma500_fixed_backend_status_source,&b);
   c->service.observer=NULL;c->service.observer_context=NULL;c->loan.execution_started=true;
   error=remesa_scene_service_run_once(&c->service,&backend_checked_ops,&sgx535_gma500_fixed_backend_status_source,&b,0x10201,91,10,2);
   if(!faults[k])CHECK(!error&&c->session.phase==SGX535_PHASE_RETIRED&&c->session.observed_events==7&&b.last_stage==19&&!b.use_reserved&&backend_model.actions==7);
   else CHECK(error&&b.last_stage<faults[k]);
   if(faults[k]==15||faults[k]==16)CHECK(b.isp_reset_asserted);
   if(faults[k]>=14)CHECK(b.fire_possible);
   CHECK(!b.prefire_load_active);
  }
  CHECK(!backend_model.locks);c->snapshot.dev->dev_private->sgx_reg=saved;fixed_irq_owner=NULL;free(c);
 }
}
