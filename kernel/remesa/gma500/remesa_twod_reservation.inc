/* SPDX-License-Identifier: GPL-2.0-only */
/* Include in accel_2d.c after native register/header definitions. The original
 * psbfb_2d_submit must check remesa_twod_owner AFTER taking lock_2d and BEFORE
 * wait/FIFO writes; use the pinned staging tool, not this include alone. */
#include "remesa_twod_reservation.h"
#include "remesa_twod_irq.h"
#ifdef __KERNEL__
#include "gma500_fixed_backend.h"
#endif
static bool remesa_twod_idle_locked(struct drm_psb_private *dev_priv)
{
 return PSB_RSGX32(PSB_CR_2D_SOCIF)==_PSB_C2_SOCIF_EMPTY&&
  !(PSB_RSGX32(PSB_CR_2D_BLIT_STATUS)&_PSB_C2B_STATUS_BUSY);
}
int remesa_twod_reserve(struct drm_psb_private *dev_priv,struct remesa_twod_reservation *r)
{
 unsigned long flags,irq_flags;int error=0;struct remesa_twod_reservation saved;
 if(!dev_priv||!r||!dev_priv->sgx_reg||!dev_priv->dev)return -EINVAL;
 if(r->held||r->device)return -EBUSY;
 if(!gma_power_is_on(dev_priv->dev))return -EIO;
 spin_lock_irqsave(&dev_priv->lock_2d,flags);
 irq_flags=sgx535_gma500_fixed_irq_lock();
 if(dev_priv->remesa_twod_owner||!remesa_twod_irq_available_locked()||!remesa_twod_idle_locked(dev_priv)){
  error=-EBUSY;goto done;
 }
 saved=*r;
 r->saved_base=PSB_RSGX32(PSB_CR_BIF_TWOD_REQ_BASE);r->device=dev_priv;r->held=true;
 dev_priv->remesa_twod_owner=r;
 error=remesa_twod_irq_claim_locked(r);
 if(error){dev_priv->remesa_twod_owner=NULL;*r=saved;}
 done:sgx535_gma500_fixed_irq_unlock(irq_flags);
 spin_unlock_irqrestore(&dev_priv->lock_2d,flags);return error;
}
int remesa_twod_release_locked(struct drm_psb_private *dev_priv,struct remesa_twod_reservation *r)
{
 int error;
 if(!dev_priv||!r||!dev_priv->sgx_reg||!dev_priv->dev)return -EINVAL;
#ifdef __KERNEL__
 lockdep_assert_held(&dev_priv->lock_2d);
#endif
 if(!gma_power_is_on(dev_priv->dev))return -EIO;
 if(dev_priv->remesa_twod_owner!=r||!r->held||r->device!=dev_priv)return -ENOENT;
 if(!remesa_twod_idle_locked(dev_priv)||
    PSB_RSGX32(PSB_CR_BIF_TWOD_REQ_BASE)!=r->saved_base)return -EBUSY;
 error=remesa_twod_irq_drop_locked(r);if(error)return error;
 dev_priv->remesa_twod_owner=NULL;memset(r,0,sizeof(*r));
 return 0;
}
int remesa_twod_release(struct drm_psb_private *dev_priv,struct remesa_twod_reservation *r)
{
 unsigned long flags,irq_flags;int error;
 if(!dev_priv||!r||!dev_priv->sgx_reg||!dev_priv->dev)return -EINVAL;
 if(!gma_power_is_on(dev_priv->dev))return -EIO;
 spin_lock_irqsave(&dev_priv->lock_2d,flags);
 irq_flags=sgx535_gma500_fixed_irq_lock();
 error=remesa_twod_release_locked(dev_priv,r);
 sgx535_gma500_fixed_irq_unlock(irq_flags);
 spin_unlock_irqrestore(&dev_priv->lock_2d,flags);return error;
}

static int remesa_twod_mmu_guard(struct drm_psb_private *dev_priv,
 struct remesa_twod_reservation *r,const void *owner,bool acquire)
{
 unsigned long flags,irq_flags;int error=0;
 if(!dev_priv||!r||!owner||!dev_priv->dev||!dev_priv->sgx_reg)return -EINVAL;
 if(!gma_power_is_on(dev_priv->dev))return -EIO;
 spin_lock_irqsave(&dev_priv->lock_2d,flags);
 irq_flags=sgx535_gma500_fixed_irq_lock();
 if(!r->held||r->device!=dev_priv||dev_priv->remesa_twod_owner!=r){
  error=-ENOENT;goto done;
 }
 error=remesa_twod_irq_guard_locked(r);if(error)goto done;
 if(acquire){
  if(r->mmu_owner){error=-EBUSY;goto done;}
  if(!remesa_twod_idle_locked(dev_priv)){error=-EBUSY;goto done;}
  r->mmu_owner=owner;
 }else{
  if(r->mmu_owner!=owner){error=-EACCES;goto done;}
  r->mmu_owner=NULL;
 }
 done:sgx535_gma500_fixed_irq_unlock(irq_flags);
 spin_unlock_irqrestore(&dev_priv->lock_2d,flags);return error;
}
int remesa_twod_mmu_guard_acquire(struct drm_psb_private *dev_priv,
 struct remesa_twod_reservation *r,const void *owner)
{return remesa_twod_mmu_guard(dev_priv,r,owner,true);}
int remesa_twod_mmu_guard_release(struct drm_psb_private *dev_priv,
 struct remesa_twod_reservation *r,const void *owner)
{return remesa_twod_mmu_guard(dev_priv,r,owner,false);}
