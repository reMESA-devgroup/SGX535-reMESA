/* SPDX-License-Identifier: GPL-2.0-only */
/* Reuse exact auditor/header model; this adds owner/refcount failure tests. */
#define main confinement_model_main
#include "test_mmu_confinement.c"
#undef main
#define GFP_KERNEL 0
#define PSB_MMU_CACHED_MEMORY 1
#define PSB_MMU_RO_MEMORY 2
#define PSB_MMU_WO_MEMORY 4
static unsigned allocations,live_allocations,fail_allocation,insertions,fail_insert;
static unsigned references[16],live_pd,destroyed,damage,return_bound;
static void *kzalloc(size_t n,int flags)
{(void)flags;if(++allocations==fail_allocation)return NULL;void *p=calloc(1,n);if(p)live_allocations++;return p;}
static void *kvcalloc(size_t n,size_t size,int flags)
{if(n&&size>SIZE_MAX/n)return NULL;return kzalloc(n*size,flags);}
static void kfree(void *p){if(p){assert(live_allocations);live_allocations--;free(p);}}
static void kvfree(void *p){kfree(p);}
static void get_page(struct page *p){unsigned i=(unsigned)(p-f.pages);assert(i<16&&references[i]);references[i]++;}
static void put_page(struct page *p){unsigned i=(unsigned)(p-f.pages);assert(i<16&&references[i]>1&&!live_pd);references[i]--;}
struct psb_mmu_pd *psb_mmu_alloc_pd(struct psb_mmu_driver *driver,int trap,int invalid)
{
 assert(driver==&f.driver&&trap==1&&!invalid&&!live_pd);
 if(++allocations==fail_allocation)return NULL;
 memset(f.tables,0,sizeof f.tables);memset(f.pages[0].words,0,4096);
 for(unsigned i=7;i<11;i++){memset(f.pages[i].words,0,4096);f.pt[i-7].count=0;}
 f.pd.hw_context=return_bound?0:-1;live_pd=1;return &f.pd;
}
void psb_mmu_free_pagedir(struct psb_mmu_pd *pd)
{assert(pd==&f.pd&&pd->hw_context==-1&&live_pd);live_pd=0;destroyed++;}
int psb_mmu_insert_pages(struct psb_mmu_pd *pd,struct page **pages,unsigned long address,u32 count,u32 desired,u32 hardware,int type)
{
 assert(pd==&f.pd&&pd->hw_context==-1&&live_pd&&!desired&&!hardware);
 assert(count&&count<=1024&&((address>>12)&1023)+count<=1024);
 unsigned dir=(u32)address>>22,n;
 for(n=0;n<4;n++)if(f.pt[n].index==dir)break;
 assert(n<4);
 f.tables[dir]=&f.pt[n];f.pages[0].words[dir]=(f.pt[n].p->pfn<<12)|1;
 insertions++;
 for(unsigned i=0;i<count;i++){
  unsigned at=(((u32)address>>12)&1023)+i;u32 flags=1;
  if(type&1)flags|=8;
  if(type&2)flags|=4;
  if(type&4)flags|=2;
  f.pt[n].p->words[at]=(page_to_pfn(pages[i])<<12)|flags;
  if(damage)f.pt[n].p->words[at]^=4;
  f.pt[n].count++;
  if(insertions==fail_insert)return -ENOMEM; /* explicit partial map */
 }
 return 0;
}
#include "../remesa_mmu_private.inc"
static void setup_owner(void)
{
 reset();allocations=live_allocations=fail_allocation=insertions=fail_insert=0;
 live_pd=destroyed=damage=return_bound=0;
 for(unsigned i=0;i<16;i++)references[i]=1;
}
static void released(void)
{
 CHECK(!live_pd&&!live_allocations&&!maps&&!active&&downs==ups);
 for(unsigned i=0;i<16;i++)CHECK(references[i]==1);
 CHECK(f.driver.needs_tlbflush==71);
}
int main(void)
{
 struct remesa_mmu_private *owner=NULL;u32 scratch[REMESA_MMU_AUDIT_OVERHEAD_WORDS+4];
 setup_owner();CHECK(!remesa_mmu_private_prepare(&f.driver,f.bindings,3,&owner));
 CHECK(owner&&owner->count==3&&owner->held==4&&live_pd&&insertions==4);
 for(unsigned i=11;i<15;i++)CHECK(references[i]==2);
 CHECK(owner->bindings[0].pages!=f.bindings[0].pages);
 f.data[0]=NULL;memset(f.bindings,0,sizeof f.bindings);
 CHECK(!psb_mmu_audit_unbound_pd(owner->pd,owner->bindings,owner->count,scratch,sizeof scratch/4));
 CHECK(!remesa_mmu_private_release(&owner)&&!owner&&destroyed==1);released();
 CHECK(!remesa_mmu_private_release(&owner));
 for(unsigned failure=1;failure<=4;failure++){
  setup_owner();fail_allocation=failure;
  CHECK(remesa_mmu_private_prepare(&f.driver,f.bindings,3,&owner)==-ENOMEM&&!owner);released();
 }
 for(unsigned failure=1;failure<=4;failure++){
  setup_owner();fail_insert=failure;
  CHECK(remesa_mmu_private_prepare(&f.driver,f.bindings,3,&owner)==-ENOMEM&&!owner&&destroyed==1);released();
 }
 setup_owner();damage=1;
 CHECK(remesa_mmu_private_prepare(&f.driver,f.bindings,3,&owner)==-EPERM&&!owner&&destroyed==1);released();
 setup_owner();f.data[1]=NULL;
 CHECK(remesa_mmu_private_prepare(&f.driver,f.bindings,3,&owner)==-EINVAL&&!owner);released();
 setup_owner();f.data[1]->pfn=0x100000UL;
 CHECK(remesa_mmu_private_prepare(&f.driver,f.bindings,3,&owner)==-ERANGE&&!owner);released();
 setup_owner();f.data[1]->pfn=f.data[0]->pfn;
 CHECK(remesa_mmu_private_prepare(&f.driver,f.bindings,3,&owner)==-EPERM&&!owner&&destroyed==1);released();
 setup_owner();f.data[1]=f.data[0];
 CHECK(remesa_mmu_private_prepare(&f.driver,f.bindings,3,&owner)==-EPERM&&!owner&&destroyed==1);released();
 setup_owner();f.data[0]=f.def.p;
 CHECK(remesa_mmu_private_prepare(&f.driver,f.bindings,3,&owner)==-EPERM&&!owner&&destroyed==1);released();
 setup_owner();CHECK(!remesa_mmu_private_prepare(&f.driver,f.bindings,3,&owner));
 owner->pd->hw_context=0;
 CHECK(remesa_mmu_private_release(&owner)==-EBUSY&&owner&&live_pd&&!destroyed);
 for(unsigned i=11;i<15;i++)CHECK(references[i]==2);
 /* CPU model only: this assignment is NOT a hardware detach operation. */
 owner->pd->hw_context=-1;CHECK(!remesa_mmu_private_release(&owner));released();
 setup_owner();return_bound=1;
 CHECK(remesa_mmu_private_prepare(&f.driver,f.bindings,3,&owner)==-EUCLEAN&&owner&&live_pd&&!destroyed);
 for(unsigned i=11;i<15;i++)CHECK(references[i]==2);
 owner->pd->hw_context=-1;CHECK(!remesa_mmu_private_release(&owner));released();
 setup_owner();CHECK(remesa_mmu_private_prepare(NULL,f.bindings,3,&owner)==-EINVAL);
 CHECK(remesa_mmu_private_prepare(&f.driver,NULL,3,&owner)==-EINVAL);
 CHECK(remesa_mmu_private_prepare(&f.driver,f.bindings,0,&owner)==-EINVAL);
 CHECK(remesa_mmu_private_prepare(&f.driver,f.bindings,33,&owner)==-E2BIG);
 f.bindings[2].page_count=2;CHECK(remesa_mmu_private_prepare(&f.driver,f.bindings,3,&owner)==-ERANGE);released();
 CHECK(remesa_mmu_private_release(NULL)==-EINVAL);
 printf("PASS %u owner/auditor checks; no publication, binding or GPU operations\n",checks);
}
