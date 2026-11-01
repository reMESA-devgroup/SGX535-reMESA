/* SPDX-License-Identifier: GPL-2.0-only. After fragment checker. */
#include "remesa_gem_scene_background.h"
#include "remesa_background_fetch.h"
#include "remesa_scene_auxiliary.h"
/* Do not retain this transactional result across prerequisite checks. */
static __attribute__((__noinline__)) int scene_background_build_checked(struct remesa_gem_private *loan,
 const struct remesa_memory_view *views,unsigned count,const struct remesa_gem_scene_controls *controls,
 const struct remesa_gem_scene_use *use,const struct remesa_gem_scene_fragments *fragments,
 struct remesa_gem_scene_background *out)
{
 const struct remesa_gem_scene_control_command *c=&controls->command;
 struct remesa_gem_scene_background value={0};struct remesa_background_binding binding;
 struct remesa_background_task task;struct retained_job *j;unsigned b,color,p,q,e,u;u32 po,qo;int error;
 for(b=0;b<count&&views[b].binding.id!=c->background_id;b++);
 for(color=0;color<count&&views[color].binding.id!=c->color_id;color++);
 if(b==count||color==count)return -ENOENT;
 if(loan->rules[b].access!=REMESA_MMU_GPU_READ||loan->rules[b].cached!=1||!views[b].cpu)return -EACCES;
 if(c->background_bytes!=68)return -EINVAL;
 error=remesa_background_binding_read((const u8 *)views[b].cpu+c->background_offset,68,c->width,c->height,
  fragments->expected_pds_window,&binding);if(error)return -error;
 error=fragment_read_view(loan,views,count,binding.primary,64,&p,&po);if(error)return error;
 error=fragment_read_view(loan,views,count,binding.secondary,4,&q,&qo);if(error)return error;
 error=remesa_background_primary((const u8 *)views[p].cpu+po,64,c->width,c->height,&task);if(error)return -error;
 error=remesa_fragment_secondary((const u8 *)views[q].cpu+qo,4);if(error)return -error;
 if(task.color_address!=views[color].binding.va)return -EACCES;
 j=ticket_job(loan->memory,loan->ticket);
 for(e=0;e<use->entry_count;e++)if(use->lower19[e]==task.use_lower19){
  struct remesa_gem_scene_use_entry entry;
  memcpy(&entry,(const u8 *)j->commands+use->command_offset+sizeof(struct remesa_gem_scene_use_command)+e*sizeof(entry),sizeof(entry));
  if(entry.data_master!=REMESA_USE_PIXEL||entry.program_bytes!=8)continue;
  if((entry.id==views[p].binding.id&&vertex_section_overlap(entry.byte_offset,8,po,64))||
     (entry.id==views[q].binding.id&&vertex_section_overlap(entry.byte_offset,8,qo,4)))return -EINVAL;
  for(u=0;u<count&&views[u].binding.id!=entry.id;u++);
  if(u==count||!views[u].cpu)return -EACCES;
  error=remesa_scene_background_aux((const u8 *)views[u].cpu+entry.byte_offset,8);if(error)return -error;
  value.use_id=entry.id;value.use_offset=entry.byte_offset;break;
 }
 if(!value.use_id)return -ENOENT;
 value.ticket=loan->ticket;value.background_id=c->background_id;value.background_offset=c->background_offset;
 value.primary_id=views[p].binding.id;value.primary_offset=po;value.secondary_id=views[q].binding.id;value.secondary_offset=qo;
 value.color_id=c->color_id;value.expected_pds_window=fragments->expected_pds_window;*out=value;return 0;
}
int remesa_gem_scene_background_check_locked(struct remesa_gem_private *loan,
 const struct remesa_memory_view *views,unsigned count,const struct remesa_gem_scene_actions *actions,
 const struct remesa_gem_scene_controls *controls,const struct remesa_gem_scene_use *use,u32 vertex_offset,
 struct remesa_gem_scene_ta *ta,struct remesa_gem_scene_states *states,struct remesa_gem_scene_vertices *vertices,
 struct remesa_gem_scene_fragments *fragments,struct remesa_gem_scene_background *out)
{
 int error;
 if(!out)return -EINVAL;
 error=remesa_gem_scene_fragments_check_locked(loan,views,count,actions,controls,use,vertex_offset,ta,states,vertices,fragments);if(error)return error;
 return scene_background_build_checked(loan,views,count,controls,use,fragments,out);
}
