/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual private verifier + native PTE encoder, reduced page/locking APIs. */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint64_t u64;typedef uint32_t u32;
#define PAGE_SHIFT 12
#define PAGE_SIZE 4096
#include "pte_constants.inc"
struct page {unsigned long pfn;u32 words[1024];};
struct psb_mmu_pd;
struct psb_mmu_driver {int sem,lock,needs_tlbflush;struct psb_mmu_pd *default_pd;};
struct psb_mmu_pt {struct psb_mmu_pd *pd;unsigned index,count;struct page *p;};
struct psb_mmu_pd {struct psb_mmu_driver *driver;struct psb_mmu_pt **tables;struct page *p;u32 pd_mask;int hw_context;};
static unsigned maps,downs,ups,checks;
static int *active;
static void down_read(int *sem){assert(!active&&!*sem);*sem=1;active=sem;downs++;}
static void up_read(int *sem){assert(active==sem&&*sem&&!maps);*sem=0;active=NULL;ups++;}
static void spin_lock(int *lock){assert(active&&!maps&&!*lock);*lock=1;}
static void spin_unlock(int *lock){assert(active&&!maps&&*lock);*lock=0;}
static int atomic_read(int *value){assert(active);return *value;}
static unsigned long page_to_pfn(struct page *p){assert(p);return p->pfn;}
static u32 *kmap_atomic(struct page *p){assert(active);maps++;return p->words;}
static void kunmap_atomic(void *p){assert(p&&maps);maps--;}
#include "pte_actual.inc"
#include "remesa_console_mapping.inc"
struct fixture {
 struct psb_mmu_driver driver;
 struct psb_mmu_pd pd,other;
 struct psb_mmu_pt *tables[1024],pt[2];
 struct page root,table[2];
};
static struct fixture f,saved;
static void ready(void)
{
 assert(!maps&&!active&&downs==ups);memset(&f,0,sizeof(f));
 f.driver.default_pd=&f.pd;
 f.pd=(struct psb_mmu_pd){.driver=&f.driver,.tables=f.tables,.p=&f.root,.pd_mask=PSB_PTE_VALID,.hw_context=0};
 f.root.pfn=0x100;
 for(unsigned i=0;i<2;i++){
  f.table[i].pfn=0x110+i;
  f.pt[i]=(struct psb_mmu_pt){.pd=&f.pd,.index=i,.count=1,.p=&f.table[i]};
  f.tables[i]=&f.pt[i];f.root.words[i]=(f.table[i].pfn<<12)|PSB_PTE_VALID;
 }
 f.table[0].words[1023]=0xffffe001;f.table[1].words[0]=0xfffff001;
}
static void verify(struct psb_mmu_pd *pd,u64 address,u64 physical,u32 pages,int expected)
{
 memcpy(&saved,&f,sizeof(f));checks++;
 assert(psb_mmu_verify_stolen_rw_sequence(pd,address,physical,pages)==expected);
 assert(!memcmp(&f,&saved,sizeof(f))&&!maps&&!active&&downs==ups&&!f.driver.lock);
}
static void check(int expected){verify(&f.pd,0x003ff000,0xffffe000,2,expected);}
int main(void)
{
 ready();check(0);
 for(unsigned bit=0;bit<32;bit++)for(unsigned i=0;i<2;i++){
  ready();f.table[i].words[i?0:1023]^=1u<<bit;check(-EACCES);
  ready();f.root.words[i]^=1u<<bit;check(-EACCES);
 }
 ready();f.tables[1]=NULL;check(-EINVAL);
 ready();f.pt[0].pd=&f.other;check(-EINVAL);
 ready();f.pt[1].index=0;check(-EINVAL);
 ready();f.pt[1].count=0;check(-EINVAL);
 ready();f.pt[0].count=1025;check(-EINVAL);
 ready();f.pt[1].p=NULL;check(-EINVAL);
 ready();f.table[1].pfn=0x100000;check(-EACCES);
 ready();f.driver.default_pd=&f.other;check(-EACCES);
 ready();f.driver.needs_tlbflush=1;check(-EACCES);
 ready();f.pd.hw_context=-1;check(-EACCES);
 ready();f.pd.pd_mask|=PSB_PTE_CACHED;check(-EACCES);
 ready();f.root.pfn=0;check(-EACCES);
 ready();f.root.pfn=0x100000;check(-EACCES);
 ready();verify(NULL,0x003ff000,0xffffe000,2,-EINVAL);
 ready();f.pd.tables=NULL;check(-EINVAL);
 ready();f.pd.p=NULL;check(-EINVAL);
 ready();f.pd.driver=NULL;check(-EINVAL);
 ready();verify(&f.pd,0x100000000ULL,0xffffe000,2,-EINVAL);
 verify(&f.pd,0x003ff000,0x100000000ULL,2,-EINVAL);
 verify(&f.pd,0x003ff001,0xffffe000,2,-EINVAL);
 verify(&f.pd,0x003ff000,0xffffe001,2,-EINVAL);
 verify(&f.pd,0x003ff000,0xffffe000,0,-EINVAL);
 verify(&f.pd,0x003ff000,0xfffff000,2,-EINVAL);
 verify(&f.pd,0xfffff000,0xffffe000,2,-EINVAL);
 verify(&f.pd,0x003ff000,0xffffe000,0xffffffffU,-EINVAL);
 ready();f.table[0].words[1023]=PSB_PTE_VALID;
 verify(&f.pd,0x003ff000,0,1,0);
 printf("CONSOLE_MAPPING_MODEL_PASS %u exact PDE/PTE controls; read-only; GPU0\n",checks);
 return 0;
}
