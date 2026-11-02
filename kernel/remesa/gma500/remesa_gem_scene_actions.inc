/* SPDX-License-Identifier: GPL-2.0-only. After retained GEM/private helpers. */
#include "remesa_gem_scene_actions.h"
#include "remesa_scene_layout.h"
#ifdef REMESA_PRIVATE_DPM_CACHE
#include "remesa_private_session.h"
#endif
struct remesa_scene_action_inputs {
 struct remesa_gem_scene_action_command command;
 struct remesa_lab_scene_fire_request fire;
 struct remesa_lab_dpm_load_request load;
 unsigned region_index,table_index,parameter_index;
};
static int scene_action_inputs(struct remesa_gem_memory *s,struct retained_job *j,
 u32 offset,struct remesa_scene_action_inputs *out)
{
 struct remesa_scene_action_inputs value={0};struct remesa_scene_layout layout;
 struct backing *objects[3];u32 ids[3];unsigned role,i;int error;
 if((offset&3)||offset>j->command_bytes||sizeof(value.command)>j->command_bytes-offset)return -EINVAL;
 memcpy(&value.command,(const u8 *)j->commands+offset,sizeof(value.command));
 if(value.command.schema!=REMESA_GEM_SCENE_ACTION_SCHEMA||value.command.bytes!=sizeof(value.command)||value.command.reserved)return -EINVAL;
 ids[0]=value.command.region_id;ids[1]=value.command.table_id;ids[2]=value.command.parameter_id;
 if(!ids[0]||!ids[1]||!ids[2]||ids[0]==ids[1]||ids[0]==ids[2]||ids[1]==ids[2])return -EINVAL;
 error=remesa_scene_layout_build(value.command.width,value.command.height,0,&layout);if(error)return -error;
 for(role=0;role<3;role++){
  for(i=0;i<j->count&&j->ids[i]!=ids[role];i++);
  if(i==j->count)return -ENOENT;
  objects[role]=&s->bo[ids[role]&31];
  if(!objects[role]->live||objects[role]->generation!=ids[role]>>5||!objects[role]->holds||
     objects[role]->lease||objects[role]->quarantine||!objects[role]->pinned||!objects[role]->reserved||
     !objects[role]->pages||objects[role]->inserted!=objects[role]->gem.size/PAGE_SIZE)return -EACCES;
  if(role==0)value.region_index=i;else if(role==1)value.table_index=i;else value.parameter_index=i;
 }
 if(j->dpm_mask!=(((u32)1<<value.table_index)|((u32)1<<value.parameter_index)))return -EACCES;
#ifdef REMESA_PRIVATE_DPM_CACHE
 if(j->dpm_previous_operation){
  if(ids[1]!=s->private_dpm_ids[0]||ids[2]!=s->private_dpm_ids[1])return -EACCES;
  error=remesa_private_session_warm_check_locked(s,(struct remesa_memory_ticket){.context=s->context,.operation=j->operation,.slot=(u32)(j-s->jobs),.generation=j->generation});
  if(error)return error;
 }
#endif
 if(j->region_mask&&j->region_mask!=((u32)1<<value.region_index))return -EBUSY;
 if(objects[0]->gem.size!=(u64)layout.allocation_pages*PAGE_SIZE)return -ENOSPC;
 value.fire=(struct remesa_lab_scene_fire_request){value.command.width,value.command.height,objects[0]->va.start,objects[0]->gem.size};
 value.load=(struct remesa_lab_dpm_load_request){objects[1]->va.start,objects[1]->gem.size,objects[2]->va.start,objects[2]->gem.size};
 *out=value;return 0;
}
int remesa_gem_scene_actions_prepare(struct remesa_gem_memory *s,
 struct remesa_memory_ticket t,u32 offset,struct remesa_gem_scene_actions *out)
{
 struct remesa_gem_scene_actions value={0};struct remesa_scene_action_inputs in;
 struct retained_job *j;struct backing *b;unsigned indices[3],i;size_t n;bool initialized;int error=-ENOENT;
 if(!s||!out)return -EINVAL;
 mutex_lock(&s->lock);j=ticket_job(s,t);if(!j)goto done;
 if(j->state!=REMESA_JOB_PREPARED||j->attached||j->cookie||j->quarantined){error=-EBUSY;goto done;}
 error=scene_action_inputs(s,j,offset,&in);if(error)goto done;
 indices[0]=in.region_index;indices[1]=in.table_index;indices[2]=in.parameter_index;
 for(i=0;i<3;i++){
  b=&s->bo[j->ids[indices[i]]&31];
  initialized=b->zero_pristine;
#ifdef REMESA_PRIVATE_DPM_CACHE
  if(i&&j->dpm_previous_operation)initialized=true;
#endif
  if(!b->initialized||!initialized||b->cached_job_view||j->cpu_views[indices[i]]||atomic_read(&b->color_vmas)){error=-ENODATA;goto done;}
 }
 value.ticket=t;value.command_offset=offset;value.binding_count=j->count;value.command=in.command;
 error=remesa_lab_dpm_actions_encode(&in.load,value.load,REMESA_LAB_DPM_LOAD_ACTIONS,&n);if(error){error=-error;goto done;}value.load_count=n;
 error=remesa_lab_scene_fire_encode(&in.fire,REMESA_LAB_SCENE_TA,value.ta,REMESA_LAB_TA_FIRE_ACTIONS,&n);if(error){error=-error;goto done;}value.ta_count=n;
 error=remesa_lab_scene_fire_encode(&in.fire,REMESA_LAB_SCENE_RASTER,value.raster,REMESA_LAB_RASTER_FIRE_ACTIONS,&n);if(error){error=-error;goto done;}value.raster_count=n;
 j->region_mask=(u32)1<<in.region_index;*out=value;error=0;
 done:mutex_unlock(&s->lock);return error;
}
#ifdef REMESA_PRIVATE_DPM_CACHE
static int scene_region_initialize(struct remesa_gem_memory *s,
 struct remesa_memory_ticket t,u32 offset,const void *owner)
{
 struct retained_job *j;struct backing *b;struct remesa_scene_action_inputs in;
 unsigned page,i;int error=-EACCES;
 mutex_lock(&s->lock);j=ticket_job(s,t);
 if(!j||j->state!=REMESA_JOB_PREPARED||j->attached||j->cookie||j->quarantined||
    j->private_scene_owner!=owner||s->private_session!=owner||!s->private_source_held||
    s->private_device_lost||!s->file||s->private_source_poisoned)goto done;
 for(i=0;i<8;i++)if(s->jobs[i].live&& &s->jobs[i]!=j){error=-EBUSY;goto done;}
 error=scene_action_inputs(s,j,offset,&in);if(error)goto done;
 b=&s->bo[j->ids[in.region_index]&31];
 if(b->holds!=1||!b->initialized||b->cached_job_view||j->cpu_views[in.region_index]||atomic_read(&b->color_vmas)){error=-EBUSY;goto done;}
 if(!b->zero_pristine){
  /* Whole-page overwrite under exclusive holds, no CPU reads or GPU work.
   * This is real REGION initialization, not a claim about consumed bytes.
   * Existing private publication flushes these new CPU writes before use. */
  for(page=0;page<b->inserted;page++)clear_highpage(b->pages[page]);
  b->zero_pristine=true;
 }
 error=0;
 done:mutex_unlock(&s->lock);return error;
}
#endif
int remesa_gem_scene_actions_check_locked(struct remesa_gem_private *loan,
 const struct remesa_memory_view *views,unsigned count,const struct remesa_gem_scene_actions *plan)
{
 struct remesa_scene_action_inputs in;struct retained_job *j;
 struct sgx535_frozen_reg_action expected[REMESA_LAB_TA_FIRE_ACTIONS];size_t n;
 unsigned indices[3],i;int error;
 if(!plan)return -EINVAL;
 error=remesa_gem_private_validate_locked(loan,views,count);if(error)return error;
 if(plan->ticket.context!=loan->ticket.context||plan->ticket.operation!=loan->ticket.operation||
    plan->ticket.slot!=loan->ticket.slot||plan->ticket.generation!=loan->ticket.generation||plan->binding_count!=count)return -EACCES;
 j=ticket_job(loan->memory,loan->ticket);
 error=scene_action_inputs(loan->memory,j,plan->command_offset,&in);if(error)return error;
 if(memcmp(&plan->command,&in.command,sizeof(in.command))||j->region_mask!=((u32)1<<in.region_index))return -EACCES;
 indices[0]=in.region_index;indices[1]=in.table_index;indices[2]=in.parameter_index;
 for(i=0;i<3;i++)if(loan->rules[indices[i]].access!=(REMESA_MMU_GPU_READ|REMESA_MMU_GPU_WRITE)||loan->rules[indices[i]].cached!=1||views[indices[i]].cpu)return -EACCES;
 if(plan->load_count!=REMESA_LAB_DPM_LOAD_ACTIONS||plan->ta_count!=REMESA_LAB_TA_FIRE_ACTIONS||plan->raster_count!=REMESA_LAB_RASTER_FIRE_ACTIONS)return -EACCES;
 error=remesa_lab_dpm_actions_encode(&in.load,expected,REMESA_LAB_DPM_LOAD_ACTIONS,&n);if(error)return -error;
 if(memcmp(expected,plan->load,n*sizeof(*expected)))return -EACCES;
 error=remesa_lab_scene_fire_encode(&in.fire,REMESA_LAB_SCENE_TA,expected,REMESA_LAB_TA_FIRE_ACTIONS,&n);if(error)return -error;
 if(memcmp(expected,plan->ta,n*sizeof(*expected)))return -EACCES;
 error=remesa_lab_scene_fire_encode(&in.fire,REMESA_LAB_SCENE_RASTER,expected,REMESA_LAB_RASTER_FIRE_ACTIONS,&n);if(error)return -error;
 return memcmp(expected,plan->raster,n*sizeof(*expected))?-EACCES:0;
}
