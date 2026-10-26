/* SPDX-License-Identifier: GPL-2.0-only */
/* Independent monotonic source lifetime. Startup observations are shared facts,
 * never imported legacy authorization. Every operation has fresh evidence. */
#ifdef REMESA_STREAM_SOURCE_TEST
#include "../tests/source_test_shim.h"
#else
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include "remesa_stream_source.h"
#include "psb_drv.h"
#include "gma500_source_snapshot.h"
#endif
static DEFINE_SPINLOCK(source_lock);
static struct drm_device *device;
static struct sgx535_source_guard lifetime;
static const void *lease;
static struct remesa_stream_observation *operation;
static bool ever_claimed,poisoned;
static bool valid(struct sgx535_gma500_fixed_backend *b)
{
 return lease && operation && operation->backend==b && operation->task==current &&
  !poisoned && !operation->evidence.invalid && !lifetime.reasons && !lifetime.producers_active;
}
static void lost(void) { poisoned=true; if(operation)operation->evidence.invalid=1; }
static void event(u32 kind,u32 q,u32 a,u32 d,u32 ledger,u32 phase)
{
 struct remesa_stream_source_event *e;
 if(!operation || operation->event_count>=REMESA_STREAM_AUDIT_EVENTS){lost();return;}
 operation->event_ns[operation->event_count]=ktime_get_ns();
 e=&operation->events[operation->event_count++];
 *e=(struct remesa_stream_source_event){kind,q,a,d,ledger,phase};
}

int remesa_stream_source_claim(struct drm_device *dev,const void *who)
{
 unsigned long f;int e=0;
 spin_lock_irqsave(&source_lock,f);
 if(!dev || device!=dev || !who || lease || ever_claimed || poisoned ||
    !sgx535_source_boot_valid(&lifetime) || lifetime.producers_active)e=-EBUSY;
 else {lease=who;ever_claimed=true;}
 spin_unlock_irqrestore(&source_lock,f);return e;
}
int remesa_stream_source_release(const void *who)
{
 unsigned long f;int e=0;spin_lock_irqsave(&source_lock,f);
 if(!lease || lease!=who || operation || poisoned)e=-EBUSY;
 else lease=NULL;
 spin_unlock_irqrestore(&source_lock,f);return e;
}
void remesa_stream_source_poison(const void *who)
{unsigned long f;spin_lock_irqsave(&source_lock,f);if(lease==who)lost();spin_unlock_irqrestore(&source_lock,f);}
int remesa_stream_source_bind(const void *who,struct remesa_stream_observation *o,
 struct sgx535_gma500_fixed_backend *b,u64 op,u64 seq)
{
 unsigned long f;int e=0;spin_lock_irqsave(&source_lock,f);
 if(lease!=who || !o || !b || !op || !seq || operation || poisoned ||
    o->evidence.state!=SGX535_CAP_UNUSED || o->evidence.invalid || o->backend || o->finished ||
    !sgx535_source_boot_valid(&lifetime) || lifetime.producers_active)e=-EBUSY;
 else {
  /* Preserve only actual boot lineage. Never clear lifetime reasons. */
  o->boundary=lifetime;o->backend=b;o->task=current;o->operation=op;o->sequence=seq;
  operation=o;sgx535_capsule_call(&o->evidence,current);
 }
 spin_unlock_irqrestore(&source_lock,f);return e;
}
int remesa_stream_source_finish(const void *who,struct remesa_stream_observation *o,const void *color)
{
 unsigned long f;int e=0;spin_lock_irqsave(&source_lock,f);
 if(!o || lease!=who || operation!=o || !valid(o->backend) || o->finished ||
    !sgx535_source_valid(&o->boundary,o->backend))e=-EIO;
 else {
  sgx535_capsule_color(&o->evidence,o->backend->owner,
   o->backend->owner->session.phase,color,4096);
  sgx535_capsule_close(&o->evidence,current,0);
  if(!sgx535_capsule_success(&o->evidence))e=-EIO;
  else o->finished=true;
 }
 if(e)lost();
 spin_unlock_irqrestore(&source_lock,f);return e;
}
/* FAST_NOHASH retains source attribution and terminal state without claiming
 * COLOR-prefix capture or capsule_success(): those belong to legacy evidence. */
int remesa_stream_source_finish_nohash(const void *who,struct remesa_stream_observation *o)
{
 unsigned long f;int e=0;struct sgx535_capsule *c;
 spin_lock_irqsave(&source_lock,f);
 if(!o || lease!=who || operation!=o || !valid(o->backend) || o->finished ||
    !sgx535_source_valid(&o->boundary,o->backend))e=-EIO;
 else {
  c=&o->evidence;
  if(c->invalid || c->state!=SGX535_CAP_TERMINAL ||
     c->owner!=o->backend->owner || c->call!=current ||
     !c->issued || !c->terminal_present || !c->service_started ||
     c->result || c->pending || c->processing || c->events!=7 ||
     c->phase!=SGX535_PHASE_RETIRED ||
     o->backend->owner->session.phase!=SGX535_PHASE_RETIRED ||
     o->backend->owner->session.observed_events!=7 || c->color_present)e=-EIO;
  else {
   sgx535_capsule_close(c,current,0);
   if(c->invalid || c->state!=SGX535_CAP_CLOSED || c->syscall_result)e=-EIO;
   else o->finished=true;
  }
 }
 if(e)lost();
 spin_unlock_irqrestore(&source_lock,f);return e;
}
int remesa_stream_source_unbind(const void *who,struct remesa_stream_observation *o)
{
 unsigned long f;int e=0;spin_lock_irqsave(&source_lock,f);
 if(!o || lease!=who || operation!=o || !valid(o->backend) || !o->finished ||
    o->backend->active || !sgx535_source_close(&o->boundary,o->backend))e=-EBUSY;
 else operation=NULL;
 if(e)lost();
 spin_unlock_irqrestore(&source_lock,f);return e;
}
void remesa_route_source_attach(struct drm_device *dev,struct module *m,sgx535_source_reader reader)
{
 unsigned long f;spin_lock_irqsave(&source_lock,f);
 if(device || !dev || !m || !reader)lost();else device=dev;
 spin_unlock_irqrestore(&source_lock,f);sgx535_provenance_source_attach(dev,m,reader);
}
void remesa_route_source_startup(struct drm_device *dev,const struct sgx535_source_facts *facts)
{
 unsigned long f;spin_lock_irqsave(&source_lock,f);
 if(dev!=device)lost();else sgx535_source_boot_start(&lifetime,facts);
 spin_unlock_irqrestore(&source_lock,f);sgx535_provenance_source_startup(dev,facts);
}
void remesa_route_source_reset_begin(struct drm_device *dev)
{
 unsigned long f;bool legacy;spin_lock_irqsave(&source_lock,f);legacy=!lease&&!ever_claimed;
 if(dev!=device || lifetime.boot!=SGX535_BOOT_RESET_EXPECTED || lease)lost();
 sgx535_source_producer(&lifetime,1);
 spin_unlock_irqrestore(&source_lock,f);if(legacy)sgx535_provenance_source_reset_begin(dev);
}
void remesa_route_source_reset_assert(struct drm_device *dev,u32 reset)
{
 unsigned long f;bool legacy;spin_lock_irqsave(&source_lock,f);legacy=!lease&&!ever_claimed;
 if(dev!=device || lease)lost();
 sgx535_source_boot_assert(&lifetime,reset);
 spin_unlock_irqrestore(&source_lock,f);if(legacy)sgx535_provenance_source_reset_assert(dev,reset);
}
void remesa_route_source_reset_end(struct drm_device *dev,const struct sgx535_source_facts *facts)
{
 unsigned long f;bool legacy;spin_lock_irqsave(&source_lock,f);legacy=!lease&&!ever_claimed;
 if(dev!=device || lease)lost();
 sgx535_source_boot_finish(&lifetime,facts);sgx535_source_producer(&lifetime,0);
 spin_unlock_irqrestore(&source_lock,f);if(legacy)sgx535_provenance_source_reset_end(dev,facts);
}
void remesa_route_source_lifecycle(struct drm_device *dev)
{
 unsigned long f;bool legacy;spin_lock_irqsave(&source_lock,f);legacy=!lease&&!ever_claimed;
 sgx535_source_lifecycle_lost(&lifetime);lost();
 spin_unlock_irqrestore(&source_lock,f);if(legacy)sgx535_provenance_source_lifecycle(dev);
}
void remesa_route_source_producer(int begin)
{
 unsigned long f;bool legacy;spin_lock_irqsave(&source_lock,f);legacy=!lease&&!ever_claimed;
 sgx535_source_producer(&lifetime,begin);
 /* Every producer after startup invalidates this independent first-owner lineage. */
 if(lifetime.boot==SGX535_BOOT_READY || lease)lost();
 spin_unlock_irqrestore(&source_lock,f);if(legacy)sgx535_provenance_source_producer(begin);
}
void remesa_route_source_pending(struct drm_device *dev,u32 a,u32 d)
{
 unsigned long f;bool legacy;u32 pending=(a&~((1U<<31)|(1U<<27)))|d;
 spin_lock_irqsave(&source_lock,f);legacy=!lease&&!ever_claimed;
 if(dev!=device)lost();
 if(!operation || operation->boundary.state!=SGX535_SOURCE_BOUNDARY)
  sgx535_source_pending(&lifetime,pending);
 else if(lease&&pending) {
  /* DPM initialization belongs to this exclusive boundary too. It precedes
   * possible TA fire; a pending IRQ is not itself a completion claim. */
  if(operation->finished || operation->evidence.state>=SGX535_CAP_TERMINAL)lost();
  event(3,operation->operation,a,d,0,0);
 }
 spin_unlock_irqrestore(&source_lock,f);if(legacy)sgx535_provenance_source_pending(dev,a,d);
}
void remesa_route_source_observe(struct drm_device *dev,sgx535_source_snapshot_fn snap)
{
 unsigned long f;bool legacy;spin_lock_irqsave(&source_lock,f);legacy=!lease&&!ever_claimed;
 if(dev!=device || !snap)lost();
 spin_unlock_irqrestore(&source_lock,f);if(legacy)sgx535_provenance_source_observe(dev,snap);
}
int remesa_route_source_begin(struct sgx535_gma500_fixed_backend *b,struct drm_device *dev)
{
 unsigned long f;int ok;bool legacy;spin_lock_irqsave(&source_lock,f);legacy=!lease&&!ever_claimed;
 ok=0;
 if(legacy)lost();
 if(valid(b)&&dev==device) {
  struct sgx535_source_facts facts={0};
  sgx535_gma500_source_snapshot(dev->dev_private,&facts);facts.coverage_complete=1;
  ok=sgx535_source_probe(&operation->boundary,&facts)&&sgx535_source_begin(&operation->boundary,b);
 }
 if(!legacy&&!ok)lost();
 spin_unlock_irqrestore(&source_lock,f);
 return legacy?sgx535_provenance_source_begin(b,dev):ok;
}
int remesa_route_source_boundary(struct sgx535_gma500_fixed_backend *b,const struct sgx535_source_facts *facts)
{
 struct sgx535_source_facts actual;unsigned long f;bool legacy;int ok=0;
 spin_lock_irqsave(&source_lock,f);legacy=!lease&&!ever_claimed;
 if(valid(b)&&facts) {
  actual=*facts;/* begin currently publishes owner after boundary; immutable device is independently leased. */
  actual.coverage_complete=device!=NULL;
  ok=sgx535_source_boundary(&operation->boundary,b,&actual);
 }
 if(!legacy&&!ok)lost();
 spin_unlock_irqrestore(&source_lock,f);
 return legacy?sgx535_provenance_source_boundary(b,facts):ok;
}
int remesa_route_source_write(struct sgx535_gma500_fixed_backend *b)
{
 unsigned long f;bool legacy;int ok;spin_lock_irqsave(&source_lock,f);legacy=!lease&&!ever_claimed;
 ok=valid(b)&&sgx535_source_valid(&operation->boundary,b);
 if(!legacy&&!ok)lost();
 spin_unlock_irqrestore(&source_lock,f);
 return legacy?sgx535_provenance_source_write(b):ok;
}
void remesa_route_admit(struct sgx535_gma500_fixed_backend *b)
{
 unsigned long f;bool legacy;spin_lock_irqsave(&source_lock,f);legacy=!lease&&!ever_claimed;
 if(!legacy) {if(!valid(b))lost();else {struct sgx535_gma500_owner *o=b->owner;
 event(0,0,0,0,o->session.observed_events,o->session.phase);
 sgx535_capsule_admit(&operation->evidence,current,b,&o->fixed_service,o,
 o->session.phase,o->session.observed_events,o->session.sequence,
 b->active&&b->power_held&&!b->fire_possible&&!b->pending_status1&&!b->pending_status2&&
 sgx535_source_valid(&operation->boundary,b));}}
 spin_unlock_irqrestore(&source_lock,f);if(legacy)sgx535_provenance_admit(b);
}
void remesa_route_service(struct sgx535_gma500_fixed_backend *b,struct sgx535_fixed_service *s,int matched)
{
 unsigned long f;bool legacy;spin_lock_irqsave(&source_lock,f);legacy=!lease&&!ever_claimed;
 if(!legacy) {if(!valid(b))lost();else {event(1,0,matched,0,s->session->observed_events,s->session->phase);sgx535_capsule_service(&operation->evidence,b,s,matched&&s==&b->owner->fixed_service);}}
 spin_unlock_irqrestore(&source_lock,f);if(legacy)sgx535_provenance_service(b,s,matched);
}
void remesa_route_issued(struct sgx535_gma500_fixed_backend *b)
{
 unsigned long f;bool legacy;spin_lock_irqsave(&source_lock,f);legacy=!lease&&!ever_claimed;
 if(!legacy) {if(!valid(b))lost();else {event(2,operation->operation,0,0,b->owner->session.observed_events,b->owner->session.phase);sgx535_capsule_issued(&operation->evidence,b);}}
 spin_unlock_irqrestore(&source_lock,f);if(legacy)sgx535_provenance_issued(b);
}
void remesa_route_sample(struct sgx535_gma500_fixed_backend *b,u32 q,u32 a,u32 d,int exclusive)
{
 unsigned long f;bool legacy;spin_lock_irqsave(&source_lock,f);legacy=!lease&&!ever_claimed;
 if(!legacy) {if(!valid(b))lost();else {if(a||d)event(4,q,a,d,0,0);else if(operation->zero_samples==~0U)lost();else operation->zero_samples++;sgx535_capsule_sample(&operation->evidence,b,q,a,d,exclusive&&q==operation->operation);}}
 spin_unlock_irqrestore(&source_lock,f);if(legacy)sgx535_provenance_sample(b,q,a,d,exclusive);
}
void remesa_route_before(struct sgx535_gma500_fixed_backend *b,struct sgx535_fixed_service *s,u32 q,u32 a,u32 d,int exclusive)
{
 unsigned long f;bool legacy;spin_lock_irqsave(&source_lock,f);legacy=!lease&&!ever_claimed;
 if(!legacy) {if(!valid(b))lost();else {event(5,q,a,d,s->session->observed_events,s->session->phase);sgx535_capsule_before(&operation->evidence,b,s,q,a,d,exclusive);}}
 spin_unlock_irqrestore(&source_lock,f);if(legacy)sgx535_provenance_before(b,s,q,a,d,exclusive);
}
void remesa_route_after(struct sgx535_gma500_fixed_backend *b,struct sgx535_fixed_service *s)
{
 unsigned long f;bool legacy;spin_lock_irqsave(&source_lock,f);legacy=!lease&&!ever_claimed;
 if(!legacy) {if(!valid(b))lost();else {event(6,operation->operation,0,0,s->session->observed_events,s->session->phase);sgx535_capsule_after(&operation->evidence,b,s,s->session->observed_events,s->session->phase);}}
 spin_unlock_irqrestore(&source_lock,f);if(legacy)sgx535_provenance_after(b,s);
}
void remesa_route_terminal(struct sgx535_gma500_fixed_backend *b,struct sgx535_fixed_service *s,int result)
{
 unsigned long f;bool legacy;spin_lock_irqsave(&source_lock,f);legacy=!lease&&!ever_claimed;
 if(!legacy) {if(!valid(b))lost();else {event(7,operation->operation,result,0,s->session->observed_events,s->session->phase);sgx535_capsule_terminal(&operation->evidence,b,s,result,s->session->phase,s->session->observed_events);}}
 spin_unlock_irqrestore(&source_lock,f);if(legacy)sgx535_provenance_terminal(b,s,result);
}

/* Read-only end boundary under the same IRQ capture lock. A retired service
 * alone does not authorize the next producer lifetime. No draining/retry. */
int remesa_stream_source_settled(const void *who)
{
 unsigned long irq,f;struct sgx535_source_facts facts={0};int e=0;
 irq=sgx535_gma500_fixed_irq_lock();spin_lock_irqsave(&source_lock,f);
 if(!lease || lease!=who || operation || poisoned || !device ||
    !sgx535_source_boot_valid(&lifetime) || lifetime.producers_active)e=-EIO;
 else {
  sgx535_gma500_source_snapshot(device->dev_private,&facts);
  if(facts.sample_valid!=1 || facts.pending || facts.busy || facts.reset ||
     facts.bif_reads || facts.bif_fault || facts.autonomous)e=-EIO;
 }
 if(e)lost();
 spin_unlock_irqrestore(&source_lock,f);sgx535_gma500_fixed_irq_unlock(irq);return e;
}

int remesa_stream_source_owned(struct sgx535_gma500_fixed_backend *b)
{
 unsigned long f;int owned;spin_lock_irqsave(&source_lock,f);
 owned=lease&&operation&&operation->backend==b;
 spin_unlock_irqrestore(&source_lock,f);return owned;
}
