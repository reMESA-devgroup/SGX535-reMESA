/* SPDX-License-Identifier: GPL-2.0-only */
/* CPU event injection, NOT hardware completion or retirement evidence. */
#define main reservation_test_main
#include "test_twod_reservation.c"
#undef main
#include <unistd.h>
#include <sys/wait.h>
static unsigned source_notifications;
static void remesa_route_source_pending(struct drm_device *dev,u32 status1,u32 status2)
{CHECK(irq_lock_held&&dev);(void)status1;(void)status2;source_notifications++;}
#include "fixed_capture_actual.inc"
static void capture(struct drm_device *dev,u32 status1,u32 status2)
{
 unsigned long flags=sgx535_gma500_fixed_irq_lock();
 sgx535_gma500_fixed_irq_capture_locked(dev,status1,status2);
 sgx535_gma500_fixed_irq_unlock(flags);
}
static void event_case(u32 status1,u32 status2,int complete)
{
 struct drm_device dev={.powered=1},foreign={.powered=1};
 struct drm_psb_private p={.dev=&dev,.sgx_reg=(void*)1,.fifo=_PSB_C2_SOCIF_EMPTY,.base=0x28000000};
 struct remesa_twod_reservation r={0};dev.dev_private=&p;
 struct remesa_twod_irq_snapshot snapshot,old;
 CHECK(!remesa_twod_reserve(&p,&r));
 /* Event capture before possible issue cannot become this operation's receipt. */
 capture(&dev,_PSB_CE_TWOD_COMPLETE,0);
 CHECK(!r.pending1&&!r.pending2&&!r.irq_issued);
 unsigned long flags=sgx535_gma500_fixed_irq_lock();
 memset(&snapshot,0xa5,sizeof snapshot);old=snapshot;
 CHECK(remesa_twod_irq_snapshot_locked(&r,3,&snapshot)==-EACCES&&!memcmp(&snapshot,&old,sizeof old));
 CHECK(remesa_twod_irq_issue_locked(&r,0)==-EINVAL&&!r.irq_issued);
 CHECK(!remesa_twod_irq_issue_locked(&r,3));
 CHECK(remesa_twod_irq_issue_locked(&r,4)==-EBUSY&&r.irq_cookie==3);
 CHECK(remesa_twod_irq_snapshot_locked(&r,4,&snapshot)==-EACCES&&!memcmp(&snapshot,&old,sizeof old));
 CHECK(remesa_twod_irq_mark_retired_locked(&r,3)==-EIO&&!r.irq_retired);
 CHECK(remesa_twod_irq_drop_locked(&r)==-EBUSY);
 sgx535_gma500_fixed_irq_unlock(flags);
 CHECK(remesa_twod_release(&p,&r)==-EBUSY&&r.held);
 CHECK(remesa_twod_mmu_guard_acquire(&p,&r,&r)==-EBUSY&&!r.mmu_owner);
 capture(&foreign,0xffffffffu,0xffffffffu);
 CHECK(!r.pending1&&!r.pending2);
 capture(&dev,status1,status2);
 flags=sgx535_gma500_fixed_irq_lock();
 CHECK(!remesa_twod_irq_snapshot_locked(&r,3,&snapshot));
 CHECK(snapshot.cookie==3&&snapshot.status1==(status1&~(1u<<31))&&snapshot.status2==status2);
 CHECK(remesa_twod_irq_mark_retired_locked(&r,4)==-EACCES&&!r.irq_retired);
 CHECK(remesa_twod_irq_mark_retired_locked(&r,3)==(complete?0:-EIO));
 CHECK(r.irq_retired==!!complete);
 sgx535_gma500_fixed_irq_unlock(flags);
 if(complete){
  CHECK(!remesa_twod_release(&p,&r)&&!r.held&&!twod_irq_owner);
 }else{
  CHECK(remesa_twod_release(&p,&r)==-EBUSY&&r.held&&twod_irq_owner==&r);
 }
 CHECK(!p.words&&!p.resets);
 /* Fault cases run in joined child models; this reset is not driver recovery. */
}
int main(void)
{
 CHECK(!reservation_test_main());
 struct drm_device dev={.powered=1};
 struct drm_psb_private p={.dev=&dev,.sgx_reg=(void*)1,.fifo=_PSB_C2_SOCIF_EMPTY,.base=0x28000000};
 struct remesa_twod_reservation r={0};dev.dev_private=&p;
 struct model_scene_owner owner={&dev};
 struct sgx535_gma500_fixed_backend backend={true,true,&owner,0,0};
 /* Real staged capture wrapper preserves the existing 3D benign bit27. */
 fixed_irq_owner=&backend;
 capture(&dev,(1u<<31)|_PSB_CE_TWOD_COMPLETE|(1u<<12),1u<<4);
 CHECK(backend.pending_status1==(1u<<12)&&backend.pending_status2==(1u<<4));
 CHECK(remesa_twod_reserve(&p,&r)==-EBUSY&&!r.held&&!p.remesa_twod_owner);
 fixed_irq_owner=NULL;
 event_case(_PSB_CE_TWOD_COMPLETE,0,1);
 event_case((1u<<31)|_PSB_CE_TWOD_COMPLETE,0,1);
 /* Each unknown status bit survives capture and blocks ledger retirement. */
 for(unsigned word=0;word<2;word++)for(unsigned bit=0;bit<32;bit++){
  if(!word&&(bit==27||bit==31))continue;
  pid_t child=fork();CHECK(child>=0);
  if(!child){event_case(_PSB_CE_TWOD_COMPLETE|(!word?(1u<<bit):0),word?(1u<<bit):0,0);_exit(0);}
  int status;CHECK(waitpid(child,&status,0)==child);CHECK(WIFEXITED(status)&&!WEXITSTATUS(status));
 }
 /* An extra fault AFTER the trusted native retirement marker still prevents
  * releasing ownership. It cannot be hidden by the retired boolean. */
 CHECK(!remesa_twod_reserve(&p,&r));
 unsigned long flags=sgx535_gma500_fixed_irq_lock();
 CHECK(!remesa_twod_irq_issue_locked(&r,9));
 sgx535_gma500_fixed_irq_unlock(flags);capture(&dev,_PSB_CE_TWOD_COMPLETE,0);
 flags=sgx535_gma500_fixed_irq_lock();CHECK(!remesa_twod_irq_mark_retired_locked(&r,9));
 sgx535_gma500_fixed_irq_unlock(flags);capture(&dev,0,1u<<4);
 CHECK(remesa_twod_release(&p,&r)==-EBUSY&&r.held);
 /* Parent intentionally ends with a retained CPU-only fault model. */
 CHECK(source_notifications>0&&!irq_lock_held&&!p.lock_2d.held);
 printf("PASS %u parent IRQ/reservation checks plus 62 joined event-fault models; no device operations\n",checks);
 return 0;
}
