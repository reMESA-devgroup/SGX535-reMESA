/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_GEM_SCENE_CONTROLS_H
#define REMESA_GEM_SCENE_CONTROLS_H
#include "remesa_gem_scene_actions.h"
#define REMESA_GEM_SCENE_CONTROL_SCHEMA 1u
#define REMESA_GEM_SCENE_CONTROL_FINALIZED_SCHEMA 2u
/* Kernel-only immutable job section, not an ioctl ABI. All IDs must belong
 * to this job. Byte counts are committed object extents, not spare capacity.
 * Selected profile: linear BGRA8, undefined Z, no stencil; schema1
 * requires depth_finalize=0, schema2 requires depth_finalize=1 and actual Z,
 * fixed event field0c=16. clear_depth_bits is validated IEEE binary32 DATA. */
struct remesa_gem_scene_control_command {
 u32 schema,bytes,width,height,color_pitch,depth_pitch,depth_cpp,clear_depth_bits;
 u32 color_id,depth_id,ta_id,ta_offset,ta_bytes;
 u32 background_id,background_offset,background_bytes;
 u32 event_id,event_offset,event_bytes,depth_finalize;
};
struct remesa_gem_scene_controls {
 struct remesa_memory_ticket ticket;
 u32 action_offset,command_offset,binding_count;
 struct remesa_gem_scene_control_command command;
 u32 raster[52],ta[14];
};
/* Prepared, unattached job with a previously prepared REGION reservation.
 * Derives address controls from actual held backing and immutable scalar DATA.
 * No image/program validation, initialization, MMIO or native admission.
 * Failure preserves output. */
int remesa_gem_scene_controls_prepare(struct remesa_gem_memory *,
 struct remesa_memory_ticket,u32 action_offset,u32 command_offset,
 struct remesa_gem_scene_controls *);
/* Same endpoint/MMU locks and bound private root as scene_actions_check_locked.
 * Requires the matching action plan and checks full private translation plus
 * control roles, permissions and exact reconstructed register/value DATA.
 * Neither successful check authorizes execution by itself. */
int remesa_gem_scene_controls_check_locked(struct remesa_gem_private *,
 const struct remesa_memory_view *,unsigned,const struct remesa_gem_scene_actions *,
 const struct remesa_gem_scene_controls *);
#endif
