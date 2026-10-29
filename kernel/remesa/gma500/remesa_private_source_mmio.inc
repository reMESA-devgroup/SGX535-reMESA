/* SPDX-License-Identifier: GPL-2.0-only */
/* Included in private remesa_stream_source.c, whose source_lock/valid/lost are
 * the existing authority. This is NOT a PCI remove/resource lifetime fix.
 * No sleeping or callback is permitted in the critical section. */
#ifdef __KERNEL__
#include <linux/jiffies.h>
#include "power.h"
#endif
#include "remesa_private_source_mmio.h"
int remesa_private_source_write(struct sgx535_gma500_fixed_backend *backend,
 unsigned offset,unsigned value)
{
 struct drm_psb_private *dev_priv;
 unsigned long flags;int error=-EAGAIN;
 if(!backend||!backend->owner||!backend->owner->dev)return -EINVAL;
 if((offset&3)||offset>PSB_SGX_SIZE-sizeof(u32))return -EINVAL;
 spin_lock_irqsave(&source_lock,flags);
 if(!backend->private_loan||!valid(backend)||
    !sgx535_source_valid(&operation->boundary,backend))goto rejected;
 dev_priv=backend->owner->dev->dev_private;
 if(!backend->active||!backend->power_held||!dev_priv||!dev_priv->sgx_reg){
  error=-ENODEV;goto rejected;
 }
 if(backend->fire_possible&&time_after_eq(jiffies,backend->deadline)){
  error=-ETIMEDOUT;goto rejected;
 }
 /* Invalidation (IRQ, lifecycle, producer or poison) must acquire this same
  * source lock. It orders wholly before or after this write, not between the
  * authority check and MMIO. Caller still owns endpoint/MMU/device exclusion. */
 PSB_WSGX32(value,offset);
 error=0;goto done;
 rejected:
 if(operation&&operation->backend==backend)lost();
 done:spin_unlock_irqrestore(&source_lock,flags);return error;
}
