// SPDX-License-Identifier: GPL-2.0-only
/* Fixed-scene BO owner for antiX 5.10.240 gma500. The optional private
 * ioctl patch is a separate integration step; source alone touches no GPU.
 */
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <drm/drm_cache.h>
#include <asm/cpufeature.h>

#include "psb_drv.h"
#include "psb_reg.h"
#include "power.h"
#include "mmu.h"
#include "gma500_bo_owner.h"
#include "gma500_capsule_observer.h"

/* The private VA tree is valid only while a single frozen scene owns it. */
static DEFINE_MUTEX(frozen_owner_lock);
static struct drm_device *frozen_claimed_device;

static int owner_claim_device(struct drm_device *dev)
{
    int ret = 0;

    mutex_lock(&frozen_owner_lock);
    if (frozen_claimed_device)
        ret = -EBUSY;
    else
        frozen_claimed_device = dev;
    mutex_unlock(&frozen_owner_lock);
    return ret;
}

static void owner_unclaim_device(struct drm_device *dev)
{
    mutex_lock(&frozen_owner_lock);
    if (frozen_claimed_device == dev)
        frozen_claimed_device = NULL;
    mutex_unlock(&frozen_owner_lock);
}

/* Shared exclusion for the offline persistent-resource integration. */
int sgx535_gma500_exclusive_claim(struct drm_device *dev)
{ return owner_claim_device(dev); }
void sgx535_gma500_exclusive_release(struct drm_device *dev)
{ owner_unclaim_device(dev); }

static int owner_domain_bounds(u32 domain, u64 mmu_end,
                               resource_size_t *first,
                               resource_size_t *last)
{
    switch (domain) {
    case SGX535_DOMAIN_PDS:
        *first = 0x20000000;
        *last = 0x2fffffff;
        return 0;
    case SGX535_DOMAIN_RASTGEOM:
        *first = 0x30000000;
        *last = 0x3fffffff;
        return 0;
    case SGX535_DOMAIN_MMU:
        *first = 0x40000000;
        *last = mmu_end - 1;
        return 0;
    default:
        return -EINVAL;
    }
}

static int owner_exclude_gtt(struct sgx535_gma500_owner *owner)
{
    struct drm_psb_private *priv = owner->dev->dev_private;
    u64 start = priv->gtt.gatt_start;
    u64 extent = (u64)priv->gtt.gatt_pages << PAGE_SHIFT;
    u64 end;

    if (!extent || start + extent > 0x100000000ULL)
        return -ERANGE;
    end = start + extent;
    if (end <= owner->va_root.start || start > owner->va_root.end)
        return 0;
    owner->gtt_exclusion.name = "gma500 existing GTT/SGX VA";
    owner->gtt_exclusion.start = max_t(u64, start, owner->va_root.start);
    owner->gtt_exclusion.end = min_t(u64, end - 1, owner->va_root.end);
    owner->gtt_exclusion.flags = 0;
    if (request_resource(&owner->va_root, &owner->gtt_exclusion))
        return -EBUSY;
    owner->gtt_excluded = true;
    return 0;
}

int sgx535_gma500_owner_release(struct sgx535_gma500_owner *owner)
{
    struct drm_psb_private *priv;
    struct psb_mmu_pd *pd;
    int role;

    if (owner && owner->borrowed_backing)
        return -EOPNOTSUPP; /* Never unmap/unclaim lender-owned resources. */
    if (!owner || !owner->initialized || !owner->dev)
        return -EINVAL;
    if (owner->fixed_service.session &&
        !sgx535_fixed_service_may_release(&owner->fixed_service))
        return -EBUSY;
    if (!owner->fixed_service.session &&
        owner->session.phase >= SGX535_PHASE_FIRE_POSSIBLE &&
        owner->session.phase != SGX535_PHASE_RETIRED &&
        owner->session.phase != SGX535_PHASE_ABORTED_BEFORE_SUBMIT)
        return -EBUSY;
    priv = owner->dev->dev_private;
    pd = psb_mmu_get_default_pd(priv->mmu);
    if (!pd) {
        for (role = 0; role < SGX535_BO_COUNT; role++) {
            if (owner->objects[role].mapped_pages)
                return -ENODEV;
        }
    }
    sgx535_provenance_release(owner);
    for (role = SGX535_BO_COUNT - 1; role >= 0; role--) {
        struct sgx535_gma500_bo *bo = &owner->objects[role];
        if (bo->mapped_pages) {
            psb_mmu_remove_pages(pd, bo->va.start, bo->mapped_pages,
                                 0, 0);
            bo->mapped_pages = 0;
        }
        if (bo->va_reserved) {
            release_resource(&bo->va);
            bo->va_reserved = false;
        }
        if (bo->cpu) {
            vunmap(bo->cpu);
            bo->cpu = NULL;
        }
        if (bo->pages) {
            drm_gem_put_pages(&bo->gem, bo->pages, true, false);
            bo->pages = NULL;
        }
        if (bo->gem_live) {
            drm_gem_object_release(&bo->gem);
            bo->gem_live = false;
        }
    }
    if (owner->gtt_excluded)
        release_resource(&owner->gtt_exclusion);
    if (owner->power_held) {
        gma_power_end(owner->dev);
        owner->power_held = false;
    }
    if (owner->device_claimed) {
        owner_unclaim_device(owner->dev);
        owner->device_claimed = false;
    }
    owner->initialized = false;
    owner->session.phase = SGX535_PHASE_EMPTY;
    return 0;
}

int sgx535_gma500_owner_flush_user_images(struct sgx535_gma500_owner *owner)
{
    u32 page_counts[SGX535_BO_COUNT];
    u32 mapped_pages[SGX535_BO_COUNT];
    int role;

    if (!owner || !owner->initialized || !owner->device_claimed ||
        owner->session.phase != SGX535_PHASE_HOST_IMAGE_FINAL ||
        owner->user_images_flushed)
        return -EINVAL;
    /* Avoid drm_clflush_pages()'s whole-machine WBINVD fallback on a CPU
     * without CLFLUSH. No target CPU capability has been assumed here. */
    if (!static_cpu_has(X86_FEATURE_CLFLUSH))
        return -EOPNOTSUPP;
    for (role = 0; role < SGX535_BO_COUNT; role++) {
        page_counts[role] = owner->objects[role].page_count;
        mapped_pages[role] = owner->objects[role].mapped_pages;
    }
    if (sgx535_frozen_validate_publication_pages(owner->descriptors,
            SGX535_BO_COUNT, owner->mmu_end, page_counts, mapped_pages))
        return -EINVAL;
    for (role = 0; role <= SGX535_BO_COLOR; role++) {
        struct sgx535_gma500_bo *bo = &owner->objects[role];

        if (!bo->cpu || !bo->pages ||
            owner->views[role].bytes != bo->cpu ||
            owner->views[role].length != bo->gem.size)
            return -EINVAL;
    }
    for (role = 0; role < SGX535_BO_COUNT; role++) {
        struct sgx535_gma500_bo *bo = &owner->objects[role];

        drm_clflush_pages(bo->pages, bo->page_count);
    }
    dma_wmb();
    /* These private CPU mappings are no longer needed. With no exported GEM
     * handles, dropping them also prevents a later accidental CPU patch from
     * silently invalidating this flush while the BO pages remain pinned. */
    for (role = 0; role <= SGX535_BO_COLOR; role++) {
        struct sgx535_gma500_bo *bo = &owner->objects[role];

        vunmap(bo->cpu);
        bo->cpu = NULL;
        owner->views[role].bytes = NULL;
        owner->views[role].length = 0;
    }
    owner->user_images_flushed = true;
    return 0;
}

int sgx535_gma500_owner_capture_color(
    struct sgx535_gma500_owner *owner,
    struct sgx535_frozen_color_summary *summary, u8 *raw_bytes)
{
    struct sgx535_gma500_bo *bo;
    void *bytes;
    int ret;

    if (!owner || !summary || !raw_bytes || !owner->initialized ||
        owner->session.phase != SGX535_PHASE_RETIRED ||
        !owner->power_held)
        return -EINVAL;
    bo = &owner->objects[SGX535_BO_COLOR];
    if (!bo->pages || bo->page_count != 1 || bo->mapped_pages != 1)
        return -EINVAL;
    /* The pre-fire flush and dropped CPU mapping leave no intentional CPU
     * writes after submission. This invalidate/readback is diagnostic; its
     * target GPU-to-CPU visibility has not been established by Gate B. */
    drm_clflush_pages(bo->pages, 1);
    dma_rmb();
    bytes = kmap(bo->pages[0]);
    if (!bytes)
        return -ENOMEM;
    ret = sgx535_frozen_summarize_color(bytes, PAGE_SIZE, summary);
    if (!ret) {
        memcpy(raw_bytes, bytes, PAGE_SIZE);
        sgx535_provenance_color(owner, bytes);
    }
    kunmap(bo->pages[0]);
    return ret ? -EIO : 0;
}

int sgx535_gma500_owner_construct(struct drm_device *dev,
                                  struct sgx535_gma500_owner *owner)
{
    static const struct sgx535_frozen_request fixed = {1, 1, 0, 0};
    struct drm_psb_private *priv;
    struct psb_mmu_pd *pd;
    u32 selected_use_registers[2];
    u64 mmu_end;
    int role, ret;

    if (!dev || !owner || owner->initialized)
        return -EINVAL;
    priv = dev->dev_private;
    if (!priv || !priv->mmu)
        return -ENODEV;
    mmu_end = priv->gtt.mmu_gatt_start;
    if (mmu_end <= 0x40000000ULL || mmu_end > 0x100000000ULL ||
        (mmu_end & (PAGE_SIZE - 1)))
        return -ERANGE;
    pd = psb_mmu_get_default_pd(priv->mmu);
    /* psb_mmu_insert_pages publishes PTEs through the BIF only when this
     * page directory has a hardware context. An unbound PD would leave the
     * constructed scene with CPU-visible mappings but no device handoff. */
    if (!pd || pd->hw_context != 0 || !priv->mmu->has_clflush ||
        !priv->sgx_reg)
        return -ENODEV;
    /* Mapping insertion can flush the live BIF. Keep the non-waking power
     * reference from before the first insertion through final teardown. */
    if (!gma_power_begin(dev, false))
        return -ENODEV;
    ret = owner_claim_device(dev);
    if (ret) {
        gma_power_end(dev);
        return ret;
    }
    memset(owner, 0, sizeof(*owner));
    owner->dev = dev;
    owner->mmu_end = mmu_end;
    owner->initialized = true;
    owner->device_claimed = true;
    owner->power_held = true;
    owner->va_root.name = "sgx535 frozen VA reservations";
    owner->va_root.start = 0x20000000;
    owner->va_root.end = mmu_end - 1;
    /* GPU VAs are not CPU iomem; exclude GTT here, not x86 E820 RAM. */
    owner->va_root.flags = 0;
    ret = owner_exclude_gtt(owner);
    if (ret)
        goto fail;

    /* GEM supplies the pinned shmem pages; this path does not expose handles
     * and deliberately does not call psb_gtt_pin, whose MMU error is ignored. */
    for (role = 0; role < SGX535_BO_COUNT; role++) {
        struct sgx535_gma500_bo *bo = &owner->objects[role];
        struct sgx535_frozen_bo *descriptor = &owner->descriptors[role];
        struct sgx535_frozen_requirement requirement;
        resource_size_t first, last;

        ret = sgx535_frozen_get_requirement(role, &requirement);
        if (ret) {
            ret = -EINVAL;
            goto fail;
        }
        descriptor->role = role;
        descriptor->domain = requirement.domain;
        descriptor->size = requirement.size;
        descriptor->owner_token = (unsigned long)bo;
        bo->page_count = requirement.size >> PAGE_SHIFT;
        ret = drm_gem_object_init(dev, &bo->gem, requirement.size);
        if (ret)
            goto fail;
        bo->gem_live = true;
        bo->pages = drm_gem_get_pages(&bo->gem);
        if (IS_ERR(bo->pages)) {
            ret = PTR_ERR(bo->pages);
            bo->pages = NULL;
            goto fail;
        }
        /* New kernel-owned BOs begin with deterministic zero backing,
         * including service/TA pages that have no private CPU view. This
         * is clean-room policy, not a historical initialization claim. */
        {
            u32 page;
            for (page = 0; page < bo->page_count; page++)
                clear_highpage(bo->pages[page]);
        }
        if (role <= SGX535_BO_COLOR) {
            bo->cpu = vmap(bo->pages, bo->page_count, VM_MAP, PAGE_KERNEL);
            if (!bo->cpu) {
                ret = -ENOMEM;
                goto fail;
            }
            owner->views[role].bytes = bo->cpu;
            owner->views[role].length = requirement.size;
        }
        if (requirement.domain == SGX535_DOMAIN_LOCAL)
            continue;
        ret = owner_domain_bounds(requirement.domain, mmu_end,
                                  &first, &last);
        if (ret)
            goto fail;
        bo->va.name = "sgx535 frozen BO";
        bo->va.flags = 0;
        ret = allocate_resource(&owner->va_root, &bo->va,
                                requirement.size, first, last,
                                requirement.alignment, NULL, NULL);
        if (ret)
            goto fail;
        bo->va_reserved = true;
        descriptor->gpu_va = bo->va.start;
    }
    ret = sgx535_frozen_session_begin(&owner->session, &fixed,
                                      sizeof(fixed), owner->descriptors,
                                      SGX535_BO_COUNT, mmu_end);
    if (ret) {
        ret = -EINVAL;
        goto fail;
    }
    /* The selected literal CPU images are fixed. GPU address relocations and
     * USE reservations still require separately qualified internal inputs. */
    ret = sgx535_frozen_initialize_user_images(owner->views);
    if (ret) {
        ret = -EINVAL;
        goto fail;
    }

    for (role = 0; role < SGX535_BO_COUNT; role++) {
        struct sgx535_gma500_bo *bo = &owner->objects[role];
        u32 page;
        if (!bo->va_reserved)
            continue;
        /* One page per call gives exact rollback accounting if a later PT
         * allocation fails. Do not unmap an uninserted PTE. */
        for (page = 0; page < bo->page_count; page++) {
            ret = psb_mmu_insert_pages(pd, &bo->pages[page],
                                       bo->va.start + ((u64)page << PAGE_SHIFT),
                                       1, 0, 0, PSB_MMU_CACHED_MEMORY);
            if (ret)
                goto fail;
            bo->mapped_pages++;
        }
    }
    /* Finish host bytes only after all internally chosen VAs are known.
     * The USE base register writes described by use_plan are NOT performed
     * here; no service may consume this image until they are owned/published. */
    ret = sgx535_frozen_plan_use_bases(owner->descriptors, SGX535_BO_COUNT,
                                       mmu_end, &owner->use_plan);
    if (ret) {
        ret = -EINVAL;
        goto fail;
    }
    if (owner->use_plan.by_data_master[0].reg != 4 ||
        owner->use_plan.by_data_master[1].reg != 3 ||
        owner->use_plan.by_data_master[0].register_offset !=
            PSB_CR_USE_CODE_BASE(4) ||
        owner->use_plan.by_data_master[1].register_offset !=
            PSB_CR_USE_CODE_BASE(3)) {
        ret = -EINVAL;
        goto fail;
    }
    selected_use_registers[0] = owner->use_plan.by_data_master[0].reg;
    selected_use_registers[1] = owner->use_plan.by_data_master[1].reg;
    ret = sgx535_frozen_apply_relocations(owner->descriptors, SGX535_BO_COUNT,
                                           mmu_end, owner->views,
                                           selected_use_registers);
    if (ret) {
        ret = -EINVAL;
        goto fail;
    }
    /* Preserve validated TA/raster pairs before publication releases the
     * private CONTROL CPU view. A later service must use this copy only. */
    ret = sgx535_frozen_extract_commands(
        &owner->views[SGX535_BO_CONTROL], &owner->commands);
    if (ret) {
        ret = -EINVAL;
        goto fail;
    }
    /* CPU request images only. Hardware context 0 is not reserved here and
     * these requests must never be queued by this BO-only draft. */
    ret = sgx535_frozen_xhw_bind_fire_wire(owner->descriptors,
            SGX535_BO_COUNT, mmu_end, 0, 0, &owner->xhw_ta);
    if (ret) {
        ret = -EINVAL;
        goto fail;
    }
    ret = sgx535_frozen_xhw_bind_fire_wire(owner->descriptors,
            SGX535_BO_COUNT, mmu_end, 1, 0, &owner->xhw_raster);
    if (ret) {
        ret = -EINVAL;
        goto fail;
    }
    ret = sgx535_frozen_session_advance(&owner->session,
                                         SGX535_PHASE_HOST_IMAGE_FINAL);
    if (ret) {
        ret = -EINVAL;
        goto fail;
    }
    ret = sgx535_fixed_service_bind(&owner->fixed_service, &owner->session,
                                    owner->descriptors, SGX535_BO_COUNT,
                                    mmu_end, &owner->commands);
    if (ret) {
        ret = -EINVAL;
        goto fail;
    }
    return 0;
fail:
    sgx535_gma500_owner_release(owner);
    return ret;
}
