/* SPDX-License-Identifier: GPL-2.0-only */
#define main confinement_model_main
#include "test_mmu_confinement.c"
#undef main
#define down_read down_write
#define up_read up_write
static void spin_lock(spinlock_t *p){CHECK(active&&!maps&&!*p);*p=1;}
static void spin_unlock(spinlock_t *p){CHECK(active&&!maps&&*p);*p=0;}
#include "../remesa_mmu_verify_access.inc"
static void ready(void)
{
 reset();f.pages[7].words[1023]=(f.data[0]->pfn<<12)|9;
 f.pages[8].words[0]=(f.data[1]->pfn<<12)|9;
 f.pages[9].words[2]=(f.data[2]->pfn<<12)|9;
 f.pages[10].words[1023]=(f.data[3]->pfn<<12)|9;
}
static void verify(unsigned binding,int expected)
{
 memcpy(&saved,&f,sizeof f);
 struct remesa_mmu_allowed_binding *b=&f.bindings[binding];
 CHECK(psb_mmu_verify_cached_rw_pages(&f.pd,b->address,b->pages,b->page_count)==expected);
 CHECK(!memcmp(&saved,&f,sizeof f));CHECK(!maps&&!active&&downs==ups&&!f.driver.lock);
}
int main(void)
{
 ready();verify(0,0);verify(1,0);verify(2,0);
 for(unsigned bit=0;bit<32;bit++){
  ready();f.pages[7].words[1023]^=1u<<bit;verify(0,-EACCES);
  ready();f.pages[8].words[0]^=1u<<bit;verify(0,-EACCES);
  ready();f.pages[0].words[0]^=1u<<bit;verify(0,-EACCES);
 }
 ready();f.data[3]->pfn=0xfffff;f.pages[10].words[1023]=0xfffff009;verify(2,0);
 ready();f.data[3]->pfn=0x100000UL;verify(2,-EACCES);
 ready();f.data[0]=NULL;verify(0,-EINVAL);
 ready();f.pt[0].count=0;verify(0,-EINVAL);
 ready();f.pt[0].count=1025;verify(0,-EINVAL);
 ready();f.pt[0].pd=&f.def;verify(0,-EINVAL);
 ready();f.pt[0].index=1;verify(0,-EINVAL);
 ready();f.tables[0]=NULL;verify(0,-EINVAL);
 ready();f.pt[0].p=NULL;verify(0,-EINVAL);
 ready();f.pd.pd_mask=9;verify(0,-EACCES);
 ready();CHECK(psb_mmu_verify_cached_rw_pages(NULL,0,f.data,1)==-EINVAL);
 CHECK(psb_mmu_verify_cached_rw_pages(&f.pd,0x100000000ULL,f.data,1)==-EINVAL);
 CHECK(psb_mmu_verify_cached_rw_pages(&f.pd,1,f.data,1)==-EINVAL);
 CHECK(psb_mmu_verify_cached_rw_pages(&f.pd,0xfffff000,f.data,2)==-EINVAL);
 CHECK(psb_mmu_verify_cached_rw_pages(&f.pd,0,NULL,1)==-EINVAL);
 printf("PASS %u exact PDE/PTE access checks; no table modification or device operations\n",checks);return 0;
}
