/* SPDX-License-Identifier: GPL-2.0-only */
/* In the real source TU, after lost() and before valid(). The old first-owner
 * lifetime is still fed every producer interval and never repaired. Ordinary
 * owned lineage is copied ONCE before the clear, never after its retirement.
 * The ordinary full-clear owner and authenticated native/GEM executor supply
 * these internal calls. Joined qualification is required before physical use. */
#ifdef REMESA_GENERAL_SCENE
#include "remesa_gem_twod_job.h"
#include "remesa_owned_fill_source.h"
enum owned_fill_phase {OF_EMPTY,OF_READY,OF_PUBLISH,OF_ISSUED,OF_RUNNING,OF_RESTORE,OF_NATIVE,OF_FAILED};
static struct {
 struct sgx535_source_guard lineage;
 const void *owner,*task;
 const struct remesa_twod_launch *launch;
 struct remesa_twod_retirement expected;
 u64 destination,last_operation;
 enum owned_fill_phase phase;
} owned_fill;
static const struct sgx535_source_guard *source_lineage(void)
{return owned_fill.owner?&owned_fill.lineage:&lifetime;}
/* Only the explicitly accounted producer reason may differ. Never ignore
 * loss, pending, reset/startup replay or a change to the boot facts. */
static bool owned_fill_history(void)
{
 const struct sgx535_source_guard *g=&owned_fill.lineage;
 return lifetime.boot==g->boot&&lifetime.boot==SGX535_BOOT_READY&&
  lifetime.state==g->state&&lifetime.state==SGX535_SOURCE_UNUSED&&
  !(lifetime.reasons&~SGX535_SOURCE_INTERFERENCE)&&
  lifetime.prepared==g->prepared&&lifetime.asserted_reset==g->asserted_reset&&
  lifetime.released_reset==g->released_reset&&lifetime.startup_pending==g->startup_pending&&
  lifetime.startup_reads==g->startup_reads&&lifetime.startup_fault==g->startup_fault&&
  lifetime.startup_autonomous==g->startup_autonomous&&lifetime.operation==g->operation;
}
static bool source_available(void)
{return !owned_fill.owner||(owned_fill.phase==OF_READY&&owned_fill_history()&&!lifetime.producers_active);}
static void source_released(void)
{memset(&owned_fill,0,sizeof(owned_fill));}
static int owned_fill_fail(void)
{owned_fill.phase=OF_FAILED;lost();return -EIO;}
static bool owned_fill_matches(const struct remesa_twod_launch *l)
{
 const struct remesa_twod_retirement *p=&owned_fill.expected;
 return l&&lease==owned_fill.owner&&owned_fill.owner&&!operation&&!poisoned&&
  current==owned_fill.task&&owned_fill_history()&&owned_fill.launch==l&&l->dev==device&&l->pd==p->pd&&
  l->ticket.context==p->ticket.context&&l->ticket.operation==p->ticket.operation&&
  l->ticket.slot==p->ticket.slot&&l->ticket.generation==p->ticket.generation&&
  l->plan.destination_id==p->destination_id&&l->plan.destination_first==p->destination_first&&
  l->plan.destination_end==p->destination_end&&l->plan.request_base==p->request_base&&
  l->plan.dwords==7&&l->count==1&&l->retirement&&l->source_owner==owned_fill.owner;
}
static bool owned_fill_proof(const struct remesa_twod_retirement *p,bool gem)
{
 const struct remesa_twod_retirement *e=&owned_fill.expected;
 return p&&p->dev==device&&p->pd==e->pd&&p->ticket.context==e->ticket.context&&
  p->ticket.operation==e->ticket.operation&&p->ticket.slot==e->ticket.slot&&
  p->ticket.generation==e->ticket.generation&&p->cookie==e->cookie&&
  p->destination_id==e->destination_id&&p->destination_first==e->destination_first&&
  p->destination_end==e->destination_end&&p->default_root==e->default_root&&
  p->request_base==e->request_base&&p->restored_request_base==e->restored_request_base&&
  p->packet_dwords==7&&p->core==0x01130000U&&p->revision==0x10201U&&
  p->status1==REMESA_TWOD_EVENT_COMPLETE&&!p->status2&&p->native_retired&&p->gem_retired==gem;
}
int remesa_owned_fill_claim(struct drm_device *dev,const void *who,u64 destination)
{
 unsigned long f;int error=0;spin_lock_irqsave(&source_lock,f);
 if(!dev||device!=dev||!who||!destination||lease||ever_claimed||poisoned||operation||
    owned_fill.owner||lifetime.state!=SGX535_SOURCE_UNUSED||!sgx535_source_boot_valid(&lifetime)||lifetime.producers_active)error=-EBUSY;
 else{
  owned_fill.lineage=lifetime;owned_fill.owner=who;owned_fill.destination=destination;
  owned_fill.phase=OF_READY;lease=who;ever_claimed=true;
 }
 spin_unlock_irqrestore(&source_lock,f);return error;
}
int remesa_owned_fill_ready(struct drm_device *dev,const void *who)
{
 unsigned long f;int error=0;spin_lock_irqsave(&source_lock,f);
 if(!who||owned_fill.owner!=who||lease!=who||device!=dev||operation||poisoned||
    !source_available()||!sgx535_source_boot_valid(&owned_fill.lineage)||
    owned_fill.lineage.producers_active||lifetime.producers_active)error=-EBUSY;
 spin_unlock_irqrestore(&source_lock,f);return error;
}
int remesa_owned_fill_publish(const void *who,const struct remesa_twod_launch *l,
 u32 root,u32 saved_base,u64 cookie)
{
 unsigned long f;int error=0;spin_lock_irqsave(&source_lock,f);
 if(!who||owned_fill.owner!=who||lease!=who||operation||poisoned||owned_fill.phase!=OF_READY||
    !sgx535_source_boot_valid(&owned_fill.lineage)||!owned_fill_history()||lifetime.producers_active||
    !l||l->source_owner!=who||l->dev!=device||!l->pd||!l->retirement||l->count!=1||l->plan.dwords!=7||
    l->plan.destination_id!=owned_fill.destination||l->plan.destination_first>=l->plan.destination_end||
    !l->ticket.context||!l->ticket.operation||!l->ticket.generation||
    l->ticket.operation<=owned_fill.last_operation||!cookie||!root||(root&4095U)||
    (l->plan.request_base&0x0fffffffU))error=owned_fill_fail();
 else{
  /* saved_base is the exact borrowed native register value, not the new
   * packet address window. Existing qualified restoration permits values
   * such as0x28000000; do not impose the packet window alignment on it. */
  owned_fill.launch=l;owned_fill.task=current;
  owned_fill.expected=(struct remesa_twod_retirement){.dev=l->dev,.pd=l->pd,.ticket=l->ticket,
   .cookie=cookie,.destination_id=l->plan.destination_id,.destination_first=l->plan.destination_first,
   .destination_end=l->plan.destination_end,.default_root=root,.request_base=l->plan.request_base,
   .restored_request_base=saved_base,.packet_dwords=7};
  owned_fill.phase=OF_PUBLISH;sgx535_source_producer(&lifetime,1);
 }
 spin_unlock_irqrestore(&source_lock,f);return error;
}
int remesa_owned_fill_issued(const struct remesa_twod_launch *l,u64 cookie)
{
 unsigned long f;int error=0;spin_lock_irqsave(&source_lock,f);
 if(!owned_fill_matches(l)||owned_fill.phase!=OF_PUBLISH||cookie!=owned_fill.expected.cookie)error=owned_fill_fail();
 else owned_fill.phase=OF_ISSUED;
 spin_unlock_irqrestore(&source_lock,f);return error;
}
int remesa_owned_fill_published(const struct remesa_twod_launch *l,int result)
{
 unsigned long f;int error=0;spin_lock_irqsave(&source_lock,f);
 /* Close only an interval which this ledger opened, including issue failure.
  * A bad identity never closes somebody else's producer interval. */
 if(owned_fill.launch==l&&(owned_fill.phase==OF_PUBLISH||owned_fill.phase==OF_ISSUED))
  sgx535_source_producer(&lifetime,0);
 if(result||!owned_fill_matches(l)||owned_fill.phase!=OF_ISSUED)error=owned_fill_fail();
 else owned_fill.phase=OF_RUNNING;
 spin_unlock_irqrestore(&source_lock,f);return error;
}
int remesa_owned_fill_restore(const struct remesa_twod_launch *l,u64 cookie)
{
 unsigned long f;int error=0;spin_lock_irqsave(&source_lock,f);
 if(!owned_fill_matches(l)||owned_fill.phase!=OF_RUNNING||cookie!=owned_fill.expected.cookie||
    lifetime.producers_active)error=owned_fill_fail();
 else {owned_fill.phase=OF_RESTORE;sgx535_source_producer(&lifetime,1);}
 spin_unlock_irqrestore(&source_lock,f);return error;
}
int remesa_owned_fill_restored(const struct remesa_twod_launch *l,
 const struct remesa_twod_retirement *proof,int result)
{
 unsigned long f;int error=0;spin_lock_irqsave(&source_lock,f);
 if(owned_fill.launch==l&&owned_fill.phase==OF_RESTORE)sgx535_source_producer(&lifetime,0);
 if(result||!owned_fill_matches(l)||owned_fill.phase!=OF_RESTORE||!owned_fill_proof(proof,false))error=owned_fill_fail();
 else owned_fill.phase=OF_NATIVE;
 spin_unlock_irqrestore(&source_lock,f);return error;
}
int remesa_owned_fill_complete(const void *who,const struct remesa_twod_retirement *proof)
{
 unsigned long f;int error=0;spin_lock_irqsave(&source_lock,f);
 if(!who||owned_fill.owner!=who||lease!=who||operation||poisoned||current!=owned_fill.task||
    owned_fill.phase!=OF_NATIVE||!owned_fill_history()||lifetime.producers_active||!owned_fill_proof(proof,true))error=owned_fill_fail();
 else{
  owned_fill.last_operation=proof->ticket.operation;owned_fill.launch=NULL;owned_fill.task=NULL;
  owned_fill.phase=OF_READY;
 }
 spin_unlock_irqrestore(&source_lock,f);return error;
}
/* Called before existing source-pending processing, under source_lock.
 * A completion can arrive while FIFO publication is still in progress, but
 * only AFTER the exact IRQ cookie was installed. No event is acknowledged or
 * removed here; native hardware/IRQ ledger checks remain mandatory. */
static void owned_fill_pending(struct drm_device *dev,u32 a,u32 d)
{
 if(!owned_fill.owner||operation)return;
 a&=~(1U<<31);
 if(dev!=device||d||(a&&!(a==REMESA_TWOD_EVENT_COMPLETE&&
    (owned_fill.phase==OF_ISSUED||owned_fill.phase==OF_RUNNING||owned_fill.phase==OF_RESTORE))))
  (void)owned_fill_fail();
}
#else
static const struct sgx535_source_guard *source_lineage(void){return &lifetime;}
static bool source_available(void){return true;}
static void source_released(void){}
static void owned_fill_pending(struct drm_device *dev,u32 a,u32 d){(void)dev;(void)a;(void)d;}
#endif
