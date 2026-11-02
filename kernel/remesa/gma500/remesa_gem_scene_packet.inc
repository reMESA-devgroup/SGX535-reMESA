/* SPDX-License-Identifier: GPL-2.0-only */
/* Capture metadata to actual GEM preparation. No ioctl/admission override. */
#ifdef __KERNEL__
#include "remesa_scene_packet.h"
#else
#include "remesa/scene_packet.h"
#endif
struct remesa_scene_packet_storage {
 struct remesa_memory_binding bindings[REMESA_GEM_JOB_BINDINGS];
 struct remesa_gem_private_rule rules[REMESA_GEM_JOB_BINDINGS];
 u8 commands[REMESA_SCENE_PACKET_COMMAND_MAX];
};
static int scene_packet_prepare_owned(struct remesa_gem_memory *s,
 const struct remesa_scene_packet *p,struct remesa_twod_reservation *guard,struct remesa_gem_scene_job *c,const void *session_owner)
{
 struct remesa_scene_packet_storage *storage;struct remesa_scene_packet_sections offsets;
 struct remesa_gem_scene_sections sections;unsigned bytes,i;int error=0;
 /* All metadata words have the same size/order as the existing immutable
  * native sections. These copies never serialize addresses or register lists. */
 _Static_assert(sizeof(struct remesa_scene_packet_actions)==sizeof(struct remesa_gem_scene_action_command),"action metadata layout");
 _Static_assert(sizeof(struct remesa_scene_packet_controls)==sizeof(struct remesa_gem_scene_control_command),"control metadata layout");
 _Static_assert(sizeof(struct remesa_scene_packet_use)==sizeof(struct remesa_gem_scene_use_entry),"USE metadata layout");
 _Static_assert(sizeof(struct remesa_scene_packet_window)==sizeof(struct remesa_gem_scene_vertex_window),"vertex metadata layout");
 _Static_assert(REMESA_SCENE_PACKET_BINDINGS==REMESA_GEM_JOB_BINDINGS,"binding capacity");
 _Static_assert(REMESA_SCENE_PACKET_USES==REMESA_SCENE_USE_MAX,"USE capacity");
 _Static_assert(REMESA_SCENE_PACKET_WINDOWS==REMESA_GEM_SCENE_VERTEX_MAX,"window capacity");
 if(!s||!p||!guard||!c||!remesa_scene_packet_valid(p))return -EINVAL;
 if(p->owner!=s->context)return -EACCES;
 if(c->memory||c->state!=REMESA_SCENE_JOB_EMPTY)return -EBUSY;
 storage=kzalloc(sizeof(*storage),GFP_KERNEL);if(!storage)return -ENOMEM;
 if(!remesa_scene_packet_commands(p,storage->commands,sizeof(storage->commands),&bytes,&offsets)){error=-EINVAL;goto done;}
 mutex_lock(&s->lock);
 for(i=0;i<p->binding_count;i++){
  u32 id=p->bindings[i].id;struct backing *b=&s->bo[id&31];
  if(!b->live||b->generation!=id>>5){error=-ENOENT;break;}
  storage->bindings[i]=(struct remesa_memory_binding){id,b->va.start,b->gem.size};
  storage->rules[i]=(struct remesa_gem_private_rule){id,p->bindings[i].access,1};
 }
 mutex_unlock(&s->lock);if(error)goto done;
 sections=(struct remesa_gem_scene_sections){offsets.actions,offsets.controls,offsets.use,offsets.vertices};
 /* Existing prepare copies all commands and takes actual job/GEM holds.
  * Revalidation rejects topology changes between this snapshot and prepare. */
 error=scene_job_prepare_owned(s,storage->bindings,p->binding_count,storage->commands,bytes,&sections,storage->rules,guard,c,session_owner);
 done:kfree(storage);return error;
}
int remesa_gem_scene_packet_prepare(struct remesa_gem_memory *s,
 const struct remesa_scene_packet *p,struct remesa_twod_reservation *guard,struct remesa_gem_scene_job *c)
{return scene_packet_prepare_owned(s,p,guard,c,NULL);}
