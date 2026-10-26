/* SPDX-License-Identifier: GPL-2.0-only */
/* Included only in the PRIVATE fixed backend translation unit. No new ioctl,
 * module parameter, write, pause, counter configuration or execution path. */
static struct sgx535_gma500_fixed_backend *bif_debug_owner;
static struct remesa_bif_debug_record bif_debug_record;
static bool bif_debug_qualified(struct sgx535_gma500_fixed_backend *b)
{
 return b&&b==fixed_irq_owner&&b==bif_debug_owner&&b->active&&b->power_held&&
  b->owner&&b->owner->initialized&&b->owner->device_claimed&&b->owner->power_held&&
  b->owner->dev&&b->owner->dev->dev_private&&gma_power_is_on(b->owner->dev);
}
static int bif_debug_read(void *opaque,u32 offset,u32 *value)
{
 struct sgx535_gma500_fixed_backend *b=opaque;
 struct drm_psb_private *dev_priv;
 if(!value||!bif_debug_qualified(b))return -ENODEV;
 dev_priv=b->owner->dev->dev_private;if(!dev_priv->sgx_reg)return -ENODEV;
 switch(offset) {
 case REMESA_BIF_INT_STAT_OFFSET:*value=PSB_RSGX32(PSB_CR_BIF_INT_STAT);return 0;
 case REMESA_BIF_FAULT_OFFSET:*value=PSB_RSGX32(PSB_CR_BIF_FAULT);return 0;
 default:return -EINVAL;
 }
}
static void bif_debug_irq_locked(struct sgx535_gma500_fixed_backend *b,u32 s1,u32 s2)
{
 if(!b||!b->owner||!b->fire_possible)return;
 (void)remesa_bif_debug_sample(&bif_debug_record,REMESA_BIF_IRQ,
  b->owner->session.sequence,bif_debug_qualified(b),bif_debug_read,b,s1,s2,
  b->owner->session.phase,b->owner->session.observed_events);
}
int sgx535_gma500_fixed_bif_pre(struct sgx535_gma500_fixed_backend *b,u32 sequence)
{
 unsigned long flags;int e;
 BUILD_BUG_ON(PSB_CR_BIF_INT_STAT!=REMESA_BIF_INT_STAT_OFFSET);
 BUILD_BUG_ON(PSB_CR_BIF_FAULT!=REMESA_BIF_FAULT_OFFSET);
 BUILD_BUG_ON(_PSB_CE2_BIF_REQUESTER_FAULT!=REMESA_BIF_REQUESTER_EVENT);
 BUILD_BUG_ON(sizeof(struct remesa_bif_debug_record)!=REMESA_BIF_DEBUG_BYTES);
 flags=sgx535_gma500_fixed_irq_lock();
 if(!sequence||!bif_debug_qualified(b)||bif_debug_record.sequence){e=-EACCES;goto done;}
 remesa_bif_debug_init(&bif_debug_record,sequence);
 e=remesa_bif_debug_sample(&bif_debug_record,REMESA_BIF_PRE,sequence,true,
  bif_debug_read,b,0,0,b->owner->session.phase,b->owner->session.observed_events);
done:sgx535_gma500_fixed_irq_unlock(flags);return e;
}
int sgx535_gma500_fixed_bif_post(struct sgx535_gma500_fixed_backend *b)
{
 unsigned long flags;int e;
 flags=sgx535_gma500_fixed_irq_lock();
 if(!b||!b->owner){e=-EINVAL;goto done;}
 e=remesa_bif_debug_sample(&bif_debug_record,REMESA_BIF_POST,
  b->owner->session.sequence,bif_debug_qualified(b),bif_debug_read,b,0,0,
  b->owner->session.phase,b->owner->session.observed_events);
done:sgx535_gma500_fixed_irq_unlock(flags);return e;
}
int sgx535_gma500_fixed_bif_snapshot(struct sgx535_gma500_fixed_backend *b,
 struct remesa_bif_debug_record *out)
{
 unsigned long flags;int e=0;
 if(!b||!out)return -EINVAL;
 flags=sgx535_gma500_fixed_irq_lock();
 /* Copy only: end() may have removed the IRQ lease; no register read here.
  * Keep the record through native_close, after all attributed IRQ handling. */
 if(b!=bif_debug_owner||!bif_debug_record.sequence)e=-ENODATA;
 else *out=bif_debug_record;
 sgx535_gma500_fixed_irq_unlock(flags);return e;
}
