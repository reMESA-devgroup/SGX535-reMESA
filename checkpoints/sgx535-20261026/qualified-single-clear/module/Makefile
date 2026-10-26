# SPDX-License-Identifier: GPL-2.0
#
#	KMS driver for the GMA500
#

gma500_gfx-y += \
	  accel_2d.o \
	  backlight.o \
	  framebuffer.o \
	  gem.o \
	  gtt.o \
	  intel_bios.o \
	  intel_i2c.o \
	  intel_gmbus.o \
	  mmu.o \
	  blitter.o \
	  power.o \
	  psb_drv.o \
	  gma_display.o \
	  gma_device.o \
	  psb_intel_display.o \
	  psb_intel_lvds.o \
	  psb_intel_modes.o \
	  psb_intel_sdvo.o \
	  psb_lid.o \
	  psb_irq.o \
	  psb_device.o \
	  mid_bios.o

gma500_gfx-$(CONFIG_ACPI) +=  opregion.o \

gma500_gfx-$(CONFIG_DRM_GMA3600) +=  cdv_device.o \
	  cdv_intel_crt.o \
	  cdv_intel_display.o \
	  cdv_intel_hdmi.o \
	  cdv_intel_lvds.o \
	  cdv_intel_dp.o

gma500_gfx-$(CONFIG_DRM_GMA600) += oaktrail_device.o \
	  oaktrail_crtc.o \
	  oaktrail_lvds.o \
	  oaktrail_lvds_i2c.o \
	  oaktrail_hdmi.o \
	  oaktrail_hdmi_i2c.o

gma500_gfx-$(CONFIG_DRM_MEDFIELD) += mdfld_device.o \
	  mdfld_output.o \
	  mdfld_intel_display.o \
	  mdfld_dsi_output.o \
	  mdfld_dsi_dpi.o \
	  mdfld_dsi_pkg_sender.o \
	  mdfld_tpo_vid.o \
	  mdfld_tmd_vid.o \
	  tc35876x-dsi-lvds.o

obj-$(CONFIG_DRM_GMA500) += gma500_gfx.o

# Exact fixed-scene path; the separately reviewed ioctl patch installs its only caller.
gma500_gfx-y += gma500_bo_owner.o frozen_kernel_contract.o \
                 frozen_fixed_service.o frozen_fixed_io.o \
                 gma500_fixed_backend.o remesa_native_bridge.o remesa_triangle_profile.o

# Offline single-variable experiment, no execution authority.
CFLAGS_frozen_kernel_contract.o += -DSGX535_EXPERIMENTAL_CONSTANT_FRAGMENT=1

ccflags-y += -I/home/gama/sgx535-offline/native1280-gears-flat-last-20261025T022600Z/include

gma500_gfx-y += remesa_gem_memory.o remesa_sequence.o remesa_gem_executor_adapter.o remesa_borrowed_owner.o remesa_owner_abi_check.o

CFLAGS_psb_drv.o += -DREMESA_RESOURCE_ONLY_IOCTL

gma500_gfx-y += remesa_retained_submit.o remesa_retained_profile.o
CFLAGS_psb_drv.o += -DREMESA_RETAINED_LAB_IOCTL

# Exact green FS experiment only; runtime gates remain default-disabled.
CFLAGS_remesa_gem_memory.o += -DREMESA_EXPERIMENTAL_GREEN_FRAGMENT=1

# Private exact independently generated VS16 case; no runtime authority.
CFLAGS_frozen_kernel_contract.o += -DSGX535_EXPERIMENTAL_OWN_VERTEX=1

# Private exact sample-center case; existing admission/runtime gates unchanged.
CFLAGS_frozen_kernel_contract.o += -DSGX535_EXPERIMENTAL_QUARTER_TRIANGLE=1

# Explicit finite1280 LAB candidate only; never enabled by runtime auto-detection.
ccflags-y += -DREMESA_LAB_DEPTH1280_PROFILE=1

# Strict private1280 payload verifier and shared digest helper.
gma500_gfx-y += remesa_depth1280_profile.o

# Private bounded RO diagnostic; no admission or execution policy change.
ccflags-y += -DREMESA_LAB_BIF_DIAGNOSTIC=1

# Independent fail-closed stream endpoint and source evidence.
gma500_gfx-y += stream_lifecycle.o remesa_stream_bridge.o remesa_stream_source.o remesa_stream_trusted.o frozen_source_guard.o frozen_capsule.o

gma500_gfx-y += remesa_stream_grammar.o

gma500_gfx-y += remesa_stream_contract.o remesa_stream_fixed_service.o remesa_stream_borrowed_owner.o remesa_stream_owner_flush.o
# Match original private contract compile profile; namespace differs, only GEN size changes.
CFLAGS_remesa_stream_contract.o += -DSGX535_EXPERIMENTAL_CONSTANT_FRAGMENT=1 -DSGX535_EXPERIMENTAL_OWN_VERTEX=1 -DSGX535_EXPERIMENTAL_QUARTER_TRIANGLE=1

# BLAKE3-256 only for explicit ABI4 FAST output hashes. No FPU/SIMD in kernel.
gma500_gfx-y += blake3.o blake3_portable.o remesa_blake3_portable_dispatch.o
CFLAGS_blake3.o += -std=gnu99 -DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512 -DBLAKE3_USE_NEON=0 -fstack-usage
CFLAGS_blake3_portable.o += -std=gnu99 -DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512 -DBLAKE3_USE_NEON=0 -fstack-usage
CFLAGS_remesa_blake3_portable_dispatch.o += -std=gnu99 -DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512 -DBLAKE3_USE_NEON=0 -fstack-usage
