/* SPDX-License-Identifier: GPL-2.0-only */
/* Read-only check of EVERY expected PFN and VALID bit. The caller pins and
 * owns the expected page array for the entire call. One spinlocked atomic PT
 * mapping covers at most 1024 entries; no allocation, MMU write or publication.
 * Keep this implementation included by mmu.c and the differential host shim. */
int psb_mmu_verify_pages(struct psb_mmu_pd *pd, u64 address,
                         struct page **pages, u32 num_pages)
{
 struct psb_mmu_pt *pt;u32 checked=0;int ret=0;
 if(!pd||!pd->driver||!pd->tables||!pages||!num_pages||
    address>0xffffffffULL||(address&(PAGE_SIZE-1))||
    (u64)num_pages>((0x100000000ULL-address)>>PAGE_SHIFT))return -EINVAL;
 down_read(&pd->driver->sem);
 while(checked<num_pages){
  u32 index=psb_mmu_pt_index((u32)address),count=1024-index,k;
  if(count>num_pages-checked)count=num_pages-checked;
  pt=psb_mmu_pt_map_lock(pd,(u32)address);
  /* A private pinned binding must have a real PT, never a dummy PDE/PTE. */
  if(!pt){ret=-EINVAL;break;}
  if(!pt->count)ret=-EINVAL;
  for(k=0;!ret&&k<count;k++){
   u32 pte=pt->v[index+k];
   if(!pages[checked+k]||!(pte&PSB_PTE_VALID)||
      (unsigned long)(pte>>PAGE_SHIFT)!=page_to_pfn(pages[checked+k])){
    ret=-EINVAL;break;
   }
  }
  /* The generic unmap helper frees empty PTs and rewrites PDEs. A verifier
   * must NEVER do that, including corrupt/empty-table failure paths. */
  kunmap_atomic(pt->v);
  spin_unlock(&pd->driver->lock);
  if(ret)break;
  checked+=count;address+=(u64)count<<PAGE_SHIFT;
 }
 up_read(&pd->driver->sem);return ret;
}
