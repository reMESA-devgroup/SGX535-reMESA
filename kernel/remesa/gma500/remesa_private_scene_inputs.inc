/* SPDX-License-Identifier: GPL-2.0-only */
/* Same memory TU. Reuse the existing input_seal==2 write exclusion. Neither
 * a content validator nor a GPU credential: trusted admission authenticates
 * bytes and derives the READ mask from the actual prepared private loan. */
#define REMESA_PRIVATE_SCENE_INPUT_SEALS 1
#include "remesa_private_session.h"
#include "remesa_parameter_window.h"
static int scene_input_objects_locked(struct remesa_gem_memory *s)
{
 unsigned i,k;u32 mask=s->private_scene_input_mask;
 if(!s->private_scene_binding_count){
  if(mask)return -EIO;
  for(i=0;i<32;i++)if(s->private_scene_input_ids[i])return -EIO;
  return 0;
 }
 if(s->private_scene_binding_count>32||!mask||
    (s->private_scene_binding_count<32&&(mask>>s->private_scene_binding_count)))return -EIO;
 for(i=0;i<32;i++){
  u32 id=s->private_scene_input_ids[i];struct backing *b;
  if(!(mask&((u32)1<<i))){if(id)return -EIO;continue;}
  if(!id)return -EIO;
  b=&s->bo[id&31];
  if(!b->live||b->generation!=id>>5||b->input_seal!=2||!b->initialized||
     !b->pinned||!b->reserved||!b->pages||!b->gem.size||
     b->inserted!=b->gem.size/PAGE_SIZE||b->lease||b->quarantine||
     atomic_read(&b->color_vmas))return -EACCES;
  for(k=0;k<i;k++)if(s->private_scene_input_ids[k]==id)return -EACCES;
 }
 return 0;
}
int remesa_private_session_seal_scene_inputs(struct remesa_gem_memory *s,
 const void *owner,struct remesa_memory_ticket t,u32 mask)
{
 struct retained_job *j;unsigned i,k;int error=-EACCES;
 if(!s||!owner||!mask)return -EINVAL;
 mutex_lock(&endpoint_lock);if(endpoint!=s)goto done;
 mutex_lock(&s->lock);
 if(s->private_session!=owner)goto unlock;
 if(s->private_device_lost||!s->file){error=-ENODEV;goto unlock;}
 if(s->private_source_poisoned){error=-EIO;goto unlock;}
 if(s->private_source_held||s->stream_bound||s->ta_cache_domains||
    s->private_work_owned||s->pds_write_guard||s->private_scene_binding_count){error=-EBUSY;goto unlock;}
 error=scene_input_objects_locked(s);if(error)goto unlock;
 for(i=0;i<5;i++)if(s->private_input_ids[i]){error=-EBUSY;goto unlock;}
 for(i=0;i<6;i++)if(s->private_work_ids[i]){error=-EBUSY;goto unlock;}
 error=remesa_private_session_dpm_locked(s,t);if(error)goto unlock;
 j=ticket_job(s,t);
 if(!j||j->state!=REMESA_JOB_PREPARED||j->cookie||j->quarantined||
    !j->count||j->count>32||(j->count<32&&(mask>>j->count))||
    (mask&(j->dpm_mask|j->region_mask))){error=-EACCES;goto unlock;}
 for(i=0;i<8;i++)if(s->jobs[i].live&& &s->jobs[i]!=j){error=-EBUSY;goto unlock;}
 for(i=0;i<j->count;i++)if(mask&((u32)1<<i)){
  u32 id=j->ids[i];struct backing *b=&s->bo[id&31];
  if(!id||!b->live||b->generation!=id>>5||b->holds!=1||b->lease||b->quarantine||
     b->input_seal||!b->initialized||!b->pinned||!b->reserved||!b->pages||
     !b->gem.size||b->inserted!=b->gem.size/PAGE_SIZE||atomic_read(&b->color_vmas)){
   error=-EACCES;goto unlock;
  }
  for(k=0;k<i;k++)if(j->ids[k]==id){error=-EACCES;goto unlock;}
 }
 /* Commit only after the entire set has passed under exclusive job holds. */
 for(i=0;i<j->count;i++)if(mask&((u32)1<<i)){
  s->private_scene_input_ids[i]=j->ids[i];s->bo[j->ids[i]&31].input_seal=2;
 }
 s->private_scene_input_mask=mask;s->private_scene_binding_count=j->count;error=0;
 unlock:mutex_unlock(&s->lock);
 done:mutex_unlock(&endpoint_lock);return error;
}

struct scene_parameter_patch_context {
 struct remesa_gem_memory *memory;
 const struct remesa_parameter_patch_policy *policy;
};
static int scene_parameter_patch_validate(const void *opaque,u32 id,u64 offset,
 const void *data,unsigned bytes)
{
 const struct scene_parameter_patch_context *c=opaque;
 const struct remesa_parameter_window *w=&c->policy->window;
 const struct backing *b=&c->memory->bo[id&31];
 /* Called only under both locks, after live READ ID/generation validation. */
 if(w->owner!=c->memory->context||w->gpu_va!=b->va.start||
    w->allocation_bytes!=b->gem.size||!remesa_parameter_window_patch_valid(w,id,offset,bytes))return -EACCES;
 return remesa_parameter_patch_bounded_valid(c->policy,id,offset,data,bytes)?0:-ERANGE;
}
int remesa_private_session_patch_scene_parameters(struct remesa_gem_memory *s,
 const void *owner,u64 retired_operation,const struct remesa_parameter_patch_policy *policy,
 u32 id,u64 offset,const void *data,unsigned bytes)
{
 struct remesa_parameter_patch_policy saved;
 struct scene_parameter_patch_context c={s,&saved};
 if(!policy)return -EINVAL;
 saved=*policy;if(!remesa_parameter_window_valid(&saved.window))return -EINVAL;
 return remesa_private_session_patch_scene_input(s,owner,retired_operation,
  id,offset,data,bytes,&c,scene_parameter_patch_validate);
}
int remesa_private_session_scene_inputs_locked(struct remesa_gem_memory *s,
 struct remesa_memory_ticket t,u32 mask)
{
 struct retained_job *j;unsigned i;int error;
 if(!s||!s->private_session||s->private_device_lost||!s->file||s->private_source_poisoned)return -EACCES;
#ifdef __KERNEL__
 lockdep_assert_held(&s->lock);
#endif
 if(!mask||mask!=s->private_scene_input_mask)return -EACCES;
 error=scene_input_objects_locked(s);if(error)return error;
 j=ticket_job(s,t);
 if(!j||j->state!=REMESA_JOB_PREPARED||j->quarantined||j->cookie||
    j->count!=s->private_scene_binding_count)return -EBUSY;
 for(i=0;i<j->count;i++)if(mask&((u32)1<<i))
  if(j->ids[i]!=s->private_scene_input_ids[i]||s->bo[j->ids[i]&31].holds!=1)return -EACCES;
 return 0;
}

/* Kernel-only data transaction. Ordinary memory WRITE/LEASE/SYNC continue to
 * reject these sealed inputs. The authenticated caller supplies a pure typed
 * policy; a userspace pointer or a caller-provided mask is not admission. */
int remesa_private_session_patch_scene_input(struct remesa_gem_memory *s,
 const void *owner,u64 retired_operation,u32 id,u64 offset,const void *data,
 unsigned bytes,const void *policy,
 int (*validate)(const void *,u32,u64,const void *,unsigned))
{
 u8 staged[512];struct backing *b;void *cpu;unsigned i,page;int error=-EACCES;
 if(!s||!owner||!retired_operation||!id||!data||!policy||!validate||
    !bytes||bytes>sizeof(staged)||(offset&3)||(bytes&3)||
    (offset&(PAGE_SIZE-1))>PAGE_SIZE-bytes)return -EINVAL;
 /* Snapshot before validation: the exact validated bytes are committed, even
  * if a trusted caller's original staging storage aliases the destination. */
 memcpy(staged,data,bytes);
 mutex_lock(&endpoint_lock);if(endpoint!=s)goto done;
 mutex_lock(&s->lock);
 if(s->private_session!=owner)goto unlock;
 if(s->private_device_lost||!s->file){error=-ENODEV;goto unlock;}
 if(s->private_source_poisoned){error=-EIO;goto unlock;}
 if(!s->private_source_held||!s->private_dpm_ids[0]||!s->private_dpm_ids[1]||
    !s->private_scene_binding_count||!s->private_scene_input_mask)goto unlock;
 if(!s->private_dpm_ready){error=-ENODATA;goto unlock;}
 if(s->private_dpm_retired_operation!=retired_operation){error=-ESTALE;goto unlock;}
 for(i=0;i<2;i++)if(!private_dpm_domain(s,&s->bo[s->private_dpm_ids[i]&31])){
  error=-EACCES;goto unlock;
 }
 if(s->stream_bound||s->executor||s->ta_cache_domains||s->private_work_owned||
    s->pds_write_guard){error=-EBUSY;goto unlock;}
 for(i=0;i<5;i++)if(s->private_input_ids[i]){error=-EBUSY;goto unlock;}
 for(i=0;i<6;i++)if(s->private_work_ids[i]){error=-EBUSY;goto unlock;}
 /* No prepared/issued/unretired job, CPU alias or output read lease may
  * coexist with the transaction, including aliases on another scene role. */
 for(i=0;i<8;i++)if(s->jobs[i].live){error=-EBUSY;goto unlock;}
 for(i=0;i<OBJECTS;i++)if(s->bo[i].live&&
    (s->bo[i].holds||s->bo[i].lease||s->bo[i].cpu||s->bo[i].quarantine||
     atomic_read(&s->bo[i].color_vmas))){
  error=-EBUSY;goto unlock;
 }
 error=scene_input_objects_locked(s);if(error)goto unlock;
 for(i=0;i<s->private_scene_binding_count;i++)
  if((s->private_scene_input_mask&((u32)1<<i))&&s->private_scene_input_ids[i]==id)break;
 if(i==s->private_scene_binding_count){error=-EACCES;goto unlock;}
 b=&s->bo[id&31];
 if(offset>b->gem.size||bytes>b->gem.size-offset){error=-ERANGE;goto unlock;}
 error=validate(policy,id,offset,staged,bytes);if(error)goto unlock;
 page=(unsigned)(offset/PAGE_SIZE);
 /* READ-only GPU inputs have no device writes to invalidate. Reuse only a
  * stable mapping retained by this backing, otherwise map one affected page.
  * No GEM allocation, pool clear, full input read/compare or hash is performed. */
 if(b->cached_job_view)cpu=(u8 *)b->cached_job_view+(u64)page*PAGE_SIZE;
 else cpu=vmap(b->pages+page,1,VM_MAP,PAGE_KERNEL);
 if(!cpu){error=-ENOMEM;goto unlock;}
 memcpy((u8 *)cpu+(offset&(PAGE_SIZE-1)),staged,bytes);
 drm_clflush_pages(b->pages+page,1);dma_wmb();
 if(!b->cached_job_view)vunmap(cpu);
 b->zero_pristine=false;
 error=0;
 unlock:mutex_unlock(&s->lock);
 done:mutex_unlock(&endpoint_lock);return error;
}
