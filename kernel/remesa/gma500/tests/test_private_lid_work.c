/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual old lid timer and new work fragment; MMIO/DRM/power/workqueue modeled.
 * NULL/steady-spin witnesses use longjmp, never invalid memory or live MMIO. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <pthread.h>
typedef uint32_t u32;
#define __iomem
#define PP_CONTROL 1
#define PP_STATUS 2
#define PP_ON (1U<<31)
#define PP_SEQUENCE_MASK 0x30000000U
#define POWER_TARGET_ON 1U
#define PSB_LID_DELAY 100U
#define DRM_DEBUG(...) ((void)0)
#define container_of(p,t,m) ((t *)((char *)(p)-offsetof(t,m)))
#define from_timer(p,t,m) container_of(t,struct drm_psb_private,m)
struct timer_list {bool pending;unsigned long expires;void (*fn)(struct timer_list *);};
struct work_struct {void (*fn)(struct work_struct *);};
struct delayed_work {struct work_struct work;};
struct workqueue_struct {unsigned id;};
static struct workqueue_struct freezable_queue;
static struct workqueue_struct *system_freezable_wq=&freezable_queue;
struct spinlock {unsigned id;};
struct drm_psb_private;
struct drm_device {struct drm_psb_private *dev_private;bool unplugged;};
struct drm_psb_private {
 struct drm_device *dev;struct {void *header;u32 *lid_state;} opregion;
 struct timer_list lid_timer;struct spinlock lid_lock;u32 lid_last_state;
 struct delayed_work remesa_private_lid_work;
 bool remesa_private_lid_initialized,remesa_private_lid_stopping,remesa_private_lid_pending;
 u32 remesa_private_lid_target;
};
static struct drm_psb_private private;
static struct drm_device device={&private,false};
static u32 lid,pp_control,pp_status;
static unsigned long jiffies;
static unsigned lid_reads,status_reads,control_reads,control_writes,brightness[101];
static unsigned entries,power_refs,power_begins,power_ends,work_inits,queues,cancels,timer_inits;
static bool powered,legacy_mode,escape_enabled,queued,running,block_brightness;
static bool brightness_started,release_brightness,cancel_waiting,stop_finished;
static unsigned null_reads;
static jmp_buf escape;
static pthread_mutex_t lid_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t control=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t change=PTHREAD_COND_INITIALIZER;
static pthread_mutex_t ownership=PTHREAD_MUTEX_INITIALIZER;
static bool lid_owned;static pthread_t lid_owner;
static bool lid_owned_by_current(void)
{
 bool result;pthread_mutex_lock(&ownership);
 result=lid_owned&&pthread_equal(lid_owner,pthread_self());
 pthread_mutex_unlock(&ownership);return result;
}
#define spin_held lid_owned_by_current()
static void lock_lid(struct spinlock *s)
{
 assert(s==&private.lid_lock&&!spin_held);assert(!pthread_mutex_lock(&lid_mutex));
 pthread_mutex_lock(&ownership);assert(!lid_owned);lid_owned=true;lid_owner=pthread_self();pthread_mutex_unlock(&ownership);
}
static void unlock_lid(struct spinlock *s)
{
 assert(s==&private.lid_lock&&spin_held);pthread_mutex_lock(&ownership);lid_owned=false;
 pthread_mutex_unlock(&ownership);assert(!pthread_mutex_unlock(&lid_mutex));
}
#define spin_lock_irqsave(s,f) do{(f)=0;lock_lid(s);}while(0)
#define spin_unlock_irqrestore(s,f) do{(void)(f);unlock_lid(s);}while(0)
static void spin_lock_init(struct spinlock *s){assert(s==&private.lid_lock);}
static bool drm_dev_enter(struct drm_device *d,int *index)
{assert(d==&device&&!spin_held);if(d->unplugged)return false;*index=1;entries++;return true;}
static void drm_dev_exit(int index){assert(index==1&&entries==1&&!spin_held);entries--;}
static bool gma_power_begin(struct drm_device *d,bool force)
{assert(d==&device&&entries==1&&!force&&!spin_held);power_begins++;if(!powered)return false;power_refs++;return true;}
static void gma_power_end(struct drm_device *d)
{assert(d==&device&&power_refs==1&&entries==1);power_ends++;power_refs--;}
static void access_check(void){assert(!spin_held);assert(legacy_mode||(entries==1&&power_refs==1));}
static u32 readl(u32 *p)
{
 access_check();lid_reads++;
 if(!p){null_reads++;assert(escape_enabled);longjmp(escape,1);}
 assert(p==&lid);return *p;
}
static u32 reg_read(unsigned reg)
{
 access_check();if(reg==PP_CONTROL){control_reads++;return pp_control;}
 assert(reg==PP_STATUS);status_reads++;
 if(escape_enabled&&status_reads>16)longjmp(escape,2);
 return pp_status;
}
static void reg_write(unsigned reg,u32 value)
{access_check();assert(reg==PP_CONTROL);control_writes++;pp_control=value;}
#define REG_READ(reg) reg_read(reg)
#define REG_WRITE(reg,value) reg_write(reg,value)
static void psb_intel_lvds_set_brightness(struct drm_device *d,int value)
{
 access_check();assert(d==&device&&(value==0||value==100));
 pthread_mutex_lock(&control);brightness[value]++;
 if(block_brightness&&value==100){
  brightness_started=true;pthread_cond_broadcast(&change);
  while(!release_brightness)pthread_cond_wait(&change,&control);
 }
 pthread_mutex_unlock(&control);
}
static bool timer_pending(struct timer_list *t){return t->pending;}
static void timer_setup(struct timer_list *t,void (*fn)(struct timer_list *),unsigned flags)
{assert(t==&private.lid_timer&&!flags);t->fn=fn;timer_inits++;}
static void add_timer(struct timer_list *t){assert(spin_held);t->pending=true;}
static void del_timer_sync(struct timer_list *t){assert(!spin_held);t->pending=false;}
#define psb_lid_timer_func baseline_lid_timer_func
#define psb_lid_timer_init baseline_lid_timer_init
#define psb_lid_timer_takedown baseline_lid_timer_takedown
#include "baseline_lid_actual.inc"
#undef psb_lid_timer_func
#undef psb_lid_timer_init
#undef psb_lid_timer_takedown
static struct delayed_work *to_delayed_work(struct work_struct *w)
{return container_of(w,struct delayed_work,work);}
static void init_work(struct delayed_work *w,void (*fn)(struct work_struct *))
{assert(w==&private.remesa_private_lid_work);w->work.fn=fn;work_inits++;}
#define INIT_DELAYED_WORK(w,fn) init_work(w,fn)
static bool queue_delayed_work(struct workqueue_struct *queue,struct delayed_work *w,unsigned delay)
{
 assert(queue==system_freezable_wq);
 assert(spin_held);assert(w==&private.remesa_private_lid_work);assert(delay==PSB_LID_DELAY);
 assert(!private.remesa_private_lid_stopping);
 pthread_mutex_lock(&control);assert(!queued);queued=true;queues++;
 pthread_mutex_unlock(&control);return true;
}
static bool cancel_delayed_work_sync(struct delayed_work *w)
{
 assert(!spin_held);assert(w==&private.remesa_private_lid_work);
 pthread_mutex_lock(&control);cancels++;queued=false;
 if(running){cancel_waiting=true;pthread_cond_broadcast(&change);}
 while(running)pthread_cond_wait(&change,&control);
 assert(!queued);pthread_mutex_unlock(&control);return true;
}
#include "remesa_private_lid.inc"
static void fixture(void)
{
 assert(!running&&!entries&&!power_refs&&!spin_held);
 private=(struct drm_psb_private){.dev=&device,.opregion={.header=&lid,.lid_state=&lid}};
 device.unplugged=false;lid=pp_control=pp_status=0;
 lid_reads=status_reads=control_reads=control_writes=0;memset(brightness,0,sizeof brightness);
 power_begins=power_ends=work_inits=queues=cancels=timer_inits=null_reads=0;
 powered=true;legacy_mode=escape_enabled=queued=block_brightness=false;
 brightness_started=release_brightness=cancel_waiting=stop_finished=false;
}
static void *run_once(void *unused)
{
 (void)unused;pthread_mutex_lock(&control);assert(queued&&!running);queued=false;running=true;
 pthread_mutex_unlock(&control);
 private.remesa_private_lid_work.work.fn(&private.remesa_private_lid_work.work);
 pthread_mutex_lock(&control);running=false;pthread_cond_broadcast(&change);
 pthread_mutex_unlock(&control);return NULL;
}
static void *stop_worker(void *unused)
{
 (void)unused;psb_lid_timer_takedown(&private);
 pthread_mutex_lock(&control);stop_finished=true;pthread_cond_broadcast(&change);
 pthread_mutex_unlock(&control);return NULL;
}
int main(void)
{
 fixture();legacy_mode=escape_enabled=true;private.opregion.header=NULL;private.opregion.lid_state=NULL;
 baseline_lid_timer_init(&private);assert(timer_inits==1&&private.lid_timer.pending);
 if(!setjmp(escape))baseline_lid_timer_func(&private.lid_timer);
 assert(null_reads==1);baseline_lid_timer_takedown(&private);
 fixture();legacy_mode=escape_enabled=true;private.lid_last_state=1;
 if(!setjmp(escape))baseline_lid_timer_func(&private.lid_timer);
 assert(status_reads==17&&brightness[0]==1&&private.lid_last_state==1);
 fixture();legacy_mode=true;private.lid_last_state=1;pp_status=PP_ON;
 baseline_lid_timer_func(&private.lid_timer);
 assert(private.lid_last_state==0&&(pp_status&PP_ON));
 fixture();private.opregion.header=NULL;psb_lid_timer_init(&private);psb_lid_timer_takedown(&private);
 assert(!queued&&!work_inits&&!cancels&&!lid_reads);
 fixture();private.opregion.lid_state=NULL;psb_lid_timer_init(&private);psb_lid_timer_takedown(&private);
 assert(!queued&&!work_inits&&!cancels);
 fixture();psb_lid_timer_init(&private);run_once(NULL);
 assert(lid_reads==1&&!status_reads&&!control_reads&&!control_writes&&!brightness[0]&&!brightness[100]);
 psb_lid_timer_takedown(&private);assert(power_begins==power_ends&&!queued);
 fixture();private.lid_last_state=1;pp_status=PP_ON;psb_lid_timer_init(&private);psb_lid_timer_init(&private);
 assert(queues==1&&work_inits==1);run_once(NULL);
 assert(private.remesa_private_lid_pending&&private.lid_last_state==1&&status_reads==1&&brightness[0]==1);
 pp_status=0;run_once(NULL);
 assert(!private.remesa_private_lid_pending&&private.lid_last_state==0&&status_reads==2&&brightness[0]==1&&control_writes==1);
 psb_lid_timer_takedown(&private);assert(!queued&&power_begins==power_ends);
 fixture();lid=1;pp_status=PP_SEQUENCE_MASK;psb_lid_timer_init(&private);
 for(unsigned i=0;i<32;i++)run_once(NULL);
 assert(private.remesa_private_lid_pending&&private.lid_last_state==0&&status_reads==32&&control_writes==1&&!brightness[100]);
 pp_status=PP_ON;run_once(NULL);
 assert(!private.remesa_private_lid_pending&&private.lid_last_state==1&&brightness[100]==1);
 psb_lid_timer_takedown(&private);
 fixture();lid=1;pp_status=PP_SEQUENCE_MASK;psb_lid_timer_init(&private);run_once(NULL);
 lid=0;pp_status=0;run_once(NULL);
 assert(!private.remesa_private_lid_pending&&!private.lid_last_state&&brightness[0]==1&&!brightness[100]&&control_writes==2);
 psb_lid_timer_takedown(&private);
 fixture();powered=false;psb_lid_timer_init(&private);run_once(NULL);
 assert(!lid_reads&&!status_reads&&!control_reads&&!control_writes&&power_begins==1&&!power_ends&&queued);
 psb_lid_timer_takedown(&private);
 fixture();device.unplugged=true;psb_lid_timer_init(&private);run_once(NULL);
 assert(!queued&&!power_begins&&!lid_reads&&!control_reads);psb_lid_timer_takedown(&private);
 fixture();lid=1;pp_status=PP_ON;block_brightness=true;psb_lid_timer_init(&private);
 pthread_t worker,stop;assert(!pthread_create(&worker,NULL,run_once,NULL));
 pthread_mutex_lock(&control);while(!brightness_started)pthread_cond_wait(&change,&control);
 pthread_mutex_unlock(&control);assert(!pthread_create(&stop,NULL,stop_worker,NULL));
 pthread_mutex_lock(&control);while(!cancel_waiting)pthread_cond_wait(&change,&control);
 assert(!stop_finished&&entries==1&&power_refs==1);release_brightness=true;pthread_cond_broadcast(&change);
 pthread_mutex_unlock(&control);assert(!pthread_join(worker,NULL));assert(!pthread_join(stop,NULL));
 assert(stop_finished&&!queued&&!running&&!entries&&!power_refs&&private.remesa_private_lid_stopping);
 unsigned before=lid_reads;remesa_private_lid_worker(&private.remesa_private_lid_work.work);assert(lid_reads==before);
 psb_lid_timer_takedown(&private);assert(cancels==2&&!queued);
 puts("PASS actual old NULL/steady-spin/premature-close witnesses; new process-context bounded observation, transition/reversal, energy/loss denial, idempotent stop and pthread join; kernel/MMIO APIs modeled, GPU0");
 return 0;
}
