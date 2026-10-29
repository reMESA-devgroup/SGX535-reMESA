/* SPDX-License-Identifier: GPL-2.0-only */
/* Included by mmu.c AFTER its private types and by the CPU test shim. */
#include "remesa_mmu_confinement.h"
#ifdef __KERNEL__
#include <linux/sort.h>
#endif
static int remesa_mmu_pfn_compare(const void *a,const void *b)
{
 u32 x=*(const u32 *)a,y=*(const u32 *)b;
 return (x>y)-(x<y);
}
static int remesa_mmu_record_pfn(u32 *pfns,u32 *n,struct page *p)
{
 unsigned long pfn;
 if(!p)return -EINVAL;
 pfn=page_to_pfn(p);
 if(pfn>0xfffffUL)return -ERANGE;
 pfns[(*n)++]=(u32)pfn;return 0;
}
static int remesa_mmu_audit_unbound(struct psb_mmu_pd *pd,
 const struct remesa_mmu_allowed_binding *b,u32 count,u32 *workspace,u32 words,
 int already_locked)
{
 struct psb_mmu_driver *driver;struct psb_mmu_pd *def;
 struct psb_mmu_pt *pt;u32 *v,*pfns;
 u32 i,j,k=0,total=0,n=0,found=0,offsets[REMESA_MMU_AUDIT_BINDINGS];
 u32 pde,valid,flags,pfn,pte;u64 end,previous=0,va;
 int ret=0;
 if(PAGE_SHIFT!=12||PAGE_SIZE!=4096)return -EOPNOTSUPP;
 if(!pd||!pd->driver||!pd->tables||!pd->p||!workspace||
    ((unsigned long)workspace%__alignof__(u32))||(count&&!b)||
    (count&&((unsigned long)b%__alignof__(struct remesa_mmu_allowed_binding))))return -EINVAL;
 if(count>REMESA_MMU_AUDIT_BINDINGS)return -E2BIG;
 for(i=0;i<count;i++){
  if(!b[i].pages||!b[i].page_count||b[i].address>0xffffffffULL||
     (b[i].address&4095)||!b[i].access||b[i].access>3||b[i].cached>1)return -EINVAL;
  if((u64)b[i].page_count>((0x100000000ULL-b[i].address)>>12))return -ERANGE;
  end=b[i].address+((u64)b[i].page_count<<12);
  if(i&&b[i].address<previous)return -EINVAL;
  previous=end;offsets[i]=total;
  if(b[i].page_count>0x100000u-total)return -E2BIG;
  total+=b[i].page_count;
 }
 if(words<total+REMESA_MMU_AUDIT_OVERHEAD_WORDS)return -ENOSPC;
 driver=pd->driver;
 /* Exclusive semaphore gives ONE coherent CPU tree snapshot. Other MMU
  * writers take its read side; per-PT spinlocks alone would permit races
  * between tables. No allocation, clflush, MMIO or spin-held sorting here. */
 if(!already_locked)down_write(&driver->sem);
#ifdef __KERNEL__
 else lockdep_assert_held_write(&driver->sem);
#endif
 def=driver->default_pd;
 if(!def||!def->p||!def->tables){ret=-EINVAL;goto done;}
 if(pd==def||pd->invalid_pde||pd->invalid_pte||pd->pd_mask!=PSB_PTE_VALID){ret=-EPERM;goto done;}
 if(pd->hw_context!=-1){ret=-EBUSY;goto done;}
 if(page_to_pfn(pd->p)==page_to_pfn(def->p)){ret=-EPERM;goto done;}
 pfns=workspace+1024;
 for(i=0;i<count;i++)for(j=0;j<b[i].page_count;j++){
  ret=remesa_mmu_record_pfn(pfns,&n,b[i].pages[j]);if(ret)goto done;
 }
 /* Include both directories' control frames. No GPU data alias may make
  * either private or restore-default translation tables writable. */
 ret=remesa_mmu_record_pfn(pfns,&n,pd->p);if(ret)goto done;
 ret=remesa_mmu_record_pfn(pfns,&n,pd->dummy_pt);if(ret)goto done;
 ret=remesa_mmu_record_pfn(pfns,&n,pd->dummy_page);if(ret)goto done;
 ret=remesa_mmu_record_pfn(pfns,&n,def->p);if(ret)goto done;
 ret=remesa_mmu_record_pfn(pfns,&n,def->dummy_pt);if(ret)goto done;
 ret=remesa_mmu_record_pfn(pfns,&n,def->dummy_page);if(ret)goto done;
 for(i=0;i<1024;i++)if(def->tables[i]){
  ret=remesa_mmu_record_pfn(pfns,&n,def->tables[i]->p);if(ret)goto done;
 }
 v=kmap_atomic(pd->p);memcpy(workspace,v,4096);kunmap_atomic(v);
 for(i=0;i<1024;i++){
  pt=pd->tables[i];pde=workspace[i];
  if(!pt){if(pde){ret=-EINVAL;goto done;}continue;}
  if(pt->pd!=pd||pt->index!=i||!pt->p||!pt->count||pt->count>1024){ret=-EINVAL;goto done;}
  ret=remesa_mmu_record_pfn(pfns,&n,pt->p);if(ret)goto done;
  if(pde!=((pfns[n-1]<<12)|PSB_PTE_VALID)){ret=-EINVAL;goto done;}
  valid=0;v=kmap_atomic(pt->p);
  for(j=0;j<1024;j++){
   pte=v[j];if(!pte)continue;
   if(!(pte&PSB_PTE_VALID)){ret=-EINVAL;break;}
   valid++;va=((u64)i<<22)|((u64)j<<12);
   while(k<count&&va>=b[k].address+((u64)b[k].page_count<<12))k++;
   if(k==count||va<b[k].address){ret=-EPERM;break;}
   pfn=pfns[offsets[k]+(u32)((va-b[k].address)>>12)];
   if((pte>>12)!=pfn){ret=-EINVAL;break;}
   flags=PSB_PTE_VALID|(b[k].cached?PSB_PTE_CACHED:0);
   if(b[k].access==REMESA_MMU_GPU_READ)flags|=PSB_PTE_RO;
   if(b[k].access==REMESA_MMU_GPU_WRITE)flags|=PSB_PTE_WO;
   if((pte&4095)!=flags){ret=-EPERM;break;}
   found++;
  }
  kunmap_atomic(v);
  if(ret)goto done;
  if(valid!=pt->count){ret=-EINVAL;goto done;}
 }
 if(found!=total){ret=-EINVAL;goto done;}
 sort(pfns,n,sizeof(*pfns),remesa_mmu_pfn_compare,NULL);
 for(i=1;i<n;i++)if(pfns[i-1]==pfns[i]){ret=-EPERM;break;}
 done:if(!already_locked)up_write(&driver->sem);return ret;
}
int psb_mmu_audit_unbound_pd(struct psb_mmu_pd *pd,
 const struct remesa_mmu_allowed_binding *b,u32 count,u32 *workspace,u32 words)
{return remesa_mmu_audit_unbound(pd,b,count,workspace,words,0);}
