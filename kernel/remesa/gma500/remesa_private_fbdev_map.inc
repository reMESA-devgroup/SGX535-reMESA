/* SPDX-License-Identifier: GPL-2.0-only */
static bool remesa_private_fb_enter(struct fb_info *info,struct drm_device **dev,int *index)
{
 struct drm_fb_helper *helper=info?info->par:NULL;
 if(!helper||!helper->dev)return false;
 *dev=helper->dev;return drm_dev_enter(*dev,index);
}
/* Called only inside a DRM entry interval, with an owned GEM reference. */
static bool remesa_private_fb_backing(struct drm_device *dev,struct drm_gem_object *obj,u64 *base)
{
 struct drm_psb_private *p=dev->dev_private;
 struct gtt_range *range;
 if(!p||!obj||obj->dev!=dev||!obj->size||(obj->size&(PAGE_SIZE-1)))return false;
 range=to_gtt_range(obj);
 if(!range->stolen||!range->private_tracked||(range->offset&(PAGE_SIZE-1)))return false;
 if(range->offset>p->vram_stolen_size||obj->size>p->vram_stolen_size-range->offset)return false;
 *base=(u64)p->stolen_base+range->offset;
 /* This qualified target has a 32-bit physical address contract. */
 return !(*base&(PAGE_SIZE-1))&&*base<0x100000000ULL&&obj->size<=0x100000000ULL-*base;
}
static vm_fault_t remesa_private_fb_fault(struct vm_fault *vmf)
{
 struct vm_area_struct *vma=vmf->vma;
 struct drm_gem_object *obj=vma->vm_private_data;
 struct drm_device *dev;u64 base,pages,index;int entry;
 vm_fault_t result=VM_FAULT_SIGBUS;
 if(!obj||!obj->dev)return result;
 dev=obj->dev;if(!drm_dev_enter(dev,&entry))return result;
 if(vma->vm_end<=vma->vm_start||vmf->address<vma->vm_start||vmf->address>=vma->vm_end)goto out;
 if(!remesa_private_fb_backing(dev,obj,&base))goto out;
 pages=obj->size>>PAGE_SHIFT;
 if(vma->vm_pgoff>=pages||((vma->vm_end-vma->vm_start)&(PAGE_SIZE-1))||
    ((vma->vm_end-vma->vm_start)>>PAGE_SHIFT)>pages-vma->vm_pgoff)goto out;
 index=(u64)vma->vm_pgoff+((vmf->address-vma->vm_start)>>PAGE_SHIFT);
 if(index>=pages||vmf->pgoff!=index)goto out;
 vma->vm_page_prot=pgprot_noncached(vma->vm_page_prot);
 result=vmf_insert_mixed(vma,vmf->address,
        __pfn_to_pfn_t((unsigned long)((base>>PAGE_SHIFT)+index),PFN_DEV));
out:
 drm_dev_exit(entry);return result;
}
static void remesa_private_fb_vm_open(struct vm_area_struct *vma)
{if(vma->vm_private_data)drm_gem_object_get(vma->vm_private_data);}
static void remesa_private_fb_vm_close(struct vm_area_struct *vma)
{
 struct drm_gem_object *obj=vma->vm_private_data;
 vma->vm_private_data=NULL;if(obj)drm_gem_object_put(obj);
}
static const struct vm_operations_struct remesa_private_fb_vm_ops={
 .fault=remesa_private_fb_fault,.open=remesa_private_fb_vm_open,.close=remesa_private_fb_vm_close
};
static int remesa_private_fb_mmap(struct fb_info *info,struct vm_area_struct *vma)
{
 struct drm_device *dev;struct drm_fb_helper *helper;
 struct drm_framebuffer *fb;struct drm_gem_object *obj;u64 base;int entry,error=-EINVAL;
 if(!remesa_private_fb_enter(info,&dev,&entry))return -ENODEV;
 helper=info->par;fb=helper->fb;
 if(!vma||vma->vm_pgoff||vma->vm_end<=vma->vm_start||!fb)goto out;
 obj=fb->obj[0];
 if(!remesa_private_fb_backing(dev,obj,&base)||
    ((vma->vm_end-vma->vm_start)&(PAGE_SIZE-1))||vma->vm_end-vma->vm_start>obj->size)goto out;
 drm_gem_object_get(obj);
 vma->vm_ops=&remesa_private_fb_vm_ops;vma->vm_private_data=obj;
 vma->vm_page_prot=pgprot_noncached(vma->vm_page_prot);
 vma->vm_flags|=VM_IO|VM_MIXEDMAP|VM_DONTEXPAND|VM_DONTDUMP;
 error=0;
out:
 drm_dev_exit(entry);return error;
}
