/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_GEM_SCENE_JOB_H
#define REMESA_GEM_SCENE_JOB_H
#include "remesa_gem_scene_event.h"
#include "remesa_scene_service.h"
struct remesa_gem_scene_sections {u32 actions,controls,use,vertices;};
enum remesa_gem_scene_job_state {REMESA_SCENE_JOB_EMPTY,REMESA_SCENE_JOB_PREPARED,REMESA_SCENE_JOB_HELD,REMESA_SCENE_JOB_ACKNOWLEDGED};
/* Caller allocates/zeros this stable context before endpoint/MMU locks. It
 * owns actual job holds/plans/views/loan, never eleven synthetic BO roles.
 * No stack-local context: retain on HELD, even when preparation fails. */
struct remesa_gem_scene_job {
 struct remesa_gem_memory *memory;enum remesa_gem_scene_job_state state;
 unsigned count;struct remesa_gem_scene_sections sections;
 struct remesa_memory_snapshot snapshot;
 struct remesa_memory_view views[REMESA_GEM_JOB_BINDINGS];
 struct remesa_gem_private loan;
 struct remesa_gem_scene_actions actions;
 struct remesa_gem_scene_controls controls;
 struct remesa_gem_scene_use use;
 struct remesa_gem_scene_ta ta;
 struct remesa_gem_scene_states states;
 struct remesa_gem_scene_vertices vertices;
 struct remesa_gem_scene_fragments fragments;
 struct remesa_gem_scene_background background;
 struct remesa_gem_scene_event event;
 struct sgx535_frozen_session session;
 struct remesa_scene_service_plan service_plan;
 struct remesa_scene_service service;
};
/* Connect immutable sections and actual held objects to existing preparers.
 * Guard already held by caller; no MMU bind, source claim, issue, CPU buffer
 * initialization or diagnostic scan. Exact input/command authority remains
 * the trusted submitter's responsibility. Publication/retirement are separate. */
int remesa_gem_scene_job_prepare(struct remesa_gem_memory *,
 const struct remesa_memory_binding *,unsigned,const void *,unsigned,
 const struct remesa_gem_scene_sections *,const struct remesa_gem_private_rule *,
 struct remesa_twod_reservation *,struct remesa_gem_scene_job *);
/* Same endpoint/MMU locks and bound root as the existing closure checker.
 * Binds the actual prepared plans; does not start a service or certify source
 * ownership/retirement. Caller authenticates the immutable input construction. */
int remesa_gem_scene_job_bind_service_locked(struct remesa_gem_scene_job *);
/* Cancel only unissued/unbound preparation. Failure retains the context and
 * holds; never infer retirement/root restoration or release source state. */
int remesa_gem_scene_job_cancel(struct remesa_gem_scene_job *);
/* Endpoint mutex, default mappings still active; no MMU writer held. */
int remesa_gem_scene_job_check_snapshot_locked(struct remesa_gem_scene_job *);
struct sgx535_gma500_fixed_backend;struct remesa_stream_observation;
/* Trusted cold transaction. Caller retains DRM/backing-session/device/power,
 * source lease and immutable complete images. Any consumed failure retains all
 * context/loan/source state; no retry/free. No registration or ioctl added. */
int remesa_gem_scene_job_execute(struct remesa_gem_scene_job *,
 struct sgx535_gma500_fixed_backend *,struct remesa_stream_observation *,const void *,unsigned,unsigned);
/* After real acknowledged execution only; separate from preparation cancel. */
int remesa_gem_scene_job_retire(struct remesa_gem_scene_job *);
#endif
