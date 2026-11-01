/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_GEM_SCENE_BACKGROUND_H
#define REMESA_GEM_SCENE_BACKGROUND_H
#include "remesa_gem_scene_fragments.h"
struct remesa_gem_scene_background {
 struct remesa_memory_ticket ticket;
 u32 background_id,background_offset,primary_id,primary_offset,secondary_id,secondary_offset,
     color_id,use_id,use_offset,expected_pds_window;
};
/* Same endpoint/MMU locks and bound private root. Caller-retained scratch;
 * final output transactional. Selected background68/primary64/secondary4 and
 * PIXEL USE8 window only; not shader-body/unknown-field/texture-layout or full
 * scene qualification. No admission or GPU operation. */
int remesa_gem_scene_background_check_locked(struct remesa_gem_private *,
 const struct remesa_memory_view *,unsigned,const struct remesa_gem_scene_actions *,
 const struct remesa_gem_scene_controls *,const struct remesa_gem_scene_use *,u32,
 struct remesa_gem_scene_ta *,struct remesa_gem_scene_states *,struct remesa_gem_scene_vertices *,
 struct remesa_gem_scene_fragments *,struct remesa_gem_scene_background *);
#endif
