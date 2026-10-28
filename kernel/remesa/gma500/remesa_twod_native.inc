/* SPDX-License-Identifier: GPL-2.0-only */
/* Include in accel_2d.c after reservation include. Caller is the authenticated
 * GEM 2D service, not arbitrary raw-packet userspace. No ioctl registration. */
#include "remesa_twod_native.h"
#include "remesa_mmu_default_root.h"
#ifdef __KERNEL__
#include <linux/ktime.h>
#include <asm/unaligned.h>
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
 unsigned long flags,irq_flags;unsigned i;u32 root;int error;
 if(!b||!l||!cookie||!l->dev||!l->dev->dev_private||!l->pd||
    !l->count||l->count>2||l->plan.dwords!=10||
    (l->plan.request_base&0x0fffffffu))return -EINVAL;
 if(b->live||b->reservation.held)return -EBUSY;
 if(b->serial==~(u64)0)return -EOVERFLOW;
 error=psb_mmu_default_root(l->dev,l->pd,&root);if(error)return error;
 dev_priv=l->dev->dev_private;
 b->launch=l;b->root=root;b->live=true;
 error=remesa_twod_reserve(dev_priv,&b->reservation);if(error)goto failed;
 spin_lock_irqsave(&dev_priv->lock_2d,flags);
 irq_flags=sgx535_gma500_fixed_irq_lock();
 if(!gma_power_is_on(dev_priv->dev)){error=-EIO;goto done;}
 b->core=PSB_RSGX32(PSB_CR_CORE_ID);b->revision=PSB_RSGX32(PSB_CR_CORE_REVISION);
 if(b->core!=0x01130000u||b->revision!=0x00010201u){error=-ENODEV;goto done;}
 error=twod_native_hw(b,dev_priv,b->reservation.saved_base,true);if(error)goto done;
 error=twod_native_capture(dev_priv,&b->stale_status1);if(error)goto done;
 /* An old event must actually be gone before possible issuance is recorded. */
 if(PSB_RSGX32(PSB_CR_EVENT_STATUS)&~(1u<<31)) {error=-EIO;goto done;}
 if(PSB_RSGX32(PSB_CR_EVENT_STATUS2)){error=-EIO;goto done;}
 PSB_WSGX32(l->plan.request_base,PSB_CR_BIF_TWOD_REQ_BASE);
 if(PSB_RSGX32(PSB_CR_BIF_TWOD_REQ_BASE)!=l->plan.request_base){error=-EIO;goto done;}
 b->cookie=++b->serial;
 error=remesa_twod_irq_issue_locked(&b->reservation,b->cookie);if(error)goto done;
 *cookie=b->cookie;
 /* The existing first-owner 3D provenance must observe this producer too.
  * Never hide a new 2D submission behind bit27's benign IRQ classification. */
 remesa_route_source_producer(1);
 for(i=0;i<10;i++)
  PSB_WSGX32(get_unaligned_le32(l->plan.command+4*i),PSB_SGX_2D_SLAVE_PORT+4*i);
 (void)PSB_RSGX32(PSB_SGX_2D_SLAVE_PORT+36);
 remesa_route_source_producer(0);
 error=twod_native_hw(b,dev_priv,l->plan.request_base,false);
 done:sgx535_gma500_fixed_irq_unlock(irq_flags);
 spin_unlock_irqrestore(&dev_priv->lock_2d,flags);
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
 unsigned long flags,irq_flags;u64 serial;int error;
 error=twod_native_match(b,l,cookie);if(error)return error;
 if(!b->complete||!b->visible)return -EBUSY;
 dev_priv=l->dev->dev_private;
 spin_lock_irqsave(&dev_priv->lock_2d,flags);irq_flags=sgx535_gma500_fixed_irq_lock();
 error=twod_native_capture(dev_priv,NULL);
 if(!error)error=twod_native_hw(b,dev_priv,l->plan.request_base,true);
 if(!error){
  PSB_WSGX32(b->reservation.saved_base,PSB_CR_BIF_TWOD_REQ_BASE);
  (void)PSB_RSGX32(PSB_CR_BIF_TWOD_REQ_BASE);
  error=twod_native_hw(b,dev_priv,b->reservation.saved_base,true);
 }
 if(!error)error=twod_native_capture(dev_priv,NULL);
 if(!error)error=remesa_twod_irq_mark_retired_locked(&b->reservation,cookie);
 if(!error)error=remesa_twod_release_locked(dev_priv,&b->reservation);
 sgx535_gma500_fixed_irq_unlock(irq_flags);spin_unlock_irqrestore(&dev_priv->lock_2d,flags);
 if(error){b->failed=true;return error;}
 serial=b->serial;memset(b,0,sizeof(*b));b->serial=serial;return 0;
}
const struct remesa_twod_native_ops remesa_twod_native_executor={
 twod_native_start,twod_native_poll,twod_native_visible,twod_native_retire
};
