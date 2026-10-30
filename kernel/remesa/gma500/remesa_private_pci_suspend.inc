/* SPDX-License-Identifier: GPL-2.0-only */
/* Private checked PCI save/disable contract; no GPU-stop or S3 proof. */
static int gma_suspend_pci(struct pci_dev *pdev)
{
 struct drm_device *dev=pci_get_drvdata(pdev);
 struct drm_psb_private *dev_priv=dev->dev_private;
 u32 bsm,vbt,msi_addr,msi_data;
 int ret;
 if(dev_priv->suspended)return 0;
 ret=pci_save_state(pdev);
 if(ret)return ret;
 ret=pcibios_err_to_errno(pci_read_config_dword(pdev,0x5c,&bsm));
 if(ret)return ret;
 ret=pcibios_err_to_errno(pci_read_config_dword(pdev,0xfc,&vbt));
 if(ret)return ret;
 ret=pcibios_err_to_errno(pci_read_config_dword(pdev,PSB_PCIx_MSI_ADDR_LOC,&msi_addr));
 if(ret)return ret;
 ret=pcibios_err_to_errno(pci_read_config_dword(pdev,PSB_PCIx_MSI_DATA_LOC,&msi_data));
 if(ret)return ret;
 dev_priv->regs.saveBSM=bsm;
 dev_priv->regs.saveVBT=vbt;
 dev_priv->msi_addr=msi_addr;
 dev_priv->msi_data=msi_data;
 pci_disable_device(pdev);
 /* This flag means PCI restore/enable is required, even if D3hot fails.
  * Do not let a failed D3 transition skip re-enable on a later resume. */
 dev_priv->suspended=true;
 return pci_set_power_state(pdev,PCI_D3hot);
}
