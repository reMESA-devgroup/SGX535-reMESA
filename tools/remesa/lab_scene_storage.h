/* SPDX-License-Identifier: MIT */
#ifndef REMESA_LAB_SCENE_STORAGE_H
#define REMESA_LAB_SCENE_STORAGE_H
#include "remesa_persistent.h"
#include "remesa/scene_dpm.h"
#include "remesa_depth_scene_image.h"
/* LAB preparation only. This is an owned resource set, NOT an admitted job,
 * executable packet, retained shader implementation or generic Gallium ABI. */
enum remesa_lab_scene_role {
 REMESA_LAB_AUX_PDS,REMESA_LAB_AUX_USE,REMESA_LAB_GENERATED,
 REMESA_LAB_BACKGROUND,REMESA_LAB_CONTROL,REMESA_LAB_COLOR,REMESA_LAB_Z,
 REMESA_LAB_REGION,REMESA_LAB_TA_TABLE,REMESA_LAB_TA_PARAMETER,
 REMESA_LAB_XHW,REMESA_LAB_SCENE_ROLES
};
struct remesa_lab_scene_storage {
 struct remesa_device *device;
 float clear_depth;
 void *objects[REMESA_LAB_SCENE_ROLES];
 struct remesa_persistent_memory_backing bindings[REMESA_LAB_SCENE_ROLES];
 struct remesa_persistent_render_target_backing color;
 struct remesa_persistent_depth_backing depth;
 struct remesa_persistent_scene_backing region;
 struct remesa_persistent_depth_scene_backing generated;
 /* Original fresh option0 CPU setup contributions, not a complete register
  * program or MMIO permission. Seven TA register/value pairs, separate408
  * raster pair. No cookie15, waits, kicks or completion claims are invented. */
 uint32_t ta_scene[14],raster_scene[2];
};
/* Uses explicit placement policies of the existing endpoint. PDS and USE
 * backing are allocated first so generated code may share their512KiB window.
 * Fixed auxiliary128KiB/32KiB and TA32MiB allocations retain the known LAB
 * storage sizes only, with no instruction bytes or control templates. Generic
 * BO contents remain UNINITIALIZED; no auxiliary programs are copied here.
 * The scene image is generated from the supplied three shader images.
 * Requires empty output, request GPU VA/extent0, disjoint caller storage and
 * external serialization. Ordinary failure unwinds only this call's BOs;
 * uncertain provider effects retain the endpoint's existing quarantine rules.
 */
int remesa_lab_scene_storage_prepare(struct remesa_device *,
 const struct remesa_depth_scene_image_request *,struct remesa_lab_scene_storage *);
/* Releases frontend ownership only, never force-frees held/quarantined BOs. */
void remesa_lab_scene_storage_destroy(struct remesa_lab_scene_storage *);
/* Build CPU raster DATA from fresh live BO snapshots and symbolic offsets.
 * Does not validate/materialize auxiliary CODE, upload bytes, submit or admit.
 * The background/event payloads must be separately checked before any use.
 * Rejects stale receipts, active leases, out-of-bounds/unknown metadata and
 * aliased output. Returns a208-byte plan only, never a ready native job. */
int remesa_lab_scene_raster_encode(const struct remesa_lab_scene_storage *,
 uint64_t background_offset,uint64_t event_offset,uint64_t event_bytes,
 uint32_t event_field0c,uint8_t out[REMESA_RASTER_CONTROL_BYTES]);
/* Raster208 + TA56 control DATA for the owned scene. No upload, execution or
 * auxiliary image authentication. Revalidates immutable generated TA backing.
 * All264 bytes are committed to disjoint output only after successful checks. */
#define REMESA_LAB_SCENE_CONTROL_BYTES (REMESA_RASTER_CONTROL_BYTES+REMESA_TA_CONTROL_BYTES)
int remesa_lab_scene_control_encode(const struct remesa_lab_scene_storage *,
 uint64_t background_offset,uint64_t event_offset,uint64_t event_bytes,
 uint32_t event_field0c,uint8_t out[REMESA_LAB_SCENE_CONTROL_BYTES]);
/* Prepare/readback/sync a new immutable control BO, then replace CONTROL.
 * Validates live input owners; ordinary failure preserves the old bundle.
 * Aux CODE/background and native job admission remain separate and unproved.
 * External serialization with all owners is mandatory. */
int remesa_lab_scene_control_stage(struct remesa_lab_scene_storage *,
 uint64_t background_offset,uint64_t event_offset,uint64_t event_bytes,
 uint32_t event_field0c);
/* Fresh fixed32MiB parameter allocation CPU load profile. Original producer
 * writes22 pairs, then calls a wait helper; this is NOT a callable executor.
 * The original helper requests status mask7 despite starting four loads.
 * The retained qualified executor requires15; callers must not downgrade its
 * completion policy to the historical CPU helper argument. No MMIO performed.
 * Contents/zeroing of TA resources and native admission remain separate. */
int remesa_lab_scene_dpm_encode(const struct remesa_lab_scene_storage *,
 struct remesa_lab_dpm_control *);
#endif
