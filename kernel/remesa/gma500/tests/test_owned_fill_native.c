/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual native adapter + source authority + frozen guard. MMIO, tasks and
 * backing/GEM retirement receipt are modeled. No GPU or pixel simulation. */
#define REMESA_NATIVE_FIXTURE_ONLY
#define REMESA_OWNED_FILL_NATIVE_TEST
#define REMESA_GENERAL_SCENE 1
#include "test_twod_native.c"
#include "frozen_source_guard.h"
#include "remesa_private_prefire_irq.h"
static struct drm_device *device;
static struct sgx535_source_guard lifetime;
static const void *lease;
static bool ever_claimed,poisoned;
static unsigned task,token,alien;
static const void *current=&task;
static struct model_lock source_lock;
struct source_observation {
 struct {bool invalid;unsigned state;} evidence;
 struct sgx535_source_guard boundary;
 struct sgx535_gma500_fixed_backend *backend;
 bool finished;
 u64 operation;
};
static struct source_observation *operation;
#define SGX535_CAP_TERMINAL 99
#undef spin_lock_irqsave
#undef spin_unlock_irqrestore
#define spin_lock_irqsave(lock,flags) do{(flags)=0;CHECK(!(lock)->held);(lock)->held=1;}while(0)
#define spin_unlock_irqrestore(lock,flags) do{CHECK(!(flags)&&(lock)->held);(lock)->held=0;}while(0)
static void event(u32 a,u32 b,u32 c,u32 d,u32 e,u32 f)
{(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;CHECK(0);}
static void sgx535_provenance_source_producer(int begin){(void)begin;CHECK(0);}
static void sgx535_provenance_source_pending(struct drm_device *dev,u32 a,u32 d)
{(void)dev;(void)a;(void)d;CHECK(0);}
#define remesa_route_source_producer owned_foreign_producer
#define remesa_route_source_pending owned_route_source_pending
#include "owned_native_source_actual.inc"
#undef remesa_route_source_producer
#undef remesa_route_source_pending
static bool owned_publication_live(void)
{return owned_fill.owner&&lifetime.producers_active==1&&!poisoned;}
static void owned_pending_forward(struct drm_device *dev,u32 a,u32 d)
{owned_route_source_pending(dev,a,d);}
int main(int argc,char **argv)
{
 CHECK(argc==2);unsigned scenario=strtoul(argv[1],NULL,10);CHECK(scenario<24);
 struct drm_device dev={.powered=1};
 struct drm_psb_private priv={.dev=&dev,.sgx_reg=(void*)1,.fifo=_PSB_C2_SOCIF_EMPTY,.base=0x28000000};
 struct psb_mmu_pd pd={&dev,0x12345000,0};
 struct remesa_twod_native native={0};struct remesa_twod_retirement proof={0};
 struct remesa_twod_completion completion={0};
 unsigned width=scenario==1?640:scenario==2?641:1280,height=scenario==1?640:scenario==2?479:800;
 struct remesa_twod_fill fill={.destination={33,0x40001000,0x01000000,width,height,width*4,1},
  .request_base=0x40000000,.width=width,.height=height};
 struct remesa_twod_launch launch={.dev=&dev,.pd=&pd,.ticket={77,1,0,1},.count=1,
  .retirement=&proof,.source_owner=&token};
 CHECK(!remesa_twod_fill_prepare(&fill,&launch.plan));
 dev.dev_private=&priv;device=&dev;active_native=&native;packet_words=7;
 lifetime=(struct sgx535_source_guard){.boot=SGX535_BOOT_READY,.prepared=1,.asserted_reset=127};
 registers[PSB_CR_CORE_ID/4]=0x01130000;registers[PSB_CR_CORE_REVISION/4]=0x10201;
 registers[PSB_CR_BIF_DIR_LIST_BASE0/4]=pd.root;
 CHECK(!remesa_owned_fill_claim(&dev,&token,33));
 switch(scenario){
 case 3:registers[PSB_CR_EVENT_STATUS/4]=_PSB_CE_TWOD_COMPLETE;break;
 case 4:owned_foreign_producer(1);break;
 case 5:launch.source_owner=&alien;break;
 case 6:registers[PSB_CR_BIF_DIR_LIST_BASE0/4]^=4096;break;
 case 7:mode=51;break;case 8:mode=52;break;
 case 9:mode=45;break;case 10:mode=10;break;case 11:mode=11;break;
 case 12:mode=13;break;case 13:mode=42;break;case 14:mode=9;break;
 default:break;
 }
 unsigned repeats=scenario==21?2:1;
 for(unsigned i=0;i<repeats;i++){
  u64 cookie=0;int error=remesa_twod_native_executor.start(&native,&launch,&cookie);
  if((scenario>=3&&scenario<=9)||scenario==12){
   CHECK(error&&native.failed&&native.reservation.held&&!proof.native_retired);
   if(scenario!=12)CHECK(!priv.words);
   CHECK(remesa_owned_fill_ready(&dev,&token)||scenario==6);
   puts("PASS owned source/native start refusal, no GPU");return 0;
  }
  CHECK(!error&&cookie==i+1&&priv.words==7*(i+1)&&!poisoned&&owned_fill.phase==OF_RUNNING);
  CHECK(lifetime.reasons==32&&!lifetime.producers_active);
  CHECK(remesa_owned_fill_ready(&dev,&token));
  error=remesa_twod_native_executor.poll(&native,&launch,cookie,1,&completion);
  if(scenario==10||scenario==11){CHECK(error&&native.failed&&native.reservation.held);return 0;}
  CHECK(!error&&native.complete&&completion.event==_PSB_CE_TWOD_COMPLETE);
  if(scenario==15)dev.powered=0;
  error=remesa_twod_native_executor.visible(&native,&launch,cookie);
  if(scenario==15){CHECK(error&&native.failed&&native.reservation.held);return 0;}
  CHECK(!error&&native.visible);
  if(scenario==22)launch.source_owner=&alien;
  error=remesa_twod_native_executor.retire(&native,&launch,cookie);
  if(scenario==13||scenario==14||scenario==22){CHECK(error&&native.failed&&native.reservation.held&&!proof.native_retired&&poisoned);return 0;}
  CHECK(!error&&!native.live&&!priv.remesa_twod_owner&&!twod_irq_owner&&proof.native_retired&&!proof.gem_retired);
  CHECK(owned_fill.phase==OF_NATIVE&&remesa_owned_fill_ready(&dev,&token));
  CHECK(priv.base==0x28000000&&registers[0x0e1c/4]==0&&lifetime.reasons==32&&!lifetime.producers_active);
  if(scenario==16||scenario==23){
   owned_pending_forward(&dev,scenario==16?_PSB_CE_TWOD_COMPLETE:1,0);
   CHECK(poisoned);
  }
  /* Only this final receipt is modeled: actual native receipt above must
   * first match GEM's independently held launch in its own core tests. */
  proof.gem_retired=true;
  if(scenario==17)proof.ticket.generation++;
  error=remesa_owned_fill_complete(&token,&proof);
  if(scenario==16||scenario==17||scenario==23){CHECK(error&&poisoned);return 0;}
  CHECK(!error&&!remesa_owned_fill_ready(&dev,&token));
  if(scenario==18){CHECK(remesa_owned_fill_complete(&token,&proof)&&poisoned);return 0;}
  if(scenario==19){owned_foreign_producer(1);CHECK(poisoned&&remesa_owned_fill_ready(&dev,&token));return 0;}
  if(scenario==20){owned_pending_forward(&dev,_PSB_CE_TWOD_COMPLETE,0);CHECK(poisoned);return 0;}
  launch.ticket.operation++;memset(&proof,0,sizeof proof);
 }
 CHECK(!poisoned&&lifetime.reasons==32&&!lifetime.producers_active&&sgx535_source_boot_valid(source_lineage()));
 puts("PASS actual native/source clear, original guard still refused; GPU0");return 0;
}
