/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_GEM_SCENE_FRAGMENTS_H
#define REMESA_GEM_SCENE_FRAGMENTS_H
#include "remesa_gem_scene_vertices.h"
struct remesa_gem_scene_fragments {
 struct remesa_memory_ticket ticket;u32 expected_pds_window,count;
 struct {u32 primary_id,primary_offset,primary_bytes,secondary_id,secondary_offset,use_id,use_offset;} draws[REMESA_GEM_SCENE_VERTEX_MAX];
};
/* Existing endpoint/MMU locks and bound root. Scratch plans caller-retained;
 * FRAGMENT output transactional. Resolve selected group40/PDS known fields and
 * CODE to READ/cache1 backing plus a held PIXEL USE8 entry. Expected window200M
 * comes from existing gma500 initialization, NOT an MMIO observation: executor
 * must establish/verify that global PDS base under source ownership before issue.
 * No shader-body certification, unknown-DATA qualification, background/event
 * closure, source admission or GPU operation is established by this checker. */
int remesa_gem_scene_fragments_check_locked(struct remesa_gem_private *,
 const struct remesa_memory_view *,unsigned,const struct remesa_gem_scene_actions *,
 const struct remesa_gem_scene_controls *,const struct remesa_gem_scene_use *,u32 vertex_offset,
 struct remesa_gem_scene_ta *,struct remesa_gem_scene_states *,struct remesa_gem_scene_vertices *,
 struct remesa_gem_scene_fragments *);
#endif
