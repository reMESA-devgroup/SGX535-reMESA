/* SPDX-License-Identifier: GPL-2.0-only */
/* Resume reuses established mappings. Failure never destroys their owners. */
static int remesa_private_gtt_resume(struct drm_device *dev)
{
 struct drm_psb_private *dev_priv=dev->dev_private;
 struct psb_gtt *pg=&dev_priv->gtt;
 struct resource *root=dev_priv->gtt_mem;
 resource_size_t table_start,table_pages,aperture_start,aperture_pages;
 u32 pge_ctl,stolen_base,pte;
 u16 gmch_ctrl;
 u64 table_entries,stolen_size,pfn;
 unsigned int i,num_pages;
 int ret;

 if(!dev_priv->gtt_initialized || !dev_priv->gtt_map ||
    !dev_priv->vram_addr || !dev_priv->scratch_page || !root)
  return -ENODEV;
 ret=pcibios_err_to_errno(pci_read_config_word(dev->pdev,PSB_GMCH_CTRL,&gmch_ctrl));
 if(ret)return ret;
 ret=pcibios_err_to_errno(pci_read_config_dword(dev->pdev,PSB_BSM,&stolen_base));
 if(ret)return ret;
 pge_ctl=PSB_RVDC32(PSB_PGETBL_CTL);
 table_start=pci_resource_start(dev->pdev,PSB_GTT_RESOURCE);
 table_pages=pci_resource_len(dev->pdev,PSB_GTT_RESOURCE)>>PAGE_SHIFT;
 if(!table_start || !table_pages){table_start=pge_ctl;table_pages=64;}
 aperture_start=pci_resource_start(dev->pdev,PSB_GATT_RESOURCE);
 aperture_pages=pci_resource_len(dev->pdev,PSB_GATT_RESOURCE)>>PAGE_SHIFT;
 if(!aperture_start || !aperture_pages){aperture_start=0x40000000;aperture_pages=(128*1024*1024)>>PAGE_SHIFT;}

 /* Every property describes an existing mapping/resource graph. A change in
  * either size, base or aperture requires rebuilding ownership, not replay. */
 if((pge_ctl & PAGE_MASK)!=pg->gtt_phys_start || stolen_base!=dev_priv->stolen_base ||
    (table_start & PAGE_MASK)!=(pg->gtt_start & PAGE_MASK) ||
    (u64)table_start>0xffffffffU || table_pages>(0xffffffffU>>PAGE_SHIFT) ||
    table_pages!=pg->gtt_pages || aperture_start!=pg->gatt_start ||
    aperture_pages!=pg->gatt_pages || !table_pages ||
    root->start!=aperture_start || root->end<root->start ||
    (u64)(root->end-root->start)+1!=((u64)aperture_pages<<PAGE_SHIFT) ||
    (u64)pg->gtt_phys_start<(u64)stolen_base+PAGE_SIZE)
  return -EINVAL;
 stolen_size=(u64)pg->gtt_phys_start-stolen_base-PAGE_SIZE;
 table_entries=((u64)table_pages<<PAGE_SHIFT)/sizeof(u32);
 if(stolen_size!=pg->stolen_size || stolen_size!=dev_priv->vram_stolen_size ||
    (stolen_size & (PAGE_SIZE-1)) ||
    (stolen_size>>PAGE_SHIFT)>table_entries ||
    (stolen_size>>PAGE_SHIFT)>aperture_pages)
  return -EINVAL;
 pfn=page_to_pfn(dev_priv->scratch_page);
 if(pfn>(0xffffffffU>>PAGE_SHIFT))return -ERANGE;

 /* Preserve saved initial gmch_ctrl/pge_ctl and all mapping identities. */
 ret=pcibios_err_to_errno(pci_write_config_word(dev->pdev,PSB_GMCH_CTRL,
                                             gmch_ctrl|_PSB_GMCH_ENABLED));
 if(ret)return ret;
 PSB_WVDC32(pge_ctl|_PSB_PGETBL_ENABLED,PSB_PGETBL_CTL);
 (void)PSB_RVDC32(PSB_PGETBL_CTL);
 num_pages=stolen_size>>PAGE_SHIFT;
 for(i=0;i<num_pages;i++){
  pte=psb_gtt_mask_pte((stolen_base>>PAGE_SHIFT)+i,PSB_MMU_CACHED_MEMORY);
  iowrite32(pte,dev_priv->gtt_map+i);
 }
 /* Retain the original successful initializer's exact scratch-write extent.
  * table_pages is a count of mapped pages, not the entry capacity above. */
 pte=psb_gtt_mask_pte(pfn,PSB_MMU_CACHED_MEMORY);
 for(;i<pg->gtt_pages;i++)iowrite32(pte,dev_priv->gtt_map+i);
 (void)ioread32(dev_priv->gtt_map+i-1);
 return 0;
}

int psb_gtt_init(struct drm_device *dev,int resume)
{
 if(resume)return remesa_private_gtt_resume(dev);
 return remesa_private_gtt_init_live(dev,0);
}

int psb_gtt_restore(struct drm_device *dev)
{
 struct drm_psb_private *dev_priv=dev->dev_private;
 struct resource *r;
 struct gtt_range *range;
 u64 entries,offset;
 int ret;
 mutex_lock(&dev_priv->gtt_mutex);
 if(!dev_priv->gtt_mem){ret=-ENODEV;goto out;}
 entries=((u64)dev_priv->gtt.gtt_pages<<PAGE_SHIFT)/sizeof(u32);
 /* Validate every existing range before touching any table entries. */
 for(r=dev_priv->gtt_mem->child;r;r=r->sibling){
  range=container_of(r,struct gtt_range,resource);
  if(!range->pages)continue;
  if(range->npage<=0 || range->roll<0 || range->roll>=range->npage ||
     r->start<dev_priv->gtt_mem->start || r->end<r->start ||
     r->end>dev_priv->gtt_mem->end || (r->start & (PAGE_SIZE-1)) ||
     ((r->end-r->start+1) & (PAGE_SIZE-1)) ||
     ((r->end-r->start+1)>>PAGE_SHIFT)!=(u64)range->npage){ret=-EINVAL;goto out;}
  offset=(r->start-dev_priv->gtt_mem->start)>>PAGE_SHIFT;
  if(offset>=entries || (u64)range->npage>entries-offset){ret=-EINVAL;goto out;}
  for(ret=0;ret<range->npage;ret++)
   if(!range->pages[ret] || page_to_pfn(range->pages[ret])>(0xffffffffU>>PAGE_SHIFT)){
    ret=-ERANGE;goto out;
   }
 }
 ret=psb_gtt_init(dev,1);
 if(ret)goto out;
 for(r=dev_priv->gtt_mem->child;r;r=r->sibling){
  range=container_of(r,struct gtt_range,resource);
  if(!range->pages)continue;
  ret=psb_gtt_insert(dev,range,1);
  if(ret)goto out;
 }
out:
 mutex_unlock(&dev_priv->gtt_mutex);
 return ret;
}
