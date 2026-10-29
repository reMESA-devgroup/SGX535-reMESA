/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual unload and ASLE worker; reduced DRM/backlight/workqueue APIs.
 * Stale accesses are logical witnesses using still-allocated fixture storage,
 * not real freed memory. Active-work joining uses pthread scheduling. GPU0. */
#define main retained_unload_regression_main
#include "test_private_device_retention.c"
#undef main
#include <stddef.h>
#include <pthread.h>
typedef uint32_t u32;
#define container_of(p,t,m) ((t *)((char *)(p)-offsetof(t,m)))
#define DRM_DEBUG_DRIVER(...) ((void)0)
#define ASLE_BCLP_VALID (1U<<31)
#define ASLE_BCLP_MSK (~ASLE_BCLP_VALID)
#define ASLE_BACKLIGHT_FAILED (1U<<12)
#define ASLE_CBLV_VALID (1U<<31)
#define ASLE_SET_BACKLIGHT (1U<<1)
#define ASLE_REQ_MSK 0xfU
struct opregion_asle {u32 aslc,bclp,cblv;};
struct backlight_device {struct {unsigned max_brightness;} props;};
static struct opregion_asle mailbox;
static struct backlight_device backlight;
static pthread_mutex_t work_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t work_change=PTHREAD_COND_INITIALIZER;
static pthread_t work_thread;
static bool pending,running,live,block_brightness,brightness_started;
static bool release_brightness,cancel_waiting,unload_finished;
static unsigned stale_uses,brightness_calls;
static void gma_backlight_set(struct drm_device *d,int value)
{
 assert(d==&device&&value==100);
 pthread_mutex_lock(&work_lock);
 brightness_calls++;if(!live)stale_uses++;
 if(block_brightness){
  brightness_started=true;pthread_cond_broadcast(&work_change);
  while(!release_brightness)pthread_cond_wait(&work_change,&work_lock);
  assert(live);
 }
 pthread_mutex_unlock(&work_lock);
}
#include "asle_set_actual.inc"
#include "asle_work_actual.inc"
static void *run_work(void *unused)
{
 (void)unused;psb_intel_opregion_asle_work(&private.opregion.asle_work);
 pthread_mutex_lock(&work_lock);running=false;pthread_cond_broadcast(&work_change);
 pthread_mutex_unlock(&work_lock);return NULL;
}
static void join_asle(void)
{
 pthread_mutex_lock(&work_lock);pending=false;
 if(running){
  cancel_waiting=true;pthread_cond_broadcast(&work_change);
  while(running)pthread_cond_wait(&work_change,&work_lock);
  pthread_mutex_unlock(&work_lock);assert(!pthread_join(work_thread,NULL));return;
 }
 pthread_mutex_unlock(&work_lock);
}
static void unregister_backlight(void)
{
 pthread_mutex_lock(&work_lock);assert(!running);live=false;
 bool dispatch=pending;pending=false;pthread_mutex_unlock(&work_lock);
 /* A queued ASLE worker may start after unregister returns. Model that
  * schedule here without actually freeing fixture storage. */
 if(dispatch)psb_intel_opregion_asle_work(&private.opregion.asle_work);
}
static void asle_fixture(void)
{
 fixture();retained=true;
 private.backlight_device=&backlight;private.opregion.asle=&mailbox;
 backlight.props.max_brightness=100;
 mailbox=(struct opregion_asle){.aslc=ASLE_SET_BACKLIGHT,.bclp=ASLE_BCLP_VALID|255U};
 pending=running=block_brightness=brightness_started=release_brightness=false;
 cancel_waiting=unload_finished=false;live=true;stale_uses=brightness_calls=0;
 asle_join_hook=join_asle;backlight_stop_hook=unregister_backlight;
}
static void *run_unload(void *unused)
{
 (void)unused;psb_driver_unload(&device);
 pthread_mutex_lock(&work_lock);unload_finished=true;pthread_cond_broadcast(&work_change);
 pthread_mutex_unlock(&work_lock);return NULL;
}
int main(void)
{
 asle_fixture();pending=true;psb_driver_unload(&device);
#ifdef EXPECT_ASLE_RACE
 assert(!asle_joins&&brightness_calls==1&&stale_uses==1&&!live);
 puts("COUNTEREXAMPLE actual baseline unload permits actual queued ASLE brightness callback after modeled backlight unregister; logical stale-object witness, GPU0");
#else
 assert(asle_joins==1&&!brightness_calls&&!stale_uses&&!live&&!pending);
 asle_fixture();block_brightness=true;running=true;
 assert(!pthread_create(&work_thread,NULL,run_work,NULL));
 pthread_mutex_lock(&work_lock);
 while(!brightness_started)pthread_cond_wait(&work_change,&work_lock);
 pthread_mutex_unlock(&work_lock);
 pthread_t unload_thread;assert(!pthread_create(&unload_thread,NULL,run_unload,NULL));
 pthread_mutex_lock(&work_lock);
 while(!cancel_waiting)pthread_cond_wait(&work_change,&work_lock);
 assert(live&&running&&!unload_finished&&!backlight_stops);
 release_brightness=true;pthread_cond_broadcast(&work_change);
 pthread_mutex_unlock(&work_lock);
 assert(!pthread_join(unload_thread,NULL));
 assert(unload_finished&&!running&&!live&&!stale_uses&&brightness_calls==1&&asle_joins==1);
 assert(mailbox.aslc==0&&mailbox.cblv==(100U|ASLE_CBLV_VALID));
 retained_unload_regression_main();
 puts("PASS queued ASLE work canceled and running actual brightness worker joined before backlight unregister; actual unload/worker with modeled workqueue/backlight ownership, GPU0");
#endif
 return 0;
}
