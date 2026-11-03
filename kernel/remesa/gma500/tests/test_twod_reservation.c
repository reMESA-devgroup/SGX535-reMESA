/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual reservation and staged legacy functions, CPU register/lock model. */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "psb_reg.h"
#include "native_defs_actual.h"
#include "../remesa_twod_reservation.h"
typedef uint16_t u16;
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"failure line%d: %s\n",__LINE__,#x);assert(x);}}while(0)
struct model_lock {unsigned held;};
struct drm_device {unsigned powered,power_holds;struct drm_psb_private *dev_private;};
struct drm_psb_private {
 struct drm_device *dev;void *sgx_reg;struct model_lock lock_2d;
 struct remesa_twod_reservation *remesa_twod_owner;
 u32 fifo,blit,base,packet[128];unsigned words,reads,resets;
};
static unsigned long jiffies;
static unsigned irq_lock_held;
#ifdef REMESA_PRIVATE_IRQ_HANDOFF
struct model_scene_session {unsigned phase;};
struct model_fixed_service {struct model_scene_session *session;const void *observer,*observer_context;bool may_release;};
#endif
struct model_scene_owner {struct drm_device *dev;
#ifdef REMESA_PRIVATE_IRQ_HANDOFF
 bool initialized,device_claimed,power_held;
 struct model_scene_session session;struct model_fixed_service fixed_service;
#endif
};
struct sgx535_gma500_fixed_backend {
 bool active,fire_possible,prefire_load_active;struct model_scene_owner *owner;u32 pending_status1,pending_status2;
#ifdef REMESA_PRIVATE_IRQ_HANDOFF
 bool power_held,use_reserved;u32 last_stage;
 void *private_loan,*private_views[16]; /* end() clears private-only metadata */
#endif
};
static struct sgx535_gma500_fixed_backend *fixed_irq_owner;
unsigned long sgx535_gma500_fixed_irq_lock(void)
{CHECK(!irq_lock_held);irq_lock_held=1;return 0;}
void sgx535_gma500_fixed_irq_unlock(unsigned long flags)
{CHECK(!flags&&irq_lock_held);irq_lock_held=0;}
#define HZ 2u
#define time_after(a,b) ((long)((b)-(a))<0)
#define spin_lock_irqsave(lock,flags) do{(flags)=0;CHECK(!irq_lock_held&&!(lock)->held);(lock)->held=1;}while(0)
#define spin_unlock_irqrestore(lock,flags) do{CHECK(!(flags)&&(lock)->held);(lock)->held=0;}while(0)
static int gma_power_is_on(struct drm_device *dev){return dev->powered;}
static int gma_power_begin(struct drm_device *dev,int wake)
{CHECK(!wake);if(!dev->powered)return 0;dev->power_holds++;return 1;}
static void gma_power_end(struct drm_device *dev)
{CHECK(dev->power_holds);dev->power_holds--;}
static u32 model_read(struct drm_psb_private *p,u32 reg)
{
 CHECK(p->lock_2d.held||irq_lock_held);p->reads++;
 switch(reg){
 case PSB_CR_2D_SOCIF:jiffies++;return p->fifo;
 case PSB_CR_2D_BLIT_STATUS:return p->blit;
 case PSB_CR_BIF_TWOD_REQ_BASE:return p->base;
 default:CHECK(reg>=PSB_SGX_2D_SLAVE_PORT&&reg<PSB_SGX_2D_SLAVE_PORT+128*4);return 0;
 }
}
static void model_write(struct drm_psb_private *p,u32 value,u32 reg)
{
 CHECK(p->lock_2d.held&&reg>=PSB_SGX_2D_SLAVE_PORT&&reg<PSB_SGX_2D_SLAVE_PORT+128*4);
 CHECK(p->words<128);p->packet[p->words++]=value;
}
#define PSB_RSGX32(reg) model_read(dev_priv,(reg))
#define PSB_WSGX32(value,reg) model_write(dev_priv,(value),(reg))
static void psb_spank(struct drm_psb_private *p)
{CHECK(p->lock_2d.held);p->resets++;}
static unsigned producer_depth;
static void remesa_route_source_producer(int begin)
{if(begin){CHECK(!producer_depth);producer_depth++;}else{CHECK(producer_depth==1);producer_depth--;}}
struct model_gtt {u32 offset;};
struct model_format {unsigned depth;};
struct drm_framebuffer {struct drm_device *dev;struct model_format *format;struct model_gtt *obj[1];u32 pitches[1];};
struct drm_fb_helper {struct drm_framebuffer *fb;};
struct fb_info {void *par;};
struct fb_copyarea {u32 dx,dy,width,height,sx,sy;};
static struct model_gtt *to_gtt_range(struct model_gtt *g){return g;}
static unsigned console_copies;
static void drm_fb_helper_cfb_copyarea(struct fb_info *i,const struct fb_copyarea *a)
{CHECK(i&&a);console_copies++;}
#include "../remesa_twod_irq.inc"
#include "../remesa_twod_reservation.inc"
/* Preserve the original kernel producer's signed loop counter verbatim. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "legacy_producer_actual.inc"
#pragma GCC diagnostic pop
static void console_copy(struct drm_psb_private *p,unsigned width,unsigned height)
{
 struct model_gtt gtt={4096};struct model_format format={32};
 struct drm_framebuffer fb={p->dev,&format,{&gtt},{width*4}};
 struct drm_fb_helper helper={&fb};struct fb_info info={&helper};
 struct fb_copyarea area={0,0,width,height,0,0};
 psbfb_copyarea_accel(&info,&area);
 CHECK(!p->dev->power_holds&&!producer_depth&&!p->lock_2d.held);
}
int main(void)
{
 struct drm_device device={.powered=1},foreign_device={.powered=1};
 struct drm_psb_private p={.dev=&device,.sgx_reg=(void*)1,.fifo=_PSB_C2_SOCIF_EMPTY,.base=0x28000000};
 struct drm_psb_private other={.dev=&foreign_device,.sgx_reg=(void*)1,.fifo=_PSB_C2_SOCIF_EMPTY,.base=0x38000000};
 struct remesa_twod_reservation r={0},r2={0},zero={0};
 device.dev_private=&p;foreign_device.dev_private=&other;
 CHECK(remesa_twod_reserve(NULL,&r)==-EINVAL&&remesa_twod_reserve(&p,NULL)==-EINVAL);
 device.powered=0;CHECK(remesa_twod_reserve(&p,&r)==-EIO&&!p.reads);device.powered=1;
 p.fifo--;CHECK(remesa_twod_reserve(&p,&r)==-EBUSY&&!memcmp(&r,&zero,sizeof r));p.fifo++;
 p.blit=_PSB_C2B_STATUS_BUSY;CHECK(remesa_twod_reserve(&p,&r)==-EBUSY&&!p.remesa_twod_owner);p.blit=0;
 for(unsigned iteration=0;iteration<3;iteration++){
  unsigned width=iteration==0?1280:iteration==1?640:641;
  unsigned height=iteration==0?800:iteration==1?640:479;
  p.words=0;unsigned before=console_copies;
  console_copy(&p,width,height);CHECK(p.words==10&&console_copies==before);
  u32 expected[10];memcpy(expected,p.packet,sizeof expected);
  CHECK(!remesa_twod_reserve(&p,&r));
  CHECK(r.held&&r.device==&p&&r.saved_base==0x28000000&&p.remesa_twod_owner==&r);
  CHECK(remesa_twod_reserve(&p,&r)==-EBUSY);
  CHECK(remesa_twod_reserve(&other,&r)==-EBUSY);
  CHECK(remesa_twod_reserve(&p,&r2)==-EBUSY&&!memcmp(&r2,&zero,sizeof r2));
  /* MMU preparation borrows the real producer/IRQ reservation. No FIFO or
   * root publication is performed by this guard. */
  unsigned token=iteration,foreign_token=0;
  CHECK(!remesa_twod_mmu_guard_acquire(&p,&r,&token)&&r.mmu_owner==&token);
  CHECK(remesa_twod_mmu_guard_acquire(&p,&r,&foreign_token)==-EBUSY);
  CHECK(remesa_twod_release(&p,&r)==-EBUSY&&r.held&&r.mmu_owner==&token);
  unsigned long guard_flags=sgx535_gma500_fixed_irq_lock();
  CHECK(remesa_twod_irq_issue_locked(&r,9)==-EBUSY&&!r.irq_issued&&!r.irq_cookie);
  CHECK(remesa_twod_irq_drop_locked(&r)==-EBUSY);
  sgx535_gma500_fixed_irq_unlock(guard_flags);
  CHECK(remesa_twod_mmu_guard_release(&other,&r,&token)==-ENOENT);
  CHECK(remesa_twod_mmu_guard_release(&p,&r,&foreign_token)==-EACCES&&r.mmu_owner==&token);
  CHECK(!remesa_twod_mmu_guard_release(&p,&r,&token)&&!r.mmu_owner);
  CHECK(remesa_twod_mmu_guard_release(&p,&r,&token)==-EACCES);
  unsigned reads=p.reads,words=p.words,resets=p.resets;
  console_copy(&p,width,height);
  CHECK(p.reads==reads&&p.words==words&&p.resets==resets&&console_copies==before+1);
  CHECK(!memcmp(expected,p.packet,sizeof expected));
  /* Direct legacy submission is blocked even when its wait would time out
   * and reset the entire engine. No read/wait/write/reset is reached. */
  p.fifo=0;u32 arbitrary[10]={0};
  CHECK(psbfb_2d_submit(&p,arbitrary,10)==-EBUSY);
  CHECK(p.reads==reads&&p.words==words&&p.resets==resets);
  CHECK(remesa_twod_release(&p,&r)==-EBUSY&&r.held&&p.remesa_twod_owner==&r);
  p.fifo=_PSB_C2_SOCIF_EMPTY;p.blit=_PSB_C2B_STATUS_BUSY;
  CHECK(remesa_twod_release(&p,&r)==-EBUSY&&r.held);p.blit=0;
  p.base=0x40000000;CHECK(remesa_twod_release(&p,&r)==-EBUSY&&r.held);p.base=r.saved_base;
  device.powered=0;CHECK(remesa_twod_release(&p,&r)==-EIO&&r.held);device.powered=1;
  CHECK(remesa_twod_release(&p,&r2)==-ENOENT&&r.held);
  CHECK(remesa_twod_release(&other,&r)==-ENOENT&&r.held);
  /* The native IRQ owner, like the old fixed 3D owner, is global. */
  CHECK(remesa_twod_reserve(&other,&r2)==-EBUSY);
  CHECK(!remesa_twod_release(&p,&r)&&!memcmp(&r,&zero,sizeof r)&&!p.remesa_twod_owner);
  CHECK(!remesa_twod_reserve(&other,&r2));CHECK(!remesa_twod_release(&other,&r2));
  CHECK(remesa_twod_release(&p,&r)==-ENOENT);
  p.words=0;console_copy(&p,width,height);
  CHECK(p.words==10&&console_copies==before+1&&!memcmp(expected,p.packet,sizeof expected));
 }
 /* An unreserved original timeout is still EIO, not replayed as a CPU copy.
  * The reset here is a counter in a CPU model, not a real device operation. */
 p.words=0;p.fifo=0;unsigned copies=console_copies,resets=p.resets;
 console_copy(&p,32,32);CHECK(!p.words&&p.resets==resets+1&&console_copies==copies);
 CHECK(!r.held&&!r2.held&&!producer_depth);
 printf("PASS %u reservation/actual legacy producer checks; CPU register model, no GPU operations\n",checks);
 return 0;
}
