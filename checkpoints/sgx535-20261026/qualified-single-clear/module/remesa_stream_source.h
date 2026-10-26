/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_STREAM_SOURCE_H
#define REMESA_STREAM_SOURCE_H
#include "gma500_capsule_observer.h"
#include "frozen_capsule.h"
struct task_struct;
#define REMESA_STREAM_AUDIT_EVENTS 64u
struct remesa_stream_source_event {u32 kind,sequence,status1,status2,ledger,phase;};
/* Separate allocation for every operation. Never aliases the legacy capsule. */
struct remesa_stream_observation {
 struct sgx535_capsule evidence;
 struct sgx535_source_guard boundary;
 struct sgx535_gma500_fixed_backend *backend;
 struct task_struct *task;
 u64 operation,sequence;
 bool finished;
 u32 event_count,zero_samples;
 struct remesa_stream_source_event events[REMESA_STREAM_AUDIT_EVENTS];
 u64 event_ns[REMESA_STREAM_AUDIT_EVENTS];
};
int remesa_stream_source_claim(struct drm_device *,const void *);
int remesa_stream_source_release(const void *);
int remesa_stream_source_bind(const void *,struct remesa_stream_observation *,
 struct sgx535_gma500_fixed_backend *,u64,u64);
int remesa_stream_source_finish(const void *,struct remesa_stream_observation *,const void *);
int remesa_stream_source_finish_nohash(const void *,struct remesa_stream_observation *);
int remesa_stream_source_unbind(const void *,struct remesa_stream_observation *);
void remesa_stream_source_poison(const void *);
int remesa_stream_source_settled(const void *);
int remesa_stream_source_owned(struct sgx535_gma500_fixed_backend *);
int remesa_route_source_begin(struct sgx535_gma500_fixed_backend *,
    struct drm_device *);
int remesa_route_source_boundary(struct sgx535_gma500_fixed_backend *,
    const struct sgx535_source_facts *);
void remesa_route_source_producer(int);
void remesa_route_source_attach(struct drm_device *, struct module *,
    sgx535_source_reader);
void remesa_route_source_observe(struct drm_device *, sgx535_source_snapshot_fn);
void remesa_route_source_startup(struct drm_device *,
    const struct sgx535_source_facts *);
void remesa_route_source_reset_begin(struct drm_device *);
void remesa_route_source_reset_assert(struct drm_device *, u32);
void remesa_route_source_reset_end(struct drm_device *,
    const struct sgx535_source_facts *);
void remesa_route_source_lifecycle(struct drm_device *);
void remesa_route_source_pending(struct drm_device *, u32, u32);
int remesa_route_source_write(struct sgx535_gma500_fixed_backend *);

void remesa_route_admit(struct sgx535_gma500_fixed_backend *);
void remesa_route_service(struct sgx535_gma500_fixed_backend *,
    struct sgx535_fixed_service *, int);
void remesa_route_issued(struct sgx535_gma500_fixed_backend *);
void remesa_route_sample(struct sgx535_gma500_fixed_backend *, u32,
    u32, u32, int);
void remesa_route_before(struct sgx535_gma500_fixed_backend *,
    struct sgx535_fixed_service *, u32, u32, u32, int);
void remesa_route_after(struct sgx535_gma500_fixed_backend *,
    struct sgx535_fixed_service *);
void remesa_route_terminal(struct sgx535_gma500_fixed_backend *,
    struct sgx535_fixed_service *, int);

#endif
