/* SPDX-License-Identifier: GPL-2.0-only. After bound action/control helpers. */
#include "remesa_gem_scene_use.h"
#include "remesa_program_registers.h"
static int scene_use_build(struct remesa_gem_memory *s,struct retained_job *j,
 u32 action_offset,u32 control_offset,u32 offset,struct remesa_gem_scene_use *out,
 u32 *program_mask)
{
 struct remesa_gem_scene_controls controls;unsigned indices[5],i,k;
 struct remesa_gem_scene_use_command command;
 struct remesa_gem_scene_use value={0};u32 mask=0;int error;
 error=scene_control_build(s,j,action_offset,control_offset,&controls,indices);if(error)return error;
 if((offset&3)||offset>j->command_bytes||sizeof(command)>j->command_bytes-offset)return -EINVAL;
 memcpy(&command,(const u8 *)j->commands+offset,sizeof(command));
 if(command.schema!=REMESA_GEM_SCENE_USE_SCHEMA||command.reserved||!command.entry_count||
    command.entry_count>REMESA_SCENE_USE_MAX||command.bytes!=sizeof(command)+command.entry_count*sizeof(struct remesa_gem_scene_use_entry)||
    command.bytes>j->command_bytes-offset)return -EINVAL;
 if(offset<=action_offset?action_offset-offset<command.bytes:offset-action_offset<sizeof(struct remesa_gem_scene_action_command))return -EINVAL;
 if(offset<=control_offset?control_offset-offset<command.bytes:offset-control_offset<sizeof(struct remesa_gem_scene_control_command))return -EINVAL;
 for(i=0;i<command.entry_count;i++){
  struct remesa_gem_scene_use_entry entry;struct backing *b;
  struct remesa_use_entry_request request;struct remesa_use_entry_plan plan;u32 fixup;
  memcpy(&entry,(const u8 *)j->commands+offset+sizeof(command)+i*sizeof(entry),sizeof(entry));
  if(!entry.id||entry.reserved)return -EINVAL;
  for(k=0;k<j->count&&j->ids[k]!=entry.id;k++);
  if(k==j->count)return -ENOENT;
  if((j->region_mask|j->dpm_mask)&((u32)1<<k))return -EACCES;
  b=&s->bo[entry.id&31];
  if(!b->live||b->generation!=entry.id>>5||b->holds!=1||b->lease||b->quarantine||
     !b->pinned||!b->reserved||!b->pages||b->inserted!=b->gem.size/PAGE_SIZE)return -EACCES;
  request=(struct remesa_use_entry_request){b->va.start,b->gem.size,entry.byte_offset,
    entry.program_bytes,entry.bank,entry.data_master};
  error=remesa_use_entry_prepare(&request,&plan);if(error)return -error;
  error=remesa_use_entry_preview_fixup(&plan,&fixup);if(error)return -error;
  if((value.bank_mask&((u32)1<<entry.bank))&&
     (value.banks[entry.bank].offset!=plan.register_offset||value.banks[entry.bank].word!=plan.register_word))return -EBUSY;
  value.bank_mask|=(u32)1<<entry.bank;value.banks[entry.bank].offset=plan.register_offset;
  value.banks[entry.bank].word=plan.register_word;value.lower19[i]=fixup;mask|=(u32)1<<k;
 }
 value.action_offset=action_offset;value.control_offset=control_offset;value.command_offset=offset;
 value.binding_count=j->count;value.entry_count=command.entry_count;
 *out=value;*program_mask=mask;return 0;
}
int remesa_gem_scene_use_prepare(struct remesa_gem_memory *s,struct remesa_memory_ticket t,
 u32 action_offset,u32 control_offset,u32 command_offset,struct remesa_gem_scene_use *out)
{
 struct remesa_gem_scene_use value;struct retained_job *j;u32 mask;int error=-ENOENT;
 if(!s||!out)return -EINVAL;
 mutex_lock(&s->lock);j=ticket_job(s,t);if(!j)goto done;
 if(j->state!=REMESA_JOB_PREPARED||j->attached||j->cookie||j->quarantined){error=-EBUSY;goto done;}
 error=scene_use_build(s,j,action_offset,control_offset,command_offset,&value,&mask);if(error)goto done;
 value.ticket=t;*out=value;
 done:mutex_unlock(&s->lock);return error;
}
int remesa_gem_scene_use_check_locked(struct remesa_gem_private *loan,
 const struct remesa_memory_view *views,unsigned count,const struct remesa_gem_scene_actions *actions,
 const struct remesa_gem_scene_controls *controls,const struct remesa_gem_scene_use *plan)
{
 struct remesa_gem_scene_use expected;struct retained_job *j;u32 mask;unsigned i;int error;
 if(!plan||!actions||!controls)return -EINVAL;
 error=remesa_gem_scene_controls_check_locked(loan,views,count,actions,controls);if(error)return error;
 if(plan->action_offset!=actions->command_offset||plan->control_offset!=controls->command_offset||
    plan->binding_count!=count||plan->ticket.context!=loan->ticket.context||plan->ticket.operation!=loan->ticket.operation||
    plan->ticket.slot!=loan->ticket.slot||plan->ticket.generation!=loan->ticket.generation)return -EACCES;
 j=ticket_job(loan->memory,loan->ticket);
 error=scene_use_build(loan->memory,j,plan->action_offset,plan->control_offset,plan->command_offset,&expected,&mask);if(error)return error;
 for(i=0;i<count;i++)if((mask&((u32)1<<i))&&(loan->rules[i].access!=REMESA_MMU_GPU_READ||loan->rules[i].cached!=1))return -EACCES;
 if(plan->entry_count!=expected.entry_count||plan->bank_mask!=expected.bank_mask||
    memcmp(plan->banks,expected.banks,sizeof(plan->banks))||memcmp(plan->lower19,expected.lower19,sizeof(plan->lower19)))return -EACCES;
 return 0;
}
