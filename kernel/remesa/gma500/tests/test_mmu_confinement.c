/* SPDX-License-Identifier: GPL-2.0-only */
/* CPU model of trusted kernel page/locking operations; no device execution. */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint64_t u64;typedef uint32_t u32;
struct rw_semaphore {int held;};typedef int spinlock_t;typedef int atomic_t;
struct drm_device;
struct page {unsigned long pfn;u32 words[1024];};
#define PAGE_SHIFT 12
#define PAGE_SIZE 4096
/* Values checked against the original qualified module psb_drv.h by runner. */
#define PSB_PTE_VALID 1u
#define PSB_PTE_WO 2u
#define PSB_PTE_RO 4u
#define PSB_PTE_CACHED 8u
#include "mmu.h"
static unsigned maps,downs,ups,sorts,checks;
static struct rw_semaphore *active;
static unsigned long page_to_pfn(struct page *p){assert(p);return p->pfn;}
static void down_write(struct rw_semaphore *s){assert(!s->held&&!active);s->held=1;active=s;downs++;}
static void up_write(struct rw_semaphore *s){assert(s==active&&s->held&&!maps);s->held=0;active=NULL;ups++;}
static u32 *kmap_atomic(struct page *p){assert(active&&active->held);maps++;return p->words;}
static void kunmap_atomic(void *p){(void)p;assert(maps);maps--;}
static void sort(void *p,size_t n,size_t bytes,int(*cmp)(const void*,const void*),void *swap)
{assert(active&&active->held&&!maps&&!swap);sorts++;qsort(p,n,bytes,cmp);}
#include "../remesa_mmu_confinement.inc"
struct fixture {
 struct psb_mmu_driver driver;struct psb_mmu_pd pd,def;
 struct psb_mmu_pt *tables[1024],*defaults[1024];struct psb_mmu_pt pt[5],default_pt;
 struct page pages[16];struct page *data[4];struct remesa_mmu_allowed_binding bindings[3];
};
static struct fixture f,saved;
static u32 space[REMESA_MMU_AUDIT_OVERHEAD_WORDS+4+16];
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"failure line%d: %s\n",__LINE__,#x);abort();}}while(0)
static void reset(void)
{
 memset(&f,0,sizeof f);
 for(unsigned i=0;i<16;i++)f.pages[i].pfn=0x100+i;
 f.driver.default_pd=&f.def;f.driver.needs_tlbflush=71;
 f.pd=(struct psb_mmu_pd){.driver=&f.driver,.hw_context=-1,.tables=f.tables,
  .p=&f.pages[0],.dummy_pt=&f.pages[1],.dummy_page=&f.pages[2],.pd_mask=1};
 f.def=(struct psb_mmu_pd){.driver=&f.driver,.hw_context=0,.tables=f.defaults,
  .p=&f.pages[3],.dummy_pt=&f.pages[4],.dummy_page=&f.pages[5],.pd_mask=1};
 f.default_pt=(struct psb_mmu_pt){.pd=&f.def,.index=17,.count=1,.p=&f.pages[6]};f.defaults[17]=&f.default_pt;
 unsigned dirs[4]={0,1,256,1023},indices[4]={1023,0,2,1023};
 for(unsigned i=0;i<4;i++){
  f.pt[i]=(struct psb_mmu_pt){.pd=&f.pd,.index=dirs[i],.count=1,.p=&f.pages[7+i]};
  f.tables[dirs[i]]=&f.pt[i];f.pages[0].words[dirs[i]]=(f.pages[7+i].pfn<<12)|1;
  f.data[i]=&f.pages[11+i];
  f.pages[7+i].words[indices[i]]=(f.data[i]->pfn<<12)|1|(i<2?4:i==2?10:8);
 }
 f.bindings[0]=(struct remesa_mmu_allowed_binding){0x003ff000,f.data,2,REMESA_MMU_GPU_READ,0};
 f.bindings[1]=(struct remesa_mmu_allowed_binding){0x40002000,f.data+2,1,REMESA_MMU_GPU_WRITE,1};
 f.bindings[2]=(struct remesa_mmu_allowed_binding){0xfffff000,f.data+3,1,REMESA_MMU_GPU_READ|REMESA_MMU_GPU_WRITE,1};
}
static void run(int expected)
{
 unsigned d=downs,u=ups;memcpy(&saved,&f,sizeof f);memset(space,0xa5,sizeof space);
 int e=psb_mmu_audit_unbound_pd(&f.pd,f.bindings,3,space,REMESA_MMU_AUDIT_OVERHEAD_WORDS+4);
 CHECK(e==expected);CHECK(!memcmp(&saved,&f,sizeof f));CHECK(!maps&&!active&&downs-d==ups-u);
 for(unsigned i=REMESA_MMU_AUDIT_OVERHEAD_WORDS+4;i<sizeof space/sizeof space[0];i++)CHECK(space[i]==0xa5a5a5a5);
}
int main(void)
{
 reset();run(0);CHECK(sorts==1);
 reset();f.data[3]->pfn=0xfffff;f.pages[10].words[1023]=0xfffff009;run(0);
 reset();f.pd.hw_context=0;run(-EBUSY);
 reset();f.pd.hw_context=1;run(-EBUSY);
 reset();f.driver.default_pd=&f.pd;run(-EPERM);
 reset();f.pd.invalid_pde=1;run(-EPERM);
 reset();f.pd.invalid_pte=1;run(-EPERM);
 reset();f.pd.pd_mask=9;run(-EPERM);
 reset();f.def.p=f.pd.p;run(-EPERM);
 for(unsigned i=0;i<3;i++) {
  reset();f.bindings[i].access=0;run(-EINVAL);
  reset();f.bindings[i].access=4;run(-EINVAL);
  reset();f.bindings[i].cached=2;run(-EINVAL);
  reset();f.bindings[i].address++;run(-EINVAL);
  reset();f.bindings[i].page_count=0;run(-EINVAL);
  reset();f.bindings[i].pages=NULL;run(-EINVAL);
 }
 reset();f.bindings[2].page_count=2;run(-ERANGE);
 reset();f.bindings[0].address=0x100000000ULL;run(-EINVAL);
 reset();f.bindings[1].address=f.bindings[0].address;run(-EINVAL);
 reset();f.bindings[0].pages[1]=NULL;run(-EINVAL);
 reset();f.data[1]->pfn=0x100000UL;run(-ERANGE);
 reset();f.data[1]->pfn=f.data[0]->pfn;f.pages[8].words[0]=(f.data[0]->pfn<<12)|5;run(-EPERM);
 for(unsigned i=0;i<11;i++) {
  reset();f.data[0]->pfn=f.pages[i].pfn;f.pages[7].words[1023]=(f.data[0]->pfn<<12)|5;run(-EPERM);
 }
 for(unsigned bit=0;bit<32;bit++) {
  reset();f.pages[7].words[1023]^=1u<<bit;
  run(bit==0?-EINVAL:bit<12?-EPERM:-EINVAL);
 }
 reset();f.pages[7].words[1023]=0;run(-EINVAL);
 reset();f.pt[0].count=2;run(-EINVAL);
 reset();f.pt[0].count=0;run(-EINVAL);
 reset();f.pt[0].count=1025;run(-EINVAL);
 reset();f.pt[0].index=1;run(-EINVAL);
 reset();f.pt[0].pd=&f.def;run(-EINVAL);
 reset();f.pt[0].p=NULL;run(-EINVAL);
 reset();f.pages[0].words[0]^=8;run(-EINVAL);
 reset();f.tables[0]=NULL;run(-EINVAL);
 for(unsigned i=0;i<1024;i++) {
  if(i==0||i==1||i==256||i==1023)continue;
  reset();f.pages[0].words[i]=2;run(-EINVAL);
 }
 for(unsigned i=0;i<1024;i++) {
  reset();f.pt[4]=(struct psb_mmu_pt){.pd=&f.pd,.index=500,.count=1,.p=&f.pages[15]};
  f.tables[500]=&f.pt[4];f.pages[0].words[500]=(f.pages[15].pfn<<12)|1;
  f.pages[15].words[i]=(f.data[0]->pfn<<12)|5;run(-EPERM);
 }
 reset();CHECK(psb_mmu_audit_unbound_pd(NULL,NULL,0,space,sizeof space/4)==-EINVAL);
 CHECK(psb_mmu_audit_unbound_pd(&f.pd,NULL,1,space,sizeof space/4)==-EINVAL);
 CHECK(psb_mmu_audit_unbound_pd(&f.pd,f.bindings,33,space,sizeof space/4)==-E2BIG);
 CHECK(psb_mmu_audit_unbound_pd(&f.pd,f.bindings,3,space,REMESA_MMU_AUDIT_OVERHEAD_WORDS+3)==-ENOSPC);
 CHECK(psb_mmu_audit_unbound_pd(&f.pd,f.bindings,3,(u32*)((unsigned char*)space+1),sizeof space/4)==-EINVAL);
 reset();memset(f.tables,0,sizeof f.tables);memset(f.pages[0].words,0,4096);
 CHECK(!psb_mmu_audit_unbound_pd(&f.pd,NULL,0,space,REMESA_MMU_AUDIT_OVERHEAD_WORDS));
 CHECK(downs==ups&&!maps&&!active);printf("PASS %u checks; %u locked snapshots; %u sorts; no hardware operations\n",checks,downs,sorts);
}
