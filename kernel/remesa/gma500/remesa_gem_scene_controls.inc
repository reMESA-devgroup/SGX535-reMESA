/* SPDX-License-Identifier: GPL-2.0-only. After bound scene action helpers. */
#include "remesa_gem_scene_controls.h"
#include "remesa_scene_control_words.h"
static int scene_control_build(struct remesa_gem_memory *s,struct retained_job *j,
 u32 action_offset,u32 offset,struct remesa_gem_scene_controls *out,unsigned indices[5])
{
 struct remesa_scene_action_inputs action;
 struct remesa_gem_scene_controls value={0};
 struct remesa_gem_scene_control_command *c=&value.command;
 struct backing *bo[5];u32 ids[5],depth[6],addresses[3],offsets[3],sizes[3];
 unsigned i,k;int error;
 error=scene_action_inputs(s,j,action_offset,&action);if(error)return error;
 if(j->region_mask!=((u32)1<<action.region_index))return -EACCES;
 if((offset&3)||offset>j->command_bytes||sizeof(*c)>j->command_bytes-offset)return -EINVAL;
 /* Distinct typed sections cannot overlap, even if bytes could satisfy both. */
 if(offset<=action_offset?action_offset-offset<sizeof(*c):offset-action_offset<sizeof(action.command))return -EINVAL;
 memcpy(c,(const u8 *)j->commands+offset,sizeof(*c));
 if(c->schema!=REMESA_GEM_SCENE_CONTROL_SCHEMA||c->bytes!=sizeof(*c)||c->reserved||
    c->width!=action.command.width||c->height!=action.command.height)return -EINVAL;
 if(!c->color_pitch||!c->depth_pitch||c->width>c->color_pitch||c->width>c->depth_pitch||
    (c->color_pitch&31)||(c->depth_pitch&31)||c->color_pitch>2048||c->depth_pitch>2048||
    (c->depth_cpp!=2&&c->depth_cpp!=4))return -EINVAL;
 /* IEEE positive finite0..1 plus negative zero, accepted by the old producer.
  * Preserve exact bits; no floating-point instruction is used in the kernel. */
 if(c->clear_depth_bits>0x3f800000U&&c->clear_depth_bits!=0x80000000U)return -EINVAL;
 ids[0]=c->color_id;ids[1]=c->depth_id;ids[2]=c->ta_id;ids[3]=c->background_id;ids[4]=c->event_id;
 for(i=0;i<5;i++){
  if(!ids[i])return -EINVAL;
  for(k=0;k<i;k++)if(ids[k]==ids[i])return -EINVAL;
  for(k=0;k<j->count&&j->ids[k]!=ids[i];k++);
  if(k==j->count)return -ENOENT;
  indices[i]=k;
  if((j->dpm_mask|j->region_mask)&((u32)1<<k))return -EACCES;
  bo[i]=&s->bo[ids[i]&31];
  if(!bo[i]->live||bo[i]->generation!=ids[i]>>5||bo[i]->holds!=1||bo[i]->lease||
     bo[i]->quarantine||!bo[i]->pinned||!bo[i]->reserved||!bo[i]->pages||
     bo[i]->inserted!=bo[i]->gem.size/PAGE_SIZE)return -EACCES;
 }
 for(i=0;i<2;i++)if(bo[i]->va.start<0x40000000ULL||bo[i]->va.start>0x100000000ULL-bo[i]->gem.size)return -ERANGE;
 if((bo[0]->va.start&3)||(bo[1]->va.start&15))return -EINVAL;
 if(bo[0]->gem.size<(u64)c->color_pitch*c->height*4||
    bo[1]->gem.size<(u64)c->depth_pitch*((c->height+31)&~31u)*c->depth_cpp)return -ENOSPC;
 offsets[0]=c->ta_offset;offsets[1]=c->background_offset;offsets[2]=c->event_offset;
 sizes[0]=c->ta_bytes;sizes[1]=c->background_bytes;sizes[2]=c->event_bytes;
 for(i=0;i<3;i++){
  u64 low=i==0?1:i==1?0x30000000ULL:0x20000000ULL;
  u64 high=i==0?0x100000000ULL:low+0x10000000ULL;
  u32 minimum=i==0?4:i==1?68:148;
  if(bo[i+2]->va.start<low||bo[i+2]->va.start>=high||bo[i+2]->gem.size>high-bo[i+2]->va.start)return -ERANGE;
  if(offsets[i]>bo[i+2]->gem.size||sizes[i]<minimum||sizes[i]>bo[i+2]->gem.size-offsets[i]||
     (i==0&&(sizes[i]&3)))return -ENOSPC;
  addresses[i]=bo[i+2]->va.start+offsets[i];
  if(addresses[i]&(i==0?3:15))return -EINVAL;
 }
 depth[0]=(((c->depth_pitch>>4)-1)<<8)|(c->depth_cpp==4?0x50000:0xa0000);
 depth[1]=bo[1]->va.start&0xf0000000U;
 for(i=2;i<6;i++)depth[i]=bo[1]->va.start&0x0ffffff0U;
 remesa_scene_raster_words(((c->width>>4)<<16)|(c->height>>4),addresses[1],addresses[2],16,
  depth,c->clear_depth_bits,0,value.raster);
 remesa_scene_ta_words(addresses[0],value.ta);
 value.action_offset=action_offset;value.command_offset=offset;value.binding_count=j->count;
 *out=value;return 0;
}
int remesa_gem_scene_controls_prepare(struct remesa_gem_memory *s,
 struct remesa_memory_ticket t,u32 action_offset,u32 command_offset,
 struct remesa_gem_scene_controls *out)
{
 struct remesa_gem_scene_controls value;struct retained_job *j;unsigned indices[5];int error=-ENOENT;
 if(!s||!out)return -EINVAL;
 mutex_lock(&s->lock);j=ticket_job(s,t);if(!j)goto done;
 if(j->state!=REMESA_JOB_PREPARED||j->attached||j->cookie||j->quarantined){error=-EBUSY;goto done;}
 error=scene_control_build(s,j,action_offset,command_offset,&value,indices);if(error)goto done;
 value.ticket=t;*out=value;
 done:mutex_unlock(&s->lock);return error;
}
int remesa_gem_scene_controls_check_locked(struct remesa_gem_private *loan,
 const struct remesa_memory_view *views,unsigned count,const struct remesa_gem_scene_actions *actions,
 const struct remesa_gem_scene_controls *plan)
{
 struct remesa_gem_scene_controls expected;struct retained_job *j;unsigned indices[5],i;int error;
 if(!plan||!actions)return -EINVAL;
 error=remesa_gem_scene_actions_check_locked(loan,views,count,actions);if(error)return error;
 if(plan->action_offset!=actions->command_offset||plan->binding_count!=count||
    plan->ticket.context!=loan->ticket.context||plan->ticket.operation!=loan->ticket.operation||
    plan->ticket.slot!=loan->ticket.slot||plan->ticket.generation!=loan->ticket.generation)return -EACCES;
 j=ticket_job(loan->memory,loan->ticket);
 error=scene_control_build(loan->memory,j,plan->action_offset,plan->command_offset,&expected,indices);if(error)return error;
 for(i=0;i<5;i++)if(loan->rules[indices[i]].access!=(i<2?3u:1u)||loan->rules[indices[i]].cached!=1)return -EACCES;
 expected.ticket=loan->ticket;
 if(memcmp(&plan->command,&expected.command,sizeof(plan->command))||
    memcmp(plan->raster,expected.raster,sizeof(plan->raster))||memcmp(plan->ta,expected.ta,sizeof(plan->ta)))return -EACCES;
 return 0;
}
