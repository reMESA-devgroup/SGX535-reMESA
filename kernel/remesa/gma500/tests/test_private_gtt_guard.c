/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual GTT bodies and entry guards; CPU MMIO/page/API models and real pthread
 * joining. Guard-page slot is allocated, so the old negative-roll witness does
 * not perform a host memory violation. It violates the logical page range. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
typedef uint32_t u32;
#define __iomem
#define PSB_MMU_CACHED_MEMORY 1u
#define WARN_ON(value) assert(!(value))
#define DRM_ERROR(...) ((void)0)
struct page {u32 pfn;};
struct mutex {pthread_mutex_t native;};
static void mutex_lock(struct mutex *m){assert(!pthread_mutex_lock(&m->native));}
static void mutex_unlock(struct mutex *m){assert(!pthread_mutex_unlock(&m->native));}
struct drm_psb_private {struct mutex gtt_mutex;struct {u32 gatt_start;} gtt;void *mmu;};
struct drm_device {bool unplugged,hardware_alive;struct drm_psb_private *dev_private;};
struct drm_gem_object {struct drm_device *dev;};
struct gtt_range {struct drm_gem_object gem;int in_gart,stolen,npage,roll;u32 offset;struct page **pages;};
static struct drm_psb_private private={.gtt_mutex={PTHREAD_MUTEX_INITIALIZER}};
static struct drm_device device,other;
static struct page canary={999},page0={10},page1={20};
static struct page *pages[]={&canary,&page0,&page1};
static u32 bar[4];
static unsigned writes,reads,canary_pfn,attaches,detaches,inserts,removes,late_access;
static int attach_error,idle_error;
static pthread_mutex_t state=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static _Thread_local unsigned entered;
static unsigned readers;
static bool block_idle,idle_started,resume_idle,remove_started,remove_done;
static bool drm_dev_enter(struct drm_device *dev,int *index)
{
 assert(!pthread_mutex_lock(&state));bool ok=!dev->unplugged;
 if(ok){readers++;entered++;*index=31;}
 assert(!pthread_mutex_unlock(&state));return ok;
}
static void drm_dev_exit(int index)
{
 assert(index==31&&entered);assert(!pthread_mutex_lock(&state));
 readers--;entered--;assert(!pthread_cond_broadcast(&changed));assert(!pthread_mutex_unlock(&state));
}
static void hardware(void)
{assert(entered);if(!device.hardware_alive)late_access++;}
static int psb_gtt_attach_pages(struct gtt_range *r)
{assert(r->gem.dev==&device);hardware();attaches++;return attach_error;}
static void psb_gtt_detach_pages(struct gtt_range *r)
{assert(r->gem.dev==&device);hardware();detaches++;}
static int psb_gtt_insert(struct drm_device *d,struct gtt_range *r,int roll)
{assert(d==&device&&r->gem.dev==d&&!roll);hardware();inserts++;return 0;}
static void psb_gtt_remove(struct drm_device *d,struct gtt_range *r)
{assert(d==&device&&r->gem.dev==d);hardware();removes++;}
static void *psb_mmu_get_default_pd(void *mmu){assert(mmu==private.mmu);return mmu;}
static void psb_mmu_insert_pages(void *pd,struct page **p,u32 base,int count,int a,int b,unsigned flags)
{assert(pd==private.mmu&&p==&pages[1]&&base==0x40000000&&count==2&&!a&&!b&&flags==1);hardware();}
static void psb_mmu_remove_pages(void *pd,u32 base,int count,int a,int b)
{assert(pd==private.mmu&&base==0x40000000&&count==2&&!a&&!b);hardware();}
static int gma_blt_wait_idle(struct drm_psb_private *p)
{
 assert(p==&private&&entered);assert(!pthread_mutex_lock(&state));idle_started=true;
 assert(!pthread_cond_broadcast(&changed));
 while(block_idle&&!resume_idle)assert(!pthread_cond_wait(&changed,&state));
 hardware();assert(!pthread_mutex_unlock(&state));return idle_error;
}
static u32 *psb_gtt_entry(struct drm_device *d,struct gtt_range *r)
{assert(d==&device&&r->gem.dev==d);hardware();return bar;}
static u32 page_to_pfn(struct page *p)
{if(p==&canary)canary_pfn++;return p->pfn;}
static u32 psb_gtt_mask_pte(u32 pfn,int flags){assert(flags==1);return (pfn<<12)|1u;}
static void iowrite32(u32 value,u32 *address)
{assert(address>=bar&&address<bar+4);hardware();*address=value;writes++;}
static u32 ioread32(u32 *address)
{assert(address>=bar&&address<bar+4);hardware();reads++;return *address;}
#include "gtt_native_actual.inc"
#include "remesa_private_gtt_guard.inc"
static struct gtt_range range;
static void fixture(void)
{
 assert(!readers&&!entered);device=(struct drm_device){false,true,&private};other=device;
 private.gtt.gatt_start=0x40000000;private.mmu=&private;
 range=(struct gtt_range){.gem={&device},.npage=2,.pages=&pages[1]};
 writes=reads=canary_pfn=attaches=detaches=inserts=removes=late_access=0;
 attach_error=idle_error=0;block_idle=idle_started=resume_idle=remove_started=remove_done=false;
}
static void *unpin_worker(void *unused){(void)unused;psb_gtt_unpin(&range);return NULL;}
static void *remove_worker(void *unused)
{
 (void)unused;assert(!pthread_mutex_lock(&state));device.unplugged=true;remove_started=true;
 assert(!pthread_cond_broadcast(&changed));
 while(readers)assert(!pthread_cond_wait(&changed,&state));
 device.hardware_alive=false;remove_done=true;assert(!pthread_cond_broadcast(&changed));
 assert(!pthread_mutex_unlock(&state));return NULL;
}
int main(void)
{
 fixture();assert(!psb_gtt_pin(&range)&&range.in_gart==1&&attaches==1&&inserts==1);
 psb_gtt_unpin(&range);assert(!range.in_gart&&detaches==1&&removes==1&&!entered&&!readers);
 fixture();attach_error=-ENOMEM;assert(psb_gtt_pin(&range)==-ENOMEM&&!range.in_gart&&!inserts);
 fixture();range.in_gart=1;idle_error=-EBUSY;psb_gtt_unpin(&range);
 assert(range.in_gart==1&&!removes&&!detaches&&!readers&&!entered);
 fixture();range.in_gart=1;psb_gtt_roll(&device,&range,1);
 assert(writes==2&&reads==1&&!canary_pfn&&range.roll==1&&bar[0]==((20u<<12)|1)&&bar[1]==((10u<<12)|1));
 /* Actual old roll accepts -1 and consumes an allocated canary page slot. */
 fixture();range.in_gart=1;int index;assert(drm_dev_enter(&device,&index));
 remesa_private_gtt_roll_live(&device,&range,-1);drm_dev_exit(index);
 assert(canary_pfn==1&&writes==3);
 fixture();range.in_gart=1;psb_gtt_roll(&device,&range,-1);
 psb_gtt_roll(&device,&range,2);psb_gtt_roll(&other,&range,1);
 assert(!writes&&!reads&&!canary_pfn&&!range.roll&&!readers);
 fixture();device.unplugged=true;device.dev_private=NULL;
 assert(psb_gtt_pin(&range)==-ENODEV);psb_gtt_unpin(&range);psb_gtt_roll(&device,&range,1);
 assert(!attaches&&!inserts&&!removes&&!detaches&&!writes&&!reads);
 assert(psb_gtt_pin(NULL)==-EINVAL);psb_gtt_unpin(NULL);psb_gtt_roll(NULL,&range,0);
 /* An already admitted outer reader does not override later inner denial. */
 fixture();assert(drm_dev_enter(&device,&index));device.unplugged=true;
 assert(psb_gtt_pin(&range)==-ENODEV);psb_gtt_unpin(&range);
 assert(entered==1&&readers==1&&!attaches);drm_dev_exit(index);
 /* A native unpin admitted before removal must finish before hardware loss. */
 fixture();range.in_gart=1;block_idle=true;pthread_t worker,remover;
 assert(!pthread_create(&worker,NULL,unpin_worker,NULL));
 assert(!pthread_mutex_lock(&state));
 while(!idle_started)assert(!pthread_cond_wait(&changed,&state));
 assert(!pthread_mutex_unlock(&state));assert(!pthread_create(&remover,NULL,remove_worker,NULL));
 assert(!pthread_mutex_lock(&state));
 while(!remove_started)assert(!pthread_cond_wait(&changed,&state));
 assert(readers==1&&!remove_done&&device.hardware_alive);
 resume_idle=true;assert(!pthread_cond_broadcast(&changed));assert(!pthread_mutex_unlock(&state));
 assert(!pthread_join(worker,NULL));assert(!pthread_join(remover,NULL));
 assert(!range.in_gart&&!late_access&&remove_done&&!readers&&!entered);
 puts("PASS actual GTT pin/unpin/roll + guards: native semantics/errors, negative-roll canary rejected, postloss no private/MMIO access, preloss join, nested denial; CPU model, GPU0.");
}
