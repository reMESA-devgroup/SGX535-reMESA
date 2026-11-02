/* SPDX-License-Identifier: GPL-2.0-only. After state-upload checker. */
#include "remesa_gem_scene_vertices.h"
#include "remesa_vertex_fetch.h"
#include "remesa_scene_use_bodies.h"
static bool vertex_section_overlap(u32 a,u32 an,u32 b,u32 bn)
{return a<=b?b-a<an:a-b<bn;}
/* States were authenticated before this checker. Resolve this draw's actual
 * state upload by identity; capacity/PARAM presence alone is not state15. */
static bool vertex_homogeneous_state(const struct remesa_gem_scene_ta *ta,
 const struct remesa_gem_scene_states *states,unsigned draw)
{
 unsigned r,s;
 if(!ta||!states||ta->reference_count>REMESA_TA_REFERENCE_MAX||states->count>REMESA_GEM_SCENE_STATE_MAX)return false;
 for(r=0;r<ta->reference_count;r++)if(ta->references[r].draw==draw&&
    ta->references[r].kind==REMESA_TA_REF_DRAW_STATE)
  for(s=0;s<states->count;s++)if(states->uploads[s].pds_id==ta->references[r].id&&
     states->uploads[s].pds_offset==ta->references[r].byte_offset)
   return states->uploads[s].source_bytes==60;
 return false;
}
/* Keep this transactional result off the prerequisite validators' stack.
 * The noinline boundary is intentional on the 8 KiB i486 kernel stack. */
static __attribute__((__noinline__)) int scene_vertices_build_checked(struct remesa_gem_private *loan,
 const struct remesa_memory_view *views,unsigned count,const struct remesa_gem_scene_actions *actions,
 const struct remesa_gem_scene_controls *controls,const struct remesa_gem_scene_use *use,u32 offset,
 struct remesa_gem_scene_ta *ta,const struct remesa_gem_scene_states *states,
 struct remesa_gem_scene_vertices *out)
{
 struct remesa_gem_scene_vertices value={0};struct remesa_gem_scene_vertex_command command;
 struct remesa_gem_scene_use_command use_command;struct retained_job *j;
 unsigned draw,p,i,v,e,u,r;bool parameters=false;int error;
 if(!out||!states)return -EINVAL;
 for(r=0;r<ta->reference_count;r++)
  if(ta->references[r].kind==REMESA_TA_REF_DRAW_PARAMETERS)parameters=true;
 j=ticket_job(loan->memory,loan->ticket);
 if((offset&3)||offset>j->command_bytes||sizeof(command)>j->command_bytes-offset)return -EINVAL;
 memcpy(&command,(const u8 *)j->commands+offset,sizeof(command));
 if(command.schema!=REMESA_GEM_SCENE_VERTEX_SCHEMA||command.reserved||command.count!=ta->draw_count+1||
    command.count>REMESA_GEM_SCENE_VERTEX_MAX||command.bytes!=sizeof(command)+command.count*sizeof(struct remesa_gem_scene_vertex_window)||
    command.bytes>j->command_bytes-offset)return -EINVAL;
 memcpy(&use_command,(const u8 *)j->commands+use->command_offset,sizeof(use_command));
 if(vertex_section_overlap(offset,command.bytes,actions->command_offset,sizeof(actions->command))||
    vertex_section_overlap(offset,command.bytes,controls->command_offset,sizeof(controls->command))||
    vertex_section_overlap(offset,command.bytes,use->command_offset,use_command.bytes))return -EINVAL;
 for(draw=0;draw<command.count;draw++){
  struct remesa_gem_scene_vertex_window w;struct remesa_vertex_fetch decoded;struct remesa_vertex_indices indices;
  unsigned ir=ta->reference_count,pr=ir;
  u32 use_id=0,use_offset=0,use_bytes=draw?(parameters?320:64):16;
  /* Resolve by authenticated kind/draw, never positional triples. The PARAM
   * stream has an additional reference between state and indexed draw. */
  for(r=0;r<ta->reference_count;r++)if(ta->references[r].draw==draw){
   if(ta->references[r].kind==(draw?REMESA_TA_REF_DRAW_INDEX:REMESA_TA_REF_PREFIX_INDEX))ir=r;
   if(ta->references[r].kind==(draw?REMESA_TA_REF_DRAW_VERTEX:REMESA_TA_REF_PREFIX_VERTEX))pr=r;
  }
  if(ir==ta->reference_count||pr==ta->reference_count||ta->references[pr].minimum_bytes<64)return -EPROTO;
  memcpy(&w,(const u8 *)j->commands+offset+sizeof(command)+draw*sizeof(w),sizeof(w));
  if(w.reserved||!w.vertex_count||w.vertex_count>65536||(!draw&&w.vertex_count!=3)||w.index_id!=ta->references[ir].id||
     w.index_offset!=ta->references[ir].byte_offset||w.index_bytes!=ta->references[ir].minimum_bytes)return -EINVAL;
  for(p=0;p<count&&views[p].binding.id!=ta->references[pr].id;p++);
  for(i=0;i<count&&views[i].binding.id!=w.index_id;i++);
  for(v=0;v<count&&views[v].binding.id!=w.vertex_id;v++);
  if(p==count||i==count||v==count)return -ENOENT;
  if(!views[p].cpu||!views[i].cpu||loan->rules[v].access!=REMESA_MMU_GPU_READ||loan->rules[v].cached!=1)return -EACCES;
  error=remesa_vertex_fetch_pds((const u8 *)views[p].cpu+ta->references[pr].byte_offset,64,draw?12:4,draw?1:0,&decoded);if(error)return -error;
  if((u64)decoded.stride_bytes*w.vertex_count!=w.vertex_bytes||w.vertex_offset>views[v].binding.bytes||
     w.vertex_bytes>views[v].binding.bytes-w.vertex_offset||
     views[v].binding.va+w.vertex_offset!=decoded.source_address)return -ERANGE;
  if(w.index_id==w.vertex_id&&vertex_section_overlap(w.index_offset,w.index_bytes,w.vertex_offset,w.vertex_bytes))return -EINVAL;
  if(w.vertex_id==views[p].binding.id&&vertex_section_overlap(w.vertex_offset,w.vertex_bytes,ta->references[pr].byte_offset,64))return -EINVAL;
  error=remesa_vertex_indices_check((const u8 *)views[i].cpu+w.index_offset,w.index_bytes,
   ta->references[ir].index_count,w.vertex_count,&indices);if(error)return -error;
  for(e=0;e<use->entry_count;e++)if(use->lower19[e]==decoded.use_lower19){
   struct remesa_gem_scene_use_entry entry;bool clamped;
   memcpy(&entry,(const u8 *)j->commands+use->command_offset+sizeof(use_command)+e*sizeof(entry),sizeof(entry));
   clamped=draw&&parameters&&entry.program_bytes==344;
   if(entry.data_master!=REMESA_USE_VERTEX)continue;
   if(clamped){if(!vertex_homogeneous_state(ta,states,draw))continue;}
   else if(entry.program_bytes!=use_bytes)continue;
   for(u=0;u<count&&views[u].binding.id!=entry.id;u++);
   if(u==count)return -ENOENT;
   if(!views[u].cpu)return -EACCES;
   error=clamped?
    remesa_scene_gears_attribute_clamped_vertex_body((const u8 *)views[u].cpu+entry.byte_offset,entry.program_bytes):draw&&parameters?
    remesa_scene_gears_attribute_vertex_body((const u8 *)views[u].cpu+entry.byte_offset,use_bytes):
    remesa_scene_vertex_body((const u8 *)views[u].cpu+entry.byte_offset,use_bytes,draw?1:0);if(error)return -error;
   if(entry.id==w.vertex_id&&vertex_section_overlap(entry.byte_offset,entry.program_bytes,w.vertex_offset,w.vertex_bytes))return -EINVAL;
   if(entry.id==w.index_id&&vertex_section_overlap(entry.byte_offset,entry.program_bytes,w.index_offset,w.index_bytes))return -EINVAL;
   use_id=entry.id;use_offset=entry.byte_offset;use_bytes=entry.program_bytes;break;
  }
  if(!use_id)return -ENOENT;
  value.draws[draw].index_id=w.index_id;value.draws[draw].index_offset=w.index_offset;value.draws[draw].index_bytes=w.index_bytes;
  value.draws[draw].vertex_id=w.vertex_id;value.draws[draw].vertex_offset=w.vertex_offset;value.draws[draw].vertex_bytes=w.vertex_bytes;
  value.draws[draw].stride_bytes=decoded.stride_bytes;value.draws[draw].minimum_index=indices.minimum;value.draws[draw].maximum_index=indices.maximum;
  value.draws[draw].use_id=use_id;value.draws[draw].use_offset=use_offset;value.draws[draw].use_bytes=use_bytes;
 }
 value.ticket=loan->ticket;value.count=command.count;*out=value;return 0;
}

int remesa_gem_scene_vertices_check_locked(struct remesa_gem_private *loan,
 const struct remesa_memory_view *views,unsigned count,const struct remesa_gem_scene_actions *actions,
 const struct remesa_gem_scene_controls *controls,const struct remesa_gem_scene_use *use,u32 offset,
 struct remesa_gem_scene_ta *ta,struct remesa_gem_scene_states *states,struct remesa_gem_scene_vertices *out)
{
 int error;
 if(!out)return -EINVAL;
 error=remesa_gem_scene_states_check_locked(loan,views,count,actions,controls,use,ta,states);if(error)return error;
 return scene_vertices_build_checked(loan,views,count,actions,controls,use,offset,ta,states,out);
}
