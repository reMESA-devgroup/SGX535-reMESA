/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual console sync/submit bodies, modeled PM, clocks, FIFO and MMIO. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <pthread.h>
#define HZ 4
#define PSB_CR_2D_SOCIF 1
#define PSB_CR_2D_BLIT_STATUS 2
#define _PSB_C2_SOCIF_EMPTY 96U
#define _PSB_C2B_STATUS_BUSY 1U
#define PSB_SGX_2D_SLAVE_PORT 0x100
struct drm_device;
struct drm_psb_private {pthread_mutex_t lock_2d;void *remesa_twod_owner;};
struct drm_device {struct drm_psb_private *dev_private;};
struct drm_framebuffer {struct drm_device *dev;};
struct drm_fb_helper {struct drm_framebuffer *fb;};
struct fb_info {struct drm_fb_helper *par;};
static struct drm_psb_private private;
static struct drm_device device={&private};
static struct drm_framebuffer fb={&device};
static struct drm_fb_helper helper={&fb};
static struct fb_info info={&helper};
static pthread_mutex_t admission=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t control=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static bool active,pause_first,paused,released,queued;
static unsigned refs,reads,writes,bad_mmio,producers,source_losses;
static unsigned long jiffies;
static int worker_result;
enum mode {IDLE,FIFO_BUSY,BLIT_BUSY,FINISHING};
static enum mode poll_mode;
static bool gma_power_begin(struct drm_device *d,bool force)
{assert(d==&device&&!force);pthread_mutex_lock(&admission);bool ok=active;if(ok)refs++;pthread_mutex_unlock(&admission);return ok;}
static void gma_power_end(struct drm_device *d)
{assert(d==&device);pthread_mutex_lock(&admission);assert(refs);refs--;pthread_mutex_unlock(&admission);}
static int suspend_model(void)
{pthread_mutex_lock(&admission);if(refs){pthread_mutex_unlock(&admission);return -EBUSY;}active=false;pthread_mutex_unlock(&admission);return 0;}
static void seam(void)
{if(!pause_first||reads!=1)return;pthread_mutex_lock(&control);paused=true;pthread_cond_broadcast(&changed);while(!released)pthread_cond_wait(&changed,&control);pthread_mutex_unlock(&control);}
static uint32_t reg_read(unsigned reg)
{
 reads++;if(!active)bad_mmio++;seam();
 if(reg==PSB_CR_2D_SOCIF)return poll_mode==FIFO_BUSY?0:poll_mode==FINISHING&&jiffies<2?0:_PSB_C2_SOCIF_EMPTY;
 if(reg==PSB_CR_2D_BLIT_STATUS)return poll_mode==BLIT_BUSY?_PSB_C2B_STATUS_BUSY:0;
 assert(reg>=PSB_SGX_2D_SLAVE_PORT);return 0;
}
static void reg_write(uint32_t v,unsigned reg)
{(void)v;assert(reg>=PSB_SGX_2D_SLAVE_PORT);if(!active)bad_mmio++;writes++;queued=true;}
#define PSB_RSGX32(r) reg_read(r)
#define PSB_WSGX32(v,r) reg_write(v,r)
#define spin_lock_irqsave(m,f) do{(f)=0;assert(!pthread_mutex_lock(m));}while(0)
#define spin_unlock_irqrestore(m,f) do{(void)(f);assert(!pthread_mutex_unlock(m));}while(0)
#define time_after_eq(a,b) ((long)((a)-(b))>=0)
#define cpu_relax() ((void)jiffies++)
static int psb_2d_wait_available(struct drm_psb_private *p,unsigned size)
{assert(p==&private&&size<=96);return 0;}
static void remesa_route_source_producer(int begin)
{source_losses++;if(begin)producers++;else{assert(producers);producers--;}}
#include "console_actual.inc"
static void fixture(void)
{
 assert(!refs&&!producers);active=true;pause_first=paused=released=queued=false;
 reads=writes=bad_mmio=source_losses=0;jiffies=0;poll_mode=IDLE;
 private.remesa_twod_owner=NULL;helper.fb=&fb;info.par=&helper;
}
static void *worker(void *unused){(void)unused;worker_result=psbfb_sync(&info);return NULL;}
int main(void)
{
 (void)gma_power_begin;(void)gma_power_end;
 assert(!pthread_mutex_init(&private.lock_2d,NULL));
 fixture();active=false;
 assert(psbfb_sync(&info)==(CHECKED_CONSOLE?-EAGAIN:0));
 assert(CHECKED_CONSOLE?!reads:bad_mmio>0);
 fixture();pause_first=true;pthread_t thread;assert(!pthread_create(&thread,NULL,worker,NULL));
 pthread_mutex_lock(&control);while(!paused)pthread_cond_wait(&changed,&control);pthread_mutex_unlock(&control);
 assert(suspend_model()==(CHECKED_CONSOLE?-EBUSY:0));
 pthread_mutex_lock(&control);released=true;pthread_cond_broadcast(&changed);pthread_mutex_unlock(&control);
 assert(!pthread_join(thread,NULL)&&!worker_result&&!refs);
 assert(CHECKED_CONSOLE?!bad_mmio:bad_mmio>0);
 fixture();assert(!psbfb_sync(&info)&&!refs&&!bad_mmio);
 fixture();poll_mode=FIFO_BUSY;assert(psbfb_sync(&info)==-EBUSY&&!refs&&!bad_mmio);
 fixture();poll_mode=BLIT_BUSY;assert(psbfb_sync(&info)==-EBUSY&&!refs&&!bad_mmio);
 fixture();poll_mode=FINISHING;assert(!psbfb_sync(&info)&&jiffies>=2&&!refs&&!bad_mmio);
#if CHECKED_CONSOLE
 fixture();helper.fb=NULL;assert(psbfb_sync(&info)==-ENODEV&&!reads&&!refs);
 fixture();info.par=NULL;assert(psbfb_sync(&info)==-ENODEV&&!reads&&!refs);
 assert(psbfb_sync(NULL)==-ENODEV);
#endif
 /* Separate unresolved counterexample: actual console submit returns after
  * FIFO writes. Its caller's reference interval is modeled as in copyarea.
  * Producer-end invalidation and an empty CPU refcount are NOT retirement. */
 fixture();uint32_t words[2]={0x11,0x22};assert(gma_power_begin(&device,false));
 assert(!psbfb_2d_submit(&private,words,2));gma_power_end(&device);
 assert(writes==2&&queued&&!refs&&!producers&&source_losses==2);
 assert(!suspend_model()&&queued);
 puts(CHECKED_CONSOLE?"CONSOLE_SYNC_POWER_GUARDS_PASS; ASYNC_CONSOLE_PM_COUNTEREXAMPLE_REMAINS; GPU0":"CONSOLE_SYNC_POWER_COUNTEREXAMPLES_CONFIRMED; ASYNC_CONSOLE_PM_COUNTEREXAMPLE_REMAINS; GPU0");
 return 0;
}
