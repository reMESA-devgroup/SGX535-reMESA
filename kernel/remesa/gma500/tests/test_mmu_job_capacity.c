/* SPDX-License-Identifier: GPL-2.0-only. Actual MMU auditor, modeled pages. */
#define main prior_mmu_capacity_fixture
#include "test_mmu_confinement.c"
#undef main
struct wide_mmu {
 struct psb_mmu_driver driver;struct psb_mmu_pd pd,def;struct psb_mmu_pt pt;
 struct psb_mmu_pt *tables[1024],*defaults[1024];
 struct page pages[7+REMESA_MMU_AUDIT_BINDINGS];
 struct page *data[REMESA_MMU_AUDIT_BINDINGS];
 struct remesa_mmu_allowed_binding bindings[REMESA_MMU_AUDIT_BINDINGS];
};
static void wide_audit(unsigned count)
{
 struct wide_mmu *w=calloc(1,sizeof *w),*copy=malloc(sizeof *copy);CHECK(w&&copy);
 size_t words=REMESA_MMU_AUDIT_OVERHEAD_WORDS+count;
 u32 *scratch=malloc((words+8)*sizeof *scratch);CHECK(scratch);
 for(unsigned i=0;i<7+REMESA_MMU_AUDIT_BINDINGS;i++)w->pages[i].pfn=0x1000+i;
 w->driver.default_pd=&w->def;w->driver.needs_tlbflush=71;
 w->pd=(struct psb_mmu_pd){.driver=&w->driver,.hw_context=-1,.tables=w->tables,.p=&w->pages[0],.dummy_pt=&w->pages[1],.dummy_page=&w->pages[2],.pd_mask=1};
 w->def=(struct psb_mmu_pd){.driver=&w->driver,.hw_context=0,.tables=w->defaults,.p=&w->pages[3],.dummy_pt=&w->pages[4],.dummy_page=&w->pages[5],.pd_mask=1};
 w->pt=(struct psb_mmu_pt){.pd=&w->pd,.index=256,.count=count,.p=&w->pages[6]};
 w->tables[256]=&w->pt;w->pages[0].words[256]=(w->pages[6].pfn<<12)|1;
 for(unsigned i=0;i<count;i++){
  w->data[i]=&w->pages[7+i];unsigned access=i%3+1,cached=i%2;
  w->bindings[i]=(struct remesa_mmu_allowed_binding){UINT64_C(0x40000000)+i*4096,w->data+i,1,access,cached};
  unsigned flags=access==REMESA_MMU_GPU_READ?PSB_PTE_RO:access==REMESA_MMU_GPU_WRITE?PSB_PTE_WO:0;
  w->pages[6].words[i]=(w->data[i]->pfn<<12)|PSB_PTE_VALID|flags|(cached?PSB_PTE_CACHED:0);
 }
 for(unsigned fault=0;fault<4;fault++){
  u32 old=w->pages[6].words[count-1];
  if(fault==1)w->pages[6].words[count-1]^=PSB_PTE_CACHED;
  if(fault==2)w->pd.hw_context=0;
  if(fault==3){w->data[count-1]->pfn=w->data[0]->pfn;w->pages[6].words[count-1]=(old&4095)|(w->data[0]->pfn<<12);}
  memcpy(copy,w,sizeof *w);for(size_t i=0;i<words+8;i++)scratch[i]=0xa5a5a5a5;
  int expected=fault==1||fault==3?-EPERM:fault==2?-EBUSY:0;
  CHECK(psb_mmu_audit_unbound_pd(&w->pd,w->bindings,count,scratch,words)==expected);
  CHECK(!memcmp(copy,w,sizeof *w)&&!maps&&!active&&downs==ups);
  for(size_t i=words;i<words+8;i++)CHECK(scratch[i]==0xa5a5a5a5);
  w->pages[6].words[count-1]=old;w->pd.hw_context=-1;w->data[count-1]->pfn=0x1000+7+count-1;
 }
 CHECK(psb_mmu_audit_unbound_pd(&w->pd,w->bindings,count,scratch,words-1)==-ENOSPC);
 CHECK(psb_mmu_audit_unbound_pd(&w->pd,w->bindings,33,scratch,words)==-E2BIG);
 free(scratch);free(copy);free(w);
}
int main(void)
{
 CHECK(!prior_mmu_capacity_fixture());wide_audit(16);wide_audit(17);wide_audit(32);
 puts("PASS actual private MMU audit16/17/32: full permission/cache readset, tail PTE tamper, alias, bound-root refusal and scratch guards; GPU0");return 0;
}
