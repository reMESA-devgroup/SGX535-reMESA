/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_GEM_SCENE_ACTIONS_H
#define REMESA_GEM_SCENE_ACTIONS_H
#include "remesa_gem_private.h"
#include "remesa_scene_actions.h"
/* Kernel-only section of immutable copied job commands, never an ioctl ABI.
 * Scalar dimensions/held IDs only; no caller GPU addresses or register lists.
 * Other native scene sections still need their own validation. */
#define REMESA_GEM_SCENE_ACTION_SCHEMA 1u
struct remesa_gem_scene_action_command {
 u32 schema,bytes,width,height,region_id,table_id,parameter_id,reserved;
};
struct remesa_gem_scene_actions {
 struct remesa_memory_ticket ticket;
 u32 command_offset,binding_count;
 struct remesa_gem_scene_action_command command;
 unsigned load_count,ta_count,raster_count;
 struct sgx535_frozen_reg_action load[REMESA_LAB_DPM_LOAD_ACTIONS];
 struct sgx535_frozen_reg_action ta[REMESA_LAB_TA_FIRE_ACTIONS];
 struct sgx535_frozen_reg_action raster[REMESA_LAB_RASTER_FIRE_ACTIONS];
};
/* Prepared/unattached job; a DPM claim must already cover its two pools.
 * Ordinary callers use the existing zero claim. The trusted general-session
 * preparer may use a distinct prior-retirement epoch checked again at closure.
 * Requires fresh-zero, unexposed exact region storage and reserves it GPU-only.
 * Builds original29/31/20 closed action groups from the actual held backing.
 * No image validation, allocation, map/clear/flush, root bind or MMIO/issue.
 * Failure leaves output/reservation unchanged. Caller retains job ownership;
 * plan is not a capability and becomes stale upon job cancellation/reuse. */
int remesa_gem_scene_actions_prepare(struct remesa_gem_memory *,
 struct remesa_memory_ticket,u32 command_offset,struct remesa_gem_scene_actions *);
/* Endpoint THEN MMU writer already held and private root bound. Revalidates
 * complete loan/views plus immutable commands, roles, R/W permissions and all
 * generated actions. No allocation, default-PD walk or hardware operation.
 * This gate does not establish initialization of other scene resources,
 * complete shaders/control, source ownership, ordering or execution admission.
 */
int remesa_gem_scene_actions_check_locked(struct remesa_gem_private *,
 const struct remesa_memory_view *,unsigned,const struct remesa_gem_scene_actions *);
#endif
