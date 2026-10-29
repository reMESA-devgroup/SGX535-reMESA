/* SPDX-License-Identifier: GPL-2.0-only */
/* Include after the original mmu.c functions and private owner. */
#include "remesa_mmu_switch.h"
#include "frozen_source_guard.h"
#ifdef __KERNEL__
#include "power.h"
#endif
#if defined(CONFIG_X86)
static int remesa_mmu_switch_quiet(struct remesa_mmu_private *s,u32 root)
{
 struct drm_psb_private *dev_priv=s->driver->dev->dev_private;
 if(!dev_priv||!dev_priv->sgx_reg||dev_priv->mmu!=s->driver||
    !gma_power_is_on(s->driver->dev))return -ENODEV;
 if(PSB_RSGX32(PSB_CR_CORE_ID)!=0x01130000u||
    PSB_RSGX32(PSB_CR_CORE_REVISION)!=0x00010201u)return -ENODEV;
 if(PSB_RSGX32(PSB_CR_BIF_DIR_LIST_BASE0)!=root||
    PSB_RSGX32(PSB_CR_BIF_BANK0)||PSB_RSGX32(PSB_CR_BIF_BANK1)||
    (PSB_RSGX32(PSB_CR_BIF_CTRL)&(_PSB_MMU_ER_MASK|
     _PSB_CB_CTRL_FLUSH|_PSB_CB_CTRL_INVALDC|_PSB_CB_CTRL_CLEAR_FAULT))||
    PSB_RSGX32(PSB_CR_SOFT_RESET)||PSB_RSGX32(PSB_CR_BIF_INT_STAT)||
    (PSB_RSGX32(PSB_CR_EVENT_STATUS)&~(1u<<31))||
    PSB_RSGX32(PSB_CR_EVENT_STATUS2)||
    PSB_RSGX32(PSB_CR_2D_SOCIF)!=_PSB_C2_SOCIF_EMPTY||
    (PSB_RSGX32(PSB_CR_2D_BLIT_STATUS)&_PSB_C2B_STATUS_BUSY)||
    (PSB_RSGX32(SGX535_SOURCE_BIF_READS_REG)&SGX535_SOURCE_BIF_READS_MASK)||
    (PSB_RSGX32(SGX535_SOURCE_EVENT_TIMER_REG)&SGX535_SOURCE_EVENT_TIMER_ENABLE)||
    (PSB_RSGX32(SGX535_SOURCE_EVENT_KICK_REG)&SGX535_SOURCE_EVENT_KICK_NOW))
  return -EBUSY;
 return 0;
}
static int remesa_mmu_switch_root(struct page *p,u32 *root)
{
 unsigned long pfn;
 if(!p)return -EINVAL;
 pfn=page_to_pfn(p);if(!pfn||pfn>0xfffffUL)return -ERANGE;
 *root=(u32)pfn<<12;return 0;
}
/* Caller holds the MMU write semaphore and whole-device exclusion. Verify
 * the actual PDE tree, not only the CPU table-pointer array: otherwise cache
 * maintenance could miss a page the restored GPU root will dereference. */
static int remesa_mmu_restore_tree(struct remesa_mmu_private *s,
 struct psb_mmu_pd *pd,u32 expected_root)
{
 u32 root,dummy_pt,dummy_page,i,*v,expected;int ret=0;
 if(!pd||pd->driver!=s->driver||!pd->tables||pd->pd_mask!=PSB_PTE_VALID)
  return -EINVAL;
 ret=remesa_mmu_switch_root(pd->p,&root);if(ret)return ret;
 if(root!=expected_root)return -ESTALE;
 ret=remesa_mmu_switch_root(pd->dummy_pt,&dummy_pt);if(ret)return ret;
 ret=remesa_mmu_switch_root(pd->dummy_page,&dummy_page);if(ret)return ret;
 /* Original alloc_pd uses either trap entries0/0 or the two owned dummy
  * frames with invalid_type0. Other default-directory modes need proof. */
 if((pd->invalid_pde||pd->invalid_pte)&&
    (pd->invalid_pde!=(dummy_pt|PSB_PTE_VALID)||
     pd->invalid_pte!=(dummy_page|PSB_PTE_VALID)))return -EOPNOTSUPP;
 for(i=0;i<1024;i++)if(pd->tables[i]){
  if(pd->tables[i]->pd!=pd||pd->tables[i]->index!=i)return -EINVAL;
  ret=remesa_mmu_switch_root(pd->tables[i]->p,&root);if(ret)return ret;
 }
 v=kmap_atomic(pd->p);
 for(i=0;i<1024;i++){
  expected=pd->invalid_pde;
  if(pd->tables[i])expected=((u32)page_to_pfn(pd->tables[i]->p)<<12)|pd->pd_mask;
  if(v[i]!=expected){ret=-ESTALE;break;}
 }
 kunmap_atomic(v);if(ret)return ret;
 v=kmap_atomic(pd->dummy_pt);
 for(i=0;i<1024;i++)if(v[i]!=pd->invalid_pte){ret=-ESTALE;break;}
 kunmap_atomic(v);return ret;
}
/* Preflight completes BEFORE any cache maintenance or root publication. */
static int remesa_mmu_sync_restore(struct remesa_mmu_private *s)
{
 struct psb_mmu_pd *pd=s->restore_pd;u32 stride,i;int ret;
 if(!s->driver->has_clflush||s->driver->clflush_add<=0||
    s->driver->clflush_add%(PAGE_SIZE/sizeof(u32)))return -EOPNOTSUPP;
 stride=s->driver->clflush_add/(PAGE_SIZE/sizeof(u32));
 if(stride<sizeof(u32)||stride>PAGE_SIZE||(stride&(stride-1)))return -EOPNOTSUPP;
 ret=remesa_mmu_restore_tree(s,pd,s->restore_root);if(ret)return ret;
 mb();
 for(i=0;i<1024;i++)if(pd->tables[i])
  remesa_mmu_private_sync_page(pd->tables[i]->p,stride);
 remesa_mmu_private_sync_page(pd->dummy_pt,stride);
 remesa_mmu_private_sync_page(pd->dummy_page,stride);
 remesa_mmu_private_sync_page(pd->p,stride);mb();
 return 0;
}
#endif
int remesa_mmu_private_bind_locked(struct remesa_mmu_private *s)
{
#if defined(CONFIG_X86)
 struct psb_mmu_pd *def;struct drm_psb_private *dev_priv;
 u32 root,restore;int ret;
 if(!s||!s->driver||!s->driver->dev||!s->pd)return -EINVAL;
#ifdef __KERNEL__
 lockdep_assert_held_write(&s->driver->sem);
#endif
 if(s->root_active||s->root_uncertain)return -EBUSY;
#ifdef REMESA_PRIVATE_IRQ_HANDOFF
 if(s->driver->remesa_private_owner)return -EBUSY;
#endif
 ret=remesa_mmu_private_sync_tables_locked(s);if(ret)return ret;
 def=s->driver->default_pd;
 if(!def||def==s->pd||def->driver!=s->driver||def->hw_context!=0||
    s->pd->hw_context!=-1||atomic_read(&s->driver->needs_tlbflush)){
  ret=-EBUSY;goto bind_done;
 }
 ret=remesa_mmu_switch_root(s->pd->p,&root);if(ret)goto bind_done;
 ret=remesa_mmu_switch_root(def->p,&restore);if(ret)goto bind_done;
 ret=remesa_mmu_restore_tree(s,def,restore);if(ret)goto bind_done;
 ret=remesa_mmu_switch_quiet(s,restore);if(ret)goto bind_done;
 dev_priv=s->driver->dev->dev_private;
 s->restore_pd=def;s->restore_root=restore;s->private_root=root;
 s->restore_ctrl=PSB_RSGX32(PSB_CR_BIF_CTRL);
 /* Retention is established BEFORE the first possibly effective MMIO write. */
 s->root_active=1;s->pd->hw_context=0;def->hw_context=-1;
#ifdef REMESA_PRIVATE_IRQ_HANDOFF
 s->driver->remesa_private_owner=s;
#endif
 PSB_WSGX32(root,PSB_CR_BIF_DIR_LIST_BASE0);wmb();
 psb_mmu_flush_pd_locked(s->driver,1);
 ret=remesa_mmu_switch_quiet(s,root);
 if(ret||PSB_RSGX32(PSB_CR_BIF_CTRL)!=s->restore_ctrl){
  s->root_uncertain=1;ret=-EIO;
 }
 bind_done:return ret;
#else
 (void)s;return -EOPNOTSUPP;
#endif
}
int remesa_mmu_private_restore_locked(struct remesa_mmu_private *s)
{
#if defined(CONFIG_X86)
 struct drm_psb_private *dev_priv;int ret;
 if(!s||!s->driver||!s->driver->dev||!s->pd)return -EINVAL;
#ifdef __KERNEL__
 lockdep_assert_held_write(&s->driver->sem);
#endif
 if(s->root_uncertain)return -EIO;
 if(!s->root_active||!s->restore_pd)return -ENOENT;
#ifdef REMESA_PRIVATE_IRQ_HANDOFF
 if(s->driver->remesa_private_owner!=s){ret=-ESTALE;goto restore_failed;}
#endif
 if(s->driver->default_pd!=s->restore_pd||s->restore_pd->driver!=s->driver||
    s->restore_pd->hw_context!=-1||s->pd->hw_context!=0){
  ret=-ESTALE;goto restore_failed;
 }
 ret=remesa_mmu_switch_quiet(s,s->private_root);if(ret)goto restore_failed;
 dev_priv=s->driver->dev->dev_private;
 if(PSB_RSGX32(PSB_CR_BIF_CTRL)!=s->restore_ctrl){ret=-ESTALE;goto restore_failed;}
 ret=remesa_mmu_sync_restore(s);if(ret)goto restore_failed;
 PSB_WSGX32(s->restore_root,PSB_CR_BIF_DIR_LIST_BASE0);wmb();
 psb_mmu_flush_pd_locked(s->driver,1);
 ret=remesa_mmu_switch_quiet(s,s->restore_root);
 if(ret||PSB_RSGX32(PSB_CR_BIF_CTRL)!=s->restore_ctrl){ret=-EIO;goto restore_failed;}
 s->restore_pd->hw_context=0;s->pd->hw_context=-1;
 s->restore_pd=NULL;s->root_active=0;
#ifdef REMESA_PRIVATE_IRQ_HANDOFF
 s->driver->remesa_private_owner=NULL;
#endif
 return 0;
 restore_failed:
 /* Do not guess which root may be visible, retry, reset or free tables. */
 s->root_uncertain=1;return ret;
#else
 (void)s;return -EOPNOTSUPP;
#endif
}

int remesa_mmu_private_bind_exclusive(struct remesa_mmu_private *s)
{
 int ret;
 if(!s||!s->driver||!s->driver->dev||!s->pd)return -EINVAL;
 down_write(&s->driver->sem);ret=remesa_mmu_private_bind_locked(s);
 up_write(&s->driver->sem);return ret;
}
int remesa_mmu_private_restore_exclusive(struct remesa_mmu_private *s)
{
 int ret;
 if(!s||!s->driver||!s->driver->dev||!s->pd)return -EINVAL;
 down_write(&s->driver->sem);ret=remesa_mmu_private_restore_locked(s);
 up_write(&s->driver->sem);return ret;
}

int remesa_mmu_private_maintain_locked(struct remesa_mmu_private *s)
{
#if defined(CONFIG_X86) && defined(REMESA_PRIVATE_IRQ_HANDOFF)
 struct drm_psb_private *dev_priv;int ret;
 if(!s||!s->driver||!s->driver->dev||!s->pd)return -EINVAL;
#ifdef __KERNEL__
 lockdep_assert_held_write(&s->driver->sem);
#endif
 if(s->root_uncertain)return -EIO;
 if(!s->root_active||!s->restore_pd)return -ENOENT;
 if(s->driver->remesa_private_owner!=s){ret=-ESTALE;goto failed;}
 if(s->driver->default_pd!=s->restore_pd||s->restore_pd->driver!=s->driver||
    s->restore_pd->hw_context!=-1||s->pd->hw_context!=0){ret=-ESTALE;goto failed;}
 ret=remesa_mmu_switch_quiet(s,s->private_root);if(ret)goto failed;
 dev_priv=s->driver->dev->dev_private;
 if(PSB_RSGX32(PSB_CR_BIF_CTRL)!=s->restore_ctrl){ret=-ESTALE;goto failed;}
 /* Factored verbatim hardware body; original wrapper still owns its lock.
  * Calling that wrapper here would recursively acquire the MMU semaphore. */
 remesa_mmu_original_flush_locked(s->driver);
 ret=remesa_mmu_switch_quiet(s,s->private_root);
 if(ret||PSB_RSGX32(PSB_CR_BIF_CTRL)!=s->restore_ctrl){ret=-EIO;goto failed;}
 return 0;
 failed:s->root_uncertain=1;return ret;
#else
 (void)s;return -EOPNOTSUPP;
#endif
}

int remesa_mmu_private_validate_bound_locked(struct remesa_mmu_private *s,
 const struct remesa_mmu_allowed_binding *expected,u32 count)
{
#if defined(CONFIG_X86)
 struct drm_psb_private *dev_priv;u32 i,j;int ret;
 if(!s||!s->driver||!s->driver->dev||!s->pd||!expected)return -EINVAL;
#ifdef __KERNEL__
 lockdep_assert_held_write(&s->driver->sem);
#endif
 if(count>REMESA_MMU_AUDIT_BINDINGS)return -E2BIG;
 if(!count||count!=s->count)return -EACCES;
 for(i=0;i<count;i++){
  const struct remesa_mmu_allowed_binding *a=&expected[i],*b=&s->bindings[i];
  if(a->address!=b->address||a->page_count!=b->page_count||a->access!=b->access||
     a->cached!=b->cached||!a->pages)return -EACCES;
  for(j=0;j<a->page_count;j++)if(a->pages[j]!=b->pages[j])return -EACCES;
 }
 if(s->root_uncertain)return -EIO;
 if(!s->root_active||!s->restore_pd)return -ENOENT;
#ifdef REMESA_PRIVATE_IRQ_HANDOFF
 if(s->driver->remesa_private_owner!=s){ret=-ESTALE;goto failed;}
#endif
 if(s->driver->default_pd!=s->restore_pd||s->restore_pd->driver!=s->driver||
    s->restore_pd->hw_context!=-1||s->pd->hw_context!=0){ret=-ESTALE;goto failed;}
 ret=remesa_mmu_switch_quiet(s,s->private_root);if(ret)goto failed;
 dev_priv=s->driver->dev->dev_private;
 if(PSB_RSGX32(PSB_CR_BIF_CTRL)!=s->restore_ctrl){ret=-ESTALE;goto failed;}
 return 0;
 failed:s->root_uncertain=1;return ret;
#else
 (void)s;(void)expected;(void)count;return -EOPNOTSUPP;
#endif
}
