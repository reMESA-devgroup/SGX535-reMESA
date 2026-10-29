/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual unload/probe bodies, modeled teardown APIs. No hardware access. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#define PAGE_SHIFT 12
#define CONFIG_ACPI 1
struct work_struct {unsigned id;};
struct opregion_asle;
struct psb_intel_opregion {void *header;struct work_struct asle_work;struct opregion_asle *asle;};
struct device {unsigned id;};
struct pci_dev {struct device dev;void *data;};
struct pci_device_id {unsigned long driver_data;};
struct drm_crtc {unsigned id;};
struct psb_gtt {int sem;unsigned mmu_gatt_start;};
struct drm_device;
struct ops {void (*chip_teardown)(struct drm_device *);};
struct drm_psb_private {
 bool modeset;void *backlight_device;struct ops *ops;
 void *pf_pd,*mmu,*scratch_page,*vdc_reg,*sgx_reg,*aux_reg;
 struct pci_dev *aux_pdev,*lpc_pdev;
 struct psb_gtt gtt;unsigned long vram_stolen_size;
 struct psb_intel_opregion opregion;struct drm_device *dev;
};
struct drm_device {struct drm_psb_private *dev_private;struct pci_dev *pdev;
 bool irq_enabled,unplugged;struct drm_crtc crtcs[2];};
static struct drm_device device;
static struct drm_psb_private private;
static struct pci_dev pci,aux,lpc;
static unsigned poll_stops,vblank_stops,irq_stops,backlight_stops,mode_stops;
static unsigned chip_stops,opregion_stops,power_stops,lifecycle_losses;
static unsigned pd_frees,mmu_frees,gtt_frees,page_frees,reg_unmaps,pci_puts,bios_frees,private_frees;
static unsigned pci_enables,pci_disables,device_allocs,loads,registrations,dev_puts;
static unsigned unplugs,revocations,fb_detaches;
static unsigned asle_joins;
static void (*asle_join_hook)(void),(*backlight_stop_hook)(void);
static bool retained,blocked,gem_retained,gem_blocked;
static int driver;
#define drm_for_each_crtc(crtc,dev) for((crtc)=(dev)->crtcs;(crtc)<(dev)->crtcs+2;(crtc)++)
static void remesa_route_source_lifecycle(struct drm_device *d){assert(d==&device);lifecycle_losses++;}
static void drm_kms_helper_poll_disable(struct drm_device *d){assert(d==&device);poll_stops++;}
static void drm_crtc_vblank_off(struct drm_crtc *c){assert(c>=device.crtcs&&c<device.crtcs+2);vblank_stops++;}
static void drm_irq_uninstall(struct drm_device *d){assert(d==&device&&d->irq_enabled);d->irq_enabled=false;irq_stops++;}
static void psb_lid_timer_takedown(struct drm_psb_private *p)
{assert(p==&private&&!backlight_stops&&!chip_stops);}
static void cancel_work_sync(struct work_struct *w)
{assert(w==&private.opregion.asle_work&&!device.irq_enabled&&!backlight_stops);
 asle_joins++;if(asle_join_hook)asle_join_hook();}
static void gma_backlight_exit(struct drm_device *d)
{assert(d==&device);if(backlight_stop_hook)backlight_stop_hook();
 else {assert(asle_joins==(private.opregion.header?1U:0U));}
 backlight_stops++;}
static void psb_modeset_cleanup(struct drm_device *d){assert(d==&device);mode_stops++;}
static void chip_teardown(struct drm_device *d){assert(d==&device);chip_stops++;}
static void psb_intel_opregion_fini(struct drm_device *d){assert(d==&device);opregion_stops++;}
static bool remesa_private_memory_device_retained(struct drm_device *d){assert(d==&device);return retained;}
static bool remesa_private_memory_rebind_blocked(void){return blocked;}
static bool remesa_private_gem_device_retained(struct drm_device *d){assert(d==&device);return gem_retained;}
static bool remesa_private_gem_rebind_blocked(void){return gem_blocked;}
static bool drm_dev_is_unplugged(struct drm_device *d){return d->unplugged;}
static void drm_dev_unplug(struct drm_device *d){assert(!d->unplugged);d->unplugged=true;unplugs++;}
static void remesa_private_stream_device_lost(struct drm_device *d){assert(d->unplugged);revocations++;}
static void remesa_private_fbdev_detach(struct drm_device *d){assert(d==&device);fb_detaches++;}
static void gma_power_uninit(struct drm_device *d){assert(d==&device);power_stops++;}
static void psb_mmu_free_pagedir(void *p){assert(p==private.pf_pd);pd_frees++;}
static void down_read(int *s){assert(s==&private.gtt.sem);}
static void up_read(int *s){assert(s==&private.gtt.sem);}
static void *psb_mmu_get_default_pd(void *mmu){assert(mmu==private.mmu);return mmu;}
static void psb_mmu_remove_pfn_sequence(void *pd,unsigned start,unsigned long pages)
{assert(pd==private.mmu&&start==private.gtt.mmu_gatt_start&&pages==16);}
static void psb_mmu_driver_takedown(void *mmu){assert(mmu==private.mmu);mmu_frees++;}
static void psb_gtt_takedown(struct drm_device *d){assert(d==&device);gtt_frees++;}
static void set_pages_wb(void *page,int n){assert(page==private.scratch_page&&n==1);}
static void __free_page(void *page){assert(page==private.scratch_page);page_frees++;}
static void iounmap(void *reg){assert(reg==private.vdc_reg||reg==private.sgx_reg||reg==private.aux_reg);reg_unmaps++;}
static void pci_dev_put(struct pci_dev *p){assert(p==&aux||p==&lpc);pci_puts++;}
static void psb_intel_destroy_bios(struct drm_device *d){assert(d==&device);bios_frees++;}
static void kfree(void *p){assert(p==&private);private_frees++;}
#include "unload_actual.inc"
static int pci_enable_device(struct pci_dev *p){assert(p==&pci);pci_enables++;return 0;}
static void pci_disable_device(struct pci_dev *p){assert(p==&pci);pci_disables++;}
static struct drm_device *drm_dev_alloc(int *d,struct device *parent)
{assert(d==&driver&&parent==&pci.dev);device_allocs++;return &device;}
static void pci_set_drvdata(struct pci_dev *p,struct drm_device *d){assert(p==&pci&&d==&device);p->data=d;}
static int psb_driver_load(struct drm_device *d,unsigned long flags){assert(d==&device&&flags==7);loads++;return 0;}
static int drm_dev_register(struct drm_device *d,unsigned long flags){assert(d==&device&&flags==7);registrations++;return 0;}
static void drm_dev_put(struct drm_device *d){assert(d==&device);dev_puts++;}
#define IS_ERR(value) ((void)(value),false)
#define PTR_ERR(value) ((void)(value),-ENOMEM)
#include "probe_actual.inc"
static struct ops ops={chip_teardown};
static void fixture(void)
{
 poll_stops=vblank_stops=irq_stops=backlight_stops=mode_stops=0;
 chip_stops=opregion_stops=power_stops=lifecycle_losses=0;
 pd_frees=mmu_frees=gtt_frees=page_frees=reg_unmaps=pci_puts=bios_frees=private_frees=0;
 pci_enables=pci_disables=device_allocs=loads=registrations=dev_puts=0;
 unplugs=revocations=fb_detaches=0;
 asle_joins=0;
 asle_join_hook=backlight_stop_hook=NULL;
 retained=blocked=gem_retained=gem_blocked=false;
 private=(struct drm_psb_private){.modeset=true,.backlight_device=&private,.ops=&ops,
  .pf_pd=(void*)0x1000,.mmu=(void*)0x2000,.scratch_page=(void*)0x3000,
  .vdc_reg=(void*)0x4000,.sgx_reg=(void*)0x5000,.aux_reg=(void*)0x6000,
  .aux_pdev=&aux,.lpc_pdev=&lpc,.gtt={0,0x40000000},.vram_stolen_size=16*4096,
  .opregion={.header=(void*)0x7000},.dev=&device};
 device=(struct drm_device){.dev_private=&private,.irq_enabled=true};
}
int main(void)
{
 fixture();retained=true;struct drm_psb_private before=private;
 psb_driver_unload(&device);
 assert(device.dev_private==&private&&!memcmp(&private,&before,sizeof private));
 assert(!pd_frees&&!mmu_frees&&!gtt_frees&&!page_frees&&!reg_unmaps&&!pci_puts&&!bios_frees&&!private_frees);
 assert(poll_stops==1&&vblank_stops==2&&irq_stops==1&&backlight_stops==1&&!mode_stops&&fb_detaches==1);
 assert(unplugs==1&&revocations==1);
 assert(chip_stops==1&&opregion_stops==1&&power_stops==1&&lifecycle_losses==1);
 fixture();gem_retained=true;before=private;psb_driver_unload(&device);
 assert(device.dev_private==&private&&!memcmp(&private,&before,sizeof private));
 assert(!pd_frees&&!mmu_frees&&!gtt_frees&&!page_frees&&!reg_unmaps&&!private_frees);
 assert(chip_stops==1&&opregion_stops==1&&power_stops==1&&irq_stops==1);
 fixture();psb_driver_unload(&device);
 assert(!device.dev_private&&pd_frees==1&&mmu_frees==1&&gtt_frees==1&&page_frees==1);
 assert(reg_unmaps==3&&pci_puts==2&&bios_frees==1&&private_frees==1&&power_stops==1);
 assert(mode_stops==1&&!fb_detaches&&unplugs==1&&revocations==1);
 fixture();private.modeset=false;psb_driver_unload(&device);
 assert(!unplugs&&!revocations&&private_frees==1);
 fixture();private.opregion.header=NULL;psb_driver_unload(&device);
 assert(!asle_joins&&backlight_stops==1&&private_frees==1);
 fixture();private.opregion.header=NULL;private.backlight_device=NULL;psb_driver_unload(&device);
 assert(!asle_joins&&!backlight_stops&&private_frees==1);
 fixture();device.irq_enabled=false;psb_driver_unload(&device);
 assert(asle_joins==1&&!irq_stops&&backlight_stops==1);
 fixture();private.backlight_device=NULL;psb_driver_unload(&device);
 assert(asle_joins==1&&!backlight_stops&&private_frees==1);
 fixture();blocked=true;struct pci_device_id id={7};
 assert(psb_pci_probe(&pci,&id)==-EBUSY);
 assert(!pci_enables&&!pci_disables&&!device_allocs&&!loads&&!registrations&&!dev_puts);
 fixture();gem_blocked=true;assert(psb_pci_probe(&pci,&id)==-EBUSY);
 assert(!pci_enables&&!device_allocs&&!loads&&!registrations);
 fixture();assert(!psb_pci_probe(&pci,&id));
 assert(pci_enables==1&&device_allocs==1&&loads==1&&registrations==1&&!pci_disables&&!dev_puts);
 puts("PASS actual unload retains SGX/MMU/GTT dependency fields, joins modeled CPU cleanup; blocked probe performs no PCI enable/allocation/load; normal paths preserved. CPU model, not GPU stop or late GEM safety.");
 return 0;
}
