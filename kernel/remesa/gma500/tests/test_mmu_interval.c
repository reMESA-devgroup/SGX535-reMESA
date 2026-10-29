/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual root/audit/cache sequences under ONE modeled writer interval. */
#define REMESA_MAINTAIN_TEST_MAIN existing_maintain_test_main
#include "test_mmu_maintain.c"
int main(void)
{
 struct remesa_mmu_private *s=NULL;
 CHECK(!existing_maintain_test_main());
 CHECK(remesa_mmu_private_bind_locked(NULL)==-EINVAL);
 CHECK(remesa_mmu_private_restore_locked(NULL)==-EINVAL);
 for(unsigned cycle=0;cycle<3;cycle++){
  fixture(&s);unsigned d=downs,u=ups;
  down_write(&s->driver->sem);
  CHECK(!remesa_mmu_private_bind_locked(s));
  CHECK(active==&s->driver->sem&&downs==d+1&&ups==u);
  CHECK(s->root_active&&!s->root_uncertain&&root_writes==1);
  CHECK(s->driver->remesa_private_owner==s);
  CHECK(cache_lines==7*64&&last_cache_page==s->pd->p);
  CHECK(!remesa_mmu_private_validate_bound_locked(s,s->bindings,s->count));
  CHECK(!remesa_mmu_private_maintain_locked(s));
  CHECK(active==&s->driver->sem&&downs==d+1&&ups==u);
  /* No GPU issue occurred. Only the UNISSUED restore path is modeled here. */
  clear_cache_trace();CHECK(!remesa_mmu_private_restore_locked(s));
  CHECK(active==&s->driver->sem&&downs==d+1&&ups==u);
  CHECK(!s->root_active&&!s->root_uncertain&&root_writes==2);
  CHECK(!s->driver->remesa_private_owner);
  CHECK(cache_lines==4*64&&last_cache_page==f.def.p);
  CHECK(s->pd->hw_context==-1&&f.def.hw_context==0);
  up_write(&s->driver->sem);CHECK(ups==u+1);destroy(&s);
 }
 /* Preflight failure and allocation failure must not release caller's lock. */
 for(unsigned fault=0;fault<2;fault++){
  fixture(&s);unsigned d=downs,u=ups;
  if(fault)fail_allocation=allocations+1;
  else registers[PSB_CR_EVENT_STATUS/4]=1;
  down_write(&s->driver->sem);
  CHECK(remesa_mmu_private_bind_locked(s)==(fault?-ENOMEM:-EBUSY));
  CHECK(!writes&&!s->root_active&&!s->root_uncertain);
  CHECK(active==&s->driver->sem&&downs==d+1&&ups==u);
  up_write(&s->driver->sem);destroy(&s);
 }
 /* A queued ordinary flush sees the durable owner even after the wrapper
  * releases sem. It may request invalidation, never touch private-root MMIO. */
 fixture(&s);CHECK(!remesa_mmu_private_bind_exclusive(s));
 unsigned before=writes;video_invalidate=0;
 psb_mmu_flush(s->driver);
 CHECK(writes==before&&s->driver->needs_tlbflush==1&&!video_invalidate);
 CHECK(s->driver->remesa_private_owner==s&&s->root_active);
 down_write(&s->driver->sem);
 CHECK(!remesa_mmu_private_maintain_locked(s));
 CHECK(writes==before+2&&!s->driver->needs_tlbflush&&video_invalidate==1);
 up_write(&s->driver->sem);clear_cache_trace();
 CHECK(!remesa_mmu_private_restore_exclusive(s)&&!s->driver->remesa_private_owner);
 before=writes;psb_mmu_flush(s->driver);CHECK(writes==before+2);
 destroy(&s);
 /* A possibly effective bind/restore write retains roots AND caller's lock.
  * CPU fixture cleanup below is not a recovery or retirement protocol. */
 for(unsigned restore_fault=0;restore_fault<2;restore_fault++){
  fixture(&s);unsigned d=downs,u=ups;
  drop_root_write=restore_fault?2:1;down_write(&s->driver->sem);
  int error=remesa_mmu_private_bind_locked(s);
  if(restore_fault){
   CHECK(!error);clear_cache_trace();error=remesa_mmu_private_restore_locked(s);
  }
  CHECK(error==-EIO&&s->root_active&&s->root_uncertain);
  CHECK(active==&s->driver->sem&&downs==d+1&&ups==u);
  before=writes;
  CHECK(remesa_mmu_private_restore_locked(s)==-EIO&&writes==before);
  CHECK(active==&s->driver->sem&&downs==d+1&&ups==u);
  up_write(&s->driver->sem);
  int old_video=video_invalidate;
  psb_mmu_flush(s->driver);
  CHECK(writes==before&&s->driver->needs_tlbflush==1&&video_invalidate==old_video);
  CHECK(s->driver->remesa_private_owner==s&&s->root_active&&s->root_uncertain);
  check_retained(&s);
 }
 CHECK(!active&&!maps&&downs==ups);
 printf("PASS %u shared MMU checks including locked interval; UNISSUED CPU model, no GPU\n",checks);
 return 0;
}
