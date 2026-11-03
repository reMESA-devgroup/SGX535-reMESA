/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual extracted source claim/producer routes and frozen guard. Reduced
 * boot/device/lock model: no MMIO, 2D rendering or physical completion. */
#include <assert.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "frozen_source_guard.h"
struct drm_device {unsigned id;};
static struct drm_device *device;
static struct sgx535_source_guard lifetime,observer;
static const void *lease;
static bool ever_claimed,poisoned;
static struct {struct {bool invalid;} evidence;} *operation;
static int source_lock;
static unsigned legacy_calls;
#define spin_lock_irqsave(lock,flags) do{assert(!*(lock));*(lock)=1;(flags)=0;}while(0)
#define spin_unlock_irqrestore(lock,flags) do{(void)(flags);assert(*(lock));*(lock)=0;}while(0)
static void sgx535_provenance_source_producer(int begin)
{assert(!source_lock);legacy_calls++;sgx535_source_producer(&observer,begin);}
#include "twod_source_actual.inc"
int main(int argc,char **argv)
{
 assert(argc==2);unsigned mode=(unsigned)strtoul(argv[1],NULL,10);assert(mode<4);
 struct drm_device dev={1};unsigned owner=0;
 device=&dev;lifetime=(struct sgx535_source_guard){.boot=SGX535_BOOT_READY,
  .prepared=1,.asserted_reset=127};observer=lifetime;
 assert(sgx535_source_boot_valid(&lifetime)&&!poisoned&&!ever_claimed);
 if(mode==0){assert(!remesa_stream_source_claim(&dev,&owner)&&lease==&owner&&!poisoned);}
 else{
  if(mode==2)assert(!remesa_stream_source_claim(&dev,&owner));
  remesa_route_source_producer(1);
  assert(lifetime.producers_active==1&&poisoned&&lifetime.reasons==SGX535_SOURCE_INTERFERENCE);
  if(mode!=3)remesa_route_source_producer(0);
  assert(lifetime.producers_active==(mode==3?1U:0U));
  assert(lifetime.reasons==32&&poisoned&&!sgx535_source_boot_valid(&lifetime));
  assert(remesa_stream_source_claim(&dev,&owner)==-EBUSY);
  assert(legacy_calls==(mode==2?0U:mode==3?1U:2U));
  if(mode!=2)assert(!lease&&!ever_claimed&&observer.reasons==32);
  else assert(lease==&owner&&ever_claimed);
 }
 printf("PASS source predecessor model%u: producer-end does not restore first-owner lineage; GPU0\n",mode);
 return 0;
}
