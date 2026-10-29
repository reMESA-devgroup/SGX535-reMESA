/* SPDX-License-Identifier: GPL-2.0-only */
/* Held-reference association and bound-root validation, no device execution. */
#define REMESA_SWITCH_TEST_MAIN existing_switch_test_main
#include "test_mmu_switch.c"
static void bound_fault(u32 reg,u32 value,int expected)
{
 struct remesa_mmu_private *s=NULL;fixture(&s);
 CHECK(!remesa_mmu_private_bind_exclusive(s));clear_cache_trace();
 unsigned before=writes;registers[reg/4]=value;
 down_write(&s->driver->sem);
 CHECK(remesa_mmu_private_validate_bound_locked(s,f.bindings,3)==expected);
 up_write(&s->driver->sem);
 CHECK(s->root_uncertain&&writes==before&&!cache_lines);check_retained(&s);
}
int main(void)
{
 CHECK(!existing_switch_test_main());
 struct remesa_mmu_private *s=NULL;
 CHECK(remesa_mmu_private_validate_bound_locked(NULL,NULL,0)==-EINVAL);
 fixture(&s);down_write(&s->driver->sem);
 CHECK(remesa_mmu_private_validate_bound_locked(s,f.bindings,3)==-ENOENT);
 up_write(&s->driver->sem);destroy(&s);
 fixture(&s);CHECK(!remesa_mmu_private_bind_exclusive(s));clear_cache_trace();
 unsigned before=writes,d=downs,u=ups;
 down_write(&s->driver->sem);
 CHECK(!remesa_mmu_private_validate_bound_locked(s,f.bindings,3));
 CHECK(downs==d+1&&ups==u&&!maps&&!cache_lines&&writes==before);
 for(unsigned i=0;i<3;i++)for(unsigned bad=0;bad<6;bad++){
  struct remesa_mmu_allowed_binding b[3];memcpy(b,f.bindings,sizeof b);
  struct page *pages[2]={f.bindings[i].pages[0],NULL};
  if(f.bindings[i].page_count==2)pages[1]=f.bindings[i].pages[1];
  switch(bad){
   case 0:b[i].address+=4096;break;
   case 1:b[i].access^=1;break;
   case 2:b[i].cached^=1;break;
   case 3:b[i].page_count++;break;
   case 4:b[i].pages=NULL;break;
   case 5:pages[0]=&f.pages[15];b[i].pages=pages;break;
  }
  CHECK(remesa_mmu_private_validate_bound_locked(s,b,3)==-EACCES);
  CHECK(!s->root_uncertain&&writes==before&&!cache_lines);
 }
 CHECK(remesa_mmu_private_validate_bound_locked(s,f.bindings,2)==-EACCES);
 CHECK(remesa_mmu_private_validate_bound_locked(s,f.bindings,33)==-E2BIG);
 up_write(&s->driver->sem);clear_cache_trace();
 CHECK(!remesa_mmu_private_restore_exclusive(s));destroy(&s);
 bound_fault(PSB_CR_BIF_DIR_LIST_BASE0,0,-EBUSY);
 bound_fault(PSB_CR_EVENT_STATUS,1,-EBUSY);
 bound_fault(PSB_CR_CORE_REVISION,0x00010301,-ENODEV);
 CHECK(!active&&!maps&&downs==ups);
 printf("PASS %u bound-translation shared checks; simulated MMIO, no GPU\n",checks);
 return 0;
}
