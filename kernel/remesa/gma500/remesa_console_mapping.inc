/* SPDX-License-Identifier: GPL-2.0-only */
/* Included in private mmu.c. Retained initial stolen insertion uses type0. */
#include "remesa_console_mapping.h"
int psb_mmu_verify_stolen_rw_sequence(struct psb_mmu_pd *pd,u64 address,
 u64 physical,u32 num_pages)
{
 u32 checked=0;int ret=0;
 if(PAGE_SHIFT!=12||PAGE_SIZE!=4096)return -EOPNOTSUPP;
 if(!pd||!pd->driver||!pd->tables||!pd->p||!num_pages||
    address>0xffffffffULL||physical>0xffffffffULL||
    ((address|physical)&4095)||
    (u64)num_pages>((0x100000000ULL-address)>>12)||
    (u64)num_pages>((0x100000000ULL-physical)>>12))return -EINVAL;
 down_read(&pd->driver->sem);
 if(pd->driver->default_pd!=pd||pd->hw_context!=0||
    pd->pd_mask!=PSB_PTE_VALID||atomic_read(&pd->driver->needs_tlbflush)||
    !page_to_pfn(pd->p)||page_to_pfn(pd->p)>0xfffffUL){
  ret=-EACCES;goto done;
 }
 while(checked<num_pages){
  u32 dir=(u32)(address>>22),index=(u32)(address>>12)&1023u;
  u32 count=1024-index,k,*directory,*entries=NULL;
  struct psb_mmu_pt *pt;unsigned long table_pfn;
  if(count>num_pages-checked)count=num_pages-checked;
  spin_lock(&pd->driver->lock);pt=pd->tables[dir];
  if(!pt||pt->pd!=pd||pt->index!=dir||!pt->count||pt->count>1024||!pt->p)
   ret=-EINVAL;
  if(!ret){
   table_pfn=page_to_pfn(pt->p);
   directory=kmap_atomic(pd->p);
   if(table_pfn>0xfffffUL||directory[dir]!=((u32)table_pfn<<12|PSB_PTE_VALID))
    ret=-EACCES;
   kunmap_atomic(directory);
  }
  if(!ret)entries=kmap_atomic(pt->p);
  for(k=0;!ret&&k<count;k++){
   u32 pfn=(u32)(physical>>12)+k;
   if(entries[index+k]!=psb_mmu_mask_pte(pfn,0))ret=-EACCES;
  }
  if(entries)kunmap_atomic(entries);
  spin_unlock(&pd->driver->lock);
  if(ret)break;
  checked+=count;address+=(u64)count<<12;physical+=(u64)count<<12;
 }
done:
 up_read(&pd->driver->sem);return ret;
}
