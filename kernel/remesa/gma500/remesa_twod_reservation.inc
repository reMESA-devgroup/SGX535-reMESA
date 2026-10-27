/* SPDX-License-Identifier: GPL-2.0-only */
/* Include in accel_2d.c after native register/header definitions. The original
 * psbfb_2d_submit must check remesa_twod_owner AFTER taking lock_2d and BEFORE
 * wait/FIFO writes; use the pinned staging tool, not this include alone. */
#include "remesa_twod_reservation.h"
static bool remesa_twod_idle_locked(struct drm_psb_private *dev_priv)
{
 return PSB_RSGX32(PSB_CR_2D_SOCIF)==_PSB_C2_SOCIF_EMPTY&&
  !(PSB_RSGX32(PSB_CR_2D_BLIT_STATUS)&_PSB_C2B_STATUS_BUSY);
}
int remesa_twod_reserve(struct drm_psb_private *dev_priv,struct remesa_twod_reservation *r)
{
 unsigned long flags;int error=0;
 if(!dev_priv||!r||!dev_priv->sgx_reg||!dev_priv->dev)return -EINVAL;
 if(r->held||r->device)return -EBUSY;
 if(!gma_power_is_on(dev_priv->dev))return -EIO;
 spin_lock_irqsave(&dev_priv->lock_2d,flags);
 if(dev_priv->remesa_twod_owner||!remesa_twod_idle_locked(dev_priv)){
  error=-EBUSY;goto done;
 }
 r->saved_base=PSB_RSGX32(PSB_CR_BIF_TWOD_REQ_BASE);r->device=dev_priv;r->held=true;
 dev_priv->remesa_twod_owner=r;
 done:spin_unlock_irqrestore(&dev_priv->lock_2d,flags);return error;
}
int remesa_twod_release(struct drm_psb_private *dev_priv,struct remesa_twod_reservation *r)
{
 unsigned long flags;int error=0;
 if(!dev_priv||!r||!dev_priv->sgx_reg||!dev_priv->dev)return -EINVAL;
 if(!gma_power_is_on(dev_priv->dev))return -EIO;
 spin_lock_irqsave(&dev_priv->lock_2d,flags);
 if(dev_priv->remesa_twod_owner!=r||!r->held||r->device!=dev_priv){error=-ENOENT;goto done;}
 if(!remesa_twod_idle_locked(dev_priv)||
    PSB_RSGX32(PSB_CR_BIF_TWOD_REQ_BASE)!=r->saved_base){error=-EBUSY;goto done;}
 dev_priv->remesa_twod_owner=NULL;memset(r,0,sizeof(*r));
 done:spin_unlock_irqrestore(&dev_priv->lock_2d,flags);return error;
}
