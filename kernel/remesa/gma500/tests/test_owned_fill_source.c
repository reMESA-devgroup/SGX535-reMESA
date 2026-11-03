/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual source accounting and frozen guard; typed completion is a CPU model,
 * NOT GPU evidence. Kernel lock/task/device/observation primitives are modeled. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
typedef uint32_t u32;
typedef uint64_t u64;
#include "frozen_source_guard.h"
#include "remesa_gem_twod_job.h"
struct drm_device {unsigned id;};
struct psb_mmu_pd {unsigned id;};
static unsigned task1,task2,token;
static const void *current=&task1;
static struct drm_device dev={1},foreign_dev={2};
static struct psb_mmu_pd pd={1};
static struct drm_device *device=&dev;
static struct sgx535_source_guard lifetime;
static const void *lease;
static bool ever_claimed,poisoned;
struct sgx535_gma500_fixed_backend {unsigned id;};
struct remesa_stream_observation {
 struct {unsigned state;bool invalid;} evidence;
 struct sgx535_source_guard boundary;
 struct sgx535_gma500_fixed_backend *backend;
 const void *task;
 u64 operation,sequence;
 bool finished;
};
static struct remesa_stream_observation *operation;
static unsigned source_lock,legacy_calls;
#define SGX535_CAP_UNUSED 0
#define spin_lock_irqsave(lock,flags) do{assert(!*(lock));*(lock)=1;(flags)=0;}while(0)
#define spin_unlock_irqrestore(lock,flags) do{(void)(flags);assert(*(lock));*(lock)=0;}while(0)
static void sgx535_capsule_call(void *c,const void *task){assert(c&&task==current&&source_lock);}
static void sgx535_provenance_source_producer(int begin){(void)begin;assert(!source_lock);legacy_calls++;}
#include "owned_source_actual.inc"
static struct remesa_twod_retirement output,proof;
static struct remesa_twod_launch launch;
static void claim(void)
{assert(!remesa_owned_fill_claim(&dev,&token,33));assert(!remesa_owned_fill_ready(&dev,&token));}
static void publish(void)
{assert(!remesa_owned_fill_publish(&token,&launch,0x1000,0,7));assert(lifetime.producers_active==1);}
static void issued(void){assert(!remesa_owned_fill_issued(&launch,7));}
static void running(void){publish();issued();assert(!remesa_owned_fill_published(&launch,0));assert(lifetime.producers_active==0);}
static void native(void)
{
 running();assert(!remesa_owned_fill_restore(&launch,7));assert(lifetime.producers_active==1);
 assert(!remesa_owned_fill_restored(&launch,&proof,0));assert(lifetime.producers_active==0);
}
static void complete(void){native();proof.gem_retired=true;assert(!remesa_owned_fill_complete(&token,&proof));}
static void pending(u32 a,u32 d)
{assert(!source_lock);source_lock=1;owned_fill_pending(&dev,a,d);source_lock=0;}
static void mutate(unsigned n)
{
 switch(n){
 case 0:proof.dev=&foreign_dev;break;case 1:proof.pd=NULL;break;
 case 2:proof.ticket.context++;break;case 3:proof.ticket.operation++;break;
 case 4:proof.ticket.slot++;break;case 5:proof.ticket.generation++;break;
 case 6:proof.cookie++;break;case 7:proof.destination_id++;break;
 case 8:proof.destination_first++;break;case 9:proof.destination_end++;break;
 case 10:proof.default_root+=4096;break;case 11:proof.request_base+=0x10000000;break;
 case 12:proof.restored_request_base+=0x10000000;break;case 13:proof.packet_dwords++;break;
 case 14:proof.core++;break;case 15:proof.revision++;break;
 case 16:proof.status1=7;break;case 17:proof.status2=1;break;
 case 18:proof.native_retired=false;break;case 19:proof.gem_retired=!proof.gem_retired;break;
 default:abort();
 }
}
int main(int argc,char **argv)
{
 assert(argc==2);unsigned n=strtoul(argv[1],NULL,10);assert(n<115);
 lifetime=(struct sgx535_source_guard){.boot=SGX535_BOOT_READY,.prepared=1,.asserted_reset=127};
 launch=(struct remesa_twod_launch){.dev=&dev,.pd=&pd,.ticket={9,1,0,1},.count=1,.retirement=&output,.source_owner=&token,
  .plan={.destination_id=33,.destination_first=0x10000000,.destination_end=0x103e8000,.request_base=0x10000000,.dwords=7}};
 proof=(struct remesa_twod_retirement){.dev=&dev,.pd=&pd,.ticket=launch.ticket,.cookie=7,.destination_id=33,
  .destination_first=launch.plan.destination_first,.destination_end=launch.plan.destination_end,
  .default_root=0x1000,.request_base=0x10000000,.packet_dwords=7,.core=0x01130000,.revision=0x10201,
  .status1=REMESA_TWOD_EVENT_COMPLETE,.native_retired=true};
 if(n==0){
  claim();complete();assert(!poisoned&&lifetime.reasons==32&&!lifetime.producers_active);
  assert(!sgx535_source_boot_valid(&lifetime)&&sgx535_source_boot_valid(source_lineage()));
  assert(!remesa_owned_fill_ready(&dev,&token));
  struct sgx535_gma500_fixed_backend b={1};struct remesa_stream_observation o={0};
  assert(!remesa_stream_source_bind(&token,&o,&b,2,2));assert(valid(&b));
  assert(!o.boundary.reasons&&o.boundary.boot==SGX535_BOOT_READY&&operation==&o);
  assert(remesa_stream_source_release(&token)==-EBUSY);return 0;
 }
 if(n==1){remesa_route_source_producer(1);remesa_route_source_producer(0);
  assert(remesa_owned_fill_claim(&dev,&token,33)==-EBUSY);assert(lifetime.reasons==32&&poisoned&&legacy_calls==2);return 0;}
 claim();
 if(n==114){
  assert(!remesa_owned_fill_publish(&token,&launch,0x1000,0x28000000,7));issued();
  assert(!remesa_owned_fill_published(&launch,0));assert(!remesa_owned_fill_restore(&launch,7));
  proof.restored_request_base=0x28000000;
  assert(!remesa_owned_fill_restored(&launch,&proof,0));proof.gem_retired=true;
  assert(!remesa_owned_fill_complete(&token,&proof)&&!remesa_owned_fill_ready(&dev,&token));return 0;
 }
 if(n>=110){
  if(n==110){launch.source_owner=NULL;assert(remesa_owned_fill_publish(&token,&launch,0x1000,0,7));}
  if(n==111){launch.source_owner=&task2;assert(remesa_owned_fill_publish(&token,&launch,0x1000,0,7));}
  if(n==112){publish();launch.source_owner=&task2;assert(remesa_owned_fill_issued(&launch,7));}
  if(n==113){running();launch.source_owner=NULL;assert(remesa_owned_fill_restore(&launch,7));}
  goto failed;
 }
 if(n>=84){
  if(n<=92){
   static const u32 reasons[]={1,2,4,8,16,64,128,256,512};
   complete();lifetime.reasons|=reasons[n-84];
  }else if(n==93){complete();sgx535_source_boot_start(&lifetime,NULL);}
  else if(n==94){complete();lifetime.asserted_reset^=1;}
  else if(n==95){complete();lifetime.startup_pending=1;}
  else if(n==96){complete();lifetime.state=SGX535_SOURCE_CLOSED;}
  else if(n==97){complete();lifetime.boot=SGX535_BOOT_INVALID;}
  else if(n==98){complete();lifetime.producers_active=1;}
  else if(n==99){native();lifetime.reasons|=SGX535_SOURCE_LIFECYCLE;proof.gem_retired=true;
   assert(remesa_owned_fill_complete(&token,&proof));goto failed;}
  else if(n==100){running();lifetime.reasons|=SGX535_SOURCE_LOST;
   assert(remesa_owned_fill_restore(&launch,7));goto failed;}
  else if(n<=104){
   publish();if(n>=102)issued();if(n>=103)assert(!remesa_owned_fill_published(&launch,0));
   if(n>=104){assert(!remesa_owned_fill_restore(&launch,7));assert(!remesa_owned_fill_restored(&launch,&proof,0));}
   struct sgx535_gma500_fixed_backend b={1};struct remesa_stream_observation o={0};
   assert(remesa_stream_source_bind(&token,&o,&b,2,2)==-EBUSY);assert(!operation&&!poisoned);return 0;
  }else{
   complete();
   if(n==105)lifetime.startup_reads=1;
   if(n==106)lifetime.startup_fault=1;
   if(n==107)lifetime.startup_autonomous=1;
   if(n==108)lifetime.released_reset=1;
   if(n==109)lifetime.operation=&token;
  }
  assert(remesa_owned_fill_ready(&dev,&token));
  assert(remesa_owned_fill_publish(&token,&launch,0x1000,0,7));goto failed;
 }
 if(n>=20&&n<60){
  if(n<40){running();assert(!remesa_owned_fill_restore(&launch,7));mutate(n-20);
   assert(remesa_owned_fill_restored(&launch,&proof,0));}
  else{native();proof.gem_retired=true;mutate(n-40);assert(remesa_owned_fill_complete(&token,&proof));}
 }else switch(n){
 case 2:assert(!remesa_stream_source_release(&token));assert(!lease&&ever_claimed&&!owned_fill.owner);
  assert(remesa_owned_fill_claim(&dev,&token,33)==-EBUSY);return 0;
 case 3:publish();assert(remesa_stream_source_release(&token)==-EBUSY);return 0;
 case 4:publish();pending(REMESA_TWOD_EVENT_COMPLETE,0);break;
 case 5:publish();issued();pending(REMESA_TWOD_EVENT_COMPLETE,0);
  assert(!poisoned);assert(!remesa_owned_fill_published(&launch,0));return 0;
 case 6:pending(REMESA_TWOD_EVENT_COMPLETE,0);break;
 case 7:running();pending(1,0);break;
 case 8:running();pending(REMESA_TWOD_EVENT_COMPLETE,1);break;
 case 9:native();pending(REMESA_TWOD_EVENT_COMPLETE,0);break;
 case 10:complete();pending(REMESA_TWOD_EVENT_COMPLETE,0);break;
 case 11:running();current=&task2;assert(remesa_owned_fill_restore(&launch,7));break;
 case 12:assert(remesa_owned_fill_restore(&launch,7));break;
 case 13:publish();assert(remesa_owned_fill_published(&launch,0));break;
 case 14:publish();issued();assert(remesa_owned_fill_published(&launch,-EIO));break;
 case 15:native();assert(remesa_owned_fill_complete(&token,&proof));break;
 case 16:complete();launch.ticket.operation++;proof.ticket.operation++;proof.gem_retired=false;complete();
  assert(owned_fill.last_operation==2&&!poisoned&&lifetime.reasons==32);return 0;
 case 17:complete();assert(remesa_owned_fill_publish(&token,&launch,0x1000,0,7));break;
 case 18:remesa_route_source_producer(1);break;
 case 19:assert(remesa_owned_fill_publish(&task2,&launch,0x1000,0,7));break;
 case 60:launch.dev=&foreign_dev;goto bad_publish;
 case 61:launch.pd=NULL;goto bad_publish;
 case 62:launch.retirement=NULL;goto bad_publish;
 case 63:launch.count=2;goto bad_publish;
 case 64:launch.plan.dwords=9;goto bad_publish;
 case 65:launch.plan.destination_id++;goto bad_publish;
 case 66:launch.plan.destination_end=launch.plan.destination_first;goto bad_publish;
 case 67:launch.ticket.context=0;goto bad_publish;
 case 68:launch.ticket.operation=0;goto bad_publish;
 case 69:launch.ticket.generation=0;goto bad_publish;
 case 70:launch.plan.request_base++;goto bad_publish;
 case 71:assert(remesa_owned_fill_publish(&token,&launch,0,0,7));break;
 case 72:assert(remesa_owned_fill_publish(&token,&launch,0x1001,0,7));break;
 case 73:
  assert(!remesa_owned_fill_publish(&token,&launch,0x1000,0x28000000,7));issued();
  assert(!remesa_owned_fill_published(&launch,0));assert(!remesa_owned_fill_restore(&launch,7));
  assert(remesa_owned_fill_restored(&launch,&proof,0));break;
 case 74:assert(remesa_owned_fill_publish(&token,&launch,0x1000,0,0));break;
 case 75:publish();assert(remesa_owned_fill_issued(&launch,8));break;
 case 76:running();launch.ticket.generation++;assert(remesa_owned_fill_restore(&launch,7));break;
 case 77:running();assert(!remesa_owned_fill_restore(&launch,7));assert(remesa_owned_fill_restored(&launch,&proof,-EIO));break;
 case 78:native();proof.gem_retired=true;current=&task2;assert(remesa_owned_fill_complete(&token,&proof));break;
 case 79:publish();assert(remesa_owned_fill_ready(&dev,&token));assert(!poisoned);return 0;
 case 80:running();pending(1U<<31,0);assert(!poisoned);return 0;
 case 81:complete();assert(!remesa_stream_source_release(&token));assert(lifetime.reasons==32);
  assert(remesa_stream_source_claim(&dev,&token)==-EBUSY);return 0;
 case 82:complete();remesa_route_source_producer(1);break;
 case 83:running();assert(remesa_owned_fill_complete(&token,&proof));break;
 default:abort();
 }
 goto failed;
 bad_publish:assert(remesa_owned_fill_publish(&token,&launch,0x1000,0,7));
 failed:assert(poisoned&&remesa_owned_fill_ready(&dev,&token));
 assert(remesa_stream_source_release(&token)==-EBUSY);assert(lease==&token);
 puts("PASS owned clear refusal retains source; GPU0");return 0;
}
