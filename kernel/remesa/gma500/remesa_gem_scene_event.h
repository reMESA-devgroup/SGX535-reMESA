/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_GEM_SCENE_EVENT_H
#define REMESA_GEM_SCENE_EVENT_H
#include "remesa_gem_scene_background.h"
struct remesa_gem_scene_event {
 struct remesa_memory_ticket ticket;
 u32 event_id,event_offset,target_id,target_offset,transfer_id,transfer_offset,end_id,end_offset;
};
/* Existing endpoint/MMU/private root locks; disjoint caller-retained scratch
 * and final output. Selected event148/target16/USE16+8 and preceding scene
 * contracts only, not general ISA/unknown-field/runtime or issue admission. */
int remesa_gem_scene_event_check_locked(struct remesa_gem_private *,const struct remesa_memory_view *,unsigned,
 const struct remesa_gem_scene_actions *,const struct remesa_gem_scene_controls *,const struct remesa_gem_scene_use *,u32,
 struct remesa_gem_scene_ta *,struct remesa_gem_scene_states *,struct remesa_gem_scene_vertices *,
 struct remesa_gem_scene_fragments *,struct remesa_gem_scene_background *,struct remesa_gem_scene_event *);
#endif
