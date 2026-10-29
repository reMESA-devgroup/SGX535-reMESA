/* SPDX-License-Identifier: GPL-2.0-only */
/* CPU counterexample: actual GEM destructor, GTT free and PCI remove bodies.
 * DRM/GTT teardown is modeled; no freed pointer is dereferenced and no GPU
 * operation is performed. A successful run confirms the unsafe ordering. */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <pthread.h>
#define container_of(ptr,type,member) ((type *)((char *)(ptr)-offsetof(type,member)))
struct drm_device {bool unplugged,hardware_alive;};
struct drm_gem_object {struct drm_device *dev;};
struct gtt_range {struct drm_gem_object gem;unsigned mmapping,in_gart,stolen;int resource;};
struct pci_dev {struct drm_device *data;};
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static bool pause_release,release_started,resume_release,unregister_release;
static unsigned offsets,releases,gtt_frees,late_gtt,unloads,puts_count;
static struct drm_device device;
static struct gtt_range object;
static struct pci_dev pci={&device};
static void drm_gem_free_mmap_offset(struct drm_gem_object *obj)
{assert(obj==&object.gem);offsets++;}
static void drm_gem_object_release(struct drm_gem_object *obj)
{
 assert(obj==&object.gem);assert(!pthread_mutex_lock(&lock));
 releases++;release_started=true;assert(!pthread_cond_broadcast(&changed));
 while(pause_release&&!resume_release)
  assert(!pthread_cond_wait(&changed,&lock));
 assert(!pthread_mutex_unlock(&lock));
}
static void psb_gtt_unpin(struct gtt_range *range)
{
 assert(range==&object&&range->in_gart==1);assert(!pthread_mutex_lock(&lock));
 if(!range->gem.dev->hardware_alive)late_gtt++;
 range->in_gart--; /* Modeled API; record the invalid access instead of UAF. */
 assert(!pthread_mutex_unlock(&lock));
}
#define WARN_ON(value) do {assert(!(value));}while(0)
static void release_resource(int *resource){assert(resource==&object.resource);}
static void kfree(void *range){assert(range==&object);gtt_frees++;}
#include "gtt_free_actual.inc"
#include "gem_free_actual.inc"
static void *pci_get_drvdata(struct pci_dev *p){return p->data;}
static void drm_dev_unregister(struct drm_device *dev)
{assert(dev==&device);if(unregister_release)psb_gem_free_object(&object.gem);}
static void drm_dev_unplug(struct drm_device *dev)
{
 /* No destructor entry is registered with SRCU in the actual body. There
  * are therefore no modeled guarded readers for this remove to join. */
 assert(!pthread_mutex_lock(&lock));dev->unplugged=true;
 assert(!pthread_mutex_unlock(&lock));drm_dev_unregister(dev);
}
static void psb_driver_unload(struct drm_device *dev)
{
 assert(!pthread_mutex_lock(&lock));dev->hardware_alive=false;unloads++;
 assert(!pthread_mutex_unlock(&lock));
}
static void drm_dev_put(struct drm_device *dev){assert(dev==&device);puts_count++;}
void remesa_private_stream_device_lost(struct drm_device *dev)
{assert(dev==&device&&dev->unplugged&&dev->hardware_alive);}
#include "remove_actual.inc"
static void fixture(void)
{
 device=(struct drm_device){.hardware_alive=true};
 object=(struct gtt_range){.gem={&device},.mmapping=1,.in_gart=1};
 pause_release=release_started=resume_release=unregister_release=false;
 offsets=releases=gtt_frees=late_gtt=unloads=puts_count=0;
}
static void *free_worker(void *unused)
{(void)unused;psb_gem_free_object(&object.gem);return NULL;}
int main(void)
{
 /* Destructor admitted before unplug is not joined by current remove. */
 fixture();pause_release=true;pthread_t worker;
 assert(!pthread_create(&worker,NULL,free_worker,NULL));
 assert(!pthread_mutex_lock(&lock));
 while(!release_started)assert(!pthread_cond_wait(&changed,&lock));
 assert(!pthread_mutex_unlock(&lock));psb_pci_remove(&pci);
 assert(device.unplugged&&unloads==1&&puts_count==1);
 assert(!pthread_mutex_lock(&lock));resume_release=true;
 assert(!pthread_cond_broadcast(&changed));assert(!pthread_mutex_unlock(&lock));
 assert(!pthread_join(worker,NULL));
 assert(offsets==1&&releases==1&&gtt_frees==1&&late_gtt==1);
 puts("WITNESS: pre-unplug GEM destructor reaches GTT after unload");
 /* A VMA/object reference may outlive the PCI remove entirely. */
 fixture();psb_pci_remove(&pci);psb_gem_free_object(&object.gem);
 assert(late_gtt==1&&gtt_frees==1);
 puts("WITNESS: post-unplug GEM destructor reaches GTT after unload");
 /* Control: unregister destruction precedes unload in the actual remove. */
 fixture();unregister_release=true;psb_pci_remove(&pci);
 assert(!late_gtt&&gtt_frees==1&&unloads==1&&puts_count==1);
 puts("CONTROL: unregister GEM destruction occurs before unload");
 puts("CPU counterexample only; not kernel SRCU or physical UAF evidence");
 return 0;
}
