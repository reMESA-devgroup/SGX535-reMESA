/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual callback/init/fini with a pointer-read scheduling seam. The fallback
 * records a would-be NULL owner lookup without dereferencing NULL. The kernel
 * blocking notifier contract is modeled by pthread rwlock, GPU0. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#define NOTIFY_DONE 0
#define NOTIFY_OK 1
struct notifier_block {int (*notifier_call)(struct notifier_block *,unsigned long,void *);};
struct work_struct {unsigned id;};
struct opregion_acpi {uint32_t drdy,csts;};
struct psb_intel_opregion {
 void *header;struct opregion_acpi *acpi;void *swsci,*asle,*vbt;
 struct work_struct asle_work;
};
struct drm_psb_private {struct psb_intel_opregion opregion;};
struct drm_device {struct drm_psb_private *dev_private;};
static struct drm_psb_private private;
static struct drm_device device={&private};
static struct opregion_acpi mailbox,fallback_mailbox;
static struct psb_intel_opregion fallback={.acpi=&fallback_mailbox};
static struct psb_intel_opregion *system_opregion;
static pthread_rwlock_t chain_lock=PTHREAD_RWLOCK_INITIALIZER;
static pthread_mutex_t control=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t change=PTHREAD_COND_INITIALIZER;
static bool registered,mapped,first_read,release_second,unregister_started,fini_finished;
static unsigned reads,null_lookups,callback_calls,cancels,unmaps,unregisters;
static bool gate_reads;
static struct psb_intel_opregion *notifier_owner_lookup(void)
{
 struct psb_intel_opregion *p;
 pthread_mutex_lock(&control);reads++;
 if(gate_reads&&reads==2){
  while(!release_second)pthread_cond_wait(&change,&control);
 }
 p=system_opregion;
 if(gate_reads&&reads==1){assert(p);first_read=true;pthread_cond_broadcast(&change);}
 if(gate_reads&&!p){null_lookups++;p=&fallback;}
 pthread_mutex_unlock(&control);return p;
}
/* Keep the source body intact. Instrument each owner lookup at the shim seam
 * so both old plain-C reads can be exercised; this is not machine-code proof. */
#define system_opregion notifier_owner_lookup()
#include "notifier_callback_actual.inc"
#undef system_opregion
static struct notifier_block psb_intel_opregion_notifier={.notifier_call=psb_intel_opregion_video_event};
static int register_acpi_notifier(struct notifier_block *n)
{
 assert(n==&psb_intel_opregion_notifier);
 assert(!pthread_rwlock_wrlock(&chain_lock));assert(!registered);registered=true;
 assert(!pthread_rwlock_unlock(&chain_lock));return 0;
}
static int unregister_acpi_notifier(struct notifier_block *n)
{
 assert(n==&psb_intel_opregion_notifier);
 pthread_mutex_lock(&control);unregister_started=true;pthread_cond_broadcast(&change);
 pthread_mutex_unlock(&control);
 assert(!pthread_rwlock_wrlock(&chain_lock));assert(registered);registered=false;unregisters++;
 assert(!pthread_rwlock_unlock(&chain_lock));return 0;
}
static void cancel_work_sync(struct work_struct *w)
{assert(w==&private.opregion.asle_work&&!registered);cancels++;}
static void iounmap(void *p)
{assert(p==&mailbox&&mapped&&!registered);mapped=false;unmaps++;}
#include "notifier_init_actual.inc"
#include "notifier_fini_actual.inc"
static int dispatch(void)
{
 int result=NOTIFY_DONE;
 assert(!pthread_rwlock_rdlock(&chain_lock));
 if(registered){callback_calls++;result=psb_intel_opregion_notifier.notifier_call(&psb_intel_opregion_notifier,0,NULL);}
 assert(!pthread_rwlock_unlock(&chain_lock));return result;
}
static void *callback_worker(void *unused)
{(void)unused;assert(dispatch()==NOTIFY_OK);return NULL;}
static void *fini_worker(void *unused)
{
 (void)unused;psb_intel_opregion_fini(&device);
 pthread_mutex_lock(&control);fini_finished=true;pthread_cond_broadcast(&change);
 pthread_mutex_unlock(&control);return NULL;
}
static void fixture(void)
{
 assert(!registered);system_opregion=NULL;
 private=(struct drm_psb_private){.opregion={.header=&mailbox,.acpi=&mailbox,
  .swsci=&mailbox,.asle=&mailbox,.vbt=&mailbox}};
 mailbox=(struct opregion_acpi){.drdy=0,.csts=42};
 fallback_mailbox=(struct opregion_acpi){.csts=99};
 mapped=true;first_read=release_second=unregister_started=fini_finished=gate_reads=false;
 reads=null_lookups=callback_calls=cancels=unmaps=unregisters=0;
}
int main(void)
{
 fixture();psb_intel_opregion_init(&device);
 assert(registered&&system_opregion==&private.opregion&&mailbox.drdy==1&&mailbox.csts==0);
 mailbox.csts=42;gate_reads=true;
 pthread_t callback_thread,fini_thread;
 assert(!pthread_create(&callback_thread,NULL,callback_worker,NULL));
 pthread_mutex_lock(&control);
 while(!first_read)pthread_cond_wait(&change,&control);
 pthread_mutex_unlock(&control);
 assert(!pthread_create(&fini_thread,NULL,fini_worker,NULL));
 pthread_mutex_lock(&control);
 while(!unregister_started)pthread_cond_wait(&change,&control);
 assert(!fini_finished&&mapped&&registered&&!unmaps&&!cancels);
#ifdef EXPECT_NOTIFIER_RACE
 assert(!system_opregion);
#else
 assert(system_opregion==&private.opregion);
#endif
 release_second=true;pthread_cond_broadcast(&change);pthread_mutex_unlock(&control);
 assert(!pthread_join(callback_thread,NULL));assert(!pthread_join(fini_thread,NULL));
 assert(!system_opregion&&!registered&&!mapped&&unregisters==1&&cancels==1&&unmaps==1);
 assert(!private.opregion.header&&!private.opregion.acpi&&!private.opregion.swsci&&!private.opregion.asle&&!private.opregion.vbt);
#ifdef EXPECT_NOTIFIER_RACE
 assert(null_lookups==1&&mailbox.csts==42&&fallback_mailbox.csts==0);
 puts("COUNTEREXAMPLE baseline clears owner before notifier join: scheduled second callback lookup would be NULL; fallback avoids actual NULL dereference. CPU source model, GPU0");
#else
 assert(!null_lookups&&mailbox.csts==0&&fallback_mailbox.csts==99);
 assert(dispatch()==NOTIFY_DONE&&callback_calls==1);
 psb_intel_opregion_fini(&device);assert(unregisters==1&&unmaps==1&&cancels==1);
 fixture();private.opregion.header=NULL;psb_intel_opregion_init(&device);psb_intel_opregion_fini(&device);
 assert(!registered&&!unregisters&&!unmaps&&!cancels&&!system_opregion);
 fixture();private.opregion.acpi=NULL;psb_intel_opregion_init(&device);psb_intel_opregion_fini(&device);
 assert(!registered&&!unregisters&&unmaps==1&&cancels==1&&!system_opregion);
 fixture();psb_intel_opregion_init(&device);assert(dispatch()==NOTIFY_OK&&mailbox.csts==0);
 psb_intel_opregion_fini(&device);assert(unregisters==1&&unmaps==1&&cancels==1);
 gate_reads=false;assert(psb_intel_opregion_video_event(NULL,0,NULL)==NOTIFY_DONE);
 puts("PASS actual init/fini preserve callback owner until blocking-notifier join; unregister before mapping cleanup; repeated/absent/normal controls. pthread contract model, GPU0");
#endif
 return 0;
}
