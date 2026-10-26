/* SPDX-License-Identifier: MIT */
#ifndef REMESA_LAB_SCENE_ACTIONS_H
#define REMESA_LAB_SCENE_ACTIONS_H
#include "../psb-dri-re/frozen_kernel_contract.h"
/* CPU construction only, fresh rev121 option0 scene, hardware context0.
 * This does not install a native backend, validate object ownership, authorize
 * MMIO, or prove that any polling/completion will succeed on hardware.
 * TA: initial context control0, flags4. Raster: after that TA, flags15,
 * cookie14=0. OOM/resume/multiple-context modes remain unsupported.
 * Canonical scene layout is derived internally; unknown cookie15 is unused.
 * The caller must bind live owned REGION backing and preserve its lifetime.
 */
struct remesa_lab_scene_fire_request {
 sgx535_u32 width,height;
 sgx535_u64 region_address,region_bytes;
};
enum remesa_lab_scene_engine { REMESA_LAB_SCENE_TA,REMESA_LAB_SCENE_RASTER };
#define REMESA_LAB_TA_FIRE_ACTIONS 31u
#define REMESA_LAB_RASTER_FIRE_ACTIONS 20u
/* Output capacity counts actions, not bytes. All arguments must be disjoint.
 * No output or written count changes on error. */
int remesa_lab_scene_fire_encode(const struct remesa_lab_scene_fire_request *,
 enum remesa_lab_scene_engine,struct sgx535_frozen_reg_action *,size_t capacity,
 size_t *written);

#define REMESA_LAB_DPM_LOAD_ACTIONS 29u
struct remesa_lab_dpm_load_request {
 sgx535_u64 table_address,table_bytes,parameter_address,parameter_bytes;
};
/* Kernel-buildable DPM load action constructor. Same fixed32MiB profile and
 * strict15 completion policy as the retained native executor. No execution.
 * All regions must be disjoint; failed encoding changes no output. */
int remesa_lab_dpm_actions_encode(const struct remesa_lab_dpm_load_request *,
 struct sgx535_frozen_reg_action *,size_t capacity,size_t *written);
#endif
