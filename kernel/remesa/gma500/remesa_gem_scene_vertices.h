/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_GEM_SCENE_VERTICES_H
#define REMESA_GEM_SCENE_VERTICES_H
#include "remesa_gem_scene_states.h"
#define REMESA_GEM_SCENE_VERTEX_SCHEMA 1u
#define REMESA_GEM_SCENE_VERTEX_MAX (REMESA_TA_REFERENCE_MAX_DRAWS+1u)
/* Immutable kernel-only job section, not ioctl ABI. Count includes the prefix.
 * Ranges describe committed DATA, not spare allocation or shader CODE. */
struct remesa_gem_scene_vertex_command {u32 schema,bytes,count,reserved;};
struct remesa_gem_scene_vertex_window {
 u32 index_id,index_offset,index_bytes,vertex_id,vertex_offset,vertex_bytes,vertex_count,reserved;
};
struct remesa_gem_scene_vertices {
 struct remesa_memory_ticket ticket;u32 count;
 struct {u32 index_id,index_offset,index_bytes,vertex_id,vertex_offset,vertex_bytes,
  stride_bytes,minimum_index,maximum_index,use_id,use_offset,use_bytes;} draws[REMESA_GEM_SCENE_VERTEX_MAX];
};
/* Same endpoint/MMU/private-root locks as states check. Caller retains both
 * scratch plans; only VERTEX output is transactional. Validates selected PDS64,
 * exact committed windows, actual u16 indices, source extent and a matching
 * held vertex USE16(prefix)/64(compiled) entry. Does NOT certify that shader
 * body, unknown PDS cells, fragment/background/event closure or execution.
 * No allocation, MMIO, pixel readback/hash, extra flush/wait or admission. */
int remesa_gem_scene_vertices_check_locked(struct remesa_gem_private *,
 const struct remesa_memory_view *,unsigned,const struct remesa_gem_scene_actions *,
 const struct remesa_gem_scene_controls *,const struct remesa_gem_scene_use *,u32 window_offset,
 struct remesa_gem_scene_ta *,struct remesa_gem_scene_states *,struct remesa_gem_scene_vertices *);
#endif
