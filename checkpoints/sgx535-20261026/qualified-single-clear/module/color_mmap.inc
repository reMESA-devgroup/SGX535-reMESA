/* Read-only persistent VMA; access is permitted only inside a successful
 * existing CPU READ lease, after trusted GPU publication and before UNLEASE.
 * VMA->vm_file holds the DRM file; VMA counts prevent object destruction.
 * No imported pages, arbitrary PFNs, CODE/input mapping or CPU write access. */
static bool color_readable(struct remesa_gem_memory *s,struct backing *b)
{
 unsigned i;
 if(!memory_enabled || !b->live || !b->pinned || !b->reserved ||
    b->quarantine || !b->lease || b->write || b->holds ||
    b->gem.size!=REMESA_COLOR_VIEW_BYTES ||
    b->va.start!=REMESA_COLOR_VIEW_GPU_VA ||
    b->inserted!=REMESA_COLOR_VIEW_PAGES)return false;
 for(i=0;i<8;i++)if(s->jobs[i].live)return false;
 return true;
}
static void color_vma_open(struct vm_area_struct *vma)
{
 struct backing *b=vma->vm_private_data;
 atomic_inc(&b->color_vmas);
}
static void color_vma_close(struct vm_area_struct *vma)
{
 struct backing *b=vma->vm_private_data;
 atomic_dec(&b->color_vmas);
}
static vm_fault_t color_fault(struct vm_fault *vmf)
{
 struct vm_area_struct *vma=vmf->vma;
 struct backing *b=vma->vm_private_data;
 struct remesa_gem_memory *s=b->memory;
 unsigned long index;
 vm_fault_t result=VM_FAULT_SIGBUS;
 if(vmf->pgoff<REMESA_COLOR_VIEW_PGOFF)return result;
 index=vmf->pgoff-REMESA_COLOR_VIEW_PGOFF;
 if(index>=REMESA_COLOR_VIEW_PAGES || (vmf->flags&FAULT_FLAG_WRITE))return result;
 mutex_lock(&s->lock);
 if(color_readable(s,b)) {
  /* Install under the lease lock. Returning vmf->page would let UNLEASE zap
   * first and the core install a stale PTE after revocation. VM_MIXEDMAP was
   * set in mmap, as Linux5.10 requires for vm_insert_page from a fault. */
  result=vmf_insert_page(vma,vmf->address,b->pages[index]);
 }
 mutex_unlock(&s->lock);return result;
}
static const struct vm_operations_struct color_vm_ops={
 .open=color_vma_open,.close=color_vma_close,.fault=color_fault,
};
int remesa_gem_color_mmap(struct file *filp,struct vm_area_struct *vma)
{
 struct drm_file *file=filp->private_data;
 struct remesa_gem_memory *s;
 struct backing *b=NULL;
 unsigned i;
 int e=-EINVAL;
 /* Preserve the original GEM mmap path for every other namespace. Offsets
  * within our reserved range never fall through into a different BO. */
 if(vma->vm_pgoff<REMESA_COLOR_VIEW_PGOFF ||
    vma->vm_pgoff>=REMESA_COLOR_VIEW_PGOFF+REMESA_COLOR_VIEW_PAGES)
  return drm_gem_mmap(filp,vma);
 if(!file || !capable(CAP_SYS_ADMIN))return -EPERM;
 if(vma->vm_pgoff!=REMESA_COLOR_VIEW_PGOFF ||
    vma->vm_end-vma->vm_start!=REMESA_COLOR_VIEW_BYTES ||
    !(vma->vm_flags&VM_SHARED) || !(vma->vm_flags&VM_READ) ||
    (vma->vm_flags&(VM_WRITE|VM_EXEC)))return -EINVAL;
 mutex_lock(&endpoint_lock);s=endpoint;
 if(!s || s->file!=file){e=-EACCES;goto done;}
 mutex_lock(&s->lock);
 for(i=0;i<OBJECTS;i++)if(s->bo[i].live && s->bo[i].reserved &&
    s->bo[i].va.start==REMESA_COLOR_VIEW_GPU_VA){b=&s->bo[i];break;}
 if(!b || !color_readable(s,b)){e=-EBUSY;goto unlock;}
 if(b->color_mapping && b->color_mapping!=filp->f_mapping){e=-EACCES;goto unlock;}
 b->color_mapping=filp->f_mapping;
 vma->vm_flags&=~(VM_MAYWRITE|VM_MAYEXEC);
 vma->vm_flags|=VM_DONTEXPAND|VM_DONTDUMP|VM_MIXEDMAP;
 vma->vm_private_data=b;vma->vm_ops=&color_vm_ops;
 color_vma_open(vma);e=0;
unlock:mutex_unlock(&s->lock);
done:mutex_unlock(&endpoint_lock);return e;
}
