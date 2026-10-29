/* SPDX-License-Identifier: GPL-2.0-only */
/* Included only by the staged private fixed backend. No ioctl/registration. */
#include "remesa_gem_private.h"
#include "remesa_mmu_switch.h"
#include "remesa_twod_reservation.h"

static int remesa_fixed_private_scene(struct sgx535_gma500_owner *o,
 struct remesa_gem_private *loan,const struct remesa_memory_view *views,unsigned count)
{
 unsigned i;
 if(!o||!o->dev||!o->dev->dev_private||!o->borrowed_backing||!loan||!loan->guard||!views||
    count!=SGX535_BO_COUNT||loan->guard->device!=o->dev->dev_private||
    loan->guard->device->dev!=o->dev)return -EACCES;
 /* Match the borrowed scene to the exact job closure before invoking any
  * publisher. LOCAL descriptors must remain outside the private GPU tree. */
 for(i=0;i<count;i++){
  const struct sgx535_frozen_bo *d=&o->descriptors[i];
  const struct sgx535_gma500_bo *b=&o->objects[i];
  const struct remesa_memory_view *v=&views[i];
  if(d->role!=i||d->domain>SGX535_DOMAIN_MMU||
     d->owner_token!=v->binding.id||d->size!=v->binding.bytes||
     b->pages!=v->pages||b->page_count!=v->page_count||b->gem.size!=d->size||
     b->gem_live||b->cpu||!v->cpu)return -EACCES;
  if(d->domain==SGX535_DOMAIN_LOCAL){
   if(loan->rules[i].access||d->gpu_va||b->va_reserved||b->mapped_pages)return -EACCES;
  }else if(!loan->rules[i].access||d->gpu_va!=v->binding.va||
           !b->va_reserved||b->va.start!=d->gpu_va||
           b->va.end!=d->gpu_va+d->size-1||b->mapped_pages!=b->page_count)
   return -EACCES;
 }
 return remesa_gem_private_validate_locked(loan,views,count);
}

static int remesa_fixed_private_run(struct sgx535_gma500_fixed_backend *b,
 enum sgx535_fixed_stage stage)
{
 int error=remesa_fixed_private_scene(b->owner,b->private_loan,
                                      b->private_views,SGX535_BO_COUNT);
 if(error)return error;
 switch(stage){
 case SGX535_FIXED_PUBLISH_CPU:
  return remesa_gem_private_publish_locked(b->private_loan,b->private_views,SGX535_BO_COUNT);
 case SGX535_FIXED_PUBLISH_TRANSLATIONS:
  return b->private_loan->host_published?0:-ENODATA;
 case SGX535_FIXED_DEVICE_MAINTAIN:
  if(!b->private_loan->host_published)return -ENODATA;
  return remesa_mmu_private_maintain_locked(b->private_loan->owner);
 default:return -EINVAL;
 }
}

int sgx535_gma500_fixed_backend_begin_private(
 struct sgx535_gma500_fixed_backend *b,struct sgx535_gma500_owner *o,
 struct remesa_gem_private *loan,const struct remesa_memory_view *views,unsigned count)
{
 int error;
 if(!b||b->active||b->private_loan)return -EBUSY;
 error=remesa_fixed_private_scene(o,loan,views,count);if(error)return error;
 if(loan->host_published)return -EALREADY;
 error=sgx535_gma500_fixed_backend_begin_guarded(b,o,loan->guard,loan);
 if(error)return error;
 memcpy(b->private_views,views,sizeof(b->private_views));b->private_loan=loan;
 return 0;
}
