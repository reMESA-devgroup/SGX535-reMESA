#ifndef SGX535_GMA500_FIXED_BACKEND_H
#define SGX535_GMA500_FIXED_BACKEND_H

#include "gma500_bo_owner.h"

/* Exact-scene backend; installation and authorization are separate.
 * begin() refuses to wake a powered-off device. */
struct sgx535_gma500_fixed_backend {
    struct sgx535_gma500_owner *owner;
    u32 last_stage;
    u64 ta_fire_begin_ns,ta_fire_end_ns,raster_fire_begin_ns,raster_fire_end_ns,cpu_flush_ns;
    bool active;
    bool power_held;
    bool use_reserved;
    bool xhw_internal;
    bool fire_possible;
    bool isp_reset_asserted;
    u32 pending_status1;
    u32 pending_status2;
    unsigned long deadline;
};

int sgx535_gma500_fixed_backend_begin(
    struct sgx535_gma500_fixed_backend *backend,
    struct sgx535_gma500_owner *owner);
int sgx535_gma500_fixed_backend_end(
    struct sgx535_gma500_fixed_backend *backend);
extern const struct sgx535_fixed_ops sgx535_gma500_fixed_backend_ops;
extern const struct sgx535_fixed_status_source
    sgx535_gma500_fixed_backend_status_source;

/* Called around the exact antiX SGX IRQ read/capture/clear sequence. The
 * returned flags belong only to this local lock/unlock pair. */
unsigned long sgx535_gma500_fixed_irq_lock(void);
void sgx535_gma500_fixed_irq_capture_locked(struct drm_device *dev,
                                             u32 status1, u32 status2);
void sgx535_gma500_fixed_irq_unlock(unsigned long flags);
/* Read-only preparation callback; no admission, power wake or device writes. */
void sgx535_gma500_source_observe_passive(struct drm_device *);

#endif
