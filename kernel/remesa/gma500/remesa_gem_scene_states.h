/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_GEM_SCENE_STATES_H
#define REMESA_GEM_SCENE_STATES_H
#include "remesa_gem_scene_ta.h"
#define REMESA_GEM_SCENE_STATE_MAX (REMESA_TA_REFERENCE_MAX_DRAWS+3u)
struct remesa_gem_scene_states {
 struct remesa_memory_ticket ticket;u32 count;
 struct {u32 pds_id,pds_offset,source_id,source_offset,source_bytes,use_id,use_offset;} uploads[REMESA_GEM_SCENE_STATE_MAX];
};
/* Same endpoint/MMU locks/private-root requirement as TA check. Caller supplies
 * stable TA scratch (updated by TA check); STATE output stays unchanged on any
 * failure. Validates selected ISP PDS60, bounded read-only DATA, matching held
 * vertex USE16 auxiliary/count, state headers and close extent. Unknown PDS
 * DATA cells remain unclaimed. No alloc/default-PD walk/hash/readback/MMIO.
 * Does not validate vertex/fragment/background/event programs, ISP payload
 * semantics or all indirect fetches, initialize surfaces or admit execution. */
int remesa_gem_scene_states_check_locked(struct remesa_gem_private *,
 const struct remesa_memory_view *,unsigned,const struct remesa_gem_scene_actions *,
 const struct remesa_gem_scene_controls *,const struct remesa_gem_scene_use *,
 struct remesa_gem_scene_ta *scratch,struct remesa_gem_scene_states *);
#endif
