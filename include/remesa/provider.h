/* SPDX-License-Identifier: MIT
 * Portable provider lifecycle core; not a Linux driver or public ioctl ABI.
 * Every call and backend callback runs under one caller-owned lock. Callbacks
 * must not re-enter this API. Context identity must be unique across surviving
 * providers/fences; initialization requires fresh storage and a fresh namespace. Backend completion receipts must be authenticated
 * by the integrating provider; this core checks identities, not their origin.
 */
#ifndef REMESA_PROVIDER_H
#define REMESA_PROVIDER_H
#include "remesa/registry.h"
/* Host job storage bound, matching the retained32-slot allocation graph.
 * This grants no kernel/native admission or new graphics capability. */
#define REMESA_PROVIDER_MAX_USES 32
#define REMESA_PROVIDER_MAX_JOBS 8
enum remesa_access { REMESA_ACCESS_READ = 1, REMESA_ACCESS_WRITE = 2 };
enum remesa_dispatch { REMESA_NOT_ISSUED, REMESA_ISSUED, REMESA_ISSUANCE_AMBIGUOUS };
enum remesa_fence_state { REMESA_FENCE_PENDING, REMESA_FENCE_RETIRED,
                          REMESA_FENCE_PUBLISHED, REMESA_FENCE_FAILED };
struct remesa_use { struct remesa_token token; uint64_t offset, bytes; unsigned access; };
struct remesa_fence { size_t slot; uint64_t generation, context; };
struct remesa_job {
    uint64_t context, operation, generation;
    struct remesa_use uses[REMESA_PROVIDER_MAX_USES];
    size_t count;
    enum remesa_fence_state state;
    unsigned live;
};
struct remesa_backend {
    /* Descriptors remain valid only during this call. NOT_ISSUED guarantees
     * no device access occurred; anything uncertain must return AMBIGUOUS. */
    enum remesa_dispatch (*dispatch)(void *, const struct remesa_job *);
    /* Called only for retired jobs. Success asserts all uses can be reused:
     * relevant device readers/writers have stopped and mapping is retained.
     * REMESA_RETRY explicitly guarantees no acknowledgement or release side
     * effect: retain the retired job/holds and retry only release. Every other
     * error preserves the existing fail-closed quarantine contract. */
    int (*release)(void *, const struct remesa_job *);
    void *data;
};
struct remesa_provider {
    struct remesa_registry *registry;
    struct remesa_backend backend;
    uint64_t context, next_operation;
    struct remesa_job jobs[REMESA_PROVIDER_MAX_JOBS];
};
int remesa_provider_init(struct remesa_provider *, struct remesa_registry *,
                         uint64_t context, const struct remesa_backend *);
/* REMESA_AMBIGUOUS returns a valid failed fence and retains all resources;
 * it is never a retryable BUSY result. Other errors leave *out unchanged. */
int remesa_provider_submit(struct remesa_provider *, const struct remesa_use *,
                           size_t count, struct remesa_fence *);
int remesa_provider_complete(struct remesa_provider *, struct remesa_fence,
                             uint64_t context, uint64_t operation, int success);
/* Visibility is a separate authenticated backend guarantee, never inferred
 * from completion. Only writable resources receive publication state. */
int remesa_provider_publish(struct remesa_provider *, struct remesa_fence,
                            uint64_t context, uint64_t operation);
/* Authenticated terminal error, including a fault after retirement but before
 * safe release. Holds/mappings remain quarantined; this is not a reset API. */
int remesa_provider_fault(struct remesa_provider *, struct remesa_fence,
                         uint64_t context, uint64_t operation);
int remesa_provider_query(struct remesa_provider *, struct remesa_fence,
                          struct remesa_job *snapshot);
/* Successful release returns resources to MAPPED and invalidates the fence.
 * Failed/ambiguous jobs cannot be released without a future reset contract. */
int remesa_provider_release(struct remesa_provider *, struct remesa_fence);
#endif
