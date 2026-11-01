/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_GEM_SCENE_TA_H
#define REMESA_GEM_SCENE_TA_H
#include "remesa_gem_scene_use.h"
#include "remesa_ta_references.h"
struct remesa_gem_scene_ta {
 struct remesa_memory_ticket ticket;
 u32 ta_id,ta_offset,ta_bytes,draw_count,reference_count;
 struct {u32 id,byte_offset,minimum_bytes,kind,index_count,draw;} references[REMESA_TA_REFERENCE_MAX];
};
/* Endpoint THEN MMU writer held, private root bound. Authenticates matching
 * action/control/USE plans, then parses ACTUAL retained TA CPU bytes and resolves
 * every reference into exact READ/cache1 job backing. No allocation, default-PD
 * walk, pixel readback/hash, synchronization or MMIO. Failure preserves output.
 * Minimum PDS spans are NOT full fetch/committed lengths. The next image/PDS
 * validation must establish those and all indirect USE/vertex/DMA references.
 * This is not native execution admission or an initialization certificate. */
int remesa_gem_scene_ta_check_locked(struct remesa_gem_private *,
 const struct remesa_memory_view *,unsigned,const struct remesa_gem_scene_actions *,
 const struct remesa_gem_scene_controls *,const struct remesa_gem_scene_use *,
 struct remesa_gem_scene_ta *);
#endif
