/* SPDX-License-Identifier: GPL-2.0-only. After scene USE/loan helpers. */
#include "remesa_gem_scene_ta.h"
struct scene_ta_resolver {
 struct remesa_gem_private *loan;const struct remesa_memory_view *views;
 unsigned count;struct remesa_gem_scene_ta *result;
};
static int scene_ta_resolve(void *context,const struct remesa_ta_reference *ref)
{
 struct scene_ta_resolver *r=context;unsigned i,n=r->result->reference_count;
 if(n>=REMESA_TA_REFERENCE_MAX)return E2BIG;
 for(i=0;i<r->count;i++){
  const struct remesa_memory_binding *b=&r->views[i].binding;u64 offset;
  if(ref->address<b->va)continue;
  offset=(u64)ref->address-b->va;
  if(offset>b->bytes||ref->minimum_bytes>b->bytes-offset)continue;
  if(r->loan->rules[i].access!=REMESA_MMU_GPU_READ||r->loan->rules[i].cached!=1)return EACCES;
  r->result->references[n].id=b->id;r->result->references[n].byte_offset=offset;
  r->result->references[n].minimum_bytes=ref->minimum_bytes;r->result->references[n].kind=ref->kind;
  r->result->references[n].index_count=ref->index_count;r->result->references[n].draw=ref->draw;
  r->result->reference_count++;return 0;
 }
 return ENOENT;
}
int remesa_gem_scene_ta_check_locked(struct remesa_gem_private *loan,
 const struct remesa_memory_view *views,unsigned count,const struct remesa_gem_scene_actions *actions,
 const struct remesa_gem_scene_controls *controls,const struct remesa_gem_scene_use *use,
 struct remesa_gem_scene_ta *out)
{
 struct remesa_gem_scene_ta value={0};struct remesa_ta_reference_counts counts;
 struct scene_ta_resolver resolver;const struct remesa_gem_scene_control_command *c;
 unsigned i;int error;
 if(!out)return -EINVAL;
 error=remesa_gem_scene_use_check_locked(loan,views,count,actions,controls,use);if(error)return error;
 c=&controls->command;
 for(i=0;i<count&&views[i].binding.id!=c->ta_id;i++);
 if(i==count||!views[i].cpu||loan->rules[i].access!=REMESA_MMU_GPU_READ||loan->rules[i].cached!=1)return -EACCES;
 resolver=(struct scene_ta_resolver){loan,views,count,&value};
 error=remesa_ta_references_walk((const u8 *)views[i].cpu+c->ta_offset,c->ta_bytes,
  scene_ta_resolve,&resolver,&counts);if(error)return -error;
 if(counts.references!=value.reference_count)return -EPROTO;
 value.ticket=loan->ticket;value.ta_id=c->ta_id;value.ta_offset=c->ta_offset;value.ta_bytes=c->ta_bytes;
 value.draw_count=counts.draws;*out=value;return 0;
}
