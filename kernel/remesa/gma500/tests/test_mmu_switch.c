/* SPDX-License-Identifier: GPL-2.0-only */
/* Instrumented CPU MMIO model; NOT a hardware context-switch qualification. */
#define REMESA_PRIVATE_TEST_MAIN private_owner_test_main
#include "test_mmu_private.c"
#include "psb_reg.h"
#include "frozen_source_guard.h"
#define _PSB_MMU_ER_MASK 0x0001ff00u
struct drm_psb_private {
 struct psb_mmu_driver *mmu;void *sgx_reg;
};
struct drm_device {struct drm_psb_private *dev_private;int powered;};
static struct drm_device dev;
static struct drm_psb_private priv;
static u32 registers[1024],write_regs[32],write_values[32];
static unsigned writes,root_writes,drop_root_write,write_barriers;
static unsigned corrupt_control;
static struct remesa_mmu_private *tracked_owner;
static u32 expected_pages[16][1024];
static atomic_t video_invalidate;
static int gma_power_is_on(struct drm_device *d){return d&&d->powered;}
static int atomic_read(atomic_t *p){return *p;}
static void atomic_set(atomic_t *p,int value){*p=value;}
static void wmb(void){assert(active&&!maps);write_barriers++;}
static u32 mmio_read(struct drm_psb_private *p,u32 reg)
{
 assert(p==&priv&&dev.powered&&active&&!maps&&reg/4<1024&&!(reg%4));
 return registers[reg/4];
}
static void mmio_write(struct drm_psb_private *p,u32 value,u32 reg)
{
 assert(p==&priv&&dev.powered&&active&&!maps&&writes<32);
 assert(reg==PSB_CR_BIF_CTRL||reg==PSB_CR_BIF_DIR_LIST_BASE0);
 if(reg==PSB_CR_BIF_DIR_LIST_BASE0){
  assert(tracked_owner&&tracked_owner->root_active&&barriers==2);
  assert(tracked_owner->pd->hw_context==0&&f.def.hw_context==-1);
  assert(last_cache_page==(value==tracked_owner->private_root?tracked_owner->pd->p:f.def.p));
 }
 write_regs[writes]=reg;write_values[writes++]=value;
 if(reg==PSB_CR_BIF_DIR_LIST_BASE0&&++root_writes==drop_root_write)return;
 registers[reg/4]=value;
 if(corrupt_control&&reg==PSB_CR_BIF_CTRL&&!(value&_PSB_CB_CTRL_INVALDC))
  registers[reg/4]^=1;
}
#define PSB_RSGX32(reg) mmio_read(dev_priv,(reg))
#define PSB_WSGX32(value,reg) mmio_write(dev_priv,(value),(reg))
/* Qualification runner extracts THIS function verbatim from retained mmu.c. */
#include "mmu_flush_pd_actual.inc"
#include "../remesa_mmu_switch.inc"
static void clear_cache_trace(void)
{
 cache_stride=64;cache_lines=barriers=0;last_cache_page=NULL;
 memset(cache_seen,0,sizeof cache_seen);
}
static void fixture(struct remesa_mmu_private **out)
{
 setup_owner();memset(registers,0,sizeof registers);
 writes=root_writes=drop_root_write=write_barriers=corrupt_control=0;video_invalidate=0;
 dev=(struct drm_device){&priv,1};priv=(struct drm_psb_private){&f.driver,(void*)1};
 f.driver.dev=&dev;f.driver.has_clflush=1;f.driver.clflush_add=65536;
 f.driver.needs_tlbflush=0;f.driver.msvdx_mmu_invaldc=&video_invalidate;
 registers[PSB_CR_CORE_ID/4]=0x01130000;
 registers[PSB_CR_CORE_REVISION/4]=0x00010201;
 registers[PSB_CR_BIF_DIR_LIST_BASE0/4]=(u32)f.def.p->pfn<<12;
 registers[PSB_CR_2D_SOCIF/4]=_PSB_C2_SOCIF_EMPTY;
 clear_cache_trace();CHECK(!*out);
 CHECK(!remesa_mmu_private_prepare(&f.driver,f.bindings,3,out));
 tracked_owner=*out;
 for(unsigned i=0;i<16;i++)memcpy(expected_pages[i],f.pages[i].words,4096);
}
static void destroy(struct remesa_mmu_private **out)
{
 CHECK(!remesa_mmu_private_release(out)&&!*out);
 CHECK(!live_pd&&!live_allocations&&!maps&&!active&&downs==ups);
 for(unsigned i=0;i<16;i++)CHECK(references[i]==1);
}
static void check_retained(struct remesa_mmu_private **out)
{
 CHECK(*out&&(*out)->root_active&&(*out)->root_uncertain);
 CHECK(remesa_mmu_private_release(out)==-EBUSY&&*out&&live_pd);
 (*out)->pd->hw_context=-1; /* metadata tampering must not release active root */
 CHECK(remesa_mmu_private_release(out)==-EBUSY&&*out&&live_pd);
 (*out)->pd->hw_context=0;
 unsigned before=writes;
 CHECK(remesa_mmu_private_restore_exclusive(*out)==-EIO&&writes==before);
 CHECK(remesa_mmu_private_bind_exclusive(*out)==-EBUSY&&writes==before);
 for(unsigned i=11;i<15;i++)CHECK(references[i]==2);
 /* CPU fixture teardown ONLY, not a real unbind/reset/recovery protocol. */
 (*out)->root_active=(*out)->root_uncertain=0;
 (*out)->pd->hw_context=-1;f.def.hw_context=0;destroy(out);
}
static void preflight_fault(u32 reg,u32 value,int expected)
{
 struct remesa_mmu_private *s=NULL;fixture(&s);
 registers[reg/4]=value;
 CHECK(remesa_mmu_private_bind_exclusive(s)==expected);
 CHECK(!writes&&!s->root_active&&!s->root_uncertain);
 CHECK(s->pd->hw_context==-1&&f.def.hw_context==0);destroy(&s);
}
int main(void)
{
 struct remesa_mmu_private *s=NULL;
 CHECK(remesa_mmu_private_bind_exclusive(NULL)==-EINVAL);
 CHECK(remesa_mmu_private_restore_exclusive(NULL)==-EINVAL);
 fixture(&s);CHECK(remesa_mmu_private_restore_exclusive(s)==-ENOENT);
 for(unsigned cycle=0;cycle<3;cycle++){
  clear_cache_trace();unsigned before=writes;
  registers[PSB_CR_EVENT_STATUS/4]=1u<<31; /* aggregate alone is benign */
  CHECK(!remesa_mmu_private_bind_exclusive(s));
  CHECK(s->root_active&&!s->root_uncertain&&s->restore_pd==&f.def);
  CHECK(s->pd->hw_context==0&&f.def.hw_context==-1);
  CHECK(writes==before+3&&write_regs[before]==PSB_CR_BIF_DIR_LIST_BASE0);
  CHECK(write_values[before]==((u32)s->pd->p->pfn<<12));
  CHECK(write_regs[before+1]==PSB_CR_BIF_CTRL&&write_values[before+1]==_PSB_CB_CTRL_INVALDC);
  CHECK(write_regs[before+2]==PSB_CR_BIF_CTRL&&!write_values[before+2]);
  CHECK(cache_lines==7*64&&last_cache_page==s->pd->p&&video_invalidate==1);
  CHECK(!f.driver.needs_tlbflush&&!maps&&!active);
  for(unsigned i=0;i<16;i++)CHECK(!memcmp(expected_pages[i],f.pages[i].words,4096));
  CHECK(remesa_mmu_private_bind_exclusive(s)==-EBUSY&&writes==before+3);
  CHECK(remesa_mmu_private_release(&s)==-EBUSY&&s);
  clear_cache_trace();CHECK(!remesa_mmu_private_restore_exclusive(s));
  CHECK(!s->root_active&&!s->root_uncertain&&!s->restore_pd);
  CHECK(s->pd->hw_context==-1&&f.def.hw_context==0);
  CHECK(writes==before+6&&write_values[before+3]==((u32)f.def.p->pfn<<12));
  CHECK(cache_lines==4*64&&last_cache_page==f.def.p);
  for(unsigned i=0;i<16;i++)CHECK(!memcmp(expected_pages[i],f.pages[i].words,4096));
  CHECK(remesa_mmu_private_release(NULL)==-EINVAL);
 }
 destroy(&s);
 const u32 zero_regs[]={PSB_CR_BIF_BANK0,PSB_CR_BIF_BANK1,PSB_CR_SOFT_RESET,
  PSB_CR_BIF_INT_STAT,PSB_CR_EVENT_STATUS2};
 for(unsigned i=0;i<sizeof zero_regs/sizeof zero_regs[0];i++)
  for(unsigned bit=0;bit<32;bit++)preflight_fault(zero_regs[i],1u<<bit,-EBUSY);
 for(unsigned bit=0;bit<31;bit++)preflight_fault(PSB_CR_EVENT_STATUS,1u<<bit,-EBUSY);
 preflight_fault(PSB_CR_CORE_ID,0,-ENODEV);
 preflight_fault(PSB_CR_CORE_REVISION,0x00010301,-ENODEV);
 preflight_fault(PSB_CR_BIF_DIR_LIST_BASE0,0,-EBUSY);
 preflight_fault(PSB_CR_2D_SOCIF,0,-EBUSY);
 preflight_fault(PSB_CR_2D_BLIT_STATUS,_PSB_C2B_STATUS_BUSY,-EBUSY);
 for(unsigned bit=0;bit<8;bit++)preflight_fault(SGX535_SOURCE_BIF_READS_REG,1u<<bit,-EBUSY);
 preflight_fault(SGX535_SOURCE_EVENT_TIMER_REG,SGX535_SOURCE_EVENT_TIMER_ENABLE,-EBUSY);
 preflight_fault(SGX535_SOURCE_EVENT_KICK_REG,SGX535_SOURCE_EVENT_KICK_NOW,-EBUSY);
 for(unsigned bit=0;bit<32;bit++)if((1u<<bit)&(_PSB_MMU_ER_MASK|
   _PSB_CB_CTRL_FLUSH|_PSB_CB_CTRL_INVALDC|_PSB_CB_CTRL_CLEAR_FAULT))
  preflight_fault(PSB_CR_BIF_CTRL,1u<<bit,-EBUSY);
 fixture(&s);dev.powered=0;
 CHECK(remesa_mmu_private_bind_exclusive(s)==-ENODEV&&!writes);dev.powered=1;destroy(&s);
 fixture(&s);f.driver.needs_tlbflush=1;
 CHECK(remesa_mmu_private_bind_exclusive(s)==-EBUSY&&!writes);destroy(&s);
 fixture(&s);f.pages[7].words[1023]^=4;
 CHECK(remesa_mmu_private_bind_exclusive(s)==-EPERM&&!writes);destroy(&s);
 fixture(&s);f.pages[0].pfn=0;
 CHECK(remesa_mmu_private_bind_exclusive(s)==-ERANGE&&!writes);destroy(&s);
 fixture(&s);fail_allocation=allocations+1;
 CHECK(remesa_mmu_private_bind_exclusive(s)==-ENOMEM&&!writes);destroy(&s);
 fixture(&s);f.driver.has_clflush=0;
 CHECK(remesa_mmu_private_bind_exclusive(s)==-EOPNOTSUPP&&!writes);destroy(&s);
 fixture(&s);drop_root_write=1;
 CHECK(remesa_mmu_private_bind_exclusive(s)==-EIO&&writes==3);check_retained(&s);
 fixture(&s);corrupt_control=1;
 CHECK(remesa_mmu_private_bind_exclusive(s)==-EIO&&writes==3);check_retained(&s);
 fixture(&s);CHECK(!remesa_mmu_private_bind_exclusive(s));
 clear_cache_trace();drop_root_write=2;
 CHECK(remesa_mmu_private_restore_exclusive(s)==-EIO&&writes==6);check_retained(&s);
 fixture(&s);CHECK(!remesa_mmu_private_bind_exclusive(s));
 clear_cache_trace();corrupt_control=1;
 CHECK(remesa_mmu_private_restore_exclusive(s)==-EIO&&writes==6);check_retained(&s);
 fixture(&s);CHECK(!remesa_mmu_private_bind_exclusive(s));
 registers[PSB_CR_BIF_INT_STAT/4]=1;
 CHECK(remesa_mmu_private_restore_exclusive(s)==-EBUSY&&writes==3);check_retained(&s);
 fixture(&s);CHECK(!remesa_mmu_private_bind_exclusive(s));
 f.driver.clflush_add=0;
 CHECK(remesa_mmu_private_restore_exclusive(s)==-EOPNOTSUPP&&writes==3);check_retained(&s);
 fixture(&s);CHECK(!remesa_mmu_private_bind_exclusive(s));
 f.def.hw_context=0;
 CHECK(remesa_mmu_private_restore_exclusive(s)==-ESTALE&&writes==3);check_retained(&s);
 fixture(&s);CHECK(!remesa_mmu_private_bind_exclusive(s));
 f.def.dummy_pt=NULL;
 CHECK(remesa_mmu_private_restore_exclusive(s)==-EINVAL&&writes==3);check_retained(&s);
 CHECK(!active&&!maps&&downs==ups);
 printf("PASS %u private-root transaction checks; simulated MMIO only, no GPU commands\n",checks);
 return 0;
}
