/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual PCI helpers, modeled PCI APIs. No hardware operations. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
typedef uint32_t u32;
#define PCI_D0 0
#define PCI_D3hot 3
#define PSB_PCIx_MSI_ADDR_LOC 3
#define PSB_PCIx_MSI_DATA_LOC 4
#define dev_err(...) ((void)0)
struct drm_psb_private {bool suspended;struct {u32 saveBSM,saveVBT;} regs;u32 msi_addr,msi_data;};
struct drm_device {struct drm_psb_private *dev_private;};
struct pci_dev {int dev;};
static struct drm_psb_private private;
static struct drm_device device={&private};
static struct pci_dev pci;
static int save_error,state_error,enable_error,config_error;
static unsigned read_fail,write_fail,reads,writes,disables,enables,states,restores;
static bool disabled;
static void *pci_get_drvdata(struct pci_dev *p){assert(p==&pci);return &device;}
static int pci_save_state(struct pci_dev *p){assert(p==&pci);return save_error;}
static int pci_read_config_dword(struct pci_dev *p,unsigned reg,void *value)
{assert(p==&pci);reads++;*(u32 *)value=reads==read_fail?~0U:reg;return reads==read_fail?config_error:0;}
static int pci_write_config_dword(struct pci_dev *p,unsigned reg,u32 value)
{assert(p==&pci);(void)reg;(void)value;writes++;return writes==write_fail?config_error:0;}
static void pci_disable_device(struct pci_dev *p){assert(p==&pci);disables++;disabled=true;}
static int pci_set_power_state(struct pci_dev *p,int state)
{assert(p==&pci&&(state==PCI_D0||state==PCI_D3hot));states++;return state_error;}
static void pci_restore_state(struct pci_dev *p){assert(p==&pci);restores++;}
static int pci_enable_device(struct pci_dev *p)
{assert(p==&pci);enables++;if(!enable_error)disabled=false;return enable_error;}
/* Target pci.h maps PCIBIOS_SET_FAILED to EIO and preserves negative errno. */
static int pcibios_err_to_errno(int err){return err>0?-EIO:err;}
#include "power_actual.inc"
static void fixture(bool suspended)
{
 private=(struct drm_psb_private){.suspended=suspended,.regs={123,456},.msi_addr=789,.msi_data=987};
 save_error=state_error=enable_error=0;config_error=0x88;
 read_fail=write_fail=reads=writes=disables=enables=states=restores=0;disabled=suspended;
}
static int suspend_call(void)
{
#if CHECKED_PCI
 return gma_suspend_pci(&pci);
#else
 gma_suspend_pci(&pci);return 0;
#endif
}
int main(void)
{
 (void)pcibios_err_to_errno;
 fixture(false);save_error=-ENOMEM;
 assert(suspend_call()==(CHECKED_PCI?-ENOMEM:0));
 assert(CHECKED_PCI?(!reads&&!disables&&!states&&!private.suspended):(disables==1&&private.suspended));
 for(unsigned i=1;i<=4;i++){
  fixture(false);read_fail=i;
  assert(suspend_call()==(CHECKED_PCI?-EIO:0));
  if(CHECKED_PCI){assert(reads==i&&!disables&&!states&&!private.suspended);assert(private.regs.saveBSM==123&&private.regs.saveVBT==456&&private.msi_addr==789&&private.msi_data==987);}
  else assert(reads==4&&disables==1&&private.suspended);
 }
 fixture(false);state_error=-EIO;
 assert(suspend_call()==(CHECKED_PCI?-EIO:0));assert(disabled&&private.suspended&&disables==1);
 /* D3 failure after disable still requires enable; no assertion of GPU recovery. */
 state_error=0;assert(gma_resume_pci(&pci)==(CHECKED_PCI?0:1));
 assert(!disabled&&!private.suspended&&enables==1);
 for(unsigned i=1;i<=4;i++){
  fixture(true);write_fail=i;
  assert(gma_resume_pci(&pci)==(CHECKED_PCI?-EIO:1));
  assert(CHECKED_PCI?(writes==i&&!enables&&private.suspended&&disabled):(writes==4&&enables==1&&!private.suspended));
 }
 fixture(true);state_error=-EIO;
 assert(gma_resume_pci(&pci)==(CHECKED_PCI?-EIO:1));
 assert(CHECKED_PCI?(!writes&&!restores&&!enables&&private.suspended):(enables==1&&!private.suspended));
 fixture(true);enable_error=-EIO;
 assert(gma_resume_pci(&pci)==(CHECKED_PCI?-EIO:0));assert(private.suspended&&disabled);
 fixture(false);assert(!suspend_call());assert(reads==4&&disabled&&private.suspended);
 assert(gma_resume_pci(&pci)==(CHECKED_PCI?0:1));assert(!disabled&&!private.suspended&&writes==4);
 fixture(true);assert(!suspend_call());assert(!reads&&!states&&!disables);
 puts(CHECKED_PCI?"PCI_HELPER_ERRORS_MODEL_PASS GPU0":"PCI_HELPER_COUNTEREXAMPLES_CONFIRMED NOT safety pass GPU0");
 return 0;
}
