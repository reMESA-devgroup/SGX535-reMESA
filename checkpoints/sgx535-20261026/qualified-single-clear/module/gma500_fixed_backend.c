// SPDX-License-Identifier: GPL-2.0-only
/* Exact-scene backend. Registration is a separate patch; compiling this file
 * must never be interpreted as permission to run it. */
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/ktime.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/string.h>

#include "psb_drv.h"
#include "power.h"
#include "mmu.h"
#include "gma500_fixed_backend.h"
#include "remesa_stream_source.h"
#include "gma500_source_snapshot.h"
#include "frozen_fixed_io.h"

#include "remesa_stream_contract_api.h"
#define CONTRACT(name) (remesa_stream_source_owned(backend) ? remesa_stream_##name : name)

#define SGX535_FIXED_POLL_READS 300000U

static DEFINE_SPINLOCK(fixed_irq_lock);
static struct sgx535_gma500_fixed_backend *fixed_irq_owner;
#ifdef REMESA_LAB_BIF_DIAGNOSTIC
#include "remesa_bif_debug.h"
#include "remesa_bif_debug.inc"
#endif

static const u32 fixed_selected_status1 =
    (1U << 28) | (1U << 25) | (1U << 24) |
    (1U << 18) | (1U << 13) | (1U << 12) |
    (1U << 3) | (1U << 2) | (1U << 1) | 1U;
static const u32 fixed_status2_mask = 1U << 4;
/* The display driver owns the 2D completion bit. Master interrupt is an
 * aggregate indicator, not a scene completion event. All other new bits
 * are delivered to the ledger, which holds the scene on an unknown event. */
static const u32 fixed_benign_status1 = (1U << 31) | (1U << 27);

unsigned long sgx535_gma500_fixed_irq_lock(void)
{
    unsigned long flags;

    spin_lock_irqsave(&fixed_irq_lock, flags);
    return flags;
}

void sgx535_gma500_fixed_irq_capture_locked(struct drm_device *dev,
                                             u32 status1, u32 status2)
{
    struct sgx535_gma500_fixed_backend *backend = fixed_irq_owner;

    remesa_route_source_pending(dev, status1, status2);

    if (backend && backend->active && backend->fire_possible &&
        backend->owner->dev == dev) {
#ifdef REMESA_LAB_BIF_DIAGNOSTIC
        /* Shared IRQ/poll capture precedes the existing status clears. */
        bif_debug_irq_locked(backend,status1,status2);
#endif
        backend->pending_status1 |= status1 & ~fixed_benign_status1;
        backend->pending_status2 |= status2;
    }
}

void sgx535_gma500_fixed_irq_unlock(unsigned long flags)
{
    spin_unlock_irqrestore(&fixed_irq_lock, flags);
}

static void source_snapshot_locked(struct drm_device *dev,
    struct sgx535_source_facts *facts)
{
    sgx535_gma500_source_snapshot(dev->dev_private, facts);
}

void sgx535_gma500_source_observe_passive(struct drm_device *dev)
{
    unsigned long flags = sgx535_gma500_fixed_irq_lock();
    remesa_route_source_observe(dev, source_snapshot_locked);
    sgx535_gma500_fixed_irq_unlock(flags);
}

static int fixed_sample_and_ack(void *context, sgx535_u32 sequence,
                                sgx535_u32 *status1, sgx535_u32 *status2,
                                int *exclusive_owned)
{
    struct sgx535_gma500_fixed_backend *backend = context;
    struct drm_psb_private *dev_priv;
    unsigned long flags;
    u32 raw1, raw2;

    if (!backend || !backend->active || !backend->fire_possible ||
        !backend->power_held || !sequence || !status1 || !status2 ||
        !exclusive_owned ||
        backend->owner->session.sequence != sequence ||
        !gma_power_is_on(backend->owner->dev))
        return -ENODEV;
    if (time_after_eq(jiffies, backend->deadline))
        return -ETIMEDOUT;
    dev_priv = backend->owner->dev->dev_private;
    flags = sgx535_gma500_fixed_irq_lock();
    if (fixed_irq_owner != backend) {
        sgx535_gma500_fixed_irq_unlock(flags);
        return -EBUSY;
    }
    raw1 = PSB_RSGX32(PSB_CR_EVENT_STATUS);
    raw2 = PSB_RSGX32(PSB_CR_EVENT_STATUS2);
    sgx535_gma500_fixed_irq_capture_locked(backend->owner->dev, raw1, raw2);
    if (raw1 & fixed_selected_status1)
        PSB_WSGX32(raw1 & fixed_selected_status1,
                   PSB_CR_EVENT_HOST_CLEAR);
    if (raw2 & fixed_status2_mask)
        PSB_WSGX32(raw2 & fixed_status2_mask,
                   PSB_CR_EVENT_HOST_CLEAR2);
    *status1 = backend->pending_status1;
    *status2 = backend->pending_status2;
    backend->pending_status1 = 0;
    backend->pending_status2 = 0;
    sgx535_gma500_fixed_irq_unlock(flags);
    *exclusive_owned = 1;
    remesa_route_sample(backend, sequence, *status1, *status2, 1);
    if (!*status1 && !*status2) {
        if (time_after_eq(jiffies, backend->deadline))
            return -ETIMEDOUT;
        usleep_range(100, 200);
    }
    return 0;
}

const struct sgx535_fixed_status_source
    sgx535_gma500_fixed_backend_status_source = {
        fixed_sample_and_ack
};

static int fixed_write(void *context, sgx535_u32 offset, sgx535_u32 value)
{
    struct sgx535_gma500_fixed_backend *backend = context;
    struct drm_psb_private *dev_priv = backend->owner->dev->dev_private;

    if (!backend->power_held || !dev_priv->sgx_reg)
        return -ENODEV;
    if (backend->fire_possible &&
        time_after_eq(jiffies, backend->deadline))
        return -ETIMEDOUT;
    if (!remesa_route_source_write(backend))
        return -EAGAIN;
    PSB_WSGX32(value, offset);
    return 0;
}

static int fixed_read(void *context, sgx535_u32 offset, sgx535_u32 *value)
{
    struct sgx535_gma500_fixed_backend *backend = context;
    struct drm_psb_private *dev_priv = backend->owner->dev->dev_private;

    if (!backend->power_held || !dev_priv->sgx_reg || !value)
        return -ENODEV;
    if (backend->fire_possible &&
        time_after_eq(jiffies, backend->deadline))
        return -ETIMEDOUT;
    *value = PSB_RSGX32(offset);
    return 0;
}

static int fixed_barrier(void *context)
{
    struct sgx535_gma500_fixed_backend *backend = context;

    if (!backend->power_held)
        return -ENODEV;
    wmb();
    return 0;
}

static const struct sgx535_fixed_io fixed_io = {
    fixed_write, fixed_read, fixed_barrier
};

static int fixed_actions(struct sgx535_gma500_fixed_backend *backend,
                         const struct sgx535_frozen_reg_action *actual,
                         const struct sgx535_frozen_reg_action *expected,
                         size_t count)
{
    if (!actual || memcmp(actual, expected, count * sizeof(*expected)))
        return -EINVAL;
    return sgx535_fixed_run_actions(&fixed_io, backend, expected, count,
                                     SGX535_FIXED_POLL_READS) ? -EIO : 0;
}

static int fixed_run(void *context, enum sgx535_fixed_stage stage,
                     const void *payload, size_t count,
                     struct sgx535_fixed_observation *observation)
{
    struct sgx535_gma500_fixed_backend *backend = context;
    struct sgx535_gma500_owner *owner;
    struct drm_psb_private *dev_priv;
    struct sgx535_frozen_reg_action expected[31];
    struct sgx535_frozen_reg_write init[12];
    struct sgx535_frozen_ta_cookie ta_info;
    struct sgx535_frozen_scene_info scene_info;
    const struct sgx535_fixed_fire_payload *fire;
    int ret = -EINVAL;
    int role;
    u64 timing_mark;

    if (!backend || !backend->active || !backend->power_held ||
        !backend->owner || !observation ||
        stage != backend->last_stage + 1U)
        return -EINVAL;
    owner = backend->owner;
    dev_priv = owner->dev->dev_private;
    if (!dev_priv || !dev_priv->sgx_reg || !gma_power_is_on(owner->dev))
        return -ENODEV;

    switch (stage) {
    case SGX535_FIXED_PUBLISH_CPU:
        if (payload || count)
            break;
        timing_mark=ktime_get_ns();
        ret = (remesa_stream_source_owned(backend) ? remesa_stream_owner_flush(owner) : sgx535_gma500_owner_flush_user_images(owner));
        backend->cpu_flush_ns=ktime_get_ns()-timing_mark;
        break;
    case SGX535_FIXED_PUBLISH_TRANSLATIONS:
        if (payload || count || !dev_priv->mmu ||
            !psb_mmu_get_default_pd(dev_priv->mmu) ||
            psb_mmu_get_default_pd(dev_priv->mmu)->hw_context != 0)
            break;
        for (role = 0; role < SGX535_BO_COUNT; role++) {
            struct sgx535_gma500_bo *bo = &owner->objects[role];
            if (bo->va_reserved &&
                (!bo->pages || bo->mapped_pages != bo->page_count))
                break;
        }
        if (role == SGX535_BO_COUNT)
            ret = 0;
        break;
    case SGX535_FIXED_DEVICE_MAINTAIN:
        if (payload || count)
            break;
        psb_mmu_flush(dev_priv->mmu);
        ret = 0;
        break;
    case SGX535_FIXED_INIT_WRITES:
        if (!payload || count != 12 ||
            CONTRACT(sgx535_frozen_rev121_init_writes)(0x00010201U, init, 12) ||
            memcmp(payload, init, sizeof(init)))
            break;
        for (role = 0; role < 12; role++) {
            ret = fixed_write(backend, init[role].offset, init[role].value);
            if (ret)
                break;
        }
        break;
    case SGX535_FIXED_XHW_INIT:
        /* The internal callback replaces the old shared-memory Xpsb queue.
         * Its responder is this exact switch, not a general XHW client. */
        if (payload || count || backend->xhw_internal)
            break;
        backend->xhw_internal = true;
        ret = 0;
        break;
    case SGX535_FIXED_TA_INFO:
        if (!backend->xhw_internal || !payload || count != 1 ||
            CONTRACT(sgx535_frozen_ta_cookie)(owner->descriptors, SGX535_BO_COUNT,
                                     owner->mmu_end, &ta_info) ||
            memcmp(payload, &ta_info, sizeof(ta_info)))
            break;
        ret = 0;
        break;
    case SGX535_FIXED_SCENE_INFO:
        if (!backend->xhw_internal || !payload || count != 1 ||
            CONTRACT(sgx535_frozen_scene_info32)(&scene_info) ||
            memcmp(payload, &scene_info, sizeof(scene_info)))
            break;
        ret = 0;
        break;
    case SGX535_FIXED_TA_LOAD:
        if (!backend->xhw_internal || count != 29 ||
            CONTRACT(sgx535_frozen_ta_load_plan)(owner->descriptors, SGX535_BO_COUNT,
                                        owner->mmu_end, expected, 29))
            break;
        ret = fixed_actions(backend, payload, expected, 29);
        if (!ret) {
            observation->load_flags = 0x1fU;
            /* Successful actions observed and consumed all four load bits. */
            observation->status2 = 15U;
            observation->initend = 0x400000U;
        }
        break;
    case SGX535_FIXED_SCENE_VALIDATE:
        if (payload || count ||
            CONTRACT(sgx535_frozen_validate_bos)(owner->descriptors, SGX535_BO_COUNT,
                                        owner->mmu_end) ||
            !owner->objects[SGX535_BO_SCENE_HW].pages)
            break;
        ret = 0;
        break;
    case SGX535_FIXED_USE_RESERVE:
        if (!payload || count != 1 || backend->use_reserved ||
            memcmp(payload, &owner->use_plan, sizeof(owner->use_plan)) ||
            !owner->device_claimed)
            break;
        backend->use_reserved = true;
        ret = 0;
        break;
    case SGX535_FIXED_USE_PROGRAM:
        if (!backend->use_reserved || !payload || count != 1 ||
            memcmp(payload, &owner->use_plan, sizeof(owner->use_plan)))
            break;
        for (role = 0; role < 2; role++) {
            const struct sgx535_frozen_use_entry *entry =
                &owner->use_plan.by_data_master[role];
            ret = fixed_write(backend, entry->register_offset,
                              entry->register_word);
            if (ret)
                break;
        }
        break;
    case SGX535_FIXED_STATUS_BASELINE:
        if (payload || count)
            break;
        {
            unsigned long flags = sgx535_gma500_fixed_irq_lock();
            ret = fixed_read(backend, PSB_CR_EVENT_STATUS,
                             &observation->status1);
            if (!ret)
                ret = fixed_read(backend, PSB_CR_EVENT_STATUS2,
                                 &observation->status2);
            sgx535_gma500_fixed_irq_unlock(flags);
        }
        break;
    case SGX535_FIXED_TA_SCHEDULE:
        if (count != 8 ||
            CONTRACT(sgx535_frozen_ta_schedule_plan)(&owner->commands, expected, 8))
            break;
        ret = fixed_actions(backend, payload, expected, 8);
        break;
    case SGX535_FIXED_TA_FIRE:
    case SGX535_FIXED_RASTER_FIRE:
        if (!payload || count != 1)
            break;
        fire = payload;
        if (stage == SGX535_FIXED_TA_FIRE) {
            if (fire->action_count != 31 ||
                memcmp(&fire->wire, &owner->xhw_ta, sizeof(fire->wire)) ||
                CONTRACT(sgx535_frozen_ta_fire_plan)(owner->descriptors,
                    SGX535_BO_COUNT, owner->mmu_end, expected, 31))
                break;
        } else if (fire->action_count != 20 ||
                   memcmp(&fire->wire, &owner->xhw_raster,
                          sizeof(fire->wire)) ||
                   CONTRACT(sgx535_frozen_raster_fire_plan)(owner->descriptors,
                       SGX535_BO_COUNT, owner->mmu_end, expected, 20)) {
            break;
        }
        if (stage == SGX535_FIXED_TA_FIRE) {
            unsigned long flags = sgx535_gma500_fixed_irq_lock();
            backend->fire_possible = true;
            backend->deadline = jiffies + owner->session.timeout_ticks;
            sgx535_gma500_fixed_irq_unlock(flags);
            remesa_route_issued(backend);
        }
        timing_mark=ktime_get_ns();
        if(stage==SGX535_FIXED_TA_FIRE)backend->ta_fire_begin_ns=timing_mark;
        else backend->raster_fire_begin_ns=timing_mark;
        ret = fixed_actions(backend, fire->actions, expected,
                            fire->action_count);
        if(stage==SGX535_FIXED_TA_FIRE)backend->ta_fire_end_ns=ktime_get_ns();
        else backend->raster_fire_end_ns=ktime_get_ns();
        break;
    case SGX535_FIXED_ISP_RESET_ASSERT:
    case SGX535_FIXED_ISP_RESET_CLEAR:
        if (count != 1 ||
            CONTRACT(sgx535_frozen_raster_schedule_plan)(&owner->commands,
                                                expected, 29))
            break;
        if (stage == SGX535_FIXED_ISP_RESET_ASSERT) {
            if (backend->isp_reset_asserted)
                break;
            /* A failed write may still have reached the device. HOLD even
             * when its completion is ambiguous. */
            backend->isp_reset_asserted = true;
            ret = fixed_actions(backend, payload, expected, 1);
        } else {
            if (!backend->isp_reset_asserted)
                break;
            ret = fixed_actions(backend, payload, expected + 1, 1);
            if (!ret)
                backend->isp_reset_asserted = false;
        }
        break;
    case SGX535_FIXED_RASTER_SCHEDULE:
        if (backend->isp_reset_asserted || count != 27 ||
            CONTRACT(sgx535_frozen_raster_schedule_plan)(&owner->commands,
                                                expected, 29))
            break;
        ret = fixed_actions(backend, payload, expected + 2, 27);
        break;
    case SGX535_FIXED_USE_RELEASE:
        if (!backend->use_reserved || !payload || count != 1 ||
            memcmp(payload, &owner->use_plan, sizeof(owner->use_plan)) ||
            owner->session.phase != SGX535_PHASE_SCENE_COMPLETED)
            break;
        /* Historical regman keeps register programming for reuse. This
         * one-shot owner only relinquishes the claim after completion. */
        backend->use_reserved = false;
        ret = 0;
        break;
    default:
        break;
    }
    if (!ret)
        backend->last_stage = stage;
    return ret;
}

const struct sgx535_fixed_ops sgx535_gma500_fixed_backend_ops = {
    fixed_run
};

static void capsule_service_begin(void *context, struct sgx535_fixed_service *s,
    const struct sgx535_fixed_ops *ops,
    const struct sgx535_fixed_status_source *source, void *operation_context)
{
    remesa_route_service(context, s,
        ops == &sgx535_gma500_fixed_backend_ops &&
        source == &sgx535_gma500_fixed_backend_status_source &&
        operation_context == context);
}
static void capsule_service_before(void *b, struct sgx535_fixed_service *s,
    sgx535_u32 sequence, sgx535_u32 a, sgx535_u32 d, int exclusive)
{
    remesa_route_before(b, s, sequence, a, d, exclusive);
}
static void capsule_service_after(void *b, struct sgx535_fixed_service *s)
{
    remesa_route_after(b, s);
}
static void capsule_service_terminal(void *b, struct sgx535_fixed_service *s, int result)
{
    remesa_route_terminal(b, s, result);
}
static const struct sgx535_fixed_service_observer capsule_service_observer = {
    capsule_service_begin, capsule_service_before, capsule_service_after,
    capsule_service_terminal
};

int sgx535_gma500_fixed_backend_begin(
    struct sgx535_gma500_fixed_backend *backend,
    struct sgx535_gma500_owner *owner)
{
    unsigned long flags;
    struct drm_psb_private *priv;
    struct sgx535_source_facts facts = {0};

    if (!backend || backend->active || !owner || !owner->initialized ||
        !owner->device_claimed || !owner->power_held ||
        owner->session.phase != SGX535_PHASE_HOST_IMAGE_FINAL ||
        owner->fixed_service.session != &owner->session ||
        !gma_power_is_on(owner->dev))
        return -ENODEV;
    /* Observe before publishing this backend as the active IRQ owner. Never
     * wake, submit, acknowledge, reset or wait for a lucky clean sample here.
     * A power reference is already held by the owner. This is NOT a drain. */
    flags = sgx535_gma500_fixed_irq_lock();
    if (fixed_irq_owner || !remesa_route_source_begin(backend, owner->dev)) {
        sgx535_gma500_fixed_irq_unlock(flags);
        return -EBUSY;
    }
    priv = owner->dev->dev_private;
    sgx535_gma500_source_snapshot(priv, &facts);
    facts.pending |= backend->pending_status1 | backend->pending_status2;
    facts.busy |= backend->fire_possible;
    /* Read-only corroboration of the retained initialization reset/no-prior-
     * operation lifetime, not a drain and not a status-only certificate. */
    facts.coverage_complete = 0; /* Adapter checks matching driver attachment. */
    if (!remesa_route_source_boundary(backend, &facts)) {
        sgx535_gma500_fixed_irq_unlock(flags);
        return -EAGAIN;
    }
    sgx535_gma500_fixed_irq_unlock(flags);
    backend->owner = owner;
    backend->active = true;
    backend->power_held = true;
    backend->last_stage = 0;
    flags = sgx535_gma500_fixed_irq_lock();
    if (fixed_irq_owner) {
        sgx535_gma500_fixed_irq_unlock(flags);
        backend->power_held = false;
        backend->active = false;
        backend->owner = NULL;
        return -EBUSY;
    }
    fixed_irq_owner = backend;
#ifdef REMESA_LAB_BIF_DIAGNOSTIC
    bif_debug_owner = backend;
    memset(&bif_debug_record,0,sizeof(bif_debug_record));
#endif
    sgx535_gma500_fixed_irq_unlock(flags);
    remesa_route_admit(backend);
    owner->fixed_service.observer = &capsule_service_observer;
    owner->fixed_service.observer_context = backend;
    return 0;
}

int sgx535_gma500_fixed_backend_end(
    struct sgx535_gma500_fixed_backend *backend)
{
    unsigned long flags;

    if (!backend || !backend->active || !backend->power_held ||
        backend->use_reserved ||
        !sgx535_fixed_service_may_release(&backend->owner->fixed_service))
        return -EBUSY;
    flags = sgx535_gma500_fixed_irq_lock();
    if (fixed_irq_owner != backend) {
        sgx535_gma500_fixed_irq_unlock(flags);
        return -EBUSY;
    }
    fixed_irq_owner = NULL;
    sgx535_gma500_fixed_irq_unlock(flags);
    backend->power_held = false;
    backend->active = false;
    return 0;
}
