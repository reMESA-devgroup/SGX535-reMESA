/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual private power entry bodies; reduced PCI/MMIO/PM/task APIs.
 * Admission/error contract controls, not driver qualification. GPU0. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <stdio.h>
#include <pthread.h>
#define PCI_D0 0
#define PCI_D3hot 3
#define PSB_GMCH_CTRL 1
#define _PSB_GMCH_ENABLED 1
#define _PSB_PGETBL_ENABLED 1
#define PSB_PGETBL_CTL 2
#define PSB_PCIx_MSI_ADDR_LOC 3
#define PSB_PCIx_MSI_DATA_LOC 4
#define dev_err(...) ((void)0)
struct device {unsigned id;};
struct pci_dev {struct device dev;void *data;};
struct drm_device;
struct ops {int (*save_regs)(struct drm_device *),(*restore_regs)(struct drm_device *),
 (*power_up)(struct drm_device *),(*power_down)(struct drm_device *);};
struct drm_psb_private {
 bool suspended,display_power;int display_count;
 bool remesa_private_power_transition,remesa_private_power_owner_usable;
 void *remesa_private_power_owner;
 struct ops *ops;struct {int saveBSM,saveVBT;} regs;
 unsigned msi_addr,msi_data,pge_ctl,gmch_ctrl;
};
struct drm_device {struct drm_psb_private *dev_private;struct pci_dev *pdev;struct device *dev;};
static struct drm_psb_private private;
static struct pci_dev pci;
static struct drm_device device={&private,&pci,&pci.dev};
static pthread_mutex_t power_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t power_ctrl_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t control=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t change=PTHREAD_COND_INITIALIZER;
static _Thread_local bool interrupt_context;
static _Thread_local int task_identity;
#define current ((void *)&task_identity)
#define in_interrupt() interrupt_context
static bool spin_held,pci_active,restore_complete,pause_started,release_pause;
static unsigned runtime_refs,bad_mmio,powerdowns_with_refs,source_losses,irq_pre,irq_post;
static unsigned enable_under_spin,enable_calls,warnings;
static int enable_error,state_error,save_error,restore_error,up_error,down_error,gtt_error,worker_result;
static bool owner_nested;
static void owner_check(void);
enum pause_mode {NONE,SUSPEND_GAP,RESUME_GAP};
static enum pause_mode mode;
#define mutex_lock(m) ((void)pthread_mutex_lock(m))
#define mutex_unlock(m) ((void)pthread_mutex_unlock(m))
#define spin_lock_irqsave(m,f) do{(f)=0;assert(!pthread_mutex_lock(m));assert(!spin_held);spin_held=true;}while(0)
#define spin_unlock_irqrestore(m,f) do{(void)(f);assert(spin_held);spin_held=false;assert(!pthread_mutex_unlock(m));}while(0)
#define WARN_ON(c) ((c)?(warnings++,1):0)
static struct pci_dev *to_pci_dev(struct device *d)
{assert(d==&pci.dev);return &pci;}
static void *pci_get_drvdata(struct pci_dev *p){assert(p==&pci);return p->data;}
static void pause_at(enum pause_mode which)
{
 if(mode!=which)return;
 pthread_mutex_lock(&control);pause_started=true;pthread_cond_broadcast(&change);
 while(!release_pause)pthread_cond_wait(&change,&control);
 pthread_mutex_unlock(&control);
}
static void remesa_route_source_lifecycle(struct drm_device *d)
{assert(d==&device);source_losses++;pause_at(SUSPEND_GAP);}
static void mmio(void){if(!pci_active)bad_mmio++;}
static int save_regs(struct drm_device *d){assert(d==&device);mmio();owner_check();return save_error;}
static int restore_regs(struct drm_device *d){assert(d==&device);mmio();owner_check();restore_complete=true;return restore_error;}
static int power_up(struct drm_device *d){assert(d==&device);mmio();return up_error;}
static int power_down(struct drm_device *d)
{assert(d==&device);mmio();if(private.display_count)powerdowns_with_refs++;return down_error;}
static void psb_irq_uninstall(struct drm_device *d){assert(d==&device);}
static void psb_irq_preinstall(struct drm_device *d){assert(d==&device);irq_pre++;mmio();}
static int psb_irq_postinstall(struct drm_device *d){assert(d==&device);irq_post++;mmio();return 0;}
static int pci_set_power_state(struct pci_dev *p,int state)
{assert(p==&pci&&(state==PCI_D0||state==PCI_D3hot));return state_error;}
static void pci_restore_state(struct pci_dev *p){assert(p==&pci);}
static int pci_write_config_dword(struct pci_dev *p,unsigned reg,unsigned value)
{assert(p==&pci);(void)reg;(void)value;return 0;}
static int pci_write_config_word(struct pci_dev *p,unsigned reg,unsigned value)
{assert(p==&pci);(void)reg;(void)value;return 0;}
static int pci_enable_device(struct pci_dev *p)
{assert(p==&pci);enable_calls++;if(spin_held)enable_under_spin++;if(!enable_error)pci_active=true;return enable_error;}
static int psb_gtt_restore(struct drm_device *d)
{assert(d==&device);mmio();pause_at(RESUME_GAP);return gtt_error;}
#define PSB_WVDC32(v,r) do{(void)(v);(void)(r);mmio();}while(0)
static int pm_runtime_get_noresume(struct device *d){assert(d==&pci.dev);runtime_refs++;return 0;}
static int pm_runtime_put_noidle(struct device *d){assert(d==&pci.dev&&runtime_refs);runtime_refs--;return 0;}
static void gma_suspend_pci(struct pci_dev *p)
{assert(p==&pci);pci_active=false;private.suspended=true;}
#include "power_actual.inc"
static void owner_check(void)
{
 if(!owner_nested)return;
 assert(gma_power_is_on(&device));assert(gma_power_begin(&device,true));
 assert(private.display_count==1);gma_power_end(&device);
 interrupt_context=true;assert(!gma_power_is_on(&device));
 assert(!gma_power_begin(&device,false));interrupt_context=false;
}
static struct ops ops={save_regs,restore_regs,power_up,power_down};
static void fixture(bool active)
{
 assert(!runtime_refs&&!spin_held);pci.data=&device;
 private=(struct drm_psb_private){.ops=&ops,.suspended=!active,.display_power=active};
 pci_active=active;restore_complete=active;pause_started=release_pause=false;
 bad_mmio=powerdowns_with_refs=source_losses=irq_pre=irq_post=enable_under_spin=enable_calls=warnings=0;
 enable_error=state_error=save_error=restore_error=up_error=down_error=gtt_error=worker_result=0;owner_nested=false;mode=NONE;
}
static void *suspend_worker(void *unused)
{(void)unused;worker_result=gma_power_suspend(&pci.dev);return NULL;}
static void *resume_worker(void *unused)
{(void)unused;worker_result=gma_power_resume(&pci.dev);return NULL;}
static void wait_pause(void)
{pthread_mutex_lock(&control);while(!pause_started)pthread_cond_wait(&change,&control);pthread_mutex_unlock(&control);}
static void unpause(void)
{pthread_mutex_lock(&control);release_pause=true;pthread_cond_broadcast(&change);pthread_mutex_unlock(&control);}
int main(void)
{
 fixture(true);mode=SUSPEND_GAP;pthread_t thread;
 assert(!pthread_create(&thread,NULL,suspend_worker,NULL));wait_pause();
 assert(!gma_power_begin(&device,false));assert(!gma_power_begin(&device,true));
 assert(!gma_power_is_on(&device));assert(!private.display_count);
 unpause();assert(!pthread_join(thread,NULL));
 assert(!worker_result&&!powerdowns_with_refs&&private.suspended&&!private.display_power);
 fixture(false);mode=RESUME_GAP;
 assert(!pthread_create(&thread,NULL,resume_worker,NULL));wait_pause();
 assert(!restore_complete&&!private.display_power);
 assert(!gma_power_begin(&device,false)&&!gma_power_begin(&device,true));
 assert(!gma_power_is_on(&device));
 unpause();assert(!pthread_join(thread,NULL));assert(!worker_result&&restore_complete);
 assert(gma_power_begin(&device,true));gma_power_end(&device);
 /* No partial forced PCI resume, sleeping PCI call or KMS reentry. */
 fixture(false);assert(!gma_power_begin(&device,true));
 assert(!enable_calls&&!irq_pre&&!irq_post&&!bad_mmio&&!runtime_refs);
 fixture(false);enable_error=-EIO;assert(gma_power_resume(&pci.dev)==-EIO);
 assert(private.suspended&&!private.display_power&&!restore_complete&&!bad_mmio);
 assert(enable_calls==1&&!enable_under_spin&&!irq_pre&&!irq_post);
 enable_error=0;assert(!gma_power_resume(&pci.dev));assert(private.display_power);
 fixture(false);state_error=-EIO;assert(gma_power_resume(&pci.dev)==-EIO);
 assert(!enable_calls&&!bad_mmio&&!private.display_power&&private.suspended);
 /* Actual save/restore callbacks can nest; borrowed current in IRQ cannot. */
 fixture(true);owner_nested=true;assert(!gma_power_suspend(&pci.dev));
 assert(!private.display_count&&!runtime_refs&&!powerdowns_with_refs);
 assert(!gma_power_resume(&pci.dev));assert(private.display_power&&!runtime_refs);
 /* Ordinary claims reject suspend before lifecycle invalidation. */
 fixture(true);assert(gma_power_begin(&device,false));assert(gma_power_suspend(&pci.dev)==-EBUSY);
 assert(!source_losses&&private.display_power);gma_power_end(&device);
 /* Any callback failure keeps admission shut; recovery remains unqualified. */
 fixture(true);save_error=-EIO;assert(gma_power_suspend(&pci.dev)==-EIO);
 assert(!private.display_power&&!private.suspended&&!powerdowns_with_refs);
 fixture(true);down_error=-EIO;assert(gma_power_suspend(&pci.dev)==-EIO);
 assert(!private.display_power&&!private.suspended&&!gma_power_begin(&device,true));
 fixture(false);up_error=-EIO;assert(gma_power_resume(&pci.dev)==-EIO);
 assert(!private.display_power&&!irq_pre&&!irq_post);
 fixture(false);gtt_error=-EIO;assert(gma_power_resume(&pci.dev)==-EIO);
 assert(!private.display_power&&!restore_complete&&!irq_pre&&!irq_post);
 fixture(false);restore_error=-EIO;assert(gma_power_resume(&pci.dev)==-EIO);
 assert(!private.display_power&&!irq_pre&&!irq_post);
 assert(!runtime_refs&&!warnings&&!enable_under_spin);
 puts("PRIVATE_POWER_ADMISSION_TESTS_PASS actual private bodies; dependencies modeled; raw IRQ/brightness and physical PM remain unqualified, GPU0");
 return 0;
}
