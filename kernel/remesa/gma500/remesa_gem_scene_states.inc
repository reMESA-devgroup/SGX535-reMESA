/* SPDX-License-Identifier: GPL-2.0-only. After TA reference checker. */
#include "remesa_gem_scene_states.h"
#include "remesa_state_upload.h"
int remesa_gem_scene_states_check_locked(struct remesa_gem_private *loan,
 const struct remesa_memory_view *views,unsigned count,const struct remesa_gem_scene_actions *actions,
 const struct remesa_gem_scene_controls *controls,const struct remesa_gem_scene_use *use,
 struct remesa_gem_scene_ta *ta,struct remesa_gem_scene_states *out)
{
 struct remesa_gem_scene_states value={0};struct retained_job *j;
 unsigned ref,p,s,e,u;int error;
 if(!ta||!out)return -EINVAL;
 error=remesa_gem_scene_ta_check_locked(loan,views,count,actions,controls,use,ta);if(error)return error;
 j=ticket_job(loan->memory,loan->ticket);
 for(ref=0;ref<ta->reference_count;ref++){
  u32 kind=ta->references[ref].kind,dwords,offset=ta->references[ref].byte_offset,n=value.count;
  struct remesa_state_upload decoded;u64 source_offset=0;u32 found_id=0,found_offset=0;
  if(kind!=REMESA_TA_REF_PREFIX_STATE&&kind!=REMESA_TA_REF_DRAW_STATE&&kind!=REMESA_TA_REF_CLOSE_STATE)continue;
  dwords=kind==REMESA_TA_REF_PREFIX_STATE?11:kind==REMESA_TA_REF_DRAW_STATE?14:2;
  for(p=0;p<count&&views[p].binding.id!=ta->references[ref].id;p++);
  if(p==count||!views[p].cpu)return -EACCES;
  error=remesa_state_upload_pds((const u8 *)views[p].cpu+offset,60,dwords,&decoded);if(error)return -error;
  for(s=0;s<count;s++){
   if(decoded.source_address<views[s].binding.va)continue;
   source_offset=(u64)decoded.source_address-views[s].binding.va;
   if(source_offset<=views[s].binding.bytes&&decoded.source_bytes<=views[s].binding.bytes-source_offset)break;
  }
  if(s==count)return -ENOENT;
  if(loan->rules[s].access!=REMESA_MMU_GPU_READ||loan->rules[s].cached!=1||!views[s].cpu)return -EACCES;
  {
   const u8 *data=(const u8 *)views[s].cpu+source_offset;u32 header=remesa_ta_ref_word(data);
   if(header!=(kind==REMESA_TA_REF_PREFIX_STATE?0x54c5u:kind==REMESA_TA_REF_DRAW_STATE?0xf41u:0x2000u))return -EINVAL;
   if(kind==REMESA_TA_REF_CLOSE_STATE&&remesa_ta_ref_word(data+4)!=
      (((controls->command.width+15)>>4)<<16|((controls->command.height+15)>>4)))return -EINVAL;
  }
  for(e=0;e<use->entry_count;e++)if(use->lower19[e]==decoded.use_lower19){
   struct remesa_gem_scene_use_entry entry;
   memcpy(&entry,(const u8 *)j->commands+use->command_offset+sizeof(struct remesa_gem_scene_use_command)+e*sizeof(entry),sizeof(entry));
   if(entry.data_master!=REMESA_USE_VERTEX||entry.program_bytes!=16)continue;
   for(u=0;u<count&&views[u].binding.id!=entry.id;u++);
   if(u==count||!views[u].cpu)return -EACCES;
   error=remesa_state_upload_auxiliary((const u8 *)views[u].cpu+entry.byte_offset,16,dwords);if(error)return -error;
   found_id=entry.id;found_offset=entry.byte_offset;break;
  }
  if(!found_id)return -ENOENT;
  if(n>=REMESA_GEM_SCENE_STATE_MAX)return -E2BIG;
  value.uploads[n].pds_id=views[p].binding.id;value.uploads[n].pds_offset=offset;
  value.uploads[n].source_id=views[s].binding.id;value.uploads[n].source_offset=source_offset;
  value.uploads[n].source_bytes=decoded.source_bytes;value.uploads[n].use_id=found_id;value.uploads[n].use_offset=found_offset;
  value.count++;
 }
 if(value.count!=ta->draw_count+2)return -EPROTO;
 value.ticket=loan->ticket;*out=value;return 0;
}
