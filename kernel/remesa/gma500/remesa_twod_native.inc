/* SPDX-License-Identifier: GPL-2.0-only */
/* Include in accel_2d.c after reservation include. Caller is the authenticated
 * GEM 2D service, not arbitrary raw-packet userspace. No ioctl registration. */
#include "remesa_twod_native.h"
#include "remesa_mmu_default_root.h"
#ifdef REMESA_TWOD_VENDOR_STREAM
#ifdef REMESA_TWOD_FILL_STREAM
#define TWOD_NATIVE_VENDOR(n) ((n)==7u||(n)==9u)
#else
#define TWOD_NATIVE_VENDOR(n) ((n)==9u)
#endif
#define TWOD_NATIVE_DWORDS_VALID(n) (TWOD_NATIVE_VENDOR(n)||(n)==10u)
#define TWOD_NATIVE_PACKET_WORDS(l) ((l)->plan.dwords)
#else
#define TWOD_NATIVE_DWORDS_VALID(n) ((n)==10u)
#define TWOD_NATIVE_PACKET_WORDS(l) 10u
#endif
#ifdef __KERNEL__
#include <linux/ktime.h>
#include <asm/unaligned.h>
#endif
#ifdef REMESA_TWOD_LAB_IOCTL
/* Failure-only LAB evidence; capture under IRQ lock, print after unlocking.
 * Never use this snapshot as completion/retirement or clear the fault. */
struct twod_native_fault_snapshot {u32 valid,event1,event2,bif,address,root,base,vfifo0,vfifo1,fifo,blit,alpha;};
static void twod_native_fault_read(struct drm_psb_private *dev_priv,
 struct twod_native_fault_snapshot *s)
{
 if(!gma_power_is_on(dev_priv->dev))return;
 s->event1=PSB_RSGX32(PSB_CR_EVENT_STATUS);s->event2=PSB_RSGX32(PSB_CR_EVENT_STATUS2);
 s->bif=PSB_RSGX32(PSB_CR_BIF_INT_STAT);s->address=PSB_RSGX32(PSB_CR_BIF_FAULT);
 s->root=PSB_RSGX32(PSB_CR_BIF_DIR_LIST_BASE0);s->base=PSB_RSGX32(PSB_CR_BIF_TWOD_REQ_BASE);
 /* Permitted SGX535 definitions: 2D_VIRTUAL_FIFO_0/1, read only. */
 s->vfifo0=PSB_RSGX32(0x0e10);s->vfifo1=PSB_RSGX32(0x0e14);
 s->fifo=PSB_RSGX32(PSB_CR_2D_SOCIF);s->blit=PSB_RSGX32(PSB_CR_2D_BLIT_STATUS);
 s->alpha=PSB_RSGX32(0x0e1c);s->valid=1;
}
#define TWOD_LAB_PHASE(value) do{phase=(value);}while(0)
#else
#define TWOD_LAB_PHASE(value) do{}while(0)
#endif
static int twod_native_match(struct remesa_twod_native *b,
 const struct remesa_twod_launch *l,u64 cookie)
{
 if(!b||!l||!b->live||b->launch!=l||!cookie||cookie!=b->cookie||
    !b->reservation.held||b->reservation.irq_cookie!=cookie)return -EACCES;
 return b->failed?-EIO:0;
}
static int twod_native_hw(struct remesa_twod_native *b,
 struct drm_psb_private *dev_priv,u32 base,bool quiet)
{
 if(!gma_power_is_on(dev_priv->dev)||dev_priv->remesa_twod_owner!=&b->reservation)
  return -EIO;
#ifdef REMESA_TWOD_VENDOR_STREAM
 /* Every new-profile state transition must match the value owned by this
  * transaction. Uncertainty retains the reservation and saved state. */
 if(TWOD_NATIVE_VENDOR(b->launch->plan.dwords)&&(!b->alpha_saved||
    PSB_RSGX32(0x0e1c)!=b->expected_alpha))return -EIO;
#endif
 if(PSB_RSGX32(PSB_CR_BIF_DIR_LIST_BASE0)!=b->root||
    PSB_RSGX32(PSB_CR_BIF_BANK0)||PSB_RSGX32(PSB_CR_BIF_BANK1)||
    (PSB_RSGX32(PSB_CR_BIF_CTRL)&(_PSB_MMU_ER_MASK|_PSB_CB_CTRL_FLUSH|
     _PSB_CB_CTRL_INVALDC|_PSB_CB_CTRL_CLEAR_FAULT))||
    PSB_RSGX32(PSB_CR_SOFT_RESET)||PSB_RSGX32(PSB_CR_BIF_INT_STAT)||
    (PSB_RSGX32(SGX535_SOURCE_EVENT_TIMER_REG)&SGX535_SOURCE_EVENT_TIMER_ENABLE)||
    (PSB_RSGX32(SGX535_SOURCE_EVENT_KICK_REG)&SGX535_SOURCE_EVENT_KICK_NOW)||
    PSB_RSGX32(PSB_CR_BIF_TWOD_REQ_BASE)!=base)return -EIO;
 if(quiet&&(!remesa_twod_idle_locked(dev_priv)||
    (PSB_RSGX32(SGX535_SOURCE_BIF_READS_REG)&SGX535_SOURCE_BIF_READS_MASK)))return -EBUSY;
 return 0;
}
/* Common IRQ/poll capture precedes ACK, under the same native IRQ lock. Never
 * clear unexpected bits or a BIF fault to manufacture a clean operation. */
static int twod_native_capture(struct drm_psb_private *dev_priv,u32 *observed)
{
 u32 status1,status2;
 if(!gma_power_is_on(dev_priv->dev))return -EIO;
 status1=PSB_RSGX32(PSB_CR_EVENT_STATUS);status2=PSB_RSGX32(PSB_CR_EVENT_STATUS2);
 sgx535_gma500_fixed_irq_capture_locked(dev_priv->dev,status1,status2);
 if(observed)*observed=status1;
 if((status1&~((1u<<31)|_PSB_CE_TWOD_COMPLETE))||status2)return -EIO;
 if(status1&_PSB_CE_TWOD_COMPLETE){
  PSB_WSGX32(_PSB_CE_TWOD_COMPLETE,PSB_CR_EVENT_HOST_CLEAR);
  (void)PSB_RSGX32(PSB_CR_EVENT_HOST_CLEAR);
 }
 return 0;
}
static int twod_native_start(void *opaque,const struct remesa_twod_launch *l,u64 *cookie)
{
 struct remesa_twod_native *b=opaque;struct drm_psb_private *dev_priv;
 unsigned long flags,irq_flags;unsigned i=0,packet_words;u32 root;int error;bool producer=false;
#ifdef REMESA_TWOD_LAB_IOCTL
 unsigned phase=0;struct twod_native_fault_snapshot snapshot={0};
 struct remesa_twod_irq_snapshot before_fifo;
#endif
 if(!b||!l||!cookie||!l->dev||!l->dev->dev_private||!l->pd||
    !l->count||l->count>2||!TWOD_NATIVE_DWORDS_VALID(l->plan.dwords)||
    (l->plan.request_base&0x0fffffffu))return -EINVAL;
 packet_words=TWOD_NATIVE_PACKET_WORDS(l);
 if(b->live||b->reservation.held)return -EBUSY;
 if(b->serial==~(u64)0)return -EOVERFLOW;
 error=psb_mmu_default_root(l->dev,l->pd,&root);if(error)return error;
 dev_priv=l->dev->dev_private;
 b->launch=l;b->root=root;b->live=true;
 error=remesa_twod_reserve(dev_priv,&b->reservation);if(error)goto failed;
 spin_lock_irqsave(&dev_priv->lock_2d,flags);
 irq_flags=sgx535_gma500_fixed_irq_lock();
 TWOD_LAB_PHASE(1);
 if(!gma_power_is_on(dev_priv->dev)){error=-EIO;goto done;}
 b->core=PSB_RSGX32(PSB_CR_CORE_ID);b->revision=PSB_RSGX32(PSB_CR_CORE_REVISION);
 if(b->core!=0x01130000u||b->revision!=0x00010201u){error=-ENODEV;goto done;}
#ifdef REMESA_TWOD_VENDOR_STREAM
 if(TWOD_NATIVE_VENDOR(packet_words)){
  b->saved_alpha=PSB_RSGX32(0x0e1c);
  /* Only the defined ONE/ZERO components may be replayed on restoration. */
  if(b->saved_alpha&~0x0000ffffu){error=-EOPNOTSUPP;goto done;}
  b->expected_alpha=b->saved_alpha;b->alpha_saved=true;
 }
#endif
 TWOD_LAB_PHASE(2);error=twod_native_hw(b,dev_priv,b->reservation.saved_base,true);if(error)goto done;
 TWOD_LAB_PHASE(3);
 error=twod_native_capture(dev_priv,&b->stale_status1);if(error)goto done;
 /* An old event must actually be gone before possible issuance is recorded. */
 if(PSB_RSGX32(PSB_CR_EVENT_STATUS)&~(1u<<31)) {error=-EIO;goto done;}
 if(PSB_RSGX32(PSB_CR_EVENT_STATUS2)){error=-EIO;goto done;}
 /* Base publication belongs to the producer interval too: do not hide a
  * fault/request triggered before the first FIFO word from legacy history. */
 remesa_route_source_producer(1);producer=true;
#ifdef REMESA_TWOD_VENDOR_STREAM
 if(TWOD_NATIVE_VENDOR(packet_words)){
  TWOD_LAB_PHASE(11);b->expected_alpha=0x0000ff00u;
  PSB_WSGX32(b->expected_alpha,0x0e1c);
  error=twod_native_hw(b,dev_priv,b->reservation.saved_base,true);if(error)goto done;
  error=twod_native_capture(dev_priv,NULL);if(error)goto done;
 }
#endif
 TWOD_LAB_PHASE(4);
 PSB_WSGX32(l->plan.request_base,PSB_CR_BIF_TWOD_REQ_BASE);
 if(PSB_RSGX32(PSB_CR_BIF_TWOD_REQ_BASE)!=l->plan.request_base){error=-EIO;goto done;}
 TWOD_LAB_PHASE(5);
 error=twod_native_hw(b,dev_priv,l->plan.request_base,true);if(error)goto done;
 TWOD_LAB_PHASE(6);
 b->cookie=++b->serial;
 error=remesa_twod_irq_issue_locked(&b->reservation,b->cookie);if(error)goto done;
 *cookie=b->cookie;
#ifdef REMESA_TWOD_LAB_IOCTL
 /* Separate delayed base-publication effects from the first FIFO word.
  * Keep reservation, backing, producer interval and IRQ cookie live while
  * sleeping outside both spinlocks. Any pre-FIFO event is a rejection,
  * including one already ACKed by IRQ and retained in the owned ledger. */
 sgx535_gma500_fixed_irq_unlock(irq_flags);
 spin_unlock_irqrestore(&dev_priv->lock_2d,flags);
 usleep_range(1000,1500);
 spin_lock_irqsave(&dev_priv->lock_2d,flags);
 irq_flags=sgx535_gma500_fixed_irq_lock();
 TWOD_LAB_PHASE(10);
 error=twod_native_hw(b,dev_priv,l->plan.request_base,true);if(error)goto done;
 error=remesa_twod_irq_snapshot_locked(&b->reservation,b->cookie,&before_fifo);
 if(error)goto done;
 if(before_fifo.status1||before_fifo.status2||
    (PSB_RSGX32(PSB_CR_EVENT_STATUS)&~(1u<<31))||PSB_RSGX32(PSB_CR_EVENT_STATUS2)){
  error=-EIO;goto done;
 }
#endif
 TWOD_LAB_PHASE(7);
 for(i=0;i<packet_words;i++){
  PSB_WSGX32(get_unaligned_le32(l->plan.command+4*i),PSB_SGX_2D_SLAVE_PORT+4*i);
#ifdef REMESA_TWOD_LAB_IOCTL
  /* Observe, never ACK/reset, between unchanged packet words. A detected
   * fault retains the partial job and forbids further FIFO writes. This is
   * an observation boundary, not attribution of an asynchronous fault. */
  TWOD_LAB_PHASE(9);
  if(!gma_power_is_on(dev_priv->dev))error=-EIO;
  else if(PSB_RSGX32(PSB_CR_BIF_INT_STAT)||PSB_RSGX32(PSB_CR_EVENT_STATUS2))error=-EIO;
  if(error){i++;goto done;}
  TWOD_LAB_PHASE(7);
#endif
 }
 (void)PSB_RSGX32(PSB_SGX_2D_SLAVE_PORT+4*(packet_words-1));
 TWOD_LAB_PHASE(8);
 error=twod_native_hw(b,dev_priv,l->plan.request_base,false);
 done:
#ifdef REMESA_TWOD_LAB_IOCTL
 if(error&&b->core==0x01130000u&&b->revision==0x00010201u)
  twod_native_fault_read(dev_priv,&snapshot);
#endif
 if(producer)remesa_route_source_producer(0);
 sgx535_gma500_fixed_irq_unlock(irq_flags);
 spin_unlock_irqrestore(&dev_priv->lock_2d,flags);
#ifdef REMESA_TWOD_LAB_IOCTL
 if(error)pr_err("remesa2d LAB start fault phase=%u words=%u valid=%u event=%08x/%08x bif=%08x addr=%08x root=%08x req=%08x vfifo=%08x/%08x fifo=%08x blit=%08x alpha=%08x\n",
  phase,i,snapshot.valid,snapshot.event1,snapshot.event2,snapshot.bif,snapshot.address,
  snapshot.root,snapshot.base,snapshot.vfifo0,snapshot.vfifo1,snapshot.fifo,snapshot.blit,snapshot.alpha);
#endif
 failed:if(error)b->failed=true;return error;
}
static int twod_native_poll(void *opaque,const struct remesa_twod_launch *l,
 u64 cookie,unsigned budget,struct remesa_twod_completion *out)
{
 struct remesa_twod_native *b=opaque;struct drm_psb_private *dev_priv;
 struct remesa_twod_irq_snapshot snapshot;struct remesa_twod_completion result;
 unsigned long flags;u64 start;u32 fifo,blit;int error;
 if(!out||!budget||budget>10000)return -EINVAL;
 error=twod_native_match(b,l,cookie);if(error)return error;
 dev_priv=l->dev->dev_private;start=ktime_get_ns();
 for(;;){
  flags=sgx535_gma500_fixed_irq_lock();
  error=twod_native_capture(dev_priv,NULL);
  if(!error)error=twod_native_hw(b,dev_priv,l->plan.request_base,false);
  if(!error)error=remesa_twod_irq_snapshot_locked(&b->reservation,cookie,&snapshot);
  if(!error&&((snapshot.status1&~_PSB_CE_TWOD_COMPLETE)||snapshot.status2))error=-EIO;
  fifo=0;blit=_PSB_C2B_STATUS_BUSY;
  if(!error){fifo=PSB_RSGX32(PSB_CR_2D_SOCIF);blit=PSB_RSGX32(PSB_CR_2D_BLIT_STATUS);}
  if(!error&&snapshot.status1==_PSB_CE_TWOD_COMPLETE&&fifo==_PSB_C2_SOCIF_EMPTY&&
     !(blit&_PSB_C2B_STATUS_BUSY)&&
     !(PSB_RSGX32(SGX535_SOURCE_BIF_READS_REG)&SGX535_SOURCE_BIF_READS_MASK)){
   result.ticket=l->ticket;result.cookie=cookie;result.event=snapshot.status1;
   result.status2=snapshot.status2;result.fifo_free=fifo;result.blit_status=blit;
   *out=result;b->complete=true;
   sgx535_gma500_fixed_irq_unlock(flags);return 0;
  }
  sgx535_gma500_fixed_irq_unlock(flags);
  if(error)break;
  if(ktime_get_ns()-start>=(u64)budget*1000000u){error=-ETIMEDOUT;break;}
  usleep_range(50,100);
 }
 b->failed=true;return error;
}
static int twod_native_visible(void *opaque,const struct remesa_twod_launch *l,u64 cookie)
{
 struct remesa_twod_native *b=opaque;struct drm_psb_private *dev_priv;
 struct remesa_twod_irq_snapshot snapshot;unsigned long flags;int error;
 error=twod_native_match(b,l,cookie);if(error)return error;
 if(!b->complete||b->visible)return -EBUSY;
 dev_priv=l->dev->dev_private;flags=sgx535_gma500_fixed_irq_lock();
 error=twod_native_capture(dev_priv,NULL);
 if(!error)error=twod_native_hw(b,dev_priv,l->plan.request_base,true);
 if(!error)error=remesa_twod_irq_snapshot_locked(&b->reservation,cookie,&snapshot);
 if(!error&&(snapshot.status1!=_PSB_CE_TWOD_COMPLETE||snapshot.status2))error=-EIO;
 /* Packet ends in the original 2D FLUSH. Completion + idle/bus quiet must
  * precede CPU invalidation in GEM publish; physical LAB must verify this
  * device writeback contract before any acceleration is advertised. */
 if(!error){dma_rmb();b->visible=true;}else b->failed=true;
 sgx535_gma500_fixed_irq_unlock(flags);return error;
}
static int twod_native_retire(void *opaque,const struct remesa_twod_launch *l,u64 cookie)
{
 struct remesa_twod_native *b=opaque;struct drm_psb_private *dev_priv;
 struct remesa_twod_retirement proof={0};
 unsigned long flags,irq_flags;u64 serial;int error;
#ifdef REMESA_TWOD_VENDOR_STREAM
 bool producer=false;
#endif
 error=twod_native_match(b,l,cookie);if(error)return error;
 if(!b->complete||!b->visible)return -EBUSY;
 dev_priv=l->dev->dev_private;
 spin_lock_irqsave(&dev_priv->lock_2d,flags);irq_flags=sgx535_gma500_fixed_irq_lock();
 error=twod_native_capture(dev_priv,NULL);
 if(!error)error=twod_native_hw(b,dev_priv,l->plan.request_base,true);
#ifdef REMESA_TWOD_VENDOR_STREAM
 if(!error&&TWOD_NATIVE_VENDOR(l->plan.dwords)){remesa_route_source_producer(1);producer=true;}
#endif
 if(!error){
  PSB_WSGX32(b->reservation.saved_base,PSB_CR_BIF_TWOD_REQ_BASE);
  (void)PSB_RSGX32(PSB_CR_BIF_TWOD_REQ_BASE);
  error=twod_native_hw(b,dev_priv,b->reservation.saved_base,true);
 }
#ifdef REMESA_TWOD_VENDOR_STREAM
 if(!error&&TWOD_NATIVE_VENDOR(l->plan.dwords)){
  b->expected_alpha=b->saved_alpha;
  PSB_WSGX32(b->saved_alpha,0x0e1c);
  error=twod_native_hw(b,dev_priv,b->reservation.saved_base,true);
 }
#endif
 if(!error)error=twod_native_capture(dev_priv,NULL);
 if(!error)error=remesa_twod_irq_mark_retired_locked(&b->reservation,cookie);
 if(!error&&l->retirement){
  /* Consume the ledger already checked above, under its existing IRQ lock.
   * No extra MMIO reads, ACK, reset, flush or completion poll is added. */
  proof.dev=l->dev;proof.pd=l->pd;proof.ticket=l->ticket;proof.cookie=cookie;
  proof.destination_id=l->plan.destination_id;
  proof.destination_first=l->plan.destination_first;proof.destination_end=l->plan.destination_end;
  proof.default_root=b->root;proof.request_base=l->plan.request_base;
  proof.restored_request_base=b->reservation.saved_base;proof.packet_dwords=TWOD_NATIVE_PACKET_WORDS(l);
  proof.core=b->core;proof.revision=b->revision;
  proof.status1=b->reservation.pending1;proof.status2=b->reservation.pending2;
 }
 if(!error)error=remesa_twod_release_locked(dev_priv,&b->reservation);
#ifdef REMESA_TWOD_VENDOR_STREAM
 if(producer)remesa_route_source_producer(0);
#endif
 sgx535_gma500_fixed_irq_unlock(irq_flags);spin_unlock_irqrestore(&dev_priv->lock_2d,flags);
 if(error){b->failed=true;return error;}
 if(l->retirement){proof.native_retired=true;*l->retirement=proof;}
 serial=b->serial;memset(b,0,sizeof(*b));b->serial=serial;return 0;
}
const struct remesa_twod_native_ops remesa_twod_native_executor={
 twod_native_start,twod_native_poll,twod_native_visible,twod_native_retire
};
