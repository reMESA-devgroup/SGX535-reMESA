/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SGX535_GMA500_CAPSULE_OBSERVER_H
#define SGX535_GMA500_CAPSULE_OBSERVER_H
#include "gma500_fixed_backend.h"
#include "frozen_source_guard.h"

/* Passive source evidence; failed/unknown guard blocks backend admission.
 * Producer taps observe existing work; they neither cause nor stop that work. */
int sgx535_provenance_source_begin(struct sgx535_gma500_fixed_backend *,
    struct drm_device *);
int sgx535_provenance_source_boundary(struct sgx535_gma500_fixed_backend *,
    const struct sgx535_source_facts *);
void sgx535_provenance_source_producer(int);
struct module;
typedef void (*sgx535_source_reader)(struct drm_device *);
typedef void (*sgx535_source_snapshot_fn)(struct drm_device *,
    struct sgx535_source_facts *);
void sgx535_provenance_source_attach(struct drm_device *, struct module *,
    sgx535_source_reader);
void sgx535_provenance_source_observe(struct drm_device *, sgx535_source_snapshot_fn);
void sgx535_provenance_source_startup(struct drm_device *,
    const struct sgx535_source_facts *);
void sgx535_provenance_source_reset_begin(struct drm_device *);
void sgx535_provenance_source_reset_assert(struct drm_device *, u32);
void sgx535_provenance_source_reset_end(struct drm_device *,
    const struct sgx535_source_facts *);
void sgx535_provenance_source_lifecycle(struct drm_device *);
void sgx535_provenance_source_pending(struct drm_device *, u32, u32);
int sgx535_provenance_source_write(struct sgx535_gma500_fixed_backend *);

/* Explicit native kernel-operation binding; does not render or rearm.
 * Request/response are immutable kernel snapshots, never userspace pointers.
 * Caller must have validated its bounded native asset and consumed max-one
 * policy before call. Close records the actual ioctl return separately from
 * the operation errno inside the response. */
int sgx535_provenance_native_call(struct drm_device *, struct module *,
    const void *operation, const void *request, size_t bytes);
int sgx535_provenance_native_close(struct drm_device *, const void *operation,
    int ioctl_result, const void *response, size_t bytes);

/* Passive evidence calls only: none returns an execution decision. */
void sgx535_provenance_admit(struct sgx535_gma500_fixed_backend *);
void sgx535_provenance_service(struct sgx535_gma500_fixed_backend *,
    struct sgx535_fixed_service *, int);
void sgx535_provenance_issued(struct sgx535_gma500_fixed_backend *);
void sgx535_provenance_sample(struct sgx535_gma500_fixed_backend *, u32,
    u32, u32, int);
void sgx535_provenance_before(struct sgx535_gma500_fixed_backend *,
    struct sgx535_fixed_service *, u32, u32, u32, int);
void sgx535_provenance_after(struct sgx535_gma500_fixed_backend *,
    struct sgx535_fixed_service *);
void sgx535_provenance_terminal(struct sgx535_gma500_fixed_backend *,
    struct sgx535_fixed_service *, int);
void sgx535_provenance_color(struct sgx535_gma500_owner *, const void *);
void sgx535_provenance_release(struct sgx535_gma500_owner *);
#endif
