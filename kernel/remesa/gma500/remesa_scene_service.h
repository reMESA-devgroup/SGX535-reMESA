/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_SCENE_SERVICE_H
#define REMESA_SCENE_SERVICE_H
#include "frozen_fixed_service.h"
/* Kernel-owned plans from the actual retained graph. No eleven-role BO array
 * or legacy XHW wire. CPU metadata stages consume these plans; MMIO stages
 * consume the unchanged original action/control producers' results. */
struct remesa_scene_service_use {sgx535_u32 mask;struct {sgx535_u32 offset,word;} banks[16];};
struct remesa_scene_service_plan {
 sgx535_u32 binding_count,load_count,ta_count,raster_count;
 struct sgx535_frozen_command_pairs commands;
 struct sgx535_frozen_reg_action load[29],ta[31],raster[20];
 struct remesa_scene_service_use use;
};
struct remesa_scene_fire_payload {struct sgx535_frozen_reg_action actions[31];sgx535_u32 action_count;};
struct remesa_scene_service;
struct remesa_scene_service_observer {
 void (*begin)(void *,struct remesa_scene_service *,const struct sgx535_fixed_ops *,const struct sgx535_fixed_status_source *,void *);
 void (*before)(void *,struct remesa_scene_service *,sgx535_u32,sgx535_u32,sgx535_u32,int);
 void (*after)(void *,struct remesa_scene_service *);
 void (*terminal)(void *,struct remesa_scene_service *,int);
};
struct remesa_scene_service {
 struct sgx535_frozen_session *session;
 const struct remesa_scene_service_plan *plan;
 struct sgx535_frozen_command_pairs commands;
 struct remesa_scene_service_use use_plan;
 struct sgx535_frozen_bootstrap boot;
 sgx535_u32 started,unsafe_possible,use_owned;
 struct sgx535_fixed_diagnostic diagnostic;
 const struct remesa_scene_service_observer *observer;void *observer_context;
};
/* Trusted retained plan/session lifetimes and external exclusion required.
 * Binding/retired phase alone is never a GPU/source/ownership certificate. */
int remesa_scene_service_bind(struct remesa_scene_service *,struct sgx535_frozen_session *,const struct remesa_scene_service_plan *);
int remesa_scene_service_prepare(struct remesa_scene_service *,const struct sgx535_fixed_ops *,void *,sgx535_u32);
int remesa_scene_service_fire_ta(struct remesa_scene_service *,const struct sgx535_fixed_ops *,void *,sgx535_u32,sgx535_u32);
int remesa_scene_service_status(struct remesa_scene_service *,const struct sgx535_fixed_ops *,void *,sgx535_u32,sgx535_u32,sgx535_u32,int);
int remesa_scene_service_timeout(struct remesa_scene_service *);
int remesa_scene_service_retire(struct remesa_scene_service *,const struct sgx535_fixed_ops *,void *);
int remesa_scene_service_may_release(const struct remesa_scene_service *);
int remesa_scene_service_run_once(struct remesa_scene_service *,const struct sgx535_fixed_ops *,const struct sgx535_fixed_status_source *,void *,sgx535_u32,sgx535_u32,sgx535_u32,sgx535_u32);
#endif
