/* SPDX-License-Identifier: GPL-2.0-only */
/* Process-context lid observation. Never spin waiting for a panel transition;
 * leave it pending and observe again at the existing lid interval. */
#include <linux/workqueue.h>
#include <drm/drm_drv.h>
static void remesa_private_lid_queue(struct drm_psb_private *p)
{
 unsigned long flags;
 spin_lock_irqsave(&p->lid_lock,flags);
 if(!p->remesa_private_lid_stopping)
  queue_delayed_work(system_freezable_wq,&p->remesa_private_lid_work,PSB_LID_DELAY);
 spin_unlock_irqrestore(&p->lid_lock,flags);
}
static void remesa_private_lid_worker(struct work_struct *work)
{
 struct drm_psb_private *p=container_of(to_delayed_work(work),
   struct drm_psb_private,remesa_private_lid_work);
 struct drm_device *dev=p->dev;u32 state,status;int index;unsigned long flags;bool stopping;
 spin_lock_irqsave(&p->lid_lock,flags);stopping=p->remesa_private_lid_stopping;
 spin_unlock_irqrestore(&p->lid_lock,flags);if(stopping)return;
 if(!drm_dev_enter(dev,&index))return;
 if(!gma_power_begin(dev,false))goto exit;
 if(!p->opregion.header||!p->opregion.lid_state)goto power_end;
 state=readl(p->opregion.lid_state);
 if(!p->remesa_private_lid_pending&&state==p->lid_last_state)goto power_end;
 if(!p->remesa_private_lid_pending||state!=p->remesa_private_lid_target){
  p->remesa_private_lid_pending=true;p->remesa_private_lid_target=state;
  if(!(state&1U))psb_intel_lvds_set_brightness(dev,0);
  status=REG_READ(PP_CONTROL);
  REG_WRITE(PP_CONTROL,(state&1U)?status|POWER_TARGET_ON:status&~POWER_TARGET_ON);
 }
 status=REG_READ(PP_STATUS);
 if(!(status&PP_SEQUENCE_MASK)&&!!(status&PP_ON)==!!(state&1U)){
  if(state&1U)psb_intel_lvds_set_brightness(dev,100);
  p->lid_last_state=state;p->remesa_private_lid_pending=false;
 }
power_end:
 gma_power_end(dev);
exit:
 drm_dev_exit(index);remesa_private_lid_queue(p);
}
void psb_lid_timer_init(struct drm_psb_private *p)
{
 if(p->remesa_private_lid_initialized||!p->opregion.header||!p->opregion.lid_state)return;
 spin_lock_init(&p->lid_lock);
 INIT_DELAYED_WORK(&p->remesa_private_lid_work,remesa_private_lid_worker);
 p->remesa_private_lid_stopping=false;p->remesa_private_lid_pending=false;
 p->remesa_private_lid_initialized=true;remesa_private_lid_queue(p);
}
void psb_lid_timer_takedown(struct drm_psb_private *p)
{
 unsigned long flags;
 if(!p->remesa_private_lid_initialized)return;
 spin_lock_irqsave(&p->lid_lock,flags);p->remesa_private_lid_stopping=true;
 spin_unlock_irqrestore(&p->lid_lock,flags);
 /* No lock across the work join. Later chip teardown may repeat this safely. */
 cancel_delayed_work_sync(&p->remesa_private_lid_work);
}
