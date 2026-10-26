#include <linux/vmalloc.h>
#include <drm/drm_cache.h>
#include <asm/cpufeature.h>
#include "gma500_bo_owner.h"
#include "remesa_stream_contract_api.h"
#include "remesa_stream_trusted.h"
int remesa_stream_owner_flush(struct sgx535_gma500_owner *owner)
{
    u32 page_counts[SGX535_BO_COUNT];
    u32 mapped_pages[SGX535_BO_COUNT];
    int role, error;bool handled;

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
    if (remesa_stream_sgx535_frozen_validate_publication_pages(owner->descriptors,
            SGX535_BO_COUNT, owner->mmu_end, page_counts, mapped_pages))
        return -EINVAL;
    for (role = 0; role <= SGX535_BO_COLOR; role++) {
        struct sgx535_gma500_bo *bo = &owner->objects[role];

        if (!bo->cpu || !bo->pages ||
            owner->views[role].bytes != bo->cpu ||
            owner->views[role].length != bo->gem.size)
            return -EINVAL;
    }
    error=remesa_stream_trusted_prefire_cache(owner,&handled);
    if(error)return error;
    if(!handled)for (role = 0; role < SGX535_BO_COUNT; role++) {
        struct sgx535_gma500_bo *bo = &owner->objects[role];
        drm_clflush_pages(bo->pages, bo->page_count);
    }
    dma_wmb();
    /* FAST revokes its validated borrowed pointers without unmapping the
     * provider's held backing. LAB retains the original private-alias drop. */
    error=remesa_stream_trusted_consume_images(owner,&handled);
    if(error)return error;
    if(!handled)for (role = 0; role <= SGX535_BO_COLOR; role++) {
        struct sgx535_gma500_bo *bo = &owner->objects[role];
        vunmap(bo->cpu);
        bo->cpu = NULL;
        owner->views[role].bytes = NULL;
        owner->views[role].length = 0;
    }
    owner->user_images_flushed = true;
    return 0;
}

