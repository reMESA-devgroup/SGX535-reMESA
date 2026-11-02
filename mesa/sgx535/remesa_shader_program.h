/* SPDX-License-Identifier: MIT
 * Original host compilation contract. This is not an SGX program image.
 */
#ifndef REMESA_SHADER_PROGRAM_H
#define REMESA_SHADER_PROGRAM_H
#include "remesa_shader_ir.h"
#include "remesa_usse.h"
#include "remesa_pds.h"
#include "remesa_pds_candidate.h"
#include "remesa_pds_structure.h"
#include "remesa_program_registers.h"
#include "remesa_shader_storage.h"
#include "remesa_usse_fragment_color.h"

/* Host policy identifiers, not hardware version/register fields. */
#define REMESA_SHADER_PROGRAM_SCHEMA 1u
enum remesa_shader_program_status {
 REMESA_PROGRAM_SEMANTIC_ONLY = 1,
 /* Strict white-diffuse VS or COL0-input FS CPU path; scalar planner absent. */
 REMESA_PROGRAM_COMPILE_OFFLINE = 2
};
struct remesa_shader_program {
 uint32_t schema;
 enum remesa_shader_program_status status;
 struct remesa_shader_ir semantic;
 struct remesa_usse_program usse; /* canonical zero for COMPILE_OFFLINE */
};
struct remesa_shader_program_report {
 enum remesa_ir_stage stage;
 enum remesa_shader_program_status status;
 unsigned virtual_instructions; /* semantic IR count for COMPILE_OFFLINE */
 int binary_error;
 size_t binary_bytes; /* supported stage body only; zero when unavailable */
 int launch_error; /* full pipeline/dispatch qualification is separate */
 /* Public program-base field facts are reported separately from executable
  * instruction evidence. Their presence must never admit a GPU shader. */
 struct remesa_program_register_evidence register_evidence;
};
/* Metadata for the exact offline COL0 FS. R10 is the original CPU PDS
 * descriptor ID; this query never prepares a PDS launch or transport. */
int remesa_shader_program_fragment_color_extent(const struct remesa_shader_program *,
 struct remesa_usse_fragment_color_extent *);
struct remesa_shader_pds_preview {
 enum remesa_ir_stage stage;
 uint64_t semantic_identity;
 struct remesa_pds_candidate_preview candidate;
};
/* A linked host semantic pair, not a GPU pipeline or executable program.
 * Identities are noncryptographic correlation keys, not resource ownership or
 * authentication. Current programs must be supplied again for revalidation. */
#define REMESA_SHADER_PIPELINE_SCHEMA 1u
struct remesa_shader_pipeline {
 uint32_t schema;
 enum remesa_shader_program_status status;
 uint64_t identity[2]; /* vertex, fragment */
 unsigned virtual_instructions[2];
 unsigned position_input_slot;
 uint32_t fragment_argb;
 int binary_error;
};
/* Links only the admitted position-passthrough and constant-color profiles.
 * Transactional, allocation-free, no NIR references or hardware bytes. */
int remesa_shader_pipeline_prepare(const struct remesa_shader_program *vertex,
 const struct remesa_shader_program *fragment,struct remesa_shader_pipeline *);
int remesa_shader_pipeline_revalidate(const struct remesa_shader_pipeline *,
 const struct remesa_shader_program *vertex,const struct remesa_shader_program *fragment);
/* Owned shader input for a draw backend. This carries the actual immutable
 * programs, rather than just their correlation keys or a constant color. A
 * backend must independently encode and qualify execution; this snapshot has
 * no GPU bytes, resource ownership or permission to use a historical shader.
 * It remains valid after the source shader objects and NIR are destroyed. */
struct remesa_shader_draw_programs {
 struct remesa_shader_program vertex,fragment;
 struct remesa_shader_pipeline pipeline;
};
/* Transactional, allocation-free; output/input overlap is rejected. */
int remesa_shader_draw_programs_prepare(const struct remesa_shader_program *vertex,
 const struct remesa_shader_program *fragment,struct remesa_shader_draw_programs *);
int remesa_shader_draw_programs_validate(const struct remesa_shader_draw_programs *);
/* Rebinding another source representation invalidates this handoff even if
 * its normalized lowering and correlation identity are unchanged. */
int remesa_shader_draw_programs_revalidate(const struct remesa_shader_draw_programs *,
 const struct remesa_shader_program *vertex,const struct remesa_shader_program *fragment);

/* Common desktop-GL/GLES frontend linkage, independent of the legacy
 * constant-fragment pipeline. These are owned semantic snapshots, not shader
 * BOs, GPU addresses or permission to execute. The older pipeline API keeps
 * its restrictions. Only currently understood IO profiles are linkable. */
enum remesa_fragment_input_profile {
 REMESA_FRAGMENT_CONSTANT = 1,
 REMESA_FRAGMENT_COLOR0_SMOOTH,
 REMESA_FRAGMENT_COLOR0_FLAT
};
struct remesa_shader_linkage {
 struct remesa_shader_program vertex,fragment;
 struct remesa_shader_vertex_layout vertex_layout;
 enum remesa_fragment_input_profile fragment_input;
 uint32_t fragment_argb; /* zero for varying input; never a replacement color */
};
/* Allocation-free and transactional. Requires POSITION4 and a matching
 * COLOR0 float4 producer when FS consumes COLOR0. Flat interpolation still
 * requires independently supported raster/provoking state at draw admission.
 * Compilation, PDS/resource allocation and execution qualification remain
 * separate; no legacy submit callback is enabled by a successful link. */
int remesa_shader_linkage_prepare(const struct remesa_shader_program *vertex,
 const struct remesa_shader_program *fragment,struct remesa_shader_linkage *);
int remesa_shader_linkage_validate(const struct remesa_shader_linkage *);
int remesa_shader_linkage_revalidate(const struct remesa_shader_linkage *,
 const struct remesa_shader_program *vertex,const struct remesa_shader_program *fragment);
/* Immutable preparation for the original native path. Owns the complete
 * semantic VS/FS pair and the independently serialized fragment CPU candidate.
 * Magenta/green prefer the one-record packed-constant candidate; the larger
 * float/PACK body remains available for the earlier yellow endpoint profile.
 * An explicit numeric VS candidate can be attached for CPU preparation only.
 * Qualified VS/PDS executable bytes remain absent.
 * This is host resource preparation, not upload or execution qualification. */
struct remesa_shader_image;
enum remesa_shader_candidate_form {
 REMESA_SHADER_PACKED_CONSTANT = 1,
 REMESA_SHADER_FLOAT_PACK_BODY = 2
};
struct remesa_shader_image_view {
 const struct remesa_shader_draw_programs *programs; /* borrowed, immutable */
 struct remesa_shader_storage_view fragment;
 struct remesa_shader_storage_view vertex; /* zero when no numeric mapping supplied */
 /* Optional generated primary FS DATA+CODE and original evidence mask.
  * Nonzero fragment_pds_error rejects its staging; ENODATA means unrequested.
  * Mask0 DATA remains unknown despite a recorded symbolic entry relocation.
  * Neither a complete CODE mask nor CPU staging qualifies GPU execution. */
 struct remesa_shader_storage_view fragment_pds,fragment_pds_mask;
 int fragment_pds_error;
 size_t vertex_byte_length; /* CPU candidate length, not executable storage */
 int execution_error;
 enum remesa_shader_candidate_form fragment_form; /* CPU grammar only */
};
/* Requires an empty output reference; unchanged on error. All source state is
 * copied and survives NIR, shader-state and source-pair destruction. */
int remesa_shader_image_prepare(const struct remesa_shader_draw_programs *,
 struct remesa_shader_image **);
/* Explicit offline numeric register assignment. Owns four independently
 * encoded CPU MOV records plus the fragment candidate. No default physical
 * bank mapping or termination is inferred. Execution still returns ENOTSUP. */
int remesa_shader_image_prepare_vertex_candidate(const struct remesa_shader_draw_programs *,
 const struct remesa_usse_vertex_mapping *,struct remesa_shader_image **);
/* Independent immutable owner; typed metadata is copied and existing payload
 * storage references are retained. Empty output, transactional failure. */
int remesa_shader_image_clone(const struct remesa_shader_image *,struct remesa_shader_image **);
int remesa_shader_image_validate(const struct remesa_shader_image *);
int remesa_shader_image_inspect(const struct remesa_shader_image *,
 struct remesa_shader_image_view *);
int remesa_shader_image_revalidate(const struct remesa_shader_image *,
 const struct remesa_shader_draw_programs *);
struct remesa_shader_request;
/* Allocation-free exact comparison of the CPU staging request to both owned
 * stages. Request owner/generation still need endpoint authentication. */
int remesa_shader_image_validate_request(const struct remesa_shader_image *,
 const struct remesa_shader_request *);
/* One canonical CPU-only STAGE serializer shared by native/persistent adapters.
 * Owner/generation are supplied by the authenticated endpoint, not GPU VA. */
int remesa_shader_image_build_request(const struct remesa_shader_image *,
 uint64_t owner,uint64_t generation,struct remesa_shader_request *);
/* Preview public-kernel USE window arithmetic for the exact owned candidate
 * candidate. request.program_bytes must match its immutable encoded byte length;
 * the entry must be losslessly representable by the observed software fixups
 * (16-byte offset units). This is not a hardware alignment qualification.
 * VS requires an explicitly supplied numeric candidate. The allocation remains symbolic:
 * this neither authenticates a mapping nor promotes CPU records to executable
 * code. No task words or instructions are emitted. Output unchanged on error. */
int remesa_shader_image_preview_use_entry(const struct remesa_shader_image *,
 const struct remesa_use_entry_request *,struct remesa_use_entry_plan *);
/* No caller-supplied flag can qualify physical banks, PACK, termination or
 * PDS fetch. The current original candidate always returns ENOTSUP. */
int remesa_shader_image_validate_execution(const struct remesa_shader_image *);
void remesa_shader_image_destroy(struct remesa_shader_image *);
/* One atomic host preparation for a linked pair. Both requests describe one
 * caller-retained symbolic address space and operation epoch. All four selected
 * CODE/DATA windows must be disjoint; shared allocations must agree exactly on
 * mapping identity. No GEM reference, pin, upload or launch packet is created. */
struct remesa_shader_pipeline_launch {
 struct remesa_shader_pipeline semantic;
 struct remesa_pds_plan stage[2];
};
/* Owned semantic PDS pair plus symbolic references to exact original USE
 * bodies. No task/control payload, PDS instruction or execution state exists
 * in this preview. DATA slots and mapping metadata are supplied explicitly;
 * their hardware interpretation and provider authentication remain absent. */
struct remesa_shader_pds_links {
 struct remesa_shader_pipeline_launch launch;
 struct remesa_pds_use_link use[2];
};
/* Original CPU compiler/linker input, in VS/FS order. The numeric VS register
 * mapping is explicit, never selected as a hardware default. PDS blocks use
 * the existing checked DATA/CODE suballocation policy; USE bindings must name
 * the exact independently generated stage lengths. Recipes are original host
 * DATA cells, not copied task words or executable instructions. All symbolic
 * bindings belong to one owner/operation/generation; no mapping is acquired.
 */
struct remesa_shader_host_pipeline_request {
 struct remesa_usse_vertex_mapping vertex_mapping;
 struct remesa_pds_block_request pds[2];
 struct remesa_pds_binding use[2];
 uint32_t banks[2];
 uint64_t data_byte_offsets[2];
 struct remesa_pds_host_data_recipe data[2];
};
struct remesa_shader_host_pipeline;
struct remesa_shader_host_pipeline_view {
 const struct remesa_shader_image *image_owner; /* borrowed immutable staging image */
 struct remesa_shader_image_view image;
 const struct remesa_shader_pds_links *links; /* borrowed immutable snapshot */
 struct remesa_pds_host_data_view data[2];
 /* Optional fragment CPU structural image. DATA is data[1] above; these
  * immutable stores contain CODE slots and per-byte known-bit masks.
  * Zero-mask bytes include initial/final CODE and unresolved DATA cells.
  * structure_error=0 means original CPU-class encoding only, never launch. */
 struct remesa_shader_storage_view structure_code,structure_code_mask,structure_data_mask;
 const struct remesa_pds_structure *structure; /* borrowed canonical CPU grammar */
 int structure_error;
 int pds_code_error,execution_error;
};
/* One transactional host build of the explicit VS candidate, constant FS,
 * DATA/CODE layout, USE links and relocated original DATA. Uses shader_image
 * and shader_storage ownership; no CODE bytes, MMIO, GPU upload or execution.
 * Copies all source state, including recipes, before publishing an empty out
 * reference. Every failure leaves out unchanged and releases temporary state.
 * Per-stage DATA is bounded by REMESA_SHADER_STORAGE_MAX_BYTES (host policy).
 */
int remesa_shader_host_pipeline_create(const struct remesa_shader_draw_programs *,
 const struct remesa_shader_host_pipeline_request *,struct remesa_shader_host_pipeline **);
int remesa_shader_host_pipeline_validate(const struct remesa_shader_host_pipeline *);
int remesa_shader_host_pipeline_inspect(const struct remesa_shader_host_pipeline *,
 struct remesa_shader_host_pipeline_view *);
/* Rebuild and compare exact semantic/layout/recipe state to current inputs.
 * Hashes alone never authenticate mapping freshness. Does not alter storage.
 */
int remesa_shader_host_pipeline_revalidate(const struct remesa_shader_host_pipeline *,
 const struct remesa_shader_draw_programs *,const struct remesa_shader_host_pipeline_request *);
/* Explicit restricted CPU pixel-task profile. The FS recipe is generated from
 * current USE relocation + caller metadata6; request.data[1] is ignored by
 * these two APIs. VS recipe/mapping/storage are reused without alteration.
 * Require a packed eight-byte original FS candidate, 48-byte FS DATA and
 * entry relocation at DATA+0. These are software profile limits, not proof
 * of a hardware launch or task resource requirement. metadata6 is never
 * inferred from color/body size. Zero padding is an original host policy.
 * No CODE is emitted, and no default draw/backend policy is changed. */
int remesa_shader_host_pipeline_create_pixel_task(const struct remesa_shader_draw_programs *,
 const struct remesa_shader_host_pipeline_request *,uint32_t metadata6,
 struct remesa_shader_host_pipeline **);
int remesa_shader_host_pipeline_revalidate_pixel_task(const struct remesa_shader_host_pipeline *,
 const struct remesa_shader_draw_programs *,const struct remesa_shader_host_pipeline_request *,
 uint32_t metadata6);
/* Explicit offline psb CPU descriptor grammar. Reuses immutable stage storage,
 * USE lower19 fixup, layout/owner validation and semantic VS/FS snapshots.
 * FS DATA recipe is generated from structure; request.data[1] is ignored.
 * Require an eight-byte constant FS body, exact DATA/CODE extents, no extra
 * padding between them and entry offset0. Texture addresses stay symbolic.
 * A2/class64 CODE is calculated; initial/final slots remain masked holes.
 * Does not implement physical task launch, infer feature R from NIR, or
 * change remesa_pds_encode()/execution admission. Outputs unchanged on error. */
int remesa_shader_host_pipeline_create_pds_structure(const struct remesa_shader_draw_programs *,
 const struct remesa_shader_host_pipeline_request *,const struct remesa_pds_structure_request *,
 struct remesa_shader_host_pipeline **);
int remesa_shader_host_pipeline_revalidate_pds_structure(const struct remesa_shader_host_pipeline *,
 const struct remesa_shader_draw_programs *,const struct remesa_shader_host_pipeline_request *,
 const struct remesa_pds_structure_request *);
void remesa_shader_host_pipeline_destroy(struct remesa_shader_host_pipeline *);
/* Original host execution requirements, not hardware task fields. Logical
 * DATA slots are supplied explicitly; no DS register/bank default is chosen.
 * The canonical VS routes also drive the existing numeric candidate emitter.
 * Every listed hardware blocker stays set regardless of caller-supplied data.
 */
#define REMESA_SHADER_EXECUTION_CONTRACT_SCHEMA 1u
enum remesa_shader_execution_blocker {
 REMESA_EXECUTION_DATA_TO_DS=1u<<0,
 REMESA_EXECUTION_TASK_CONTROL=1u<<1,
 REMESA_EXECUTION_PDS_INSTRUCTIONS=1u<<2,
 REMESA_EXECUTION_VS_INPUT_BANK=1u<<3,
 REMESA_EXECUTION_VS_EXPORT=1u<<4,
 REMESA_EXECUTION_USSE_TERMINATION=1u<<5,
 REMESA_EXECUTION_DEVICE_COMPLETION=1u<<6
};
struct remesa_shader_execution_contract {
 uint32_t schema,blockers;
 uint64_t operation,generation,owner_cookie;
 uint64_t fragment_identity;
 uint32_t fragment_bytes,fragment_form;
 struct remesa_pds_host_task_plan task[2];
 struct remesa_usse_vertex_export_plan vertex;
 int pds_instruction_error,fragment_termination_error,completion_error,execution_error;
};
/* Resolve actual immutable DATA, USE entries, explicit logical routing and VS
 * export dependencies into one self-contained snapshot. Success means CPU
 * planning only. Does not mutate DATA/CODE, callbacks, Gallium bindings, bank
 * leases or device state. No pointer into the source requests is retained.
 * All outputs unchanged on errors, with full-owner/descriptor alias checks.
 */
int remesa_shader_host_pipeline_prepare_contract(const struct remesa_shader_host_pipeline *,
 const struct remesa_pds_host_transfer_request requests[2],struct remesa_shader_execution_contract *);
int remesa_shader_host_pipeline_validate_contract(const struct remesa_shader_host_pipeline *,
 const struct remesa_pds_host_transfer_request requests[2],const struct remesa_shader_execution_contract *);
/* Resolve/consume the three CPU profile values from the actual immutable DATA
 * via the validated complete VS/FS transfer graph. This checks the exact
 * profile, fresh retained bindings, slot addresses and the packed FS body.
 * Three is a CPU source count only. Physical DS, hardware task arity, dispatch
 * and termination remain unsupported; no caller status can qualify them. */
int remesa_shader_host_pipeline_prepare_pixel_task_output(const struct remesa_shader_host_pipeline *,
 const struct remesa_pds_host_transfer_request requests[2],struct remesa_pds_cpu_pixel_task_output *);
int remesa_shader_host_pipeline_validate_pixel_task_output(const struct remesa_shader_host_pipeline *,
 const struct remesa_pds_host_transfer_request requests[2],const struct remesa_pds_cpu_pixel_task_output *);
/* Final emission boundary: validates the complete current contract, rejects
 * caller-cleared blocker states, then returns ENOTSUP without writing bytes.
 * No control flag or prepared host plan can qualify executable PDS CODE.
 */
int remesa_shader_host_pipeline_encode_contract(const struct remesa_shader_host_pipeline *,
 const struct remesa_pds_host_transfer_request requests[2],const struct remesa_shader_execution_contract *,
 void *,size_t,size_t *written);
int remesa_shader_pipeline_prepare_launch(const struct remesa_shader_program *vertex,
 const struct remesa_shader_program *fragment,const struct remesa_pds_request *vertex_request,
 const struct remesa_pds_request *fragment_request,struct remesa_shader_pipeline_launch *);
int remesa_shader_pipeline_revalidate_launch(const struct remesa_shader_pipeline_launch *,
 const struct remesa_shader_program *vertex,const struct remesa_shader_program *fragment,
 const struct remesa_pds_request *vertex_current,const struct remesa_pds_request *fragment_current);
/* Validates an owned host compilation, then previews caller-chosen candidate
 * PDS fields. This is NOT NIR-to-machine-code compilation or a launch plan.
 * No candidate is consumed by remesa_shader_program_encode[_launch]. */
int remesa_shader_program_preview_pds(const struct remesa_shader_program *,
 const struct remesa_pds_candidate_request *,struct remesa_shader_pds_preview *);
/* Owns by-value immutable semantic/lowering snapshots; no pointer into NIR.
 * Success means host lowering only. It never means executable GPU code.
 * Destination is unchanged on any error. */
int remesa_shader_program_prepare(const struct remesa_shader_ir *,
                                 struct remesa_shader_program *);
/* Specialize a synchronized uniform color into the same owned constant
 * fragment profile as NIR lowering. Only the existing opaque endpoint colors
 * are admitted; this does not compile a GPU uniform load or encode hardware.
 * Destination is unchanged on error. */
int remesa_shader_program_prepare_color(uint32_t argb,struct remesa_shader_program *);
int remesa_shader_program_validate(const struct remesa_shader_program *);
int remesa_shader_program_report(const struct remesa_shader_program *,
                                struct remesa_shader_program_report *);
/* Encode only the bounded stage body supported by remesa_usse_encode, not a
 * complete launch. No partial output or historical-program fallback. Output
 * and written must be disjoint from the complete immutable program object. */
int remesa_shader_program_encode(const struct remesa_shader_program *,
                                void *,size_t,size_t *);
/* Prepare only symbolic resource requirements. This does not pin resources,
 * authenticate mappings, or generate a GPU launch. Current ownership is
 * supplied afresh and checked again before any future emission. */
int remesa_shader_program_prepare_launch(const struct remesa_shader_program *,
 const struct remesa_pds_request *,struct remesa_pds_plan *);
int remesa_shader_program_encode_launch(const struct remesa_shader_program *,
 const struct remesa_pds_plan *,const struct remesa_pds_request *,
 void *,size_t,size_t *);
/* Explicit host ownership release; idempotent. Not GEM retirement. */
void remesa_shader_program_finish(struct remesa_shader_program *);
/* Explicit selected compiler path; every existing scalar/pair ABI is unchanged.
 * These resource fields describe exact typed CPU producers and host storage,
 * not general shader limits or executable device admission. */
struct remesa_shader_selected_resources {
 uint32_t profile,logical_use_bytes,reserved_use_bytes,host_alignment;
 uint64_t semantic_identity;
 struct remesa_shader_vertex_layout vertex;
 uint32_t fragment_input_slot,fragment_components,r_id,r_components,r_mode,r_flag;
 uint32_t constant_argb;
};
int remesa_shader_program_selected_resources(const struct remesa_shader_program *,
 struct remesa_shader_selected_resources *);
/* Separate exact positive physical gears profile; default selected_resources
 * keeps refusing both gears variants. Attribute normal only, CODE320, existing
 * PDS input record48 (P/N plus retained unused float4), no generic task fields,
 * W/viewport/FLAT or execution claim. Caller must retain parameter source37/40. */
int remesa_shader_program_gears_attribute_resources(const struct remesa_shader_program *,
 struct remesa_shader_selected_resources *);
int remesa_shader_program_prepare_selected_pds(const struct remesa_shader_program *,
 const struct remesa_pds_selected_request *,struct remesa_pds_selected_plan *);
/* Missing default three-attribute transport and admitted draw packet are
 * explicit blockers after successful selected USE/PDS byte production. */
enum remesa_shader_selected_blocker {
 REMESA_SELECTED_VERTEX_TRANSPORT=1u<<0,
 REMESA_SELECTED_DRAW_PACKET=1u<<1
};
struct remesa_shader_selected_pipeline;
struct remesa_shader_selected_pipeline_view {
 const struct remesa_shader_program *programs; /* borrowed arrays[2], VS/FS */
 const struct remesa_shader_selected_resources *resources;
 const struct remesa_pds_selected_plan *plans;
 struct remesa_shader_storage_view use[2],pds[2],known_mask[2];
 uint32_t blockers;
 int vertex_transport_error,draw_packet_error,execution_error;
};
/* Actual encoded USE and resolved primary PDS DATA+CODE/masks are copied into
 * six existing immutable host stores. The owner copies semantic programs and
 * all pointer-free selected plan inputs. VBO bytes/mappings remain caller
 * owned and unauthenticated; source bounds do not qualify their transport.
 * The pair is exact3xvec4 white Lambert VS plus exact COL0 FS. GREEN
 * driver baseline remains a standalone PDS profile; its pairing with WHITE64
 * is unqualified and rejected. Reserved USE tails are metadata only: stores
 * contain committed64/8 bytes, never uninitialized allocation padding.
 * Output must be empty; every failure is transactional. */
int remesa_shader_selected_pipeline_prepare(const struct remesa_shader_program programs[2],
 const struct remesa_pds_selected_request requests[2],struct remesa_shader_selected_pipeline **);
int remesa_shader_selected_pipeline_validate(const struct remesa_shader_selected_pipeline *);
int remesa_shader_selected_pipeline_revalidate(const struct remesa_shader_selected_pipeline *,
 const struct remesa_shader_program programs[2],const struct remesa_pds_selected_request current[2]);
int remesa_shader_selected_pipeline_inspect(const struct remesa_shader_selected_pipeline *,
 struct remesa_shader_selected_pipeline_view *);
int remesa_shader_selected_pipeline_clone(const struct remesa_shader_selected_pipeline *,
 struct remesa_shader_selected_pipeline **);
/* Serializes all four actual USE/PDS blobs via the existing CPU-candidate
 * request builder. No execution flag, bank lease, mapping, upload or submit. */
int remesa_shader_selected_pipeline_build_request(const struct remesa_shader_selected_pipeline *,
 uint64_t owner,uint64_t generation,struct remesa_shader_request *);
int remesa_shader_selected_pipeline_validate_request(const struct remesa_shader_selected_pipeline *,
 const struct remesa_shader_request *);
int remesa_shader_selected_pipeline_validate_execution(const struct remesa_shader_selected_pipeline *);
void remesa_shader_selected_pipeline_destroy(struct remesa_shader_selected_pipeline *);
#endif
