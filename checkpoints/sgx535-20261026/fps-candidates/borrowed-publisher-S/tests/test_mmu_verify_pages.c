/* Actual mmu reference and batch implementation, synthetic PTs only. GPU0. */
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
typedef uint64_t u64;typedef uint32_t u32;typedef int spinlock_t;
#define PAGE_SHIFT 12
#define PAGE_SIZE 4096u
#define PSB_PTE_SHIFT 12
#define PSB_PDE_SHIFT 22
#define PSB_PTE_VALID 1u
struct page {unsigned long pfn;u32 *words;};
struct psb_mmu_driver {int sem;spinlock_t lock;int needs_tlbflush;};
struct psb_mmu_pd;
struct psb_mmu_pt {struct psb_mmu_pd *pd;u32 index,count;struct page *p;u32 *v;};
struct psb_mmu_pd {struct psb_mmu_driver *driver;struct psb_mmu_pt **tables;struct page *p;u32 invalid_pde,invalid_pte;int hw_context;};
static unsigned downs,ups,locks,unlocks,maps,unmaps,pfn_reads,mmu_writes;
static void *current_map;
static void down_read(int *s){assert(!*s);*s=1;++downs;}
static void up_read(int *s){assert(*s&&!current_map);*s=0;++ups;}
static void spin_lock(spinlock_t *s){assert(!*s);*s=1;++locks;}
static void spin_unlock(spinlock_t *s){assert(*s&&!current_map);*s=0;++unlocks;}
static void *kmap_atomic(struct page *p){assert(p&&p->words&&!current_map);current_map=p->words;++maps;return current_map;}
static void kunmap_atomic(void *p){assert(p&&p==current_map);current_map=NULL;++unmaps;}
static unsigned long page_to_pfn(struct page *p){assert(p);++pfn_reads;return p->pfn;}
static void psb_mmu_clflush(struct psb_mmu_driver *d,void *p){(void)d;(void)p;++mmu_writes;}
static void atomic_set(int *p,int v){*p=v;++mmu_writes;}
static void psb_mmu_free_pt(struct psb_mmu_pt *p){(void)p;++mmu_writes;}
#include "actual_mmu_reference.inc"
int psb_mmu_verify_pages(struct psb_mmu_pd *,u64,struct page **,u32);
#include "../module/remesa_mmu_verify_pages.inc"
#define MAX_PAGES 8192u
static struct psb_mmu_driver driver;
static struct psb_mmu_pd pd;
static struct psb_mmu_pt *tables[1024],pts[1024];
static struct page directory,pt_pages[1024],expected[MAX_PAGES],*pages[MAX_PAGES];
static u32 pde[1024],ptes[1024][1024];
static void stats_clear(void){assert(!driver.sem&&!driver.lock&&!current_map);downs=ups=locks=unlocks=maps=unmaps=pfn_reads=mmu_writes=0;}
static void balanced(void){assert(downs==ups&&locks==unlocks&&maps==unmaps&&!driver.sem&&!driver.lock&&!current_map&&!mmu_writes);}
static void fixture(u64 address,u32 count){
 assert(count<=MAX_PAGES);memset(&driver,0,sizeof(driver));memset(tables,0,sizeof(tables));memset(pde,0,sizeof(pde));memset(ptes,0,sizeof(ptes));memset(pts,0,sizeof(pts));
 directory=(struct page){.pfn=1,.words=pde};pd=(struct psb_mmu_pd){.driver=&driver,.tables=tables,.p=&directory,.hw_context=-1};
 for(u32 i=0;i<count;i++){
  u32 addr=(u32)(address+((u64)i<<PAGE_SHIFT)),di=addr>>22,ti=(addr>>12)&1023;
  expected[i]=(struct page){.pfn=0x20000+i};pages[i]=&expected[i];
  pt_pages[di]=(struct page){.pfn=0x1000+di,.words=ptes[di]};
  pts[di].pd=&pd;pts[di].index=di;pts[di].p=&pt_pages[di];++pts[di].count;tables[di]=&pts[di];
  pde[di]=((u32)pt_pages[di].pfn<<12)|PSB_PTE_VALID;
  ptes[di][ti]=((u32)expected[i].pfn<<12)|PSB_PTE_VALID;
 }
 stats_clear();
}
static int reference(u64 address,struct page **wanted,u32 count){
 for(u32 i=0;i<count;i++){
  unsigned long pfn;
  if(!wanted[i]||psb_mmu_virtual_to_pfn(&pd,(u32)(address+((u64)i<<12)),&pfn)||pfn!=page_to_pfn(wanted[i]))return -EINVAL;
 }return 0;
}
static u64 checksum(void){u64 v=0;for(unsigned i=0;i<1024;i++){v=v*131+pde[i];for(unsigned j=0;j<1024;j++)v=v*131+ptes[i][j];v=v*131+(tables[i]?i+1:0);}return v;}
static unsigned cases;
static void differential(u64 address,u32 count,bool success){
 int old=reference(address,pages,count);balanced();stats_clear();u64 before=checksum();
 int now=psb_mmu_verify_pages(&pd,address,pages,count);assert(now==old);assert((now==0)==success);balanced();assert(before==checksum());
 if(success){assert(pfn_reads==count);assert(downs==1);unsigned expected_pts=(((address>>12)&1023)+count+1023)/1024;assert(maps==expected_pts&&locks==expected_pts);}
 ++cases;
}
int main(void){
 const struct {u64 address;u32 count;} valid[]={{0,1},{0x1000,1},{0x3ff000,2},{0x3fe000,2050},{0x407ef000,8192},{0x30100000,8192},{0xfffff000ULL,1},{0xffc00000ULL,1024},{0x20000000,1},{0x20000000,1023},{0x20001000,1024}};
 for(unsigned i=0;i<sizeof(valid)/sizeof(valid[0]);i++){
  u64 a=valid[i].address;u32 n=valid[i].count;fixture(a,n);differential(a,n,true);
  u32 bad[]={0,n-1,n/2,1023,1024,2047};
  for(unsigned j=0;j<sizeof(bad)/sizeof(bad[0]);j++)if(bad[j]<n){
   u32 at=(u32)(a+((u64)bad[j]<<12));
   fixture(a,n);ptes[at>>22][(at>>12)&1023]^=1;differential(a,n,false);
   fixture(a,n);ptes[at>>22][(at>>12)&1023]^=1u<<12;differential(a,n,false);
   fixture(a,n);pages[bad[j]]=NULL;differential(a,n,false);
  }
 }
 /* GEM pages need not be physically contiguous. */
 fixture(0x407ef000,8192);
 for(u32 i=0;i<8192;i++){
  u32 at=(u32)(0x407ef000ULL+((u64)i<<12));
  expected[i].pfn=0x10000+((i*7919u)%0x70000);
  ptes[at>>22][(at>>12)&1023]=((u32)expected[i].pfn<<12)|PSB_PTE_VALID|0x60;
 }
 differential(0x407ef000,8192,true);
 u32 seed=0x535121;
 for(unsigned k=0;k<64;k++){
  seed=seed*1664525u+1013904223u;u32 n=1+(seed%8192);seed=seed*1664525u+1013904223u;u64 a=(u64)(seed%(1048576u-n+1))<<12;
  fixture(a,n);differential(a,n,true);
  seed=seed*1664525u+1013904223u;u32 bad=seed%n,at=(u32)(a+((u64)bad<<12));ptes[at>>22][(at>>12)&1023]^=0x1000;differential(a,n,false);
 }
 fixture(0,1);u64 unchanged=checksum();
 assert(psb_mmu_verify_pages(NULL,0,pages,1)==-EINVAL);struct psb_mmu_driver *d=pd.driver;pd.driver=NULL;assert(psb_mmu_verify_pages(&pd,0,pages,1)==-EINVAL);pd.driver=d;
 struct psb_mmu_pt **t=pd.tables;pd.tables=NULL;assert(psb_mmu_verify_pages(&pd,0,pages,1)==-EINVAL);pd.tables=t;
 assert(psb_mmu_verify_pages(&pd,0,NULL,1)==-EINVAL);assert(psb_mmu_verify_pages(&pd,0,pages,0)==-EINVAL);
 assert(psb_mmu_verify_pages(&pd,1,pages,1)==-EINVAL);assert(psb_mmu_verify_pages(&pd,0x100000000ULL,pages,1)==-EINVAL);assert(psb_mmu_verify_pages(&pd,0xfffff000ULL,pages,2)==-EINVAL);assert(psb_mmu_verify_pages(&pd,0,pages,0xffffffffU)==-EINVAL);assert(!downs&&unchanged==checksum());balanced();
 fixture(0x3ff000,2);tables[1]=NULL;differential(0x3ff000,2,false);
 fixture(0,1);pts[0].count=0;unchanged=checksum();assert(psb_mmu_verify_pages(&pd,0,pages,1)==-EINVAL);balanced();assert(unchanged==checksum());
 /* Existing dummy fallback can accept its sentinel PFN. Private production
  * verifier deliberately rejects missing PT, a conservative strengthening. */
 fixture(0,1);tables[0]=NULL;pd.invalid_pde=pde[0]=0x1001;pd.invalid_pte=(0x20000u<<12)|1;
 assert(!reference(0,pages,1));balanced();stats_clear();unchanged=checksum();assert(psb_mmu_verify_pages(&pd,0,pages,1)==-EINVAL);balanced();assert(unchanged==checksum());
 printf("PASS actual PTE batch: %u differential cases, all PFNs/VALID checked, first/last/PT-boundary faults,8192-page pools,32-bit end/overflow/null/missing/dummy/empty rejects,lock/map balance,PT/PDE unchanged; synthetic CPU GPU0\n",cases);return 0;
}
