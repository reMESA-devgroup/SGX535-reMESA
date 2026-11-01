/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/build_bug.h>
#include "gma500_fixed_backend.h"
#include "remesa_backend_abi_baseline.h"
/* Keep every existing native/observer/private field at its original offset. */
static_assert(offsetof(struct sgx535_gma500_fixed_backend,owner)==offsetof(struct remesa_private_baseline_backend,owner), "backend ABI owner");
static_assert(offsetof(struct sgx535_gma500_fixed_backend,last_stage)==offsetof(struct remesa_private_baseline_backend,last_stage), "backend ABI last_stage");
static_assert(offsetof(struct sgx535_gma500_fixed_backend,ta_fire_begin_ns)==offsetof(struct remesa_private_baseline_backend,ta_fire_begin_ns), "backend ABI ta_fire_begin_ns");
static_assert(offsetof(struct sgx535_gma500_fixed_backend,ta_fire_end_ns)==offsetof(struct remesa_private_baseline_backend,ta_fire_end_ns), "backend ABI ta_fire_end_ns");
static_assert(offsetof(struct sgx535_gma500_fixed_backend,raster_fire_begin_ns)==offsetof(struct remesa_private_baseline_backend,raster_fire_begin_ns), "backend ABI raster_fire_begin_ns");
static_assert(offsetof(struct sgx535_gma500_fixed_backend,raster_fire_end_ns)==offsetof(struct remesa_private_baseline_backend,raster_fire_end_ns), "backend ABI raster_fire_end_ns");
static_assert(offsetof(struct sgx535_gma500_fixed_backend,cpu_flush_ns)==offsetof(struct remesa_private_baseline_backend,cpu_flush_ns), "backend ABI cpu_flush_ns");
static_assert(offsetof(struct sgx535_gma500_fixed_backend,active)==offsetof(struct remesa_private_baseline_backend,active), "backend ABI active");
static_assert(offsetof(struct sgx535_gma500_fixed_backend,power_held)==offsetof(struct remesa_private_baseline_backend,power_held), "backend ABI power_held");
static_assert(offsetof(struct sgx535_gma500_fixed_backend,use_reserved)==offsetof(struct remesa_private_baseline_backend,use_reserved), "backend ABI use_reserved");
static_assert(offsetof(struct sgx535_gma500_fixed_backend,xhw_internal)==offsetof(struct remesa_private_baseline_backend,xhw_internal), "backend ABI xhw_internal");
static_assert(offsetof(struct sgx535_gma500_fixed_backend,fire_possible)==offsetof(struct remesa_private_baseline_backend,fire_possible), "backend ABI fire_possible");
static_assert(offsetof(struct sgx535_gma500_fixed_backend,isp_reset_asserted)==offsetof(struct remesa_private_baseline_backend,isp_reset_asserted), "backend ABI isp_reset_asserted");
static_assert(offsetof(struct sgx535_gma500_fixed_backend,pending_status1)==offsetof(struct remesa_private_baseline_backend,pending_status1), "backend ABI pending_status1");
static_assert(offsetof(struct sgx535_gma500_fixed_backend,pending_status2)==offsetof(struct remesa_private_baseline_backend,pending_status2), "backend ABI pending_status2");
static_assert(offsetof(struct sgx535_gma500_fixed_backend,deadline)==offsetof(struct remesa_private_baseline_backend,deadline), "backend ABI deadline");
static_assert(offsetof(struct sgx535_gma500_fixed_backend,private_loan)==offsetof(struct remesa_private_baseline_backend,private_loan), "backend ABI private_loan");
static_assert(offsetof(struct sgx535_gma500_fixed_backend,private_views)==offsetof(struct remesa_private_baseline_backend,private_views), "backend ABI private_views");
static_assert(offsetof(struct sgx535_gma500_fixed_backend,prefire_load_active)==offsetof(struct remesa_private_baseline_backend,prefire_load_active), "backend ABI prefire_load_active");
static_assert(offsetof(struct sgx535_gma500_fixed_backend,general_scene)>=sizeof(struct remesa_private_baseline_backend), "general extension follows retained backend");
