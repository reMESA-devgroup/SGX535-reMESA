/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual staged begin/end plus IRQ handoff, source/lifecycle callbacks modeled. */
#define REMESA_PRIVATE_IRQ_HANDOFF 1
#define main reservation_model_main
#include "test_twod_reservation.c"
#undef main
#include <unistd.h>
#include <sys/wait.h>
#define sgx535_gma500_owner model_scene_owner
#define SGX535_PHASE_HOST_IMAGE_FINAL 1u
struct sgx535_source_facts {unsigned pending,busy,coverage_complete;};
static unsigned begins,boundaries,admissions,allow_begin=1,allow_boundary=1;
static const char capsule_service_observer;
static int remesa_route_source_begin(void *b,struct drm_device *dev)
{CHECK(irq_lock_held&&b&&dev);begins++;return allow_begin;}
static void sgx535_gma500_source_snapshot(struct drm_psb_private *p,struct sgx535_source_facts *facts)
{CHECK(irq_lock_held&&p&&facts);}
static int remesa_route_source_boundary(void *b,struct sgx535_source_facts *facts)
{CHECK(irq_lock_held&&b&&!facts->coverage_complete);boundaries++;return allow_boundary;}
static void remesa_route_admit(void *b)
{CHECK(!irq_lock_held&&fixed_irq_owner==b);admissions++;}
static int sgx535_fixed_service_may_release(struct model_fixed_service *s)
{CHECK(!irq_lock_held&&s);return s->may_release;}
static void remesa_route_source_pending(struct drm_device *dev,u32 a,u32 b)
{CHECK(irq_lock_held&&dev);(void)a;(void)b;}
#include "fixed_capture_actual.inc"
#include "fixed_handoff_actual.inc"
static void scenario(unsigned fault)
{
 struct drm_device dev={.powered=1},otherdev={.powered=1};
 struct drm_psb_private p={.dev=&dev,.sgx_reg=(void*)1,.fifo=_PSB_C2_SOCIF_EMPTY,.base=0x28000000};
 struct remesa_twod_reservation r={0},r2={0};unsigned token=0,wrong=0;
 struct model_scene_owner owner={.dev=&dev,.initialized=true,.device_claimed=true,.power_held=true,.session={SGX535_PHASE_HOST_IMAGE_FINAL}};
 owner.fixed_service.session=&owner.session;
 struct sgx535_gma500_fixed_backend b={0};dev.dev_private=&p;
 unsigned admitted=admissions,begin=begins,boundary=boundaries;
 CHECK(!remesa_twod_reserve(&p,&r));CHECK(!remesa_twod_mmu_guard_acquire(&p,&r,&token));
 CHECK(sgx535_gma500_fixed_backend_begin(&b,&owner)==-EBUSY&&begins==begin);
 CHECK(sgx535_gma500_fixed_backend_begin_guarded(&b,&owner,&r,&wrong)==-EBUSY);
 CHECK(!b.active&&twod_irq_owner==&r&&!fixed_irq_owner&&!twod_irq_fixed_lender);
 owner.initialized=false;
 CHECK(sgx535_gma500_fixed_backend_begin_guarded(&b,&owner,&r,&token)==-ENODEV);
 owner.initialized=true;allow_begin=0;
 CHECK(sgx535_gma500_fixed_backend_begin_guarded(&b,&owner,&r,&token)==-EBUSY);
 allow_begin=1;allow_boundary=0;
 CHECK(sgx535_gma500_fixed_backend_begin_guarded(&b,&owner,&r,&token)==-EAGAIN);
 CHECK(!b.active&&twod_irq_owner==&r&&!fixed_irq_owner&&boundaries==boundary+1);
 allow_boundary=1;
 CHECK(!sgx535_gma500_fixed_backend_begin_guarded(&b,&owner,&r,&token));
 CHECK(admissions==admitted+1&&b.active&&b.power_held&&b.owner==&owner);
 CHECK(!twod_irq_owner&&fixed_irq_owner==&b&&twod_irq_fixed_lender==&r&&r.mmu_owner==&token);
 CHECK(p.remesa_twod_owner==&r&&r.held);
 CHECK(remesa_twod_mmu_guard_release(&p,&r,&token)==-ENOENT&&r.mmu_owner==&token);
 CHECK(remesa_twod_release(&p,&r)==-ENOENT&&r.held);
 CHECK(remesa_twod_reserve(&p,&r2)==-EBUSY);
 unsigned reads=p.reads,words=p.words,resets=p.resets;u32 packet[10]={0};
 CHECK(psbfb_2d_submit(&p,packet,10)==-EBUSY&&p.reads==reads&&p.words==words&&p.resets==resets);
 CHECK(sgx535_gma500_fixed_backend_end(&b)==-EBUSY&&fixed_irq_owner==&b);
 owner.fixed_service.may_release=true;
 if(fault){
  b.fire_possible=true;
  if(fault==1||fault==2){
   unsigned long flags=sgx535_gma500_fixed_irq_lock();
   sgx535_gma500_fixed_irq_capture_locked(&dev,fault==1?(1u<<27):0,fault==2?1u:0);
   sgx535_gma500_fixed_irq_unlock(flags);
   CHECK(fault==1?b.pending_status1==(1u<<27):b.pending_status2==1);
   CHECK(sgx535_gma500_fixed_backend_end(&b)==-EBUSY);
  }else{
   r.mmu_owner=&wrong;CHECK(sgx535_gma500_fixed_backend_end(&b)==-EIO);
  }
  CHECK(fixed_irq_owner==&b&&twod_irq_fixed_lender==&r&&r.held&&!twod_irq_owner);
  /* Retained child model state; no fake event clear/retirement/recovery. */
  return;
 }
 CHECK(!sgx535_gma500_fixed_backend_end(&b));
 CHECK(!b.active&&!b.power_held&&!fixed_irq_owner&&!twod_irq_fixed_lender);
 CHECK(twod_irq_owner==&r&&r.mmu_owner==&token&&p.remesa_twod_owner==&r);
 CHECK(remesa_twod_release(&p,&r)==-EBUSY);
 CHECK(!remesa_twod_mmu_guard_release(&p,&r,&token));CHECK(!remesa_twod_release(&p,&r));
 CHECK(!p.remesa_twod_owner&&!twod_irq_owner);
 /* Existing unguarded entry still passes through the same original gates. */
 struct sgx535_gma500_fixed_backend normal={0};
 CHECK(!sgx535_gma500_fixed_backend_begin(&normal,&owner));normal.fire_possible=true;
 unsigned long flags=sgx535_gma500_fixed_irq_lock();
 sgx535_gma500_fixed_irq_capture_locked(&dev,1u<<27,0);
 sgx535_gma500_fixed_irq_unlock(flags);CHECK(!normal.pending_status1);
 normal.fire_possible=false;CHECK(!sgx535_gma500_fixed_backend_end(&normal));
 CHECK(!fixed_irq_owner&&!twod_irq_fixed_lender&&!twod_irq_owner);
 (void)otherdev;
}
int main(void)
{
 scenario(0);
 for(unsigned fault=1;fault<=3;fault++){
  pid_t child=fork();CHECK(child>=0);
  if(!child){scenario(fault);_exit(0);}
  int status;CHECK(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
 }
 printf("PASS %u IRQ handoff parent checks plus3 joined retained-fault models; lifecycle/source callbacks modeled; no GPU\n",checks);
 return 0;
}
