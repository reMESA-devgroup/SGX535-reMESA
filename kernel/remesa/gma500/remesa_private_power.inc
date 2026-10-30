/* SPDX-License-Identifier: GPL-2.0-only */
/* Private admission/PCI error contract. This does not establish GPU stop,
 * IRQ retirement, raw brightness MMIO safety, or runtime-PM support. */
#include <linux/sched.h>
#include <linux/interrupt.h>

static bool remesa_private_power_owner(struct drm_psb_private *dev_priv)
{
 return dev_priv->remesa_private_power_transition &&
        dev_priv->remesa_private_power_owner == current && !in_interrupt();
}

/* power_ctrl_lock protects admission, the count, and these transition fields.
 * Only the PM task may nest claims from save/restore callbacks. An interrupt
 * borrowing that task's current pointer must never inherit its permission. */
static void remesa_private_power_finish(struct drm_psb_private *dev_priv, bool ready)
{
 unsigned long flags;
 spin_lock_irqsave(&power_ctrl_lock, flags);
 dev_priv->display_power=ready;
 dev_priv->remesa_private_power_owner_usable=false;
 dev_priv->remesa_private_power_owner=NULL;
 dev_priv->remesa_private_power_transition=false;
 spin_unlock_irqrestore(&power_ctrl_lock, flags);
}

static void remesa_private_power_usable(struct drm_psb_private *dev_priv, bool usable)
{
 unsigned long flags;
 spin_lock_irqsave(&power_ctrl_lock, flags);
 dev_priv->remesa_private_power_owner_usable=usable;
 spin_unlock_irqrestore(&power_ctrl_lock, flags);
}

static int gma_resume_pci(struct pci_dev *pdev)
{
 struct drm_device *dev=pci_get_drvdata(pdev);
 struct drm_psb_private *dev_priv=dev->dev_private;
 int ret;
 if(!dev_priv->suspended)return 0;
 ret=pci_set_power_state(pdev,PCI_D0);
 if(ret)return ret;
 pci_restore_state(pdev);
 ret=pcibios_err_to_errno(pci_write_config_dword(pdev,0x5c,dev_priv->regs.saveBSM));
 if(ret)return ret;
 ret=pcibios_err_to_errno(pci_write_config_dword(pdev,0xFC,dev_priv->regs.saveVBT));
 if(ret)return ret;
 ret=pcibios_err_to_errno(pci_write_config_dword(pdev,PSB_PCIx_MSI_ADDR_LOC,dev_priv->msi_addr));
 if(ret)return ret;
 ret=pcibios_err_to_errno(pci_write_config_dword(pdev,PSB_PCIx_MSI_DATA_LOC,dev_priv->msi_data));
 if(ret)return ret;
 ret=pci_enable_device(pdev);
 if(ret){dev_err(&pdev->dev,"pci_enable failed: %d\n",ret);return ret;}
 dev_priv->suspended=false;
 return 0;
}

int gma_power_suspend(struct device *_dev)
{
 struct pci_dev *pdev=to_pci_dev(_dev);
 struct drm_device *dev=pci_get_drvdata(pdev);
 struct drm_psb_private *dev_priv=dev->dev_private;
 unsigned long flags;
 int ret=0;
 mutex_lock(&power_mutex);
 spin_lock_irqsave(&power_ctrl_lock,flags);
 if(dev_priv->suspended)goto unlock;
 if(dev_priv->display_count){ret=-EBUSY;goto unlock;}
 if(!dev_priv->display_power){ret=-EIO;goto unlock;}
 dev_priv->remesa_private_power_transition=true;
 dev_priv->remesa_private_power_owner=current;
 dev_priv->remesa_private_power_owner_usable=true;
 spin_unlock_irqrestore(&power_ctrl_lock,flags);
 remesa_route_source_lifecycle(dev);
 /* Existing raw IRQ/MMIO retirement remains unqualified. */
 psb_irq_uninstall(dev);
 ret=dev_priv->ops->save_regs(dev);
 if(ret){remesa_private_power_finish(dev_priv,false);goto done;}
 remesa_private_power_usable(dev_priv,false);
 ret=dev_priv->ops->power_down(dev);
 if(!ret)ret=gma_suspend_pci(pdev);
 remesa_private_power_finish(dev_priv,false);
 goto done;
unlock:
 spin_unlock_irqrestore(&power_ctrl_lock,flags);
done:
 mutex_unlock(&power_mutex);
 return ret;
}

int gma_power_resume(struct device *_dev)
{
 struct pci_dev *pdev=to_pci_dev(_dev);
 struct drm_device *dev=pci_get_drvdata(pdev);
 struct drm_psb_private *dev_priv=dev->dev_private;
 unsigned long flags;
 int ret;
 mutex_lock(&power_mutex);
 spin_lock_irqsave(&power_ctrl_lock,flags);
 if(dev_priv->display_power&&!dev_priv->suspended){
  spin_unlock_irqrestore(&power_ctrl_lock,flags);
  mutex_unlock(&power_mutex);return 0;
 }
 dev_priv->remesa_private_power_transition=true;
 dev_priv->remesa_private_power_owner=current;
 dev_priv->remesa_private_power_owner_usable=false;
 spin_unlock_irqrestore(&power_ctrl_lock,flags);
 remesa_route_source_lifecycle(dev);
 ret=gma_resume_pci(pdev);
 if(ret)goto done;
 ret=dev_priv->ops->power_up(dev);
 if(ret)goto done;
 remesa_private_power_usable(dev_priv,true);
 PSB_WVDC32(dev_priv->pge_ctl|_PSB_PGETBL_ENABLED,PSB_PGETBL_CTL);
 pci_write_config_word(pdev,PSB_GMCH_CTRL,dev_priv->gmch_ctrl|_PSB_GMCH_ENABLED);
 ret=psb_gtt_restore(dev);
 if(ret)goto done;
 ret=dev_priv->ops->restore_regs(dev);
 if(ret)goto done;
 psb_irq_preinstall(dev);
 ret=psb_irq_postinstall(dev);
done:
 remesa_private_power_finish(dev_priv,!ret);
 mutex_unlock(&power_mutex);
 return ret;
}

bool gma_power_is_on(struct drm_device *dev)
{
 struct drm_psb_private *dev_priv=dev->dev_private;
 unsigned long flags;
 bool ready;
 spin_lock_irqsave(&power_ctrl_lock,flags);
 ready=dev_priv->remesa_private_power_transition ?
       (remesa_private_power_owner(dev_priv)&&dev_priv->remesa_private_power_owner_usable) :
       dev_priv->display_power;
 spin_unlock_irqrestore(&power_ctrl_lock,flags);
 return ready;
}

bool gma_power_begin(struct drm_device *dev, bool force_on)
{
 struct drm_psb_private *dev_priv=dev->dev_private;
 unsigned long flags;
 bool ready;
 (void)force_on;
 spin_lock_irqsave(&power_ctrl_lock,flags);
 ready=dev_priv->remesa_private_power_transition ?
       (remesa_private_power_owner(dev_priv)&&dev_priv->remesa_private_power_owner_usable) :
       dev_priv->display_power;
 if(ready){
  dev_priv->display_count++;
  pm_runtime_get_noresume(&dev->pdev->dev);
 }
 /* Runtime PM enable is #if 0 in retained psb_drv.c. A force request while
  * offline cannot partially restore PCI, or reenter KMS under caller locks.
  * System resume must finish before ordinary claims become available. */
 spin_unlock_irqrestore(&power_ctrl_lock,flags);
 return ready;
}

void gma_power_end(struct drm_device *dev)
{
 struct drm_psb_private *dev_priv=dev->dev_private;
 unsigned long flags;
 spin_lock_irqsave(&power_ctrl_lock,flags);
 if(WARN_ON(dev_priv->display_count<=0)){
  spin_unlock_irqrestore(&power_ctrl_lock,flags);return;
 }
 dev_priv->display_count--;
 spin_unlock_irqrestore(&power_ctrl_lock,flags);
 pm_runtime_put_noidle(&dev->pdev->dev);
}
