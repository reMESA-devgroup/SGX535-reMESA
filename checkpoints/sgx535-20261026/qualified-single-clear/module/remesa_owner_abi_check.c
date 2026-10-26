#include <linux/build_bug.h>
#define sgx535_gma500_bo remesa_baseline_bo
#define sgx535_gma500_owner remesa_baseline_owner
#include "remesa_owner_abi_baseline.h"
#undef sgx535_gma500_bo
#undef sgx535_gma500_owner
#undef SGX535_GMA500_FROZEN_BO_OWNER_H
#include "gma500_bo_owner.h"
static_assert(sizeof(struct sgx535_gma500_bo)==sizeof(struct remesa_baseline_bo), "BO ABI stride");
static_assert(offsetof(struct sgx535_gma500_owner,dev)==offsetof(struct remesa_baseline_owner,dev), "observer ABI dev");
static_assert(offsetof(struct sgx535_gma500_owner,va_root)==offsetof(struct remesa_baseline_owner,va_root), "observer ABI va_root");
static_assert(offsetof(struct sgx535_gma500_owner,gtt_exclusion)==offsetof(struct remesa_baseline_owner,gtt_exclusion), "observer ABI gtt_exclusion");
static_assert(offsetof(struct sgx535_gma500_owner,gtt_excluded)==offsetof(struct remesa_baseline_owner,gtt_excluded), "observer ABI gtt_excluded");
static_assert(offsetof(struct sgx535_gma500_owner,objects)==offsetof(struct remesa_baseline_owner,objects), "observer ABI objects");
static_assert(offsetof(struct sgx535_gma500_owner,descriptors)==offsetof(struct remesa_baseline_owner,descriptors), "observer ABI descriptors");
static_assert(offsetof(struct sgx535_gma500_owner,views)==offsetof(struct remesa_baseline_owner,views), "observer ABI views");
static_assert(offsetof(struct sgx535_gma500_owner,commands)==offsetof(struct remesa_baseline_owner,commands), "observer ABI commands");
static_assert(offsetof(struct sgx535_gma500_owner,xhw_ta)==offsetof(struct remesa_baseline_owner,xhw_ta), "observer ABI xhw_ta");
static_assert(offsetof(struct sgx535_gma500_owner,xhw_raster)==offsetof(struct remesa_baseline_owner,xhw_raster), "observer ABI xhw_raster");
static_assert(offsetof(struct sgx535_gma500_owner,use_plan)==offsetof(struct remesa_baseline_owner,use_plan), "observer ABI use_plan");
static_assert(offsetof(struct sgx535_gma500_owner,session)==offsetof(struct remesa_baseline_owner,session), "observer ABI session");
static_assert(offsetof(struct sgx535_gma500_owner,fixed_service)==offsetof(struct remesa_baseline_owner,fixed_service), "observer ABI fixed_service");
static_assert(offsetof(struct sgx535_gma500_owner,mmu_end)==offsetof(struct remesa_baseline_owner,mmu_end), "observer ABI mmu_end");
static_assert(offsetof(struct sgx535_gma500_owner,initialized)==offsetof(struct remesa_baseline_owner,initialized), "observer ABI initialized");
static_assert(offsetof(struct sgx535_gma500_owner,device_claimed)==offsetof(struct remesa_baseline_owner,device_claimed), "observer ABI device_claimed");
static_assert(offsetof(struct sgx535_gma500_owner,user_images_flushed)==offsetof(struct remesa_baseline_owner,user_images_flushed), "observer ABI user_images_flushed");
static_assert(offsetof(struct sgx535_gma500_owner,power_held)==offsetof(struct remesa_baseline_owner,power_held), "observer ABI power_held");
