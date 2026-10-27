/* SPDX-License-Identifier: GPL-2.0-only */
/* Include after private MMU types/map helpers in original mmu.c. */
#include "remesa_mmu_verify_access.h"
int psb_mmu_verify_cached_rw_pages(struct psb_mmu_pd *pd,u64 address,
 struct page **pages,u32 num_pages)
{
 struct psb_mmu_pt *pt;u32 checked=0;int ret=0;
 if(PAGE_SHIFT!=12||PAGE_SIZE!=4096)return -EOPNOTSUPP;
 if(!pd||!pd->driver||!pd->tables||!pd->p||!pages||!num_pages||
    address>0xffffffffULL||(address&4095)||
    (u64)num_pages>((0x100000000ULL-address)>>12))return -EINVAL;
 down_read(&pd->driver->sem);
 if(pd->pd_mask!=PSB_PTE_VALID){ret=-EACCES;goto done;}
 while(checked<num_pages){
  u32 dir=(u32)(address>>22),index=(u32)(address>>12)&1023u;
  u32 count=1024-index,k,*directory,*entries;
  unsigned long table_pfn;
  if(count>num_pages-checked)count=num_pages-checked;
  spin_lock(&pd->driver->lock);pt=pd->tables[dir];
  if(!pt){spin_unlock(&pd->driver->lock);ret=-EINVAL;break;}
  if(pt->pd!=pd||pt->index!=dir||!pt->count||pt->count>1024||!pt->p)
   ret=-EINVAL;
  if(!ret){
   table_pfn=page_to_pfn(pt->p);
   directory=kmap_atomic(pd->p);
   if(table_pfn>0xfffffUL||directory[pt->index]!=((u32)table_pfn<<12|PSB_PTE_VALID))ret=-EACCES;
   kunmap_atomic(directory);
  }
  entries=ret?NULL:kmap_atomic(pt->p);
  for(k=0;!ret&&k<count;k++){
   unsigned long pfn;
   if(!pages[checked+k]){ret=-EINVAL;break;}
   pfn=page_to_pfn(pages[checked+k]);
   if(pfn>0xfffffUL||entries[index+k]!=((u32)pfn<<12|PSB_PTE_VALID|PSB_PTE_CACHED))ret=-EACCES;
  }
  /* Do not use the generic unmap helper: it may free an empty PT. */
  if(entries)kunmap_atomic(entries);
  spin_unlock(&pd->driver->lock);
  if(ret)break;
  checked+=count;address+=(u64)count<<12;
 }
 done:up_read(&pd->driver->sem);return ret;
}
