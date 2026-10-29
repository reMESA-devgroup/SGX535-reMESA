/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual guards, color fault and before/after remove; modeled DRM SRCU contract
 * with real pthread synchronization. No kernel/hardware or GPU execution. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
typedef unsigned vm_fault_t;
#define VM_FAULT_SIGBUS 2u
#define FAULT_FLAG_WRITE 1u
#include "remesa_color_view.h"
struct page {unsigned value;};
struct drm_device {bool unplugged;};
struct drm_minor {struct drm_device *dev;};
struct drm_file {struct drm_minor *minor;};
struct file {struct drm_file *private_data;};
struct drm_gem_object {struct drm_device *dev;};
struct vm_area_struct {void *vm_private_data;};
struct vm_fault {struct vm_area_struct *vma;unsigned long pgoff,address;unsigned flags;};
struct pci_dev {struct drm_device *data;};
struct mutex {bool held;};
struct remesa_gem_memory {struct drm_device *dev;struct mutex lock;};
struct backing {struct remesa_gem_memory *memory;struct page **pages;};
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t change=PTHREAD_COND_INITIALIZER;
static unsigned readers,calls,teardowns,dev_puts,unregisters,inserts,orphans,device_orphans;
static unsigned live_orphans;static bool native_postclose_finished;
static bool block_call,call_started,release_call,remove_done;
static _Thread_local unsigned entries;
static bool drm_dev_enter(struct drm_device *dev,int *index)
{
 bool ok;assert(!pthread_mutex_lock(&lock));ok=!dev->unplugged;
 if(ok){readers++;entries++;*index=123;}
 assert(!pthread_mutex_unlock(&lock));return ok;
}
static void drm_dev_exit(int index)
{
 assert(index==123&&entries);assert(!pthread_mutex_lock(&lock));
 entries--;assert(readers);readers--;assert(!pthread_cond_broadcast(&change));
 assert(!pthread_mutex_unlock(&lock));
}
static long backend_call(void)
{
 assert(entries);assert(!pthread_mutex_lock(&lock));calls++;call_started=true;
 assert(!pthread_cond_broadcast(&change));
 while(block_call&&!release_call)assert(!pthread_cond_wait(&change,&lock));
 assert(!pthread_mutex_unlock(&lock));return 1234;
}
static long psb_unlocked_ioctl(struct file *file,unsigned cmd,unsigned long arg)
{assert(file&&cmd==7&&arg==9);return backend_call();}
#ifdef CONFIG_COMPAT
static long drm_compat_ioctl(struct file *file,unsigned cmd,unsigned long arg)
{assert(file&&cmd==7&&arg==9);return backend_call();}
#endif
static int remesa_gem_color_mmap(struct file *file,struct vm_area_struct *vma)
{assert(file&&vma);return (int)backend_call();}
static vm_fault_t psb_gem_fault(struct vm_fault *fault)
{assert(fault&&fault->vma);return (vm_fault_t)backend_call();}
static void remesa_stream_postclose(struct drm_device *dev,struct drm_file *file)
{assert(dev&&file);(void)backend_call();native_postclose_finished=true;}
static void drm_fb_helper_lastclose(struct drm_device *dev)
{assert(dev);(void)backend_call();}
void remesa_private_stream_file_lost(struct drm_device *dev,struct drm_file *file)
{assert(dev&&file);if(entries){assert(native_postclose_finished);native_postclose_finished=false;live_orphans++;}
 else {assert(dev->unplugged);orphans++;}}
void remesa_private_stream_device_lost(struct drm_device *dev)
{assert(dev&&dev->unplugged&&!entries&&!teardowns&&!readers);device_orphans++;}
static void *pci_get_drvdata(struct pci_dev *pci){return pci->data;}
static void drm_dev_unregister(struct drm_device *dev){(void)dev;unregisters++;}
static void drm_dev_unplug(struct drm_device *dev)
{
 assert(!entries);assert(!pthread_mutex_lock(&lock));dev->unplugged=true;
 assert(!pthread_cond_broadcast(&change));
 while(readers)assert(!pthread_cond_wait(&change,&lock));
 assert(!pthread_mutex_unlock(&lock));drm_dev_unregister(dev);
}
static void psb_driver_unload(struct drm_device *dev){(void)dev;teardowns++;}
static void drm_dev_put(struct drm_device *dev){(void)dev;dev_puts++;}
static void mutex_lock(struct mutex *m){assert(entries&&!m->held);m->held=true;}
static void mutex_unlock(struct mutex *m){assert(entries&&m->held);m->held=false;}
static bool color_readable(struct remesa_gem_memory *s,struct backing *b)
{assert(entries&&s->lock.held&&b->memory==s);return true;}
static vm_fault_t vmf_insert_page(struct vm_area_struct *vma,unsigned long address,struct page *page)
{assert(entries&&vma&&address==0x123000&&page);inserts++;return 0;}
#include "remesa_private_entry_guard.inc"
#include "color_fault_actual.inc"
#include "remove_actual.inc"
#include "baseline_remove_actual.inc"
static struct drm_device device;
static struct drm_minor minor={&device};
static struct drm_file drm_file={&minor};
static struct file file={&drm_file};
static struct pci_dev pci={&device};
static void fixture(void)
{
 assert(!entries&&!readers);device.unplugged=false;
 calls=teardowns=dev_puts=unregisters=inserts=orphans=device_orphans=0;
 live_orphans=0;native_postclose_finished=false;
 block_call=call_started=release_call=remove_done=false;
}
static void *ioctl_worker(void *unused)
{(void)unused;assert(remesa_private_unlocked_ioctl(&file,7,9)==1234);return NULL;}
static void *remove_worker(void *old)
{
 if(old)baseline_remove(&pci);else psb_pci_remove(&pci);
 assert(!pthread_mutex_lock(&lock));remove_done=true;
 assert(!pthread_cond_broadcast(&change));assert(!pthread_mutex_unlock(&lock));return NULL;
}
static void scenario(bool old)
{
 fixture();block_call=true;pthread_t ioctl_thread,remove_thread;
 assert(!pthread_create(&ioctl_thread,NULL,ioctl_worker,NULL));
 assert(!pthread_mutex_lock(&lock));
 while(!call_started)assert(!pthread_cond_wait(&change,&lock));
 assert(!pthread_mutex_unlock(&lock));
 assert(!pthread_create(&remove_thread,NULL,remove_worker,old?(void*)1:NULL));
 assert(!pthread_mutex_lock(&lock));
 while(old?!remove_done:!device.unplugged)assert(!pthread_cond_wait(&change,&lock));
 assert(readers==1&&calls==1);
 assert(old?(teardowns==1&&remove_done):(!teardowns&&!remove_done));
 assert(!pthread_mutex_unlock(&lock));
 if(!old){
  struct drm_gem_object object={&device};struct vm_area_struct vma={&object};struct vm_fault vmf={&vma,0,0,0};
  assert(remesa_private_unlocked_ioctl(&file,7,9)==-ENODEV);
  assert(remesa_private_mmap(&file,&vma)==-ENODEV);
  assert(remesa_private_gem_fault(&vmf)==VM_FAULT_SIGBUS);
#ifdef CONFIG_COMPAT
  assert(remesa_private_compat_ioctl(&file,7,9)==-ENODEV);
#endif
  struct page page={0};struct page *pages[]={&page};
  struct remesa_gem_memory memory={&device,{false}};struct backing backing={&memory,pages};
  vma.vm_private_data=&backing;vmf.pgoff=REMESA_COLOR_VIEW_PGOFF;vmf.address=0x123000;
  assert(color_fault(&vmf)==VM_FAULT_SIGBUS&&!inserts);
  remesa_private_postclose(&device,&drm_file);
  remesa_private_lastclose(&device);
  assert(orphans==1);
 }
 assert(!pthread_mutex_lock(&lock));assert(calls==1);release_call=true;
 assert(!pthread_cond_broadcast(&change));assert(!pthread_mutex_unlock(&lock));
 assert(!pthread_join(ioctl_thread,NULL));assert(!pthread_join(remove_thread,NULL));
 assert(!readers&&teardowns==1&&dev_puts==1&&unregisters==1);
 assert(device_orphans==(old?0u:1u));
}
int main(void)
{
 scenario(true);scenario(false);fixture();
 remesa_private_postclose(&device,&drm_file);
 remesa_private_lastclose(&device);assert(calls==2&&!orphans&&!readers&&live_orphans==1&&!native_postclose_finished);
 struct drm_gem_object object={&device};struct vm_area_struct vma={&object};struct vm_fault vmf={&vma,0,0,0};
 assert(remesa_private_mmap(&file,&vma)==1234);
 assert(remesa_private_gem_fault(&vmf)==1234);
#ifdef CONFIG_COMPAT
 assert(remesa_private_compat_ioctl(&file,7,9)==1234);
#else
 assert(remesa_private_compat_ioctl==NULL);
#endif
 struct page page={0};struct page *pages[]={&page};struct remesa_gem_memory memory={&device,{false}};
 struct backing backing={&memory,pages};vma.vm_private_data=&backing;
 vmf.pgoff=REMESA_COLOR_VIEW_PGOFF;vmf.address=0x123000;
 assert(!color_fault(&vmf)&&inserts==1&&!readers&&!entries);
 assert(remesa_private_unlocked_ioctl(NULL,7,9)==-ENODEV);
 puts("PASS actual guarded ioctl/compat/mmap/fault entries, color fault, old remove overlap reproduced, new unplug joins CPU entries; DRM contract modeled with pthreads, GPU0");
}
