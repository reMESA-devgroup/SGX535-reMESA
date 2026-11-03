/* SPDX-License-Identifier: MIT */
#ifndef REMESA_LINUX_MEMORY_H
#define REMESA_LINUX_MEMORY_H
#include "remesa_persistent.h"
#include "remesa/memory_uapi.h"
#include "remesa/twod_copy_uapi.h"
#include "remesa/twod_fill_uapi.h"
#include "remesa_retained_transport.h"
#include "remesa/private_session_lab_uapi.h"
#include "remesa/scene_probe_uapi.h"
#include "remesa/gears_scene_probe_uapi.h"
#include "remesa/general_session_uapi.h"
#include "remesa/general_parameter_uapi.h"
#include "remesa/matrix_submit_uapi.h"
struct remesa_linux_memory;
/* Ordinary typed matrix packet, separate from all LAB catalogue selectors.
 * One attempted ioctl per endpoint, no retry. Native identity/retirement and
 * successful kernel/source closure are required; uncertainty poisons ownership.
 * No hashes, INFO transfers, readback or gate enable performed here. */
int remesa_linux_memory_matrix_submit(struct remesa_linux_memory *,
 const struct remesa_scene_packet *,uint32_t budget_ms,struct remesa_matrix_submit_request *);
enum remesa_dispatch remesa_linux_memory_matrix_dispatch(void *,
 const struct remesa_gpu_scene_submit *,struct remesa_gpu_scene_receipt *);
/* Last raw response, including errors; host snapshot only, no I/O. */
int remesa_linux_memory_matrix_response(const struct remesa_linux_memory *,struct remesa_matrix_submit_request *);
/* Bounded LAB general/native session, distinct from retained11-ID ABI.
 * No retry on an uncertain ioctl; all local objects then remain poisoned. */
int remesa_linux_memory_general_open(struct remesa_linux_memory *,const uint32_t[18],
 uint32_t profile,uint32_t maximum,struct remesa_general_session_request *);
/* Explicit typed candidate; same bounded source/ownership, no LAB v1 claim. */
int remesa_linux_memory_general_typed_open(struct remesa_linux_memory *,const uint32_t[18],
 uint32_t profile,uint32_t maximum,struct remesa_general_session_request *);
/* Synchronous CPU data update only after a native retired SUBMIT. Kernel
 * selects the admitted PARAM owner; no user ID/VA/policy. Ack sequence is not
 * a GPU operation. Conclusive atomic refusals do not advance the sequence;
 * any uncertain ioctl/envelope poisons all objects, without retry/cleanup. */
int remesa_linux_memory_general_parameters(struct remesa_linux_memory *,uint32_t draw,
 uint32_t offset,const uint32_t *bits,uint32_t words,struct remesa_general_parameter_request *);
int remesa_linux_memory_general_parameter_request(const struct remesa_linux_memory *,struct remesa_general_parameter_request *);
int remesa_linux_memory_general_parameter_response(const struct remesa_linux_memory *,struct remesa_general_parameter_request *);
int remesa_linux_memory_general_submit(struct remesa_linux_memory *,uint32_t budget_ms,
 struct remesa_general_session_request *);
int remesa_linux_memory_general_close(struct remesa_linux_memory *,struct remesa_general_session_request *);
/* Explicit fresh-endpoint LAB opt-in only. Save each actual returned wire for
 * a separate collector, including uncertain responses. Disabled by default:
 * normal adapter submissions do not copy diagnostic response records. Getter
 * performs no ioctl or retry and is not completion/admission evidence alone. */
int remesa_linux_memory_general_record_enable(struct remesa_linux_memory *);
int remesa_linux_memory_general_response(const struct remesa_linux_memory *,struct remesa_general_session_request *);
int remesa_linux_memory_general_request(const struct remesa_linux_memory *,struct remesa_general_session_request *);
/* Explicit adapter of that same bounded LAB session to the real provider.
 * OPEN freezes exact capture metadata, validates backing and issues no GPU job.
 * Each dispatch issues ONE SUBMIT and returns ONE authenticated host receipt;
 * no replay loop, per-job INFO/hash/readback, automatic gate or retry.
 * The caller MUST use capture_submit_keep and retain the capture until an
 * acknowledged general_close, then release it. A normal consuming submit is
 * incompatible with an active native session. Published COLOR uses its READ
 * lease. Changed packets and repeated host operations are locally refused.
 * Kernel profiles/admission and 64-job LAB bound remain unchanged. This is
 * lifecycle integration, not production arbitrary-program GL admission.
 * External serialization includes OPEN/dispatch/COLOR read/CLOSE/release. */
int remesa_linux_memory_general_scene_open(struct remesa_linux_memory *,
 const struct remesa_scene_packet *,uint32_t profile,uint32_t maximum,uint32_t budget_ms,
 struct remesa_general_session_request *);
int remesa_linux_memory_general_scene_typed_open(struct remesa_linux_memory *,
 const struct remesa_scene_packet *,uint32_t profile,uint32_t maximum,uint32_t budget_ms,
 struct remesa_general_session_request *);
/* Data-only companion for a retained capture opened explicitly in typed mode.
 * Checks the cached capture/host operation and actual backing, then uses the
 * kernel's own draw policy. Frontend generation is not a native BO generation.
 * No GPU job or fence is created; success acknowledges only the data epoch. */
int remesa_linux_memory_general_scene_parameters(void *,
 const struct remesa_gpu_scene_parameters *,struct remesa_gpu_scene_parameter_receipt *);
enum remesa_dispatch remesa_linux_memory_general_scene_dispatch(void *,
 const struct remesa_gpu_scene_submit *,struct remesa_gpu_scene_receipt *);
/* Borrow fd/context already bound by the future kernel per-file integration.
 * Never discovers/installs a driver or opens a live node. Current stock/native
 * installations do not provide this resource service. Archived experimental
 * modules register a default-disabled endpoint; its availability and exclusive
 * ownership must be qualified separately. No draw support is provided here.
 * Externally serialized, close only after persistent resources are destroyed. */
struct remesa_linux_memory;
/* Begin a kernel-issued per-file resource session; no GPU submission. */
struct remesa_memory_recorder {
 int (*record)(void *,const struct remesa_memory_request *,const struct remesa_memory_request *,int,int,bool);
 void *priv;
};
int remesa_linux_memory_begin_recorded(int,struct remesa_linux_memory **,const struct remesa_memory_recorder *);
int remesa_linux_memory_begin(int,struct remesa_linux_memory **);
int remesa_linux_memory_attach(int,uint64_t,struct remesa_linux_memory **);
int remesa_linux_memory_info(struct remesa_linux_memory *,uint32_t,struct remesa_memory_info *);
/* Exact retained ID/size/VA/pin/PTE ownership admission for a future executor.
 * Performs INFO only, never submission. This proves CPU PTE ownership, not
 * GPU TLB visibility/completion. An uncertain response poisons the object. */
int remesa_linux_memory_validate_backing(struct remesa_linux_memory *,uint32_t,
                                        uint64_t,size_t);
uint64_t remesa_linux_memory_context(const struct remesa_linux_memory *);
/* Read-only host ownership inventory for separate LAB collectors. Includes
 * CPU-only owners omitted from scene GPU bindings. No ioctl, lease, allocation
 * or authority change; mappings/pins must still be checked by kernel INFO.
 * Poisoned entries remain visible, including an uncertain CREATE with no ID.
 * Output is unchanged on error and cannot alias owner metadata or CPU leases.
 * External serialization is required; this is not a kernel/PRIME handle ABI. */
enum remesa_linux_memory_inventory_flags {
 REMESA_MEMORY_INVENTORY_LIVE=1u,REMESA_MEMORY_INVENTORY_LEASE=2u,
 REMESA_MEMORY_INVENTORY_WRITE=4u,REMESA_MEMORY_INVENTORY_POISON=8u
};
struct remesa_linux_memory_inventory {
 uint64_t context;
 uint32_t version,count;
 struct {uint64_t gpu_va,bytes;uint32_t id,flags;} entries[32];
};
int remesa_linux_memory_inventory_snapshot(const struct remesa_linux_memory *,
 struct remesa_linux_memory_inventory *);
/* Upload caller-owned complete pages without reading the allocation first.
 * No allocation, READ or SYNC; one write lease, bounded WRITE chunks, unlease.
 * Caller owns publication: use sync_for_device when required, or the qualified
 * private executor's existing CPU/GPU publication before SUBMIT. While a
 * private LAB session is active only its sealed role0 PDS may be updated;
 * the kernel still validates every written page against the typed guard.
 * Invalid local ranges never enter ioctl. Any uncertain lease or failure after
 * successful lease retains/poisons the endpoint; no retry or guessed unlease.
 * External serialization and immutable source bytes throughout are required.
 */
int remesa_linux_memory_upload_pages(struct remesa_linux_memory *,uint32_t,
 uint64_t offset,const void *,size_t bytes);
/* Reuse this endpoint's exact fd/drm_file, not a second native-draw path.
 * Kernel admission/borrowed ownership and recorder remain mandatory. */
int remesa_linux_memory_submit_retained(struct remesa_linux_memory *,
 struct remesa_retained_request *,bool *,remesa_retained_recorder,void *);
/* Explicit bounded LAB session on this same fd/context; never enabled by
 * ordinary dispatch or advertised as general GL support. OPEN validates owned
 * mappings once. SUBMIT does one synchronous ioctl with no INFO, allocation,
 * hashes, readback, logs or retry; its exact receipt includes retirement and
 * CPU publication. Raw responses are returned even on uncertain failure, for
 * a separate LAB collector. They still require independent pixel validation.
 * On any entered-ioctl failure all this endpoint's BOs are retained/poisoned;
 * close/free is forbidden through it. forget drops host metadata only.
 * Callers must externally serialize every operation on the borrowed fd and
 * retain resources until acknowledged CLOSE. At most one OPEN attempt; no
 * rearm after CLOSE. Memory-only diagnostic recorders are refused.
 */
int remesa_linux_memory_private_lab_open(struct remesa_linux_memory *,
 const uint32_t ids[11],uint32_t maximum,struct remesa_private_session_lab_request *);
int remesa_linux_memory_private_lab_submit(struct remesa_linux_memory *,
 uint32_t budget_ms,struct remesa_private_session_lab_request *);
int remesa_linux_memory_private_lab_close(struct remesa_linux_memory *,
 struct remesa_private_session_lab_request *);
/* One closed general-scene LAB attempt on this exact resource endpoint.
 * Packet must be a retained real scene_capture snapshot owned by this context;
 * the caller keeps it and its owners until acknowledged return. Only the eight
 * existing kernel profiles are selected; this does not admit ordinary GL.
 * Performs one ioctl, no INFO/hash/readback/retry. Kernel validates actual held
 * images, addresses, permissions, source ownership and complete retirement.
 * Raw response is returned after any entered ioctl. Every entered failure or
 * malformed receipt poisons ALL endpoint resources; only INFO/forget are safe.
 * External serialization, disjoint aligned packet/output and no CPU leases.
 * At most one entered attempt per endpoint; kernel also enforces once/boot. */
int remesa_linux_memory_scene_lab_submit(struct remesa_linux_memory *,
 const struct remesa_scene_packet *,uint32_t profile,uint32_t budget_ms,
 struct remesa_scene_probe_request *);
/* Adapter for persistent_set_native_scene, still restricted to closed LAB5/6.
 * The authenticated kernel receipt is translated into the supplied host job
 * identity only after exact native completion/publication/retirement checks.
 * Local/pre-ioctl refusal is NOT_ISSUED; entered uncertainty is AMBIGUOUS.
 * No caller flags or shader/capability admission are bypassed. */
enum remesa_dispatch remesa_linux_memory_scene_lab_dispatch(void *,
 const struct remesa_gpu_scene_submit *,struct remesa_gpu_scene_receipt *);
/* Separate explicit bound-Gallium closed LAB7/8 adapter. Same ownership,
 * identity, retirement, publication and uncertainty checks; does not change
 * the old adapter's5/6 selection or enable a kernel gate. Requires the matching
 * independently qualified eight-profile kernel, never implicit fallback. */
enum remesa_dispatch remesa_linux_memory_gallium_scene_lab_dispatch(void *,
 const struct remesa_gpu_scene_submit *,struct remesa_gpu_scene_receipt *);
/* Separate closed clip-coordinate/MATRIX_WINDOW profiles9/10; never selects7/8. */
enum remesa_dispatch remesa_linux_memory_gallium_matrix_scene_lab_dispatch(void *,
 const struct remesa_gpu_scene_submit *,struct remesa_gpu_scene_receipt *);
/* Diagnostic snapshot of the already returned raw144-byte kernel response;
 * no ioctl/hash/readback. Requires an entered attempt and disjoint output. */
int remesa_linux_memory_scene_lab_response(const struct remesa_linux_memory *,
 struct remesa_scene_probe_request *);

/* Separate18-binding/11USE gears LAB transport. Requires a matching separately
 * registered kernel ABI/catalogue, not the old17-binding module. Same once-only,
 * ownership, immutable receipt, identity, retirement and all-resource poison
 * contract as scene_lab_submit. Never enables a gate, hashes or reads pixels. */
int remesa_linux_memory_gears_scene_lab_submit(struct remesa_linux_memory *,
 const struct remesa_scene_packet *,uint32_t profile,uint32_t budget_ms,
 struct remesa_gears_scene_probe_request *);
enum remesa_dispatch remesa_linux_memory_gears_scene_lab_dispatch(void *,
 const struct remesa_gpu_scene_submit *,struct remesa_gpu_scene_receipt *);
/* Explicit state15/W2 profiles3/4. Never selected by the old adapter or
 * inferred from target size. Requires the separately staged four-profile
 * kernel; registration is not physical qualification or general GL support. */
enum remesa_dispatch remesa_linux_memory_gears_homogeneous_lab_dispatch(void *,
 const struct remesa_gpu_scene_submit *,struct remesa_gpu_scene_receipt *);
/* Explicit actual-API CODE344/state15 profiles5/6, separately registered LAB
 * candidates. Does not change the older dispatchers or raster admission. */
enum remesa_dispatch remesa_linux_memory_gears_clamped_api_lab_dispatch(void *,
 const struct remesa_gpu_scene_submit *,struct remesa_gpu_scene_receipt *);
int remesa_linux_memory_gears_scene_lab_response(const struct remesa_linux_memory *,
 struct remesa_gears_scene_probe_request *);
/* Optional LAB query after authenticated retirement. One read-only ioctl, no
 * readback/hash/retry. Raw trace output survives query failure. Invalid trace
 * refuses diagnostic approval without undoing the already proven retirement. */
int remesa_linux_memory_gears_scene_lab_trace(struct remesa_linux_memory *,
 struct remesa_gears_scene_trace *);

/* Synchronous typed native copy on this exact owned fd/context. Caller starts
 * version/context/response fields at zero; local owned IDs/mappings and leases
 * are checked before one ioctl. No INFO, hashes, CPU maps/readbacks or retry.
 * Success requires a new monotonic operation and exact SGX535/rev121
 * completion/visibility/retirement. An
 * ambiguous transport/response poisons both allocations; they cannot be
 * unpinned/destroyed through this object. Kernel quarantine survives FD close.
 * Memory-only diagnostic recorders do not record this ABI and are refused.
 * Requires the separately enabled privileged copy endpoint; not GL admission.
 */
int remesa_linux_memory_native_copy(void *,const struct remesa_gpu_copy *);
/* Destination-only variant of the same authenticated synchronous contract. */
int remesa_linux_memory_fill_2d(struct remesa_linux_memory *,struct remesa_twod_fill_request *);
int remesa_linux_memory_native_fill(void *,const struct remesa_gpu_fill *);
int remesa_linux_memory_copy_2d(struct remesa_linux_memory *,struct remesa_twod_copy_request *);
/* Drop ONLY host staging/metadata after all consumers stop. No kernel cleanup,
 * no ioctl, no claim of retirement; caller still owns and must close the fd. */
void remesa_linux_memory_forget(struct remesa_linux_memory *);
int remesa_linux_memory_detach(struct remesa_linux_memory *);
const struct remesa_persistent_ops *remesa_linux_memory_ops(void);
#endif
