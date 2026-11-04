/* SPDX-License-Identifier: GPL-2.0-only. After vertex/index checker. */
#include "remesa_gem_scene_fragments.h"
#include "remesa_fragment_fetch.h"
static int fragment_read_view(struct remesa_gem_private *loan,const struct remesa_memory_view *views,
 unsigned count,u32 address,u32 bytes,unsigned *index,u32 *offset)
{
 unsigned i;u64 at;
 for(i=0;i<count;i++){
  if(address<views[i].binding.va)continue;
  at=(u64)address-views[i].binding.va;
  if(at>views[i].binding.bytes||bytes>views[i].binding.bytes-at)continue;
  if(loan->rules[i].access!=REMESA_MMU_GPU_READ||loan->rules[i].cached!=1||!views[i].cpu)return -EACCES;
  *index=i;*offset=at;return 0;
 }
 return -ENOENT;
}
/* Keep this transactional result off the prerequisite validators' stack.
 * The noinline boundary is intentional on the 8 KiB i486 kernel stack. */
static __attribute__((__noinline__)) int scene_fragments_build_checked(struct remesa_gem_private *loan,
 const struct remesa_memory_view *views,unsigned count,const struct remesa_gem_scene_use *use,
 struct remesa_gem_scene_ta *ta,struct remesa_gem_scene_states *states,struct remesa_gem_scene_vertices *vertices,
 bool preserve_depth,struct remesa_gem_scene_fragments *out)
{
 struct remesa_gem_scene_fragments value={0};struct retained_job *j;unsigned draw,slot,s,p,q,e,u;int error;
 if(!out)return -EINVAL;
 j=ticket_job(loan->memory,loan->ticket);value.expected_pds_window=0x20000000u;
 for(slot=0;slot<vertices->count-(preserve_depth?1:0);slot++){
  unsigned upload=states->count,ref;
  struct remesa_fragment_binding binding;struct remesa_fragment_task task;u32 po,qo,use_id=0,use_offset=0;
  draw=slot+(preserve_depth?1:0);
  /* PARAM entries are uploads too, but are never ISP fragment state. Locate
   * the actual checked draw/prefix state, independently of upload ordinals. */
  for(ref=0;ref<ta->reference_count;ref++)if(ta->references[ref].draw==draw&&
     ta->references[ref].kind==(draw?REMESA_TA_REF_DRAW_STATE:REMESA_TA_REF_PREFIX_STATE)){
   for(upload=0;upload<states->count;upload++)
    if(states->uploads[upload].pds_id==ta->references[ref].id&&
       states->uploads[upload].pds_offset==ta->references[ref].byte_offset)break;
   break;
  }
  if(upload==states->count)return -EPROTO;

  for(s=0;s<count&&views[s].binding.id!=states->uploads[upload].source_id;s++);
  if(s==count||!views[s].cpu)return -EACCES;
  error=remesa_fragment_binding_read((const u8 *)views[s].cpu+states->uploads[upload].source_offset+(draw?8:12),12,
    value.expected_pds_window,draw?1:0,&binding);if(error)return -error;
  error=fragment_read_view(loan,views,count,binding.primary,binding.primary_bytes,&p,&po);if(error)return error;
  error=fragment_read_view(loan,views,count,binding.secondary,4,&q,&qo);if(error)return error;
  error=remesa_fragment_primary((const u8 *)views[p].cpu+po,binding.primary_bytes,draw?1:0,&task);if(error)return -error;
  error=remesa_fragment_secondary((const u8 *)views[q].cpu+qo,4);if(error)return -error;
  for(e=0;e<use->entry_count;e++)if(use->lower19[e]==task.use_lower19){
   struct remesa_gem_scene_use_entry entry;
   memcpy(&entry,(const u8 *)j->commands+use->command_offset+sizeof(struct remesa_gem_scene_use_command)+e*sizeof(entry),sizeof(entry));
   if(entry.data_master!=REMESA_USE_PIXEL||entry.program_bytes!=8)continue;
   if((entry.id==views[p].binding.id&&vertex_section_overlap(entry.byte_offset,8,po,binding.primary_bytes))||
      (entry.id==views[q].binding.id&&vertex_section_overlap(entry.byte_offset,8,qo,4)))return -EINVAL;
   for(u=0;u<count&&views[u].binding.id!=entry.id;u++);
   if(u==count||!views[u].cpu)return -EACCES;
   error=remesa_scene_fragment_body((const u8 *)views[u].cpu+entry.byte_offset,8,draw?1:0);if(error)return -error;
   use_id=entry.id;use_offset=entry.byte_offset;break;
  }
  if(!use_id)return -ENOENT;
  value.draws[slot].primary_id=views[p].binding.id;value.draws[slot].primary_offset=po;value.draws[slot].primary_bytes=binding.primary_bytes;
  value.draws[slot].secondary_id=views[q].binding.id;value.draws[slot].secondary_offset=qo;
  value.draws[slot].use_id=use_id;value.draws[slot].use_offset=use_offset;value.count++;
 }
 value.ticket=loan->ticket;*out=value;return 0;
}

int remesa_gem_scene_fragments_check_locked(struct remesa_gem_private *loan,
 const struct remesa_memory_view *views,unsigned count,const struct remesa_gem_scene_actions *actions,
 const struct remesa_gem_scene_controls *controls,const struct remesa_gem_scene_use *use,u32 vertex_offset,
 struct remesa_gem_scene_ta *ta,struct remesa_gem_scene_states *states,struct remesa_gem_scene_vertices *vertices,
 struct remesa_gem_scene_fragments *out)
{
 int error;
 if(!out)return -EINVAL;
 error=remesa_gem_scene_vertices_check_locked(loan,views,count,actions,controls,use,vertex_offset,ta,states,vertices);if(error)return error;
 return scene_fragments_build_checked(loan,views,count,use,ta,states,vertices,controls->command.schema==3,out);
}
