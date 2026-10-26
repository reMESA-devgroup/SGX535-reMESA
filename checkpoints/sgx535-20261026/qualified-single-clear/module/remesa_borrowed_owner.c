/* SPDX-License-Identifier: GPL-2.0-only */
/* No image seeds, shader bytes, MMIO, page-table updates or allocation claims.
 * This constructs the qualified fixed service's software view of held GEM. */
#ifdef REMESA_BORROWED_OWNER_TEST
#include "tests/test_borrowed_owner_shim.h"
#else
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "gma500_bo_owner.h"
#endif
#include "remesa_borrowed_owner.h"
static bool all_zero(const void *p,unsigned bytes)
{
 const unsigned char *b=p;unsigned i;
 for(i=0;i<bytes;i++)if(b[i])return false;
 return true;
}
int remesa_borrowed_owner_init(struct remesa_borrowed_owner *binding,
 struct sgx535_gma500_owner *owner,struct drm_device *dev,struct remesa_memory_ticket ticket,
 const struct remesa_memory_view *views,unsigned count,u64 mmu_end,
 const struct sgx535_frozen_command_pairs *commands,remesa_borrowed_verify verify,void *lender)
{
 struct sgx535_gma500_owner *candidate;
 struct sgx535_frozen_command_pairs extracted;
 const struct sgx535_frozen_request request={1,1,0,0};
 unsigned i,j;int e;
 if(!binding || !all_zero(binding,sizeof(*binding)) || !owner || !dev || !commands ||
    !verify || !lender || !views || count!=SGX535_BO_COUNT || !ticket.context ||
    !ticket.operation || ticket.operation>0xffffffffULL || !ticket.generation || ticket.slot>=8 ||
    !all_zero(owner,sizeof(*owner)))return -EINVAL;
 e=verify(lender,dev,ticket,views,count,mmu_end);if(e)return e;
 candidate=kzalloc(sizeof(*candidate),GFP_KERNEL);if(!candidate)return -ENOMEM;
 candidate->dev=dev;candidate->mmu_end=mmu_end;
 for(i=0;i<count;i++) {
  struct sgx535_frozen_requirement required;
  struct sgx535_gma500_bo *bo=&candidate->objects[i];
  struct sgx535_frozen_bo *d=&candidate->descriptors[i];
  if(sgx535_frozen_get_requirement(i,&required) || !views[i].binding.id ||
     views[i].binding.bytes!=required.size || !views[i].pages || !views[i].cpu ||
     views[i].page_count!=required.size/4096 ||
     views[i].binding.va>0xffffffffULL ||
     views[i].binding.bytes>0x100000000ULL-views[i].binding.va){e=-EINVAL;goto fail;}
  for(j=0;j<views[i].page_count;j++)if(!views[i].pages[j]){e=-EINVAL;goto fail;}
  for(j=0;j<i;j++)if(views[i].binding.id==views[j].binding.id ||
     views[i].pages==views[j].pages){e=-EINVAL;goto fail;}
  d->role=i;d->domain=required.domain;d->size=required.size;
  d->owner_token=views[i].binding.id;
  bo->pages=views[i].pages;bo->page_count=views[i].page_count;bo->gem.size=required.size;
  /* These flags describe the lender's reservation; no insertion/removal or
   * GEM initialization occurs. LOCAL CPU backing has no hardware address. */
  if(required.domain!=SGX535_DOMAIN_LOCAL) {
   d->gpu_va=views[i].binding.va;bo->va.start=d->gpu_va;
   bo->va.end=d->gpu_va+required.size-1;bo->va_reserved=true;
   bo->mapped_pages=bo->page_count;
  }
 }
 if(sgx535_frozen_validate_bos(candidate->descriptors,count,mmu_end)){e=-EINVAL;goto fail;}
 {
  struct sgx535_frozen_cpu_view control;
  control.bytes=views[SGX535_BO_CONTROL].cpu;control.length=views[SGX535_BO_CONTROL].binding.bytes;
  if(sgx535_frozen_extract_commands(&control,&extracted) ||
     memcmp(&extracted,commands,sizeof(extracted))){e=-EINVAL;goto fail;}
 }
 candidate->commands=*commands;
 if(sgx535_frozen_session_begin(&candidate->session,&request,sizeof(request),
       candidate->descriptors,count,mmu_end) ||
    sgx535_frozen_plan_use_bases(candidate->descriptors,count,mmu_end,&candidate->use_plan) ||
    sgx535_frozen_xhw_bind_fire_wire(candidate->descriptors,count,mmu_end,0,0,&candidate->xhw_ta) ||
    sgx535_frozen_xhw_bind_fire_wire(candidate->descriptors,count,mmu_end,1,0,&candidate->xhw_raster) ||
    sgx535_frozen_session_advance(&candidate->session,SGX535_PHASE_HOST_IMAGE_FINAL)) {
  e=-EINVAL;goto fail;
 }
 /* A new service has a self-pointer: bind only after copying final bookkeeping.
  * No extra ownership is acquired; these flags are verifier-attested loans. */
 e=verify(lender,dev,ticket,views,count,mmu_end);if(e)goto fail;
 candidate->initialized=true;candidate->device_claimed=true;candidate->power_held=true;
 candidate->borrowed_backing=true;
 *owner=*candidate;
 if(sgx535_fixed_service_bind(&owner->fixed_service,&owner->session,
      owner->descriptors,count,mmu_end,&owner->commands)) {
  memset(owner,0,sizeof(*owner));e=-EINVAL;goto fail;
 }
 binding->owner=owner;binding->ticket=ticket;binding->live=true;e=0;
fail:
 kfree(candidate);return e;
}
int remesa_borrowed_owner_deinit(struct remesa_borrowed_owner *binding)
{
 struct sgx535_gma500_owner *owner;unsigned i;
 if(!binding || !binding->live || !binding->owner)return -EINVAL;
 owner=binding->owner;
 if(!owner->borrowed_backing || !owner->initialized ||
    owner->fixed_service.session!=&owner->session)return -EINVAL;
 if(owner->fixed_service.observer || owner->fixed_service.observer_context ||
    owner->fixed_service.use_owned)return -EBUSY;
 /* Started work requires an authenticated retirement and detached executor.
  * Initialization-only service bookkeeping is safe to discard. */
 if(owner->fixed_service.started &&
    (owner->session.phase!=SGX535_PHASE_RETIRED ||
     owner->session.sequence!=binding->ticket.operation ||
     owner->session.observed_events!=REMESA_JOB_LIFECYCLE_ALL ||
     !sgx535_fixed_service_may_release(&owner->fixed_service)))return -EBUSY;
 if(!owner->fixed_service.started && (owner->fixed_service.unsafe_possible ||
    owner->session.phase!=SGX535_PHASE_HOST_IMAGE_FINAL))return -EBUSY;
 for(i=0;i<SGX535_BO_COUNT;i++)if(owner->objects[i].cpu || owner->objects[i].gem_live)return -EBUSY;
 memset(owner,0,sizeof(*owner));memset(binding,0,sizeof(*binding));return 0;
}
