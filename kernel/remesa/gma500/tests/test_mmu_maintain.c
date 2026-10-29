/* SPDX-License-Identifier: GPL-2.0-only */
/* Original maintenance body, held write-semaphore and simulated MMIO. */
#define REMESA_PRIVATE_IRQ_HANDOFF 1
struct psb_mmu_driver;
static void remesa_mmu_original_flush_locked(struct psb_mmu_driver *);
#define REMESA_SWITCH_TEST_MAIN existing_switch_test_main
#include "test_mmu_switch.c"
#include "mmu_maintain_actual.inc"
static void maintain_fault(u32 reg,u32 value,int expected)
{
 struct remesa_mmu_private *s=NULL;fixture(&s);
 CHECK(!remesa_mmu_private_bind_exclusive(s));clear_cache_trace();
 registers[reg/4]=value;unsigned before=writes;
 down_write(&s->driver->sem);
 CHECK(remesa_mmu_private_maintain_locked(s)==expected&&writes==before);
 up_write(&s->driver->sem);
 CHECK(s->root_uncertain&&s->root_active&&!cache_lines);check_retained(&s);
}
#ifndef REMESA_MAINTAIN_TEST_MAIN
#define REMESA_MAINTAIN_TEST_MAIN main
#endif
int REMESA_MAINTAIN_TEST_MAIN(void)
{
 struct remesa_mmu_private *s=NULL;
 CHECK(!existing_switch_test_main());
 CHECK(remesa_mmu_private_maintain_locked(NULL)==-EINVAL);
 fixture(&s);down_write(&s->driver->sem);
 CHECK(remesa_mmu_private_maintain_locked(s)==-ENOENT);
 up_write(&s->driver->sem);destroy(&s);
 for(unsigned pending=0;pending<2;pending++){
  fixture(&s);f.driver.needs_tlbflush=pending;unsigned d=downs,u=ups;
  psb_mmu_flush(&f.driver);
  CHECK(downs==d+1&&ups==u+1&&!active&&writes==2&&!root_writes);
  CHECK(write_regs[0]==PSB_CR_BIF_CTRL&&write_regs[1]==PSB_CR_BIF_CTRL);
  CHECK(write_values[0]==(pending?_PSB_CB_CTRL_INVALDC:_PSB_CB_CTRL_FLUSH));
  CHECK(!write_values[1]&&!f.driver.needs_tlbflush&&video_invalidate==1);
  destroy(&s);
 }
 for(unsigned pending=0;pending<2;pending++){
  fixture(&s);CHECK(!remesa_mmu_private_bind_exclusive(s));clear_cache_trace();
  s->driver->needs_tlbflush=pending;video_invalidate=0;
  unsigned before=writes,d=downs,u=ups;
  down_write(&s->driver->sem);
  CHECK(!remesa_mmu_private_maintain_locked(s));
  CHECK(downs==d+1&&ups==u&&active==&s->driver->sem);
  CHECK(writes==before+2&&write_regs[before]==PSB_CR_BIF_CTRL);
  CHECK(write_values[before]==(pending?_PSB_CB_CTRL_INVALDC:_PSB_CB_CTRL_FLUSH));
  CHECK(write_regs[before+1]==PSB_CR_BIF_CTRL&&!write_values[before+1]);
  CHECK(!s->driver->needs_tlbflush&&video_invalidate==1);
  CHECK(!cache_lines&&!s->root_uncertain&&root_writes==1);
  up_write(&s->driver->sem);clear_cache_trace();
  CHECK(!remesa_mmu_private_restore_exclusive(s));destroy(&s);
 }
 maintain_fault(PSB_CR_BIF_DIR_LIST_BASE0,0,-EBUSY);
 maintain_fault(PSB_CR_EVENT_STATUS,1,-EBUSY);
 maintain_fault(PSB_CR_BIF_BANK0,1,-EBUSY);
 maintain_fault(PSB_CR_CORE_REVISION,0x00010301,-ENODEV);
 fixture(&s);CHECK(!remesa_mmu_private_bind_exclusive(s));clear_cache_trace();
 unsigned before=writes;corrupt_control=1;down_write(&s->driver->sem);
 CHECK(remesa_mmu_private_maintain_locked(s)==-EIO&&writes==before+2);
 up_write(&s->driver->sem);check_retained(&s);
 CHECK(!active&&!maps&&downs==ups);
 printf("PASS %u locked-maintenance checks; original flush body, simulated MMIO, no GPU\n",checks);
 return 0;
}
