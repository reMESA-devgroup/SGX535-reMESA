#ifndef SGX535_GMA500_FROZEN_BO_OWNER_H
#define SGX535_GMA500_FROZEN_BO_OWNER_H

#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <drm/drm_gem.h>

#include "frozen_kernel_contract.h"
#include "frozen_fixed_service.h"

struct drm_device;

struct sgx535_gma500_bo {
    struct drm_gem_object gem;
    struct page **pages;
    void *cpu;
    struct resource va;
    u32 page_count;
    u32 mapped_pages;
    bool gem_live;
    bool va_reserved;
};

/* One private scene owner per device. The optional ioctl integration is
 * separate; active SGX mappings still require target preflight. */
struct sgx535_gma500_owner {
    struct drm_device *dev;
    struct resource va_root;
    struct resource gtt_exclusion;
    bool gtt_excluded;
    struct sgx535_gma500_bo objects[SGX535_BO_COUNT];
    struct sgx535_frozen_bo descriptors[SGX535_BO_COUNT];
    struct sgx535_frozen_cpu_view views[SGX535_BO_COUNT];
    struct sgx535_frozen_command_pairs commands;
    struct sgx535_frozen_xhw_bind_fire_wire xhw_ta;
    struct sgx535_frozen_xhw_bind_fire_wire xhw_raster;
    struct sgx535_frozen_use_plan use_plan;
    struct sgx535_frozen_session session;
    struct sgx535_fixed_service fixed_service;
    u64 mmu_end;
    bool initialized;
    bool device_claimed;
    bool user_images_flushed;
    bool power_held;
};


#endif
