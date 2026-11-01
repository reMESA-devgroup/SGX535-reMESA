/* SPDX-License-Identifier: MIT */
#ifndef REMESA_PERSISTENT_H
#define REMESA_PERSISTENT_H
#include "remesa_device.h"
#include "remesa_copy.h"
#include "remesa/provider.h"
#include "remesa_render_target.h"
#include "remesa_program_registers.h"
#include "remesa_pds.h"
#include "remesa_ta_scene.h"
#include "remesa/scene_dpm.h"
#include "remesa/scene_use_limits.h"
#include "remesa/ta_references.h"
#define REMESA_PERSISTENT_BUFFERS 32
/* Internal provider seam, NOT ioctl ABI. All calls/receipts require external
 * serialization and an authenticated provider. GEM ownership transfers even
 * when create returns an error with a handle. Other operations must be atomic
 * on error; EOWNERDEAD explicitly means uncertain side effects: retain backing.
 * This seam does not define PTEs, cache operations or GPU instruction bytes. */
struct remesa_gpu_submit {
 uint64_t context,operation,gpu_va,bytes;
 uint32_t gem_handle;
 const struct remesa_draw_packet *packet; /* retained until release succeeds */
};
struct remesa_gpu_receipt {
 uint64_t context,operation;
 bool retired,published,failed; /* publication is independent of retirement */
};
struct remesa_persistent_ops {
 int (*create)(void *,size_t,uint32_t *);
 int (*pin)(void *,uint32_t);
 int (*gpu_map)(void *,uint32_t,uint64_t,size_t);
 int (*gpu_unmap)(void *,uint32_t,uint64_t,size_t);
 int (*unpin)(void *,uint32_t);
 int (*destroy)(void *,uint32_t);
 int (*cpu_map)(void *,uint32_t,bool,void **);
 int (*cpu_unmap)(void *,uint32_t,bool);
 int (*sync_for_device)(void *,uint32_t);
 enum remesa_dispatch (*dispatch)(void *,const struct remesa_gpu_submit *);
 int (*poll)(void *,uint64_t,uint64_t,uint64_t,struct remesa_gpu_receipt *);
 /* Authenticated retirement plus any required visibility/acknowledgement.
  * Success guarantees commands are no longer read and backing is reusable.
  * The persistent SGX mapping remains installed. EAGAIN is allowed only when
  * no acknowledgement/release side effect occurred; retrying release never
  * resubmits hardware. Other errors retain quarantine semantics. */
 int (*release)(void *,uint64_t,uint64_t);
};
/* CPU placement policy only. These classes do not grant hardware access or
 * establish requester coverage. Every allocation still shares the same VA
 * ledger, reserved ranges, budget, pin/map owner and quarantine rules. */
enum remesa_persistent_va_class {
 REMESA_VA_DEFAULT=0, REMESA_VA_PROGRAM, REMESA_VA_RASTER,
 REMESA_VA_SURFACE, REMESA_VA_CLASS_COUNT
};
struct remesa_persistent_va_range {
 uint64_t start,end,alignment; /* half-open, alignment0 means page size */
};
struct remesa_persistent_config {
 const struct remesa_persistent_ops *ops;void *priv;
 uint64_t context,va_start,va_end;size_t byte_budget;
 bool offline_test_backend,qualified_hardware_backend;
 bool resource_only_backend; /* real allocation transport, dispatch unavailable */
 /* Transfer-storage limits, not GPU rendering/sampling capabilities. Zero
  * dimensions/alignment use layout defaults; bytes are clamped to budget/VA. */
 struct remesa_resource_limits resource_limits;
 /* Occupied VA belonging to other owners of this address space (for example
  * retained LAB BOs). Copied at open; never mapped, freed or relocated here.
  * Half-open page-aligned ranges, inside [va_start,va_end), pairwise disjoint.
  * No ranges means the original private-address-space allocation policy. */
 /* An all-zero entry preserves the original root-range/page policy. Explicit
  * entries must be contained in the root range; overlapping policies are
  * allowed because all live allocations use the same collision ledger.
  * PROGRAM is used by generated code/state/images, SURFACE by typed Z/region.
  * Generic resources retain DEFAULT unless explicitly allocated by class. */
 struct remesa_persistent_va_range placement[REMESA_VA_CLASS_COUNT];
 unsigned reserved_count;
 struct {uint64_t start,end;} reserved[REMESA_PERSISTENT_BUFFERS];
};
/* Zero-initialize device before open. Caller retains the endpoint until all
 * Gallium screens/contexts and fences referring to it have been destroyed.
 * context must be unique among surviving endpoints and authenticated by the
 * integrating kernel backend; it is not a caller-controlled hardware ID.
 * Fresh address-space identity/configuration only; no device discovery or
 * qualification is performed here. The real GEM/MMU binding is not supplied
 * by this library. No mode silently falls back to host rasterization. */
int remesa_persistent_open(struct remesa_device *,const struct remesa_persistent_config *);
/* Explicit native-copy opt-in, before any resources/shaders/captures exist.
 * Leaves draw admission and resource-only flags unchanged. Callback uses the
 * same cfg.priv and ownership as the memory provider. No reentry permitted. */
int remesa_persistent_set_native_copy(struct remesa_device *,remesa_gpu_copy_fn);
int remesa_persistent_copy_region(void *,const struct remesa_copy_region *);
int remesa_persistent_set_native_fill(struct remesa_device *,remesa_gpu_fill_fn);
int remesa_persistent_fill_region(void *,const struct remesa_fill_region *);
/* Completion receipts may arrive out of order; visibility and backing release
 * drain in operation order with nonblocking dependency polls. This is host
 * sequencing, not evidence of reusable SGX execution.
 * Poll detached jobs. Failed/ambiguous jobs and their backing remain retained.
 * Close returns EBUSY while resources/jobs survive; it never force-frees them. */
int remesa_persistent_collect(struct remesa_device *);
/* Explicit generic BO placement for native resource composition. No supplied
 * VA or external backing is accepted. Empty disjoint output required; destroy
 * through device.ops->destroy_bo. Does not submit or qualify any GPU path. */
int remesa_persistent_allocate_placed(struct remesa_device *,size_t bytes,
 enum remesa_persistent_va_class,void **bo);
/* Transfer a provider lifetime to this owner before any allocation. The
 * callback runs once only, after every job/capture/buffer/shader is gone.
 * A failed release retains the owner and caches the error; never retries an
 * uncertain provider close. No callback reentry or device mutation permitted.
 * Failed setup leaves ownership with the caller. */
int remesa_persistent_take_provider(struct remesa_device *,int (*)(void *),void *);
int remesa_persistent_close(struct remesa_device *);
/* Live allocation metadata for building native job bindings. No lifetime hold,
 * byte-content verification, requester qualification or execution permission.
 * Caller serializes destruction/leases and must use the kernel's actual job
 * prepare/check mechanisms before issuing. Includes typed BOs without claiming
 * their payload valid. Any active endpoint CPU lease rejects the snapshot. */
struct remesa_persistent_memory_backing {
 uint64_t owner,generation,gpu_va;
 uint32_t handle;
 size_t logical_bytes,allocation_bytes;
};
int remesa_persistent_memory_snapshot(const struct remesa_device *,void *bo,
 struct remesa_persistent_memory_backing *);
/* Resolve a linear BGRA8 target from this endpoint's live generic BO.
 * Dimensions/pitch are framebuffer metadata, never supplied GPU addresses.
 * This is an ownership-bound CPU descriptor; it does not publish pixels,
 * authorize rendering or change the existing color submission ABI. */
struct remesa_persistent_render_target_backing {
 uint64_t owner,generation,gpu_va;
 uint32_t handle;
 size_t allocation_bytes;
 struct remesa_render_target_state state;
};
int remesa_persistent_render_target_snapshot(const struct remesa_device *,void *bo,
 uint32_t width,uint32_t height,uint32_t pitch_pixels,
 struct remesa_persistent_render_target_backing *);
/* Fresh scene/region backing for the recovered zero-initialized rev121 CPU
 * option0 branch. Allocates the full page-rounded layout, explicitly zeroes
 * and verifies it, then synchronizes through the provider. Zero content is
 * not proof of generated region tables or GPU visibility. No job is issued. */
int remesa_persistent_scene_allocate(struct remesa_device *,uint32_t width,
 uint32_t height,void **bo);
struct remesa_persistent_scene_backing {
 uint64_t owner,generation,gpu_va,region_gpu_va;
 uint32_t handle,width,height;
 size_t allocation_bytes;
 struct remesa_scene_layout layout;
};
/* Live owned mapping only; snapshots are CPU state, not hardware cookies or
 * lifetime holds. Direct host writes and color submission remain forbidden. */
int remesa_persistent_scene_snapshot(const struct remesa_device *,void *bo,
 struct remesa_persistent_scene_backing *);
/* Explicit preparation only: separate padded Z storage through the existing
 * allocator/pin/map owner. No CPU fill is a depth clear, and no scene/DSA,
 * Gallium format or submission support is enabled. Caller destroys the BO
 * through device.ops->destroy_bo. A nonempty output is rejected unchanged.
 * Dimensions are restricted to the existing CPU depth descriptor profile.
 * Requires external serialization; output cannot alias endpoint storage. */
int remesa_persistent_depth_allocate(struct remesa_device *,uint32_t width,
 uint32_t height,uint32_t cpp,void **bo);
struct remesa_persistent_depth_backing {
 uint64_t owner,generation,gpu_va;
 uint32_t handle,width,height,pitch_pixels,cpp;
 size_t allocation_bytes;
 struct remesa_depth_surface_state initial_surface;
};
/* Derive the address/descriptor from this endpoint's live pinned/mapped BO,
 * never caller-supplied VA or a foreign handle. The surface remains UNDEFINED;
 * neither this snapshot nor successful software mapping authenticates GPU
 * requester coverage, TLB visibility, initialization or release after a draw.
 * Any active CPU lease on this endpoint refuses snapshot/allocation. */
int remesa_persistent_depth_snapshot(const struct remesa_device *,void *bo,
 struct remesa_persistent_depth_backing *);
/* Explicit preparation of one immutable word0-only ISP update. Uses the
 * existing allocation/pin/map/VA ledger, never supplied addresses. Layout
 * offsets are host packing policy: STATE8, USE16, PDS60 and TA-reference8 in
 * one page, with zero-filled gaps/tail. They are not a complete scene/job.
 * No bank is programmed or acquired: bank is an explicit entry-plan input,
 * and the caller must qualify bank ownership/requester coverage separately.
 * Success means staged bytes/readback only, not GPU execution admission.
 * Destroy through device.ops->destroy_bo. Public write mapping and the color
 * diagnostic submit reject this typed allocation. Ordinary errors roll back;
 * uncertain provider effects retain quarantine and prevent endpoint close.
 * Externally serialized, output must be empty and disjoint from request.
 */
int remesa_persistent_isp_update_prepare(struct remesa_device *,
 const struct remesa_isp_opaque_triangle_request *,uint32_t use_bank,void **bo);
struct remesa_persistent_isp_update_backing {
 uint64_t owner,generation,gpu_va;
 uint32_t handle;
 size_t allocation_bytes,state_offset,use_offset,pds_offset,ta_offset;
 size_t state_bytes,use_bytes,pds_bytes,ta_bytes;
 struct remesa_use_entry_plan use_entry;
 uint8_t pds_known_mask[60];
};
/* Owned live backing only; rejects foreign/deleted/quarantined objects and
 * any active CPU lease. Does not authenticate GPU visibility or execution.
 * State is immutable for the entire BO lifetime; output is transactional. */
int remesa_persistent_isp_update_snapshot(const struct remesa_device *,void *bo,
 struct remesa_persistent_isp_update_backing *);
/* Distinct final-state object, not an ISP word0 update or vertex PDS. Uses
 * the same owner/immutable staging path, source header2000/rounded extent and
 * close reference12. Does not close/submit a live scene or write any register.
 * CPU preparation success is not physical termination qualification. */
int remesa_persistent_scene_close_prepare(struct remesa_device *,uint32_t width,
 uint32_t height,uint32_t use_bank,void **bo);
struct remesa_persistent_scene_close_backing {
 /* Same byte-range metadata; ta_bytes is12, including the final CPU command. */
 struct remesa_persistent_isp_update_backing image;
};
/* The two snapshot APIs reject objects belonging to the other kind. */
int remesa_persistent_scene_close_snapshot(const struct remesa_device *,void *bo,
 struct remesa_persistent_scene_close_backing *);
enum remesa_persistent_state_kind {
 REMESA_PERSISTENT_STATE_ISP_WORD0=1, REMESA_PERSISTENT_STATE_SCENE_CLOSE=2,
 REMESA_PERSISTENT_STATE_CONSTANT_DRAW=3, REMESA_PERSISTENT_STATE_SCENE_ZCLEAR=4,
 REMESA_PERSISTENT_STATE_AFTER_ZCLEAR=5, REMESA_PERSISTENT_STATE_FRAGMENT_UPDATE=6,
 REMESA_PERSISTENT_STATE_LINKED_DRAW=7
};
/* Explicit f41 draw delta /54c5 green clear state, not complete first-scene
 * state. All fragment references derive from this shader token's owned PDS;
 * no caller VA or borrowed program bytes are accepted. New state retains the
 * entire shader owner until its own backing really disposes, including after
 * public shader deletion. No job hold, bank lease or execution is admitted. */
struct remesa_persistent_constant_state_request {
 struct remesa_isp_opaque_triangle_request isp;
 struct remesa_ta_window_viewport_request viewport;
 enum remesa_ta_scalar_profile scalar_profile;
};
int remesa_persistent_constant_state_prepare(struct remesa_device *,void *shader_token,
 const struct remesa_persistent_constant_state_request *,uint32_t use_bank,void **bo);
/* Explicit cached predecessor contracts, not a live cache or job scheduler.
 * AFTER_ZCLEAR emits b41/52B; FRAGMENT_UPDATE emits40/16B and requires all
 * non-fragment state to be unchanged in the caller's ordered scene. Both
 * retain their owned shader dependency exactly like the full selected delta. */
int remesa_persistent_after_zclear_state_prepare(struct remesa_device *,void *shader_token,
 const struct remesa_persistent_constant_state_request *,uint32_t use_bank,void **bo);
int remesa_persistent_fragment_state_prepare(struct remesa_device *,void *shader_token,
 uint32_t use_bank,void **bo);
int remesa_persistent_zclear_state_prepare(struct remesa_device *,void *shader_token,
 uint32_t use_bank,void **bo);
struct remesa_persistent_scene_state_backing {
 struct remesa_persistent_isp_update_backing image;
 unsigned kind;
 uint64_t shader_owner,shader_generation;
 uint32_t shader_code_handle,shader_pds_handle;
};
/* These new state kinds are rejected by the old ISP/scene-close snapshots.
 * Output unchanged on failure; externally serialized with owner destruction. */
int remesa_persistent_scene_state_snapshot(const struct remesa_device *,void *bo,
 struct remesa_persistent_scene_state_backing *);
struct remesa_shader_request;
struct remesa_shader_bindings;
struct remesa_shader_draw_programs;
struct remesa_shader_image;
struct remesa_usse_vertex_mapping;
struct remesa_draw_plan;
/* Explicit numeric VS CPU candidate plus the original FS candidate. Returns
 * ENOTSUP with an owned token after successful CPU-GEM staging; destroy through
 * device.ops->destroy_shader_programs. No SGX mapping or execution. A nonempty
 * output slot is rejected unchanged. This does not alter Gallium callbacks. */
int remesa_persistent_prepare_vertex_candidate(struct remesa_device *,
 const struct remesa_shader_draw_programs *,const struct remesa_usse_vertex_mapping *,void **);
/* Explicit already-generated image staging. Clones immutable ownership; source
 * may die after return. Authenticated cfg.context and next endpoint generation
 * select the request identity. ENOTSUP+token means verified CPU backing only. */
int remesa_persistent_prepare_image(struct remesa_device *,const struct remesa_shader_image *,void **);
/* CPU-only original-program staging in an owned GEM object. The allocation is
 * pinned for CPU access but has NO SGX mapping, execution address or job hold.
 * A staged CPU candidate is never passed to the diagnostic executor. Tokens
 * are owned by their creating endpoint and must be destroyed before close.
 * Identity cookies are never reused within that endpoint lifetime. At most
 * REMESA_SHADER_COOKIE_LIMIT preparations can publish a token before close;
 * this bounds host metadata, not hardware resources. Retired cookies remain
 * protected from query output aliases and are released only on successful close.
 * Uncertain lease/release failures retain backing and make close return EBUSY.
 */
struct remesa_persistent_shader_backing {
 uint64_t owner,generation;
 uint32_t handle;
 size_t allocation_bytes;
 bool pinned,staged,quarantined;
};
/* Alias-only validation against retained host identities, program images and
 * raw draw snapshots, including quarantined ones. Does not require storage
 * readiness, verify GEM contents or qualify execution. Used before copying
 * endpoint diagnostics so failed staging cannot hide the diagnostic record. */
int remesa_persistent_shader_output_check(const struct remesa_device *,
 const void *output,size_t bytes);
/* Copy the immutable request and storage identity for an owned staged token.
 * This is CPU readback verification, not instruction visibility or execution.
 * Outputs unchanged on failure; external endpoint serialization is required. */
int remesa_persistent_shader_snapshot(const struct remesa_device *,void *token,
 struct remesa_shader_request *,struct remesa_persistent_shader_backing *);
/* Token-specific program-ID/range snapshot derived from the owned allocation.
 * No GPU address or execution entry is supplied. Both outputs are copied only
 * on success, must not alias each other or owned storage, and are valid as
 * bindings only while the token lives under external endpoint serialization. */
int remesa_persistent_shader_bindings(const struct remesa_device *,void *token,
 struct remesa_shader_request *,struct remesa_shader_bindings *);
/* Explicit CPU lease/readback against the immutable body and zero-filled tail.
 * It never maps SGX VA, syncs for execution or submits. A changed body/tail or
 * uncertain lease invalidates admission and retains backing in quarantine. */
int remesa_persistent_shader_verify(struct remesa_device *,void *token);
/* Immutable original pre-VS VB/IB preparation under an owned program token.
 * CPU-GEM allocation only: no SGX mapping, cache sync, submission or ISA.
 * One successful geometry per token; replacement returns EBUSY. Ordinary
 * failures release only tentative geometry; uncertain provider failures keep
 * backing quarantined and close blocked. Source leases remain held for this
 * call. The target framebuffer is described, never owned by this snapshot. */
int remesa_persistent_shader_stage_draw(struct remesa_device *,void *token,
 const struct remesa_shader_draw_inputs *);
/* Borrow the resource owner's single immutable raw input/state snapshot.
 * No provider call or GPU operation. Bytes remain valid until token destruction;
 * both outputs must be disjoint and outside every live payload/retained cookie
 * on this endpoint. Unchanged on failure; serialize against destruction. */
int remesa_persistent_shader_draw_snapshot(const struct remesa_device *,void *token,
 struct remesa_shader_draw_inputs *,struct remesa_draw_plan *);
/* Provider-local handle and byte ranges, not GPU addresses or authentication.
 * VB occupies offset zero. IB is padded to a four-byte host serialization
 * boundary; that alignment is not an independently qualified SGX requirement.
 * Nonindexed geometry has zero index_offset/index_bytes. These ranges preserve
 * the original offsets/start/bias; they contain no CPU-executed vertex output.
 * Output unchanged on error; externally serialized with token destruction. */
struct remesa_persistent_draw_backing {
 uint64_t owner,generation;
 uint32_t handle;
 size_t allocation_bytes,vertex_bytes,index_offset,index_bytes;
 bool pinned,staged;
};
int remesa_persistent_shader_draw_backing(const struct remesa_device *,void *token,
 struct remesa_persistent_draw_backing *);
/* Explicit immutable raw VB/IB promotion through the existing allocator,
 * pin/map/VA ledger. Keeps the original offsets, start, bias and stride in
 * the token's draw snapshot; never performs a vertex transform. Byte-exact
 * CPU readback and sync succeed before publication. Token owns this storage
 * until destruction; no independently destroyable BO or writable map escapes.
 * No physical fetch instruction, job hold, requester or execution is admitted.
 * Ordinary failure rolls back only the tentative allocation; uncertain side
 * effects quarantine the token and block endpoint close. Externally serialized.
 */
int remesa_persistent_shader_prepare_draw_gpu(struct remesa_device *,void *token);
struct remesa_persistent_draw_gpu_backing {
 uint64_t owner,generation,gpu_va;
 uint32_t handle,min_vertex,max_vertex;
 size_t allocation_bytes,vertex_bytes,index_offset,index_bytes;
};
/* VA is allocator-derived. Committed byte ranges exclude deterministic zero
 * padding and must be used for fetch bounds. Unchanged on failure. */
int remesa_persistent_shader_draw_gpu_backing(const struct remesa_device *,void *token,
 struct remesa_persistent_draw_gpu_backing *);
/* Opt-in immutable GPU ownership for the exact generated POSITION4 VS16 and
 * packed constant FS8 pair, in that order. Reuses allocation/pin/map/VA ledger,
 * verifies zero-padded code byte-for-byte and syncs before publication. Banks
 * are explicit software entry-plan inputs, not bank leases or admission.
 * The token owns this BO; ordinary errors roll back only tentative storage,
 * uncertainty quarantines ownership. No execution state, task or job is granted.
 * Optional PDS storage and unproved host grammars return ENOTSUP unchanged. */
int remesa_persistent_shader_prepare_gpu(struct remesa_device *,void *token,
 const uint32_t banks[2]);
struct remesa_persistent_shader_gpu_backing {
 uint64_t owner,generation,gpu_va;
 uint32_t handle;
 size_t allocation_bytes,program_offset[2],program_bytes[2];
 struct remesa_use_entry_plan entry[2];
};
/* Allocator-derived code addresses and exact committed ranges, never arbitrary
 * caller VAs. No CPU lease or independently destroyable BO escapes. Snapshot
 * is unchanged on failure and requires external serialization with teardown. */
int remesa_persistent_shader_gpu_backing(const struct remesa_device *,void *token,
 struct remesa_persistent_shader_gpu_backing *);
struct remesa_shader_linkage;
/* Common shader CODE ownership, independent of the legacy shader-image token.
 * Generates bytes from the owned semantic pair using the selected-resource
 * compiler, then allocates/pins/maps in the existing PROGRAM VA domain.
 * Reserved capacity is zeroed but is never counted as effective CODE.
 * No PDS task, bank lease, job, submission or execution permission is granted.
 * Empty disjoint output required. Destroy through device.ops->destroy_bo.
 * Uncertain failures retain backing; a non-NULL output transfers ownership
 * even on error. Endpoint close may refuse until known-good recovery.
 */
int remesa_persistent_linked_program_prepare(struct remesa_device *,
 const struct remesa_shader_linkage *,const uint32_t banks[2],void **bo);
struct remesa_persistent_linked_program_backing {
 uint64_t owner,generation,gpu_va;
 uint32_t handle;
 size_t allocation_bytes,program_offset[2],program_bytes[2],reserved_bytes[2];
 struct remesa_use_entry_plan entry[2];
};
/* Bounded metadata only, without hashing/readback. Caller serializes object
 * destruction and must acquire kernel job holds before GPU use. */
int remesa_persistent_linked_program_snapshot(const struct remesa_device *,void *bo,
 struct remesa_persistent_linked_program_backing *);
/* Raw input storage for the currently compiled three-float4 vertex profile.
 * Bytes are copied verbatim; no vertex transform, lighting or viewport runs.
 * Indexed input uses host u16 values, emitted as LE. Without indices, count
 * must be a triangle list and an identity index list is generated. Limits
 * below are bounded software preparation policies, not GPU capability claims.
 */
#define REMESA_LINKED_MAX_VERTICES 4095u
struct remesa_linked_geometry_input {
 const void *vertices;size_t vertex_bytes,index_bytes;
 uint32_t vertex_count,index_count;
 const uint16_t *indices;
};
int remesa_persistent_linked_geometry_prepare(struct remesa_device *,
 const struct remesa_linked_geometry_input *,void **bo);
struct remesa_persistent_linked_geometry_backing {
 uint64_t owner,generation,gpu_va;
 uint32_t handle,vertex_count,index_count,min_vertex,max_vertex,stride_bytes;
 size_t allocation_bytes,vertex_bytes,index_offset,index_bytes;
};
int remesa_persistent_linked_geometry_snapshot(const struct remesa_device *,void *bo,
 struct remesa_persistent_linked_geometry_backing *);
/* Derives all PDS addresses/task fields from retained CODE and raw geometry
 * owners. No caller GPU addresses or task words. The existing selected
 * WHITE_LAMBERT3/COLOR0 CPU producer pair remains the admission boundary.
 * Successful preparation retains both dependencies even if callers destroy
 * their original references. It grants no native job/execution permission. */
int remesa_persistent_linked_pds_prepare(struct remesa_device *,void *program,
 void *geometry,void **bo);
struct remesa_persistent_linked_pds_backing {
 uint64_t owner,generation,gpu_va;
 uint32_t handle;
 /* VS primary, FS primary, empty FS secondary; only primaries have plans. */
 size_t allocation_bytes,object_offset[3],image_bytes[3],reserved_bytes[3];
 struct remesa_persistent_linked_program_backing program;
 struct remesa_persistent_linked_geometry_backing geometry;
 struct remesa_pds_selected_plan plans[2];
 uint8_t known_mask[288];
};
int remesa_persistent_linked_pds_snapshot(const struct remesa_device *,void *bo,
 struct remesa_persistent_linked_pds_backing *);
/* Encode the existing POSITION4+COLOR0 triangle command from a live PDS
 * closure. No caller-supplied VA, descriptor word, stride or output format.
 * Bounds are the immutable index payload, not the allocation's spare capacity.
 * The caller must retain the PDS owner and serialize access until the command
 * is consumed by a separately owned scene. This is not a complete TA stream,
 * fragment/viewport state, native job, fence or execution permission.
 * Positive errno; disjoint output remains unchanged on failure. */
int remesa_persistent_linked_index_encode(const struct remesa_device *,void *pds,
 uint32_t first_index,uint32_t index_count,uint8_t out[20]);
/* Owned 14-DWORD ISP/fragment/viewport/scalar draw state for the currently
 * selected POSITION4+COLOR0 pair. Derives primary/secondary references and
 * scalar output layout from the live PDS owner. Retains that complete closure
 * until actual disposal, including uncertain failures. Explicit typed ISP
 * and viewport input only; no caller VA, shader bytes or descriptor words.
 * This is a draw delta, NOT initial scene bounds, clear, close or native job.
 * MATRIX_WINDOW bytes are a CPU contract until separately tested on the GPU.
 * Empty/disjoint output required; uncertain errors may return a retained BO. */
struct remesa_persistent_linked_state_request {
 struct remesa_isp_opaque_triangle_request isp;
 struct remesa_ta_window_viewport_request viewport;
};
int remesa_persistent_linked_state_prepare(struct remesa_device *,void *pds,
 const struct remesa_persistent_linked_state_request *,uint32_t state_bank,void **bo);
struct remesa_persistent_linked_state_backing {
 struct remesa_persistent_isp_update_backing image;
 struct remesa_persistent_linked_pds_backing dependencies;
};
int remesa_persistent_linked_state_snapshot(const struct remesa_device *,void *bo,
 struct remesa_persistent_linked_state_backing *);
/* Ordered dynamic draw-body storage: (state reference8 + indexed draw20)*N
 * followed by the owned scene-close12. Not an initial scene: bounds/clears,
 * attachments, auxiliary raster programs and native submission are separate.
 * The stream retains each distinct state and close owner; states transitively
 * retain PDS/CODE/geometry. No user VA, raw command bytes or GPU permission.
 * All state/shader banks must agree across the complete body; incompatible
 * register values reject before allocation. Caller serializes owner access.
 */
#define REMESA_LINKED_BODY_MAX_DRAWS 16u
struct remesa_linked_body_draw {void *state;uint32_t first_index,index_count;};
int remesa_persistent_linked_body_prepare(struct remesa_device *,
 const struct remesa_linked_body_draw *,size_t count,void *scene_close,void **bo);
struct remesa_persistent_linked_body_backing {
 uint64_t owner,generation,gpu_va;
 uint32_t handle,draw_count,dependency_count;
 size_t allocation_bytes,command_bytes,close_offset;
};
int remesa_persistent_linked_body_snapshot(const struct remesa_device *,void *bo,
 struct remesa_persistent_linked_body_backing *);
/* Self-contained immutable storage for the existing selected Z-clear draw:
 * generated POSITION VS16/green FS8, full-target single-triangle geometry,
 * primary/secondary PDS, special54c5 state and its per-count auxiliary/PDS,
 * and TA28. No attachment, raster/EOT program, job or physical admission.
 * Green is the existing state-constructor dependency; this is not a COLOR
 * clear. Caller must establish real COLOR/Z backing and initialization policy.
 * All program addresses derive from this allocation, banks are explicit.
 */
int remesa_persistent_zclear_prefix_prepare(struct remesa_device *,
 uint32_t width,uint32_t height,float depth,uint32_t vertex_bank,
 uint32_t pixel_bank,uint32_t state_bank,void **bo);
struct remesa_persistent_zclear_prefix_backing {
 uint64_t owner,generation,gpu_va;
 uint32_t handle,width,height,index_count;
 size_t allocation_bytes,used_bytes,vertex_offset,index_offset;
 size_t vertex_capacity_bytes,vertex_referenced_bytes,index_capacity_bytes,index_referenced_bytes;
 size_t use_offset[3],use_bytes[3],pds_offset[4],pds_bytes[4];
 size_t state_offset,state_bytes,ta_offset,ta_bytes;
 float clear_depth; /* Same validated scalar used by the immutable clear geometry. */
 struct remesa_use_entry_plan entries[3];
};
int remesa_persistent_zclear_prefix_snapshot(const struct remesa_device *,void *bo,
 struct remesa_persistent_zclear_prefix_backing *);
/* Immutable TA command owner: generated Z-clear28 + dynamic body, including
 * its close12. Retains both owners and checks their dimensions/bank agreement.
 * Does not yet own attachments, raster/background/EOT/DPM or authorize a job.
 * The allocation list describes the complete command-referenced READ set,
 * not measured hardware fetch lengths; backing capacity is reported explicitly.
 * Original body storage is retained for lifecycle but is not referenced by
 * the combined GPU stream. No ioctl/readback/hash occurs during snapshots.
 * Mapping/owner access is externally serialized. */
int remesa_persistent_linked_ta_prepare(struct remesa_device *,void *zclear,
 void *body,void **bo);
struct remesa_persistent_linked_ta_backing {
 uint64_t owner,generation,gpu_va;
 uint32_t handle,width,height,draw_count,read_allocation_count;
 size_t allocation_bytes,command_bytes,draw_offset,close_offset;
 struct {
  uint64_t generation,gpu_va;
  uint32_t handle,access; /* REMESA_ACCESS_READ only */
  size_t allocation_bytes;
 } reads[REMESA_PERSISTENT_BUFFERS];
};
int remesa_persistent_linked_ta_snapshot(const struct remesa_device *,void *bo,
 struct remesa_persistent_linked_ta_backing *);
/* Bind live COLOR/Z backing to a combined TA without transferring raw VAs.
 * Generates color target DATA16 and TA control DATA56 in immutable storage.
 * Requires a normal BGRA8 allocation and matching padded cpp4 Z allocation.
 * Retains all three owners; caller deletion does not release their backing.
 * This does not initialize COLOR/Z, generate raster/EOT state or submit a job.
 * No content/clear/preserve or physical format support is inferred from it.
 */
int remesa_persistent_linked_targets_prepare(struct remesa_device *,void *ta,
 void *color,void *depth,uint32_t color_pitch_pixels,void **bo);
struct remesa_persistent_linked_targets_backing {
 uint64_t owner,generation,gpu_va;
 uint32_t handle,color_access,depth_access;
 size_t allocation_bytes,used_bytes,color_data_offset,ta_control_offset;
 struct remesa_persistent_linked_ta_backing commands;
 struct remesa_persistent_render_target_backing color;
 struct remesa_persistent_depth_backing depth;
};
int remesa_persistent_linked_targets_snapshot(const struct remesa_device *,void *bo,
 struct remesa_persistent_linked_targets_backing *);
/* Explicit full committed COLOR-plane initialization through registered native
 * fill, before capture. Derives pitch/height/handle from this live target graph;
 * includes row padding columns, excludes page tail, and accepts no raw VA.
 * Requires synchronous authenticated completion/publication/retirement from
 * the existing callback. No CPU fallback, implicit per-frame clear, memoization
 * or new initialization certificate. Return0 is that callback's attestation,
 * not new physical qualification or permission to clear scene admission bits.
 * Held child owners may have been publicly released; frozen scenes/CPU leases
 * reject. Ordinary failure is NOT issued; uncertainty quarantines/retains.
 */
int remesa_persistent_linked_targets_color_initialize(struct remesa_device *,
 void *targets,uint32_t packed_bgra);
/* Owned original background constructor for the current BGRA8 target.
 * Generates auxiliary USE8, primary PDS64, empty secondary4 in PROGRAM and
 * background object68 in RASTER. Retains the target binding and both backing
 * allocations. Requires matching pixel-bank words with the entire TA closure.
 * Uses exact logical target dimensions and depth1; does not enlarge the plane,
 * clear COLOR, generate EOT/event CODE, authorize sampling or submit a job.
 * Undefined COLOR remains undefined: a future job must establish valid input.
 */
int remesa_persistent_linked_background_prepare(struct remesa_device *,void *targets,
 uint32_t pixel_bank,void **bo);
struct remesa_persistent_linked_background_backing {
 uint64_t owner,generation,gpu_va,program_generation,program_gpu_va;
 uint32_t handle,program_handle;
 size_t allocation_bytes,object_bytes,program_allocation_bytes,program_used_bytes;
 size_t use_offset,use_bytes,primary_offset,primary_bytes,secondary_offset,secondary_bytes;
 struct remesa_use_entry_plan entry;
 uint8_t primary_known_mask[64];
 struct remesa_persistent_linked_targets_backing targets;
};
int remesa_persistent_linked_background_snapshot(const struct remesa_device *,void *bo,
 struct remesa_persistent_linked_background_backing *);
/* Generated PSB event USE auxiliaries only, not a complete event or a job.
 * PROGRAM layout: transfer16@0, end8@32; used40 with zero reserved padding.
 * BGRA8 target ownership selects original mode0. Retains the entire target/TA
 * dependency closure until successful disposal, including uncertain teardown.
 * Pixel bank must agree with all TA command references. No caller raw VA/CODE.
 * Ordinary failures unwind; uncertain provider effects quarantine and return
 * the retained object when available. External serialization is required.
 * Snapshots use metadata only; no per-frame readback, ioctl, hash or sync.
 */
int remesa_persistent_linked_event_auxiliary_prepare(struct remesa_device *,
 void *targets,uint32_t pixel_bank,void **bo);
struct remesa_persistent_linked_event_auxiliary_backing {
 uint64_t owner,generation,gpu_va;
 uint32_t handle,use_lower19[2]; /* transfer, end */
 size_t allocation_bytes,used_bytes,program_offset[2],program_bytes[2];
 struct remesa_use_entry_plan entries[2];
 struct remesa_persistent_linked_targets_backing targets;
 /* CPU serialization, NOT bytes appended to the auxiliary USE allocation.
  * DATA0/4 derive from end/transfer entry plans; DATA2 points to the live
  * target owner's committed16-byte COLOR-state region. Unwritten original
  * cells6/7/15 remain policy-zero/mask0. No event CODE or PDS BO exists here.
  * Do not confuse event_data_bytes64 with auxiliary used_bytes40/capacity.
  */
 size_t event_data_bytes;
 uint8_t event_data[64],event_data_known_mask[64];
};
int remesa_persistent_linked_event_auxiliary_snapshot(const struct remesa_device *,
 void *bo,struct remesa_persistent_linked_event_auxiliary_backing *);
/* Owned fixed392ab event profile using the preserved original CPU recipe.
 * No caller CODE/VA accepted: derive DATA from one live auxiliary/target
 * graph, publish once with the existing immutable lease/verification/sync.
 * Holds the entire graph through public owner release and uncertain disposal.
 * This is a profile-specific program owner, NOT a general PDS ISA encoder or
 * native execution admission. Existing scene NEEDS flags remain unchanged.
 * Original reserve400, commit148, DATA64/CODE84, descriptor field0c16; none
 * of these is claimed as the hardware fetch extent. Public writes and legacy
 * color submission reject. Snapshots have no provider I/O/hash/sync.
 */
int remesa_persistent_linked_event_profile_prepare(struct remesa_device *,
 void *event_auxiliary,void **bo);
struct remesa_persistent_linked_event_profile_backing {
 uint64_t owner,generation,gpu_va;
 uint32_t handle,descriptor_field0c;
 size_t allocation_bytes,reserved_bytes,used_bytes,data_bytes,code_offset,code_bytes;
 struct remesa_persistent_linked_event_auxiliary_backing auxiliary;
};
int remesa_persistent_linked_event_profile_snapshot(const struct remesa_device *,
 void *bo,struct remesa_persistent_linked_event_profile_backing *);
/* Own a CPU DPM load plan and retain two live storage allocations. Uses only
 * the already demonstrated fixed32MiB table/parameter capacity profile.
 * Table belongs to SURFACE40000000..ffffffff, parameter to RASTER30000000..
 * 3fffffff with1MiB alignment. Addresses derive from this endpoint's BOs.
 * No table/parameter map, clear or sync is performed; their initialization
 * and GPU access still need the native job contract. This is NOT MMIO or
 * permission to execute a load. Native completion must retain mask15, never
 * substitute original CPU wait argument7. Plan DATA240/page4096 only.
 * Shares references safely; no per-frame allocation, hashing or readback.
 * Existing ownership, leases, provider sync, error/quarantine rules apply.
 */
int remesa_persistent_linked_dpm_prepare(struct remesa_device *,void *table,
 void *parameter,void **bo);
struct remesa_persistent_linked_dpm_backing {
 uint64_t owner,generation,gpu_va;
 uint32_t handle,table_access,parameter_access;
 size_t allocation_bytes,data_bytes;
 struct remesa_persistent_memory_backing table,parameter;
 struct remesa_lab_dpm_control plan;
};
int remesa_persistent_linked_dpm_snapshot(const struct remesa_device *,void *bo,
 struct remesa_persistent_linked_dpm_backing *);
/* Dimension-bound region owner for a live COLOR/Z/TA target chain. Reuses
 * the original option0 layout and existing scene allocator's one-time zero
 * policy. Holds targets through leases and uncertain disposal; public writes
 * and legacy color submission remain rejected. No raw VA/dimensions accepted.
 * Snapshots derive7 TA register/value pairs and the separate408 pair from
 * the live region base plus original cookie offsets. They do no I/O/hash/sync.
 * This is CPU storage/register-plan metadata, not MMIO permission, tile-list
 * generation, native job holds or GPU execution. No cookie15 is invented.
 */
int remesa_persistent_linked_region_prepare(struct remesa_device *,void *targets,
 void **bo);
/* Explicit raw scratch variant for a kernel zero-origin admission path.
 * Same dimension-bound owner/layout, no CPU write/read/sync or initialized
 * content assertion. Public reads remain EAGAIN until a real writer completes.
 * Metadata/capture may retain undefined storage; native admission still must
 * authenticate fresh kernel zero provenance and publication. The existing
 * prepare API keeps its original CPU zero/verification/sync behavior.
 */
int remesa_persistent_linked_region_allocate(struct remesa_device *,void *targets,
 void **bo);
struct remesa_persistent_linked_region_backing {
 struct remesa_persistent_scene_backing region;
 struct remesa_persistent_linked_targets_backing targets;
 uint32_t ta_scene[14],raster_scene[2],access;
};
int remesa_persistent_linked_region_snapshot(const struct remesa_device *,void *bo,
 struct remesa_persistent_linked_region_backing *);
/* Conservative allocation-level GPU references for one coherent scene.
 * Borrows four live frontend owners, with external serialization; does NOT
 * create job holds, issue ioctls or admit execution. All three scene owners
 * must refer to the identical COLOR/Z/TA target owner. The event argument
 * accepts either the previous auxiliary owner or the new fixed-profile PDS
 * owner; the latter adds one READ reference and retains its auxiliary. An
 * admitted physical event is still pending, so all NEEDS bits remain set.
 * No raw VA/CODE accepted.
 * Logical and rounded extents are not measured GPU fetch lengths. The old
 * copied body BO and CPU-only DPM plan BO are deliberately not GPU references.
 * Undefined attachment/DPM contents remain undefined. Every returned result
 * retains all NEEDS bits below; success is resource metadata only.
 */
enum remesa_scene_reference_role {
 REMESA_SCENE_REF_TA=1u<<0,REMESA_SCENE_REF_TARGET_DATA=1u<<1,
 REMESA_SCENE_REF_COLOR=1u<<2,REMESA_SCENE_REF_DEPTH=1u<<3,
 REMESA_SCENE_REF_BACKGROUND=1u<<4,REMESA_SCENE_REF_BACKGROUND_PROGRAM=1u<<5,
 REMESA_SCENE_REF_EVENT_AUXILIARY=1u<<6,REMESA_SCENE_REF_REGION=1u<<7,
 REMESA_SCENE_REF_DPM_TABLE=1u<<8,REMESA_SCENE_REF_DPM_PARAMETER=1u<<9,
 REMESA_SCENE_REF_EVENT_PDS=1u<<10
};
enum remesa_scene_incomplete {
 REMESA_SCENE_NEEDS_EVENT_PDS=1u<<0,REMESA_SCENE_NEEDS_COLOR_LOAD_INIT=1u<<1,
 REMESA_SCENE_NEEDS_DPM_INIT=1u<<2,REMESA_SCENE_NEEDS_RASTER_CONTROL=1u<<3,
 REMESA_SCENE_NEEDS_NATIVE_ADMISSION=1u<<4
};
struct remesa_persistent_scene_footprint {
 uint64_t owner;
 uint32_t width,height,draw_count,allocation_count,incomplete;
 struct {
  uint64_t generation,gpu_va;
  uint32_t handle,access,roles;
  size_t logical_bytes,allocation_bytes;
 } allocations[REMESA_PERSISTENT_BUFFERS];
};
int remesa_persistent_scene_footprint(const struct remesa_device *,void *background,
 void *event_auxiliary,void *region,void *dpm,
 struct remesa_persistent_scene_footprint *);
/* Retain and freeze one validated scene reference set before native job
 * preparation. Internal userspace token, not a fence or ioctl ABI. No provider
 * I/O, allocation, initialization, dispatch or capability admission. All
 * footprint NEEDS bits remain set. External serialization is required.
 * Captures keep references after frontend owner destruction. Public CPU maps
 * and legacy submission otherwise admitted return EBUSY; immutable write and
 * backend admission gates retain their existing errors. Up to eight
 * captures may coexist, including shared graphs. Release cancels an UNISSUED
 * capture only; it is never GPU completion/retirement or uncertain-job teardown.
 * A future submission API must transfer these holds into its authenticated
 * native lifecycle; it cannot call release to acknowledge GPU execution.
 * If cancellation reaches disposal and disposal fails, the capture token is
 * consumed but uncertain backing stays quarantined; retrying returns ESTALE.
 */
struct remesa_scene_capture {uint64_t owner,serial;uint32_t slot,reserved;};
int remesa_persistent_scene_capture(struct remesa_device *,void *background,
 void *event_auxiliary,void *region,void *dpm,struct remesa_scene_capture *);
int remesa_persistent_scene_capture_snapshot(const struct remesa_device *,
 struct remesa_scene_capture,struct remesa_persistent_scene_footprint *);
/* Derive original raster208/TA56 and separate region register groups from
 * one frozen captured graph, including after all public owners are released.
 * Requires a fixed event PDS owner; auxiliary-only captures return ENOTSUP.
 * Clear depth comes from the retained Z-clear prefix, stencil is0 and the
 * original depth finalization predicate isfalse. Z remains UNDEFINED.
 * These are CPU command groups for a future authenticated native consumer,
 * never GPU CODE/BOs, MMIO writes, initialization or execution admission.
 * All incomplete bits remain; no allocation/provider I/O/hash/sync occurs.
 * External serialization required; disjoint aligned output unchanged on error.
 */
struct remesa_persistent_scene_controls {
 uint64_t owner,serial;
 uint32_t width,height,incomplete;
 size_t raster_bytes,ta_bytes;
 uint8_t raster[REMESA_RASTER_CONTROL_BYTES],ta[REMESA_TA_CONTROL_BYTES];
 uint32_t ta_scene[14],raster_scene[2];
};
int remesa_persistent_scene_controls_snapshot(const struct remesa_device *,
 struct remesa_scene_capture,struct remesa_persistent_scene_controls *);
/* Complete USE entry windows of this frozen graph, not program-byte validation
 * or a hardware bank lease. Preserve exact producer lengths, not reservations.
 * The bound is prefix3 + close1 + at most3 per draw + background1 + event2.
 * No allocation/provider I/O, execution admission or capability change. */
#define REMESA_SCENE_USE_ENTRIES (3u+1u+REMESA_LINKED_BODY_MAX_DRAWS*3u+1u+2u)
struct remesa_persistent_scene_use {
 uint64_t owner,serial;
 uint32_t entry_count,bank_mask,incomplete;
 struct {uint32_t offset,word;} banks[16];
 struct {
  uint64_t generation;
  uint32_t handle,bank,data_master,lower19;
  size_t byte_offset,program_bytes;
 } entries[REMESA_SCENE_USE_ENTRIES];
};
int remesa_persistent_scene_use_snapshot(const struct remesa_device *,
 struct remesa_scene_capture,struct remesa_persistent_scene_use *);
/* Resolve the exact emitted CPU TA shadow against this frozen typed graph.
 * minimum_bytes comes from TA syntax; object_bytes is the committed producer
 * extent. Neither is a measured GPU fetch length. No provider readback/hash,
 * initialization, instruction certification or native admission. */
struct remesa_persistent_scene_ta {
 uint64_t owner,serial;
 uint32_t draw_count,reference_count,incomplete;
 struct {
  uint64_t generation;
  uint32_t handle,byte_offset,minimum_bytes,object_bytes,kind,index_count,draw;
 } references[REMESA_TA_REFERENCE_MAX];
};
int remesa_persistent_scene_ta_snapshot(const struct remesa_device *,
 struct remesa_scene_capture,struct remesa_persistent_scene_ta *);
/* Exact committed geometry windows for prefix + each draw in the frozen TA.
 * Metadata only: no provider I/O, index scan, shader validation or admission.
 * Frontend index validation does not replace the native actual-input check. */
struct remesa_persistent_scene_vertices {
 uint64_t owner,serial;uint32_t draw_count,window_count,incomplete;
 struct {
  uint64_t index_generation,vertex_generation;
  uint32_t index_handle,index_offset,index_bytes,vertex_handle,vertex_offset,vertex_bytes,vertex_count;
 } windows[REMESA_TA_REFERENCE_MAX_DRAWS+1u];
};
int remesa_persistent_scene_vertices_snapshot(const struct remesa_device *,
 struct remesa_scene_capture,struct remesa_persistent_scene_vertices *);
int remesa_persistent_scene_capture_release(struct remesa_device *,
 struct remesa_scene_capture);
/* Explicit CPU-profile PDS ownership, derived solely from the same token's
 * immutable code and raw-geometry GPU owners. Restricts POSITION4/input0,
 * stride16, bias0; bounds come from the original validated draw snapshot.
 * No arbitrary VA, task word or code bytes are accepted. Object order is
 * primaryVS64, primaryFS56, empty-secondary4, each32-byte aligned. */
#define REMESA_PERSISTENT_SHADER_PDS_BYTES 132u
int remesa_persistent_shader_prepare_pds_gpu(struct remesa_device *,void *token);
struct remesa_persistent_shader_pds_gpu_backing {
 uint64_t owner,generation,gpu_va;
 uint32_t handle,min_vertex,max_vertex,vertex_stride,use_lower19[2];
 size_t allocation_bytes,object_offset[3],object_bytes[3],data_bytes[3],code_bytes[3];
 /* Producer evidence, not hardware qualification. FS DATA0 remains mask0
  * despite the separately derived owned USE fixup; all padding is mask0. */
 uint8_t known_mask[REMESA_PERSISTENT_SHADER_PDS_BYTES];
};
/* Token owns this immutable allocation and all referenced owners. Snapshot
 * grants no job hold, physical DATA preload, bank lease or execution. Outputs
 * unchanged on failure; external serialization with destruction is required. */
int remesa_persistent_shader_pds_gpu_backing(const struct remesa_device *,void *token,
 struct remesa_persistent_shader_pds_gpu_backing *);
struct remesa_pds_host_data_view;
/* Snapshot and CPU-stage two original HOST DATA windows in one owned pinned
 * backing under the existing shader token. VS then FS, at most1024bytes each.
 * Internal sixteen-byte packing is host policy, not DS layout or fetch rules.
 * No CODE, SGX VA/map, cache publication, task launch or execution is supplied.
 * One pair per token. Ordinary failures roll back; uncertain provider effects
 * quarantine backing and block close. External serialization is required. */
int remesa_persistent_shader_stage_pds_data(struct remesa_device *,void *token,
 const struct remesa_pds_host_data_view *);
struct remesa_persistent_pds_backing {
 uint64_t owner,generation;
 uint32_t handle;
 size_t allocation_bytes,data_offset[2],data_bytes[2];
 bool pinned,staged;
};
/* Fresh token-specific provider ID and exact HOST ranges, never execution
 * addresses. Both stages are published together after byte-exact CPU readback.
 * Output unchanged on failure; teardown invalidates the binding. */
int remesa_persistent_shader_pds_backing(const struct remesa_device *,void *token,
 struct remesa_persistent_pds_backing *);
/* One immutable allocation replaces separate shader/raw/PDS/state backing for
 * the bounded diagnostic scene. It does not own Z/color/auxiliaries, create
 * native job holds or authorize execution. Request VA/bytes must both be0;
 * allocation and every internal reference derive from this endpoint. Generated
 * programs are copied, so their host images may be released after success.
 * Public writes and color-submit reject this allocation; destroy_bo owns it.
 * Uncertain map/unmap/sync retains quarantine. External serialization required. */
struct remesa_depth_scene_image_request;
struct remesa_persistent_depth_scene_backing {
 uint64_t owner,generation,gpu_va;
 uint32_t handle,width,height;
 /* enum remesa_depth_vertex_profile from remesa_depth_scene_image.h. This
  * reports the selected immutable CPU recipe, not GPU qualification. */
 uint32_t vertex_profile;
 size_t allocation_bytes,used_bytes,ta_offset,ta_bytes;
 struct remesa_use_entry_plan use_entries[3]; /* VS, FS, state upload */
};
int remesa_persistent_depth_scene_prepare(struct remesa_device *,
 const struct remesa_depth_scene_image_request *,void **bo);
int remesa_persistent_depth_scene_snapshot(const struct remesa_device *,void *bo,
 struct remesa_persistent_depth_scene_backing *);
int remesa_persistent_depth_scene_verify(struct remesa_device *,void *bo);
/* Immutable raster208 + TA56 control DATA; zeroes/checks rounded padding.
 * No referenced-BO ownership, admission, native holds or GPU execution.
 * Public write mapping and color submission reject this typed allocation.
 * Ordinary failure preserves output; uncertain provider effects quarantine.
 * External serialization with all referenced owners is required. */
#define REMESA_CONTROL_IMAGE_BYTES (REMESA_RASTER_CONTROL_BYTES+REMESA_TA_CONTROL_BYTES)
int remesa_persistent_control_prepare(struct remesa_device *,
 const struct remesa_raster_control_request *,
 const struct remesa_raster_object_reference *,void **bo);
int remesa_persistent_control_snapshot(const struct remesa_device *,void *bo,
 struct remesa_persistent_memory_backing *);
int remesa_persistent_control_verify(struct remesa_device *,void *bo);
#endif
