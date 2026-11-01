/* SPDX-License-Identifier: GPL-2.0-only. After background checker. */
#include "remesa_gem_scene_event.h"
#include "remesa_event_fetch.h"
static int scene_event_use(struct remesa_gem_private *loan,const struct remesa_memory_view *views,unsigned count,
 const struct remesa_gem_scene_use *use,u32 lower19,u32 bytes,u32 *id,u32 *offset)
{
 struct retained_job *j=ticket_job(loan->memory,loan->ticket);unsigned e,u;int error;
 for(e=0;e<use->entry_count;e++)if(use->lower19[e]==lower19){
  struct remesa_gem_scene_use_entry entry;
  memcpy(&entry,(const u8 *)j->commands+use->command_offset+sizeof(struct remesa_gem_scene_use_command)+e*sizeof(entry),sizeof(entry));
  if(entry.data_master!=REMESA_USE_PIXEL||entry.program_bytes!=bytes)continue;
  for(u=0;u<count&&views[u].binding.id!=entry.id;u++);
  if(u==count||!views[u].cpu)return -EACCES;
  error=bytes==16?remesa_scene_event_transfer_aux((const u8 *)views[u].cpu+entry.byte_offset,16):
    remesa_scene_event_end_aux((const u8 *)views[u].cpu+entry.byte_offset,8);if(error)return -error;
  *id=entry.id;*offset=entry.byte_offset;return 0;
 }
 return -ENOENT;
}
static __attribute__((__noinline__)) int scene_event_build_checked(struct remesa_gem_private *loan,
 const struct remesa_memory_view *views,unsigned count,const struct remesa_gem_scene_controls *controls,
 const struct remesa_gem_scene_use *use,struct remesa_gem_scene_event *out)
{
 const struct remesa_gem_scene_control_command *c=&controls->command;
 struct remesa_gem_scene_event value={0};struct remesa_event_fetch decoded;
 unsigned e,t,color,i;u32 at;int error;
 for(e=0;e<count&&views[e].binding.id!=c->event_id;e++);
 for(color=0;color<count&&views[color].binding.id!=c->color_id;color++);
 if(e==count||color==count)return -ENOENT;
 if(c->event_bytes!=148||((views[e].binding.va+c->event_offset)&31))return -EINVAL;
 if(loan->rules[e].access!=REMESA_MMU_GPU_READ||loan->rules[e].cached!=1||!views[e].cpu)return -EACCES;
 error=remesa_event_fetch_read((const u8 *)views[e].cpu+c->event_offset,148,&decoded);if(error)return -error;
 error=fragment_read_view(loan,views,count,decoded.source_address,16,&t,&at);if(error)return error;
 if(e==t&&vertex_section_overlap(c->event_offset,148,at,16))return -EINVAL;
 error=remesa_event_target_check((const u8 *)views[t].cpu+at,16,c->color_pitch,c->height,views[color].binding.va);if(error)return -error;
 error=scene_event_use(loan,views,count,use,decoded.transfer_lower19,16,&value.transfer_id,&value.transfer_offset);if(error)return error;
 error=scene_event_use(loan,views,count,use,decoded.end_lower19,8,&value.end_id,&value.end_offset);if(error)return error;
 for(i=0;i<2;i++){
  u32 id=i?value.end_id:value.transfer_id,offset=i?value.end_offset:value.transfer_offset,bytes=i?8:16;
  if((id==c->event_id&&vertex_section_overlap(offset,bytes,c->event_offset,148))||
     (id==views[t].binding.id&&vertex_section_overlap(offset,bytes,at,16)))return -EINVAL;
 }
 if(value.end_id==value.transfer_id&&vertex_section_overlap(value.end_offset,8,value.transfer_offset,16))return -EINVAL;
 value.ticket=loan->ticket;value.event_id=c->event_id;value.event_offset=c->event_offset;
 value.target_id=views[t].binding.id;value.target_offset=at;*out=value;return 0;
}
int remesa_gem_scene_event_check_locked(struct remesa_gem_private *loan,const struct remesa_memory_view *views,unsigned count,
 const struct remesa_gem_scene_actions *actions,const struct remesa_gem_scene_controls *controls,const struct remesa_gem_scene_use *use,u32 offset,
 struct remesa_gem_scene_ta *ta,struct remesa_gem_scene_states *states,struct remesa_gem_scene_vertices *vertices,
 struct remesa_gem_scene_fragments *fragments,struct remesa_gem_scene_background *background,struct remesa_gem_scene_event *out)
{
 int error;
 if(!out)return -EINVAL;
 error=remesa_gem_scene_background_check_locked(loan,views,count,actions,controls,use,offset,ta,states,vertices,fragments,background);if(error)return error;
 return scene_event_build_checked(loan,views,count,controls,use,out);
}
