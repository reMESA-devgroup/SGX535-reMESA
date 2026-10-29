/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual authority, poison, old fixed_write and new source/MMIO fragment.
 * CPU-only reduced owner, pthread source lock and recorded MMIO, not hardware. */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sched.h>
typedef uint32_t u32;
#include "frozen_source_guard.h"
struct task_struct {unsigned id;};
struct drm_psb_private {void *sgx_reg;};
struct drm_device {struct drm_psb_private *dev_private;};
struct sgx535_gma500_owner {struct drm_device *dev;};
struct sgx535_gma500_fixed_backend {
 struct sgx535_gma500_owner *owner;void *private_loan;
 bool active,power_held,fire_possible;unsigned long deadline;
};
struct observation {
 struct sgx535_gma500_fixed_backend *backend;struct task_struct *task;
 struct {bool invalid;} evidence;struct sgx535_source_guard boundary;
};
static pthread_mutex_t source_lock=PTHREAD_MUTEX_INITIALIZER;
static _Thread_local unsigned source_held;
static _Thread_local struct task_struct *current;
#define spin_lock_irqsave(lock,flags) do{(flags)=0;assert(!source_held);assert(!pthread_mutex_lock(lock));source_held=1;}while(0)
#define spin_unlock_irqrestore(lock,flags) do{(void)(flags);assert(source_held);source_held=0;assert(!pthread_mutex_unlock(lock));}while(0)
static const void *lease;
static struct observation *operation;
static bool poisoned,ever_claimed;
static struct sgx535_source_guard lifetime;
static unsigned long jiffies;
#define time_after_eq(a,b) ((long)((a)-(b))>=0)
#define PSB_SGX_SIZE 0x8000U
static atomic_bool reached,attempted,invalidated;
static atomic_uint sequence,write_sequence,invalidate_sequence,writes,writes_after_invalid;
static bool concurrent,baseline;
static void record_write(unsigned value,unsigned offset)
{
 assert(value==0x1234&&offset==0x80);
 if(concurrent){
  atomic_store(&reached,true);
  while(!atomic_load(baseline?&invalidated:&attempted))sched_yield();
 }
 assert(baseline?!source_held:source_held);
 if(poisoned)atomic_fetch_add(&writes_after_invalid,1);
 atomic_fetch_add(&writes,1);
 atomic_store(&write_sequence,atomic_fetch_add(&sequence,1)+1);
}
#define PSB_WSGX32(value,offset) record_write(value,offset)
static int sgx535_provenance_source_write(struct sgx535_gma500_fixed_backend *b)
{(void)b;abort();}
#include "source_authority_actual.inc"
#include "old_write_actual.inc"
#include "remesa_private_source_mmio.inc"
static struct task_struct owner_task={1},wrong_task={2};
static int token;
static struct drm_psb_private priv={(void*)1};
static struct drm_device dev={&priv};
static struct sgx535_gma500_owner owner={&dev};
static struct sgx535_gma500_fixed_backend backend;
static struct observation observation;
static void fixture(void)
{
 assert(!source_held);
 lease=&token;ever_claimed=true;poisoned=false;lifetime=(struct sgx535_source_guard){0};
 backend=(struct sgx535_gma500_fixed_backend){.owner=&owner,.private_loan=&token,.active=true,.power_held=true,.deadline=100};
 observation=(struct observation){.backend=&backend,.task=&owner_task};
 observation.boundary.state=SGX535_SOURCE_BOUNDARY;observation.boundary.operation=&backend;
 operation=&observation;priv.sgx_reg=(void*)1;dev.dev_private=&priv;
 current=&owner_task;jiffies=0;concurrent=baseline=false;
 atomic_store(&sequence,0);atomic_store(&write_sequence,0);atomic_store(&invalidate_sequence,0);
 atomic_store(&writes,0);atomic_store(&writes_after_invalid,0);
 atomic_store(&reached,false);atomic_store(&attempted,false);atomic_store(&invalidated,false);
}
static void *invalidate(void *argument)
{
 (void)argument;
 while(!atomic_load(&reached))sched_yield();
 atomic_store(&attempted,true);
 remesa_stream_source_poison(&token);
 atomic_store(&invalidate_sequence,atomic_fetch_add(&sequence,1)+1);
 atomic_store(&invalidated,true);return NULL;
}
int main(void)
{
 fixture();concurrent=baseline=true;
 pthread_t thread;assert(!pthread_create(&thread,NULL,invalidate,NULL));
 assert(!fixed_write(&backend,0x80,0x1234));assert(!pthread_join(thread,NULL));
 assert(atomic_load(&writes_after_invalid)==1);
 assert(atomic_load(&invalidate_sequence)<atomic_load(&write_sequence));
 fixture();concurrent=true;
 assert(!pthread_create(&thread,NULL,invalidate,NULL));
 assert(!remesa_private_source_write(&backend,0x80,0x1234));
 assert(!pthread_join(thread,NULL));
 assert(atomic_load(&writes)==1&&!atomic_load(&writes_after_invalid));
 assert(atomic_load(&write_sequence)<atomic_load(&invalidate_sequence));
 assert(remesa_private_source_write(&backend,0x80,0x1234)==-EAGAIN);
 assert(atomic_load(&writes)==1);
 for(unsigned fault=0;fault<9;fault++){
  fixture();unsigned offset=0x80;int expected=-EAGAIN;
  if(fault==0)remesa_stream_source_poison(&token);
  if(fault==1)current=&wrong_task;
  if(fault==2)backend.power_held=false,expected=-ENODEV;
  if(fault==3)dev.dev_private=NULL,expected=-ENODEV;
  if(fault==4)priv.sgx_reg=NULL,expected=-ENODEV;
  if(fault==5)backend.fire_possible=true,jiffies=100,expected=-ETIMEDOUT;
  if(fault==6)offset=PSB_SGX_SIZE,expected=-EINVAL;
  if(fault==7)offset=3,expected=-EINVAL;
  if(fault==8)backend.private_loan=NULL;
  assert(remesa_private_source_write(&backend,offset,0x1234)==expected);
  assert(!atomic_load(&writes)&&!source_held);
 }
 puts("PASS old check/MMIO race reproduced, new source lock orders CPU write before invalidation; poisoned/task/power/mapping/deadline/bounds refused; pthread CPU model, GPU0");
}
