/* SPDX-License-Identifier: GPL-2.0-only */
#include "remesa_mmu_default_root.h"
int psb_mmu_default_root(struct drm_device *dev,struct psb_mmu_pd *pd,u32 *out)
{
 unsigned long pfn;int error=0;
 if(!dev||!pd||!pd->driver||!out)return -EINVAL;
 if(PAGE_SHIFT!=12||PAGE_SIZE!=4096)return -EOPNOTSUPP;
 down_read(&pd->driver->sem);
 if(pd->driver->dev!=dev||pd->driver->default_pd!=pd||pd->hw_context!=0||
    !pd->p||!pd->tables||pd->pd_mask!=PSB_PTE_VALID||
    !pd->driver->has_clflush||atomic_read(&pd->driver->needs_tlbflush)){
  error=-EACCES;goto done;
 }
 pfn=page_to_pfn(pd->p);
 if(!pfn||pfn>0xfffffUL){error=-ERANGE;goto done;}
 *out=(u32)pfn<<12;
 done:up_read(&pd->driver->sem);return error;
}
