/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_GEM_SCENE_USE_H
#define REMESA_GEM_SCENE_USE_H
#include "remesa_gem_scene_controls.h"
#include "remesa_scene_use_limits.h"
#define REMESA_GEM_SCENE_USE_SCHEMA 1u
/* Kernel-only immutable command section followed by entry_count records.
 * No GPU address, register word, raw CODE or arbitrary hardware operation.
 * Window metadata alone does not authenticate actual instruction contents. */
struct remesa_gem_scene_use_command {u32 schema,bytes,entry_count,reserved;};
struct remesa_gem_scene_use_entry {u32 id,byte_offset,program_bytes,bank,data_master,reserved;};
struct remesa_gem_scene_use {
 struct remesa_memory_ticket ticket;
 u32 action_offset,control_offset,command_offset,binding_count,entry_count,bank_mask;
 struct {u32 offset,word;} banks[16];
 u32 lower19[REMESA_SCENE_USE_MAX];
};
/* Prepared/unattached job, after REGION reservation. Derives all window/bank
 * words from held allocation IDs plus immutable bounded program windows.
 * No bank lease/programming, image validation, publication or execution. */
int remesa_gem_scene_use_prepare(struct remesa_gem_memory *,struct remesa_memory_ticket,
 u32 action_offset,u32 control_offset,u32 command_offset,struct remesa_gem_scene_use *);
/* Existing endpoint/MMU locks and bound private root. Requires the matching
 * action/control plans, exact READ/cache1 program backing and complete loan.
 * Does not certify that the list covers every program referenced by TA/PDS;
 * that requires the subsequent complete image/reference validation gate. */
int remesa_gem_scene_use_check_locked(struct remesa_gem_private *,
 const struct remesa_memory_view *,unsigned,const struct remesa_gem_scene_actions *,
 const struct remesa_gem_scene_controls *,const struct remesa_gem_scene_use *);
#endif
