/* SPDX-License-Identifier: MIT */
#ifndef REMESA_SCREEN_H
#define REMESA_SCREEN_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "remesa_texture_descriptor.h"
#include "remesa_copy.h"
/* Opaque factory ABI also used by the standalone ownership-model tests.
 * The Gallium adapter qualification compiles against pinned Mesa headers. */
struct pipe_screen;
struct remesa_device;
/* Explicit CPU-recording LAB attachment opt-in before children exist.
 * Enables Z24X8 host storage/clear; every draw with Z bound stays ENOTSUP. */
int remesa_screen_enable_cpu_depth24_lab(struct pipe_screen *);
struct pipe_context; struct pipe_resource; struct pipe_fence_handle;
/* Explicit original window-serializer branch for the bound Gallium viewport.
 * Resource-only endpoints may bind finite general viewport state; legacy
 * executable endpoints keep their old restrictions. This call derives bounds
 * from the currently retained color target and accepts only positive X,
 * negative Y, integral in-target rectangles and the existing dyadic Z contract.
 * depth_bits selects the CPU codec normalization (16/24), not an attachment
 * capability or initialized-depth claim. This does not infer window/FBO
 * identity, grant raster state, upload, draw admission or execution. A caller
 * must select this explicit window branch appropriately; other orientations
 * reject rather than silently invert a second time. Output is unchanged on
 * error; no allocation, mapping, hashing or provider operation is performed.
 */
int remesa_context_window_viewport_encode(struct pipe_context *,uint32_t depth_bits,
 unsigned char out[28]);
struct remesa_ta_window_viewport_request;
/* Same explicit original window branch and validation as the encoder above,
 * returned as owned typed input for the existing scene-state builder. No CPU
 * vertex transform, upload, allocation, draw or physical viewport claim.
 * MATRIX_WINDOW is distinct from the physically qualified INITIAL_ZERO path.
 * Disjoint aligned output storage required; errors preserve the output. */
int remesa_context_window_viewport_snapshot(struct pipe_context *,uint32_t depth_bits,
 struct remesa_ta_window_viewport_request *);
struct remesa_vertex_parameter_values;
/* Two dense vec4s: bound scale.xyz/1 and translate.xyz/0, plus eight zero
 * coefficient words for the existing16-word upload policy. No vertex values
 * are evaluated. This supplies affine parameters, not perspective divide or
 * proof that arbitrary W is supported by a consuming program. */
int remesa_context_window_viewport_parameters_snapshot(struct pipe_context *,
 struct remesa_vertex_parameter_values *);
struct remesa_shader_program_report;
struct remesa_shader_pipeline;
struct remesa_shader_linkage;
/* Copy the current context-owned VS/FS into the common desktop/GLES link.
 * Includes POSITION/COLOR0 IO and interpolation metadata, independent of the
 * older constant-fragment draw ABI. Uniform specialization needs synchronized
 * draw resources and remains unsupported here. No state/error mutation,
 * allocation, GPU upload, draw admission or capability change. The snapshot
 * remains valid after shader deletion/context teardown. Output is disjoint
 * from the context, screen and owned shader handles; unchanged on error. */
int remesa_context_shader_linkage_snapshot(struct pipe_context *,
 struct remesa_shader_linkage *);
struct remesa_shader_pipeline_launch;struct remesa_pds_request;
/* Latest create_vs/create_fs result, retained independently of transient
 * context_error resets by binds, transfers or draws. A successful semantic
 * compile and a bounded body do not qualify a complete hardware launch.
 * uniform_specialization denotes the existing CPU uniform-color path. */
struct remesa_shader_compile_status {
 uint64_t attempts;
 int error,binary_error,launch_error;
 size_t binary_bytes;
 bool uniform_specialization;
};
/* Persistent draw rejection evidence, independent of transient state-setter
 * errors. A frontend must not publish old pixels after a rejected draw just
 * because a later bind succeeded. Saturating count; never reset implicitly.
 * Read-only, output disjoint from context and owned objects. */
struct remesa_draw_rejection_status { uint64_t failures; int error; };
int remesa_context_draw_rejections(struct pipe_context *,struct remesa_draw_rejection_status *);
/* PIPE_SHADER_VERTEX/FRAGMENT only; ENODATA before the first create attempt.
 * Read-only, output unchanged on error; does not reset context_error. Output
 * must not overlap the context, screen or any context-owned shader handle. */
int remesa_context_shader_compile_status(struct pipe_context *,unsigned stage,
 struct remesa_shader_compile_status *);
struct remesa_pds_parameter_source_request;
/* Resolve the complete bound gears VS constants into the existing typed
 * parameter source image. User bytes are snapshotted at bind; resource bytes
 * are synchronized and read at this call. No CPU vertex/light arithmetic,
 * allocation, upload, PDS launch or GPU submission follows. Requires the
 * exact37/40 or41/44 effective/transfer contract of the bound profile.
 * Outputs are disjoint from owned context objects and unchanged on error. */
int remesa_context_vertex_parameter_source(struct pipe_context *,
 const struct remesa_pds_parameter_source_request *,void *,size_t,size_t *);
/* Snapshot the bound complete gears VS constant block into the common owned
 * parameter object. Resource-only persistent device required; uses the source
 * resolver above, preserving user snapshots and synchronized resource reads.
 * No caller VA, transform, program/PDS launch, scene or GPU submission. Empty
 * output required; uncertain ownership follows the persistent constructor. */
int remesa_context_prepare_vertex_parameters(struct pipe_context *,uint32_t use_bank,
 void **bo);
struct pipe_draw_info; struct pipe_draw_start_count_bias;
struct remesa_gears_geometry;
/* Own a packed pre-VS triangle stream for the currently bound complete gears
 * shader and its exact POSITION[/NORMAL] elements. Synchronizes/maps resources
 * only for the call, deduplicating aliases; no reference ownership is adopted
 * from draw-info. No CPU transform/lighting, upload or submission follows.
 * Requires empty output, unchanged on rejection; source buffers may change or
 * be released after success without changing the returned geometry. */
int remesa_context_gears_geometry_snapshot(struct pipe_context *,
 const struct pipe_draw_info *,const struct pipe_draw_start_count_bias *,
 struct remesa_gears_geometry **);
struct remesa_linked_vertex_inputs;
/* Snapshot raw application POSITION/NORMAL/LIGHT float4 attributes for the
 * bound WHITE_LAMBERT3 semantic link. Accept interleaved/independent resource
 * sources, including zero-stride broadcasts. Synchronize writers before CPU
 * reads; deduplicate aliased VB/IB resources and release all acquired maps.
 * No CPU shader/transform, upload or GPU admission. The owned result remains
 * independent of later source changes and can feed the linked GEM owner.
 * Triangle strips/fans are assembled to lists with winding and the bound
 * rasterizer's first/last provoking convention preserved; those two modes
 * require a valid bound rasterizer before mapping any resource. Triangle-list
 * snapshot admission and ordinary draw/capability gates remain unchanged.
 * Empty/disjoint output required. The caller serializes source/binding access. */
int remesa_context_linked_vertex_inputs_snapshot(struct pipe_context *,
 const struct pipe_draw_info *,const struct pipe_draw_start_count_bias *,
 struct remesa_linked_vertex_inputs **);
/* Before children, explicitly allow typed persistent COLOR/Z GEM storage.
 * Resource-only graphics/draw/capability gates stay unchanged; this is not the
 * CPU-depth LAB mode. Rejects nonpersistent or incompatible allocation owners. */
int remesa_screen_enable_linked_attachment_storage(struct pipe_screen *);
struct remesa_isp_opaque_triangle_request;
/* Snapshot the bound opaque blend/depth state for typed GEM COLOR/Z. Uses the
 * existing full-RGBA/disabled-or-LESS-write state contract. No default state is
 * inferred, no upload/clear/synchronization/encoding or draw admission occurs.
 * Disabled depth canonicalizes to ALWAYS/no-write. Rejects missing/invalid
 * bindings, failed attachments and output aliasing. Output unchanged on error;
 * snapshot is independent of later state deletion. Caller serializes bindings. */
int remesa_context_linked_isp_snapshot(struct pipe_context *,
 struct remesa_isp_opaque_triangle_request *);
struct remesa_linked_scene_request;struct remesa_scene_capture;
/* Compose a library scene from the bound semantic programs and raw application
 * VB/IB snapshot plus the bound typed COLOR/Z GEM attachments. options supplies
 * only the existing typed ISP/viewport/bank/clear contract; attachment, size,
 * linkage and geometry fields MUST be zero and are derived here. It remains
 * the native caller's responsibility to authenticate the selected state/load
 * profile: this preparation is not ordinary pipe_draw_vbo or GL admission.
 * Fresh attachments only; existing content/preserve is refused. No CPU vertex
 * execution, attachment clear, submission or capability change. All mapping
 * leases end before the frozen capture is returned. External serialization. */
int remesa_context_prepare_linked_scene(struct pipe_context *,
 const struct pipe_draw_info *,const struct pipe_draw_start_count_bias *,
 const struct remesa_linked_scene_request *options,struct remesa_scene_capture *);
/* Explicit canonical attribute-normal gears preparation. Same cold attachment
 * and output/serialization contract; snapshots bound constants and raw P/N,
 * initializes the unused third float4 for the physical 48-byte fetch profile.
 * No shader execution on CPU, normal draw/caps or native-admission change. */
int remesa_context_prepare_gears_attribute_scene(struct pipe_context *,
 const struct pipe_draw_info *,const struct pipe_draw_start_count_bias *,
 const struct remesa_linked_scene_request *options,struct remesa_scene_capture *);
/* Homogeneous counterpart: options.state MUST be zero. Derives ISP from the
 * bound opaque blend/DSA and MATRIX_WINDOW from the bound viewport, using
 * existing snapshot validation before resource IO. Other options/ownership
 * rules are unchanged. No CPU vertex transform or normal GL/caps admission.
 */
int remesa_context_prepare_gears_homogeneous_scene(struct pipe_context *,
 const struct pipe_draw_info *,const struct pipe_draw_start_count_bias *,
 const struct remesa_linked_scene_request *options,struct remesa_scene_capture *);
/* Explicit cold-scene execution using the same bound inputs as preparation.
 * Preallocates one fence shared by COLOR/Z and the context, reserves both pass
 * transitions before dispatch, and leaves resources unchanged on NOT_ISSUED.
 * Uncertain issuance poisons both attachments/context and retains backend
 * quarantine. Success returns one owned pipe fence; normal transfers wait for
 * authenticated retirement/publication. Caller serializes the entire call.
 * This is not ordinary draw_vbo admission or an OpenGL capability claim. */
int remesa_context_submit_linked_scene(struct pipe_context *,
 const struct pipe_draw_info *,const struct pipe_draw_start_count_bias *,
 const struct remesa_linked_scene_request *options,struct pipe_fence_handle **);
/* Submit one already frozen persistent capture against its exact currently
 * bound COLOR/DEPTH objects. Reuses normal pass/writer/context fences but
 * keeps the capture on success and conclusive NOT_ISSUED. The native session
 * must already be open; CLOSE acknowledgement precedes caller release.
 * This does not rebuild inputs, consume queued clears, infer unchanged GL
 * state or enable ordinary draws/caps. Rejects mapped/mismatched attachments.
 * Uncertainty poisons the same context/targets as ordinary scene submission. */
int remesa_context_submit_captured_scene(struct pipe_context *,
 struct remesa_scene_capture,struct pipe_fence_handle **);
/* Replace one retained draw's complete coefficients using the currently
 * bound VS constant source and the existing PARAM encoder. Shader, geometry,
 * viewport and targets of the capture remain frozen. No CPU transform, new
 * PARAM BO, scene construction, GPU submission or fence is performed here.
 * Requires exact bound attachments and retired owners. An uncertain native
 * data acknowledgement poisons context and attachments as for submission. */
int remesa_context_update_captured_parameters(struct pipe_context *,
 struct remesa_scene_capture,uint32_t draw);
/* Canonical gears counterpart: same shared COLOR/Z/context fence and failure
 * contract, using the explicit bound gears preparation. Native authentication
 * remains mandatory; does not enable normal draw/caps or a kernel gate. */
int remesa_context_submit_gears_attribute_scene(struct pipe_context *,
 const struct pipe_draw_info *,const struct pipe_draw_start_count_bias *,
 const struct remesa_linked_scene_request *options,struct pipe_fence_handle **);
/* Same shared fence/failure path with the bound homogeneous preparation.
 * Requires a separately qualified matching native backend/catalogue. */
int remesa_context_submit_gears_homogeneous_scene(struct pipe_context *,
 const struct pipe_draw_info *,const struct pipe_draw_start_count_bias *,
 const struct remesa_linked_scene_request *options,struct pipe_fence_handle **);
/* Explicit context-local cold-scene LAB adapter for ordinary draw_vbo.
 * Reuses the homogeneous submit/fence/failure path and canonical banks4/3/4,
 * Zclear1. Bound ISP/viewport, shaders, constants and raw geometry remain the
 * source of the scene. Unsupported raster/sampling state is refused. No cap,
 * API version, kernel gate or production admission change; a matching native
 * backend is still required. Existing COLOR/Z contents remain unsupported.
 * No resource IO or submission occurs when enabling. Serialized API entry. */
int remesa_context_enable_homogeneous_draw_lab(struct pipe_context *);
/* Explicit finite actual-OpenGL W2/CODE344 recipe, separate from default
 * frontend admission. Still needs a matching gated native LAB backend. */
enum remesa_homogeneous_api_lab_profile { REMESA_HOMOGENEOUS_API_W2_CLAMPED = 1 };
int remesa_context_enable_homogeneous_api_lab(struct pipe_context *,
 enum remesa_homogeneous_api_lab_profile);
/* Before children/resources, after linked attachment storage admission.
 * Enables a named LAB frontend with the implemented P/N/COLOR0/coefficient
 * shader limits and bounded renderbuffer dimensions. New contexts opt into
 * the cold draw adapter. No GL version override, accelerated-cap claim,
 * kernel gate or native recipe admission follows. Default screen unchanged.
 * Caller serializes setup; enabled storage alone cannot authorize rendering. */
int remesa_screen_enable_homogeneous_frontend_lab(struct pipe_screen *);

struct remesa_shader_host_pipeline_request;
struct remesa_shader_host_pipeline;
/* Snapshot the currently bound owned semantic pair into the original host
 * compiler/linker. Explicit numeric mapping and original DATA recipes are
 * supplied by the caller; no default mapping, upload or execution follows.
 * Uniform-color specialization requires a synchronized draw and is rejected.
 * Requires an empty output reference disjoint from the context, screen and
 * all owned shader handles; no context state or bindings change. */
int remesa_context_prepare_host_shader_pipeline(struct pipe_context *,
 const struct remesa_shader_host_pipeline_request *,struct remesa_shader_host_pipeline **);
int remesa_context_revalidate_host_shader_pipeline(struct pipe_context *,
 const struct remesa_shader_host_pipeline *,const struct remesa_shader_host_pipeline_request *);
struct remesa_pds_host_transfer_request;
struct remesa_shader_execution_contract;
/* Resolve explicit host task inputs and VS routes only after checking the
 * current bound programs and fresh original layout/recipe metadata. Snapshot
 * contains no context pointers and cannot qualify execution. No state change.
 */
int remesa_context_prepare_shader_contract(struct pipe_context *,
 const struct remesa_shader_host_pipeline *,const struct remesa_shader_host_pipeline_request *,
 const struct remesa_pds_host_transfer_request *,struct remesa_shader_execution_contract *);
int remesa_context_revalidate_shader_contract(struct pipe_context *,
 const struct remesa_shader_host_pipeline *,const struct remesa_shader_host_pipeline_request *,
 const struct remesa_pds_host_transfer_request *,const struct remesa_shader_execution_contract *);
struct remesa_pds_cpu_pixel_task_output;
/* Explicit CPU pixel profile; derives only FS DATA and reuses the existing
 * VS mapping/recipe/storage. request.data[1] is ignored. metadata6 is supplied
 * explicitly, never inferred from a shader. No binding/error/default changes,
 * physical DS selection, CODE emission, mapping, upload or submission. */
int remesa_context_prepare_pixel_task_pipeline(struct pipe_context *,
 const struct remesa_shader_host_pipeline_request *,uint32_t metadata6,
 struct remesa_shader_host_pipeline **);
struct remesa_pds_structure_request;
/* Explicit CPU structural encoder for the currently bound VS/constant-FS.
 * Owns DATA, computed A2/class64 CODE fragments and masks in existing storage.
 * No default draw policy, GPU mapping, admission or submission is changed. */
int remesa_context_prepare_pds_structure_pipeline(struct pipe_context *,
 const struct remesa_shader_host_pipeline_request *,const struct remesa_pds_structure_request *,
 struct remesa_shader_host_pipeline **);
int remesa_context_revalidate_pds_structure_pipeline(struct pipe_context *,
 const struct remesa_shader_host_pipeline *,const struct remesa_shader_host_pipeline_request *,
 const struct remesa_pds_structure_request *);
int remesa_context_prepare_pixel_task_output(struct pipe_context *,
 const struct remesa_shader_host_pipeline *,const struct remesa_shader_host_pipeline_request *,
 uint32_t metadata6,const struct remesa_pds_host_transfer_request *,
 struct remesa_pds_cpu_pixel_task_output *);
int remesa_context_revalidate_pixel_task_output(struct pipe_context *,
 const struct remesa_shader_host_pipeline *,const struct remesa_shader_host_pipeline_request *,
 uint32_t metadata6,const struct remesa_pds_host_transfer_request *,
 const struct remesa_pds_cpu_pixel_task_output *);
/* Snapshot the currently bound, context-owned VS/constant-FS semantic pair.
 * ENODATA for an unbound stage; uniform specialization remains unsupported.
 * No context mutation, mapping, code generation or hardware admission. */
int remesa_context_shader_pipeline_report(struct pipe_context *,struct remesa_shader_pipeline *);
/* Prepare/revalidate symbolic storage for the currently bound owned pair.
 * Caller retains allocations and authenticates mapping metadata; these APIs
 * acquire no references and never upload or submit shader bytes. */
int remesa_context_prepare_shader_launch(struct pipe_context *,
 const struct remesa_pds_request *vertex,const struct remesa_pds_request *fragment,
 struct remesa_shader_pipeline_launch *);
int remesa_context_revalidate_shader_launch(struct pipe_context *,
 const struct remesa_shader_pipeline_launch *,const struct remesa_pds_request *vertex,
 const struct remesa_pds_request *fragment);
struct remesa_pds_candidate_request;struct remesa_shader_pds_preview;
/* Research preview for actual context-owned shader handles; no allocation,
 * upload, command submission, capability change or context-state mutation. */
int remesa_context_shader_pds_preview(struct pipe_context *,unsigned stage,
 void *shader,const struct remesa_pds_candidate_request *,struct remesa_shader_pds_preview *);
/* Actual callback-created, context-owned shader handles only. Encoding supports
 * the bounded packed constant FS body; a complete VS/PDS pipeline remains
 * unavailable. Available only in REMESA_WITH_NIR builds; no context mutation,
 * allocation/upload or submission. report separates body and launch errors. */
int remesa_context_shader_program_report(struct pipe_context *,unsigned stage,
 void *shader,struct remesa_shader_program_report *);
int remesa_context_shader_program_encode(struct pipe_context *,unsigned stage,
 void *shader,void *,size_t,size_t *);
/* Poll/wait the existing resource writer; preserves exact backend errors.
 * Does not issue a draw or manufacture completion. */
int remesa_resource_wait_writer(struct pipe_context *,struct pipe_resource *,uint64_t timeout);
/* Internal serialized import. Backend owns both BO and completion evidence. */
int remesa_resource_attach_writer(struct pipe_context *,struct pipe_resource *,void *,struct pipe_fence_handle **);
/* Validated metadata from a context-owned view; frontend cookie is not a GEM
 * generation or visibility proof. Output unchanged on failure. */
int remesa_context_texture_view_storage(struct pipe_context *,
 const struct pipe_sampler_view *,struct remesa_texture_storage *);
int remesa_context_texture_view_level_storage(struct pipe_context *,
 const struct pipe_sampler_view *,unsigned,struct remesa_texture_storage *);
/* Poll the real frontend writer before a trusted retained-mapping query.
 * No maps, submissions, hardware descriptor bytes or sampling admission. */
int remesa_context_prepare_texture_descriptor(struct pipe_context *,
 const struct pipe_sampler_view *,const struct pipe_sampler_state *,uint64_t,
 remesa_texture_mapping_query,void *,struct remesa_texture_descriptor *);
/* Same writer/CPU-lease gate as preparation, followed by immutable mapping,
 * layout and frontend content-epoch comparison. No submission or GPU proof. */
int remesa_context_revalidate_texture_descriptor(struct pipe_context *,
 const struct remesa_texture_descriptor *,const struct pipe_sampler_view *,
 const struct pipe_sampler_state *,uint64_t,remesa_texture_mapping_query,void *);
struct pipe_screen *remesa_screen_create(struct remesa_device *device);
/* Owned attachments for an eventual window frontend; not a GLX drawable or
 * admission override. Zero-initialize, externally serialize, and retain the
 * owning screen. The current render profile remains 32x32 BGRA8, no depth.
 * Reconfiguration allocates fresh planes before releasing old references;
 * existing contexts/readers keep their own resource references. Contents of
 * replacement planes are undefined and must not be treated as preserved. */
struct remesa_drawable_buffers {
 struct pipe_resource *front,*back;
 uint32_t width,height;
 uint64_t generation,swaps; /* host identities/counters, not GPU receipts */
};
int remesa_drawable_buffers_replace(struct pipe_screen *,uint32_t,uint32_t,
 bool double_buffer,bool depth,struct remesa_drawable_buffers *);
void remesa_drawable_buffers_release(struct remesa_drawable_buffers *);
/* Consumes device only on success. Release runs after the final context,
 * resource and fence disappear. Without release, backend ownership is borrowed
 * and its caller must keep it alive for all children. Release must return a
 * nonzero error without destroying pending/quarantined backing; EBUSY/EAGAIN
 * mean side-effect-free retry. On error the closing screen retains ownership.
 * Not an OpenGL factory. */
struct pipe_screen *remesa_screen_create_owned(struct remesa_device *,int (*release)(struct remesa_device *));
/* For a closing owner retained because release failed. EBUSY/EAGAIN retry
 * collection only; terminal errors retain quarantine. Success consumes screen.
 * Caller must not invoke after a successful destroy or collection. */
int remesa_screen_collect(struct pipe_screen *);
/* Explicit attribution: completed CPU clears, never SGX clear submissions. */
/* Normal rendering requests target preservation. false explicitly permits
 * diagnostic discard; it does not establish a kernel load/store capability. */
int remesa_context_set_framebuffer_load(struct pipe_context *,bool preserve);
uint64_t remesa_context_cpu_clear_count(const struct pipe_context *);
/* Serialized opt-in before children, using the screen's same device.priv.
 * Callback attests a retired/published native copy; never general draw/API
 * admission. Unsupported or uncertain native copies do not fall back to CPU. */
int remesa_screen_enable_native_copy(struct pipe_screen *,remesa_copy_region_fn);
/* Native clear opt-in has the same before-children lifetime/receipt contract. */
int remesa_screen_enable_native_fill(struct pipe_screen *,remesa_fill_region_fn);
uint64_t remesa_context_native_fill_count(const struct pipe_context *);
/* Successful native callback count, not independent GPU execution evidence. */
uint64_t remesa_context_native_copy_count(const struct pipe_context *);
/* Completed CPU byte-copy callbacks, never SGX draw/blit submissions. */
uint64_t remesa_context_cpu_copy_count(const struct pipe_context *);
const char *remesa_screen_unavailable_reason(void);
/* Explicit private canonical frame collection. Storage capability alone
 * never admits a GPU draw; a complete runtime backend is mandatory. */
int remesa_screen_enable_gears_runtime(struct pipe_screen *);
int remesa_context_gears_bottom_left(struct pipe_context *,bool *);
#endif
