/* SPDX-License-Identifier: GPL-2.0-only */
/* Include after the original MMU types/functions and confinement auditor. */
#include "remesa_mmu_private.h"
#ifdef __KERNEL__
#include <linux/slab.h>
#include <linux/vmalloc.h>
#endif
struct remesa_mmu_private {
 struct psb_mmu_driver *driver;
 struct psb_mmu_pd *pd;
 struct remesa_mmu_allowed_binding bindings[REMESA_MMU_AUDIT_BINDINGS];
 struct page **pages;
 u32 count,held;
};
#if defined(CONFIG_X86)
static void remesa_mmu_private_sync_page(struct page *page,u32 stride)
{
 unsigned char *v=(void *)kmap_atomic(page);u32 offset;
 for(offset=0;offset<PAGE_SIZE;offset+=stride)psb_clflush(v+offset);
 kunmap_atomic(v);
}
#endif
int remesa_mmu_private_sync_tables(struct remesa_mmu_private *s)
{
#if defined(CONFIG_X86)
 u32 stride,i,*workspace;int ret;
 if(!s||!s->driver||!s->pd)return -EINVAL;
 /* Original driver stores the VA advance for one PTE cache line, not
  * a byte count: PAGE_SIZE * CPUID_line_bytes / sizeof(u32). */
 if(!s->driver->has_clflush||s->driver->clflush_add<=0||
    s->driver->clflush_add%(PAGE_SIZE/sizeof(u32)))return -EOPNOTSUPP;
 stride=s->driver->clflush_add/(PAGE_SIZE/sizeof(u32));
 if(stride<sizeof(u32)||stride>PAGE_SIZE||(stride&(stride-1)))return -EOPNOTSUPP;
 workspace=kvcalloc(s->held+REMESA_MMU_AUDIT_OVERHEAD_WORDS,
                    sizeof(*workspace),GFP_KERNEL);
 if(!workspace)return -ENOMEM;
 ret=psb_mmu_audit_unbound_pd(s->pd,s->bindings,s->count,workspace,
                            s->held+REMESA_MMU_AUDIT_OVERHEAD_WORDS);
 kvfree(workspace);if(ret)return ret;
 /* Caller exclusion spans audit and cache maintenance; no other thread
  * may mutate/release this owner. Semaphore additionally excludes MMU writers. */
 down_write(&s->driver->sem);
 if(s->pd->hw_context!=-1){ret=-EBUSY;goto done_sync;}
 mb();
 for(i=0;i<1024;i++)if(s->pd->tables[i])
  remesa_mmu_private_sync_page(s->pd->tables[i]->p,stride);
 remesa_mmu_private_sync_page(s->pd->dummy_pt,stride);
 remesa_mmu_private_sync_page(s->pd->dummy_page,stride);
 remesa_mmu_private_sync_page(s->pd->p,stride);
 mb();
 done_sync:up_write(&s->driver->sem);return ret;
#else
 (void)s;return -EOPNOTSUPP;
#endif
}
int remesa_mmu_private_release(struct remesa_mmu_private **owner)
{
 struct remesa_mmu_private *s;u32 i;int bound;
 if(!owner)return -EINVAL;
 s=*owner;if(!s)return 0;
 if(s->pd){
  down_write(&s->driver->sem);
  bound=s->pd->hw_context!=-1;
  up_write(&s->driver->sem);
  if(bound)return -EBUSY;
  /* Device/caller exclusion spans this check and the original destructor.
   * That destructor performs MMIO for bound directories: never pass one. */
  psb_mmu_free_pagedir(s->pd);
 }
 for(i=0;i<s->held;i++)put_page(s->pages[i]);
 kvfree(s->pages);kfree(s);*owner=NULL;return 0;
}
int remesa_mmu_private_prepare(struct psb_mmu_driver *driver,
 const struct remesa_mmu_allowed_binding *b,u32 count,
 struct remesa_mmu_private **out)
{
 struct remesa_mmu_private *s;u32 total=0,i,j,offset=0,*workspace=NULL;
 u64 previous=0,end;int ret,drop;
 if(!driver||!b||!count||!out||*out)return -EINVAL;
 if(PAGE_SHIFT!=12||PAGE_SIZE!=4096)return -EOPNOTSUPP;
 if(count>REMESA_MMU_AUDIT_BINDINGS)return -E2BIG;
 for(i=0;i<count;i++){
  if(!b[i].pages||!b[i].page_count||b[i].address>0xffffffffULL||
     (b[i].address&4095)||!b[i].access||b[i].access>3||b[i].cached>1)return -EINVAL;
  if((u64)b[i].page_count>((0x100000000ULL-b[i].address)>>12))return -ERANGE;
  end=b[i].address+((u64)b[i].page_count<<12);
  if(i&&b[i].address<previous)return -EINVAL;
  previous=end;
  if(b[i].page_count>0x100000u-total)return -E2BIG;
  total+=b[i].page_count;
 }
 s=kzalloc(sizeof(*s),GFP_KERNEL);if(!s)return -ENOMEM;
 s->driver=driver;s->count=count;
 s->pages=kvcalloc(total,sizeof(*s->pages),GFP_KERNEL);
 if(!s->pages){ret=-ENOMEM;goto fail;}
 workspace=kvcalloc(total+REMESA_MMU_AUDIT_OVERHEAD_WORDS,sizeof(*workspace),GFP_KERNEL);
 if(!workspace){ret=-ENOMEM;goto fail;}
 for(i=0;i<count;i++){
  s->bindings[i]=b[i];s->bindings[i].pages=s->pages+offset;
  for(j=0;j<b[i].page_count;j++){
   struct page *page=b[i].pages[j];
   if(!page){ret=-EINVAL;goto fail;}
   if(page_to_pfn(page)>0xfffffUL){ret=-ERANGE;goto fail;}
   get_page(page);s->pages[s->held++]=page;
  }
  offset+=b[i].page_count;
 }
 s->pd=psb_mmu_alloc_pd(driver,1,0);
 if(!s->pd){ret=-ENOMEM;goto fail;}
 if(s->pd->hw_context!=-1){ret=-EBUSY;goto fail;}
 for(i=0;i<count;i++){
  const struct remesa_mmu_allowed_binding *binding=&s->bindings[i];
  u64 va=binding->address;u32 remaining=binding->page_count,done=0;
  int type=binding->cached?PSB_MMU_CACHED_MEMORY:0;
  if(binding->access==REMESA_MMU_GPU_READ)type|=PSB_MMU_RO_MEMORY;
  if(binding->access==REMESA_MMU_GPU_WRITE)type|=PSB_MMU_WO_MEMORY;
  while(remaining){
   /* Bound each original insert to one PDE, including the last 32-bit VA
    * chunk. Avoid an address+size wrap spanning the entire 4GiB space. */
   u32 chunk=1024u-((u32)(va>>12)&1023u);
   if(chunk>remaining)chunk=remaining;
   ret=psb_mmu_insert_pages(s->pd,binding->pages+done,(unsigned long)va,
                          chunk,0,0,type);
   if(ret)goto fail;
   done+=chunk;remaining-=chunk;va+=(u64)chunk<<12;
  }
 }
 ret=psb_mmu_audit_unbound_pd(s->pd,s->bindings,count,workspace,
                            total+REMESA_MMU_AUDIT_OVERHEAD_WORDS);
 if(ret)goto fail;
 kvfree(workspace);*out=s;return 0;
 fail:
 kvfree(workspace);drop=remesa_mmu_private_release(&s);
 if(drop){*out=s;return -EUCLEAN;}
 return ret;
}
