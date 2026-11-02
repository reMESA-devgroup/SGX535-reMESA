/* SPDX-License-Identifier: MIT
 * Original host launch/relocation requirements. These are NOT SGX PDS
 * instructions, task-control words, relocation bitfields or proven VA limits.
 * Code/data resources below are symbolic future program storage requirements;
 * host USSE serialization must never be uploaded as a shader program.
 */
#ifndef REMESA_PDS_H
#define REMESA_PDS_H
#include "remesa_usse.h"
#include "remesa_program_registers.h"
#include "remesa_shader_storage.h"
#define REMESA_PDS_SCHEMA 1u
#define REMESA_PDS_RELOCATIONS 2u
/* IDs and slot count are original host plan definitions. */
enum remesa_pds_role { REMESA_PDS_CODE=1,REMESA_PDS_DATA=2 };
enum remesa_pds_relocation_kind { REMESA_PDS_SYMBOLIC_ADDRESS=1 };
struct remesa_pds_binding {
 /* address/allocation_bytes describe the complete caller-retained allocation
  * in the request's one symbolic address space, not just its selected window.
  * Distinct cookies must name disjoint extents. A shared cookie must carry one
  * exact mapping identity; its CODE/DATA windows may be disjoint subranges. */
 uint64_t allocation_cookie,owner_cookie,mapping_generation;
 uint64_t address,offset,bytes,allocation_bytes;
};
struct remesa_pds_request {
 uint64_t operation,generation,owner_cookie;
 struct remesa_pds_binding code,data;
};
/* Original host suballocation policy, not a SGX PDS layout specification.
 * allocation.offset/bytes retain a bounded window within one allocation.
 * DATA begins at that window; CODE follows DATA and explicit host padding to
 * code_alignment bytes in this symbolic address space. Alignment is supplied
 * by the caller, never inferred from an opcode or hardware descriptor.
 * No storage is allocated, mapped, written, pinned or made executable here. */
struct remesa_pds_block_request {
 uint64_t operation,generation;
 struct remesa_pds_binding allocation;
 uint64_t data_bytes,code_bytes,code_alignment;
};
struct remesa_pds_relocation {
 uint32_t role,kind;
 uint64_t address,bytes;
};
struct remesa_pds_plan {
 uint32_t schema;
 enum remesa_ir_stage stage;
 uint64_t program_identity;
 /* Immutable canonical semantic copy, not hardware bytes or GEM ownership. */
 struct remesa_usse_program program;
 struct remesa_pds_request request;
 uint32_t relocation_count,reserved;
 struct remesa_pds_relocation relocations[REMESA_PDS_RELOCATIONS];
};
struct remesa_pds_encoding_status {
 enum remesa_encoding_evidence instruction_width,alignment,opcode;
 enum remesa_encoding_evidence task_control,usse_address,constant_address;
 enum remesa_encoding_evidence register_banks,launch_semantics;
};
/* Caller supplies already-owned symbolic storage; this module acquires no
 * GEM references and does not independently verify PFNs, pins or VA mappings.
 * Exact program identity, operation/generation and bounds are revalidated.
 * All outputs unchanged on failure. */
int remesa_pds_prepare(const struct remesa_usse_program *,const struct remesa_pds_request *,struct remesa_pds_plan *);
/* Derive exact disjoint windows transactionally. This does not assert a
 * hardware data/code prefix, instruction width or receiver fetch rule. */
int remesa_pds_request_from_block(const struct remesa_pds_block_request *,struct remesa_pds_request *);
int remesa_pds_validate(const struct remesa_usse_program *,const struct remesa_pds_plan *);
/* Fresh trusted caller metadata must match the immutable request exactly. This
 * does not independently establish PFNs or verify the metadata provider. */
int remesa_pds_validate_binding(const struct remesa_usse_program *,const struct remesa_pds_plan *,const struct remesa_pds_request *current);
/* Symbolic connection between an exact USE entry and a caller-selected DATA
 * location. Only address_bits[18:0] have the existing software fixup meaning;
 * no upper task-control bits, MOVS source, destination or consumption arity
 * are assigned. The four-byte location is an original host relocation policy,
 * NOT evidence of PDS data-bank layout, fetch width or task word count.
 * Neither this record nor the candidate CPU program may be uploaded/executed.
 */
struct remesa_pds_use_link {
 struct remesa_pds_request request;
 struct remesa_pds_binding program;
 struct remesa_use_entry_plan entry;
 enum remesa_ir_stage stage;
 uint32_t address_bits;
 uint64_t program_identity,data_byte_offset,data_address;
};
/* VS/FS in fixed order, one symbolic address space and operation epoch.
 * Checks all six selected CODE/DATA/USE windows and their complete allocation
 * identities, exact entry bounds, distinct concurrent USE bank settings and
 * four-byte DATA locations. Caller retains/authenticates every allocation;
 * these helpers acquire no references or executable mappings. Failure leaves
 * both outputs unchanged; output/input overlap and pointer wrap are rejected.
 */
int remesa_pds_prepare_use_links(const struct remesa_pds_plan plans[2],
 const struct remesa_pds_binding programs[2],const uint32_t banks[2],
 const uint64_t data_byte_offsets[2],struct remesa_pds_use_link links[2]);
/* Fresh source bindings and PDS plans must match the immutable link exactly.
 * Self-consistency is not authentication of a GPU mapping or task consumer. */
int remesa_pds_validate_use_links(const struct remesa_pds_plan plans[2],
 const struct remesa_pds_binding programs[2],const struct remesa_pds_use_link links[2]);
/* Original numerical preview of the retained CPU address/prefix convention.
 * It is NOT a complete hardware descriptor or PDS program. T+4 is absent.
 * For the audited primary FS CPU relocation, T+8 bits25:24 are known zero
 * after the public kernel whole-word fixup; its known mask is UINT32_MAX.
 * The VS projection retains unknown bits25:24 and mask0xfcffffff.
 * block_address and code_window_address preserve both competing fetch points;
 * neither is named the first PC. CODE/DATA must be disjoint windows of one
 * retained allocation. Limits/alignment ensure lossless CPU packing only.
 * No task-control, memory publication, GPU address authentication or execution.
 */
struct remesa_pds_record_preview {
 uint64_t operation,generation,owner_cookie;
 uint64_t allocation_cookie,mapping_generation;
 uint64_t block_address,prefix_bytes,code_window_address,code_bytes;
 uint64_t use_entry_address,use_program_bytes,data_fixup_address;
 uint32_t address_prefix_bits,address_prefix_known_mask,high_address_region;
 uint32_t use_address_bits;
};
/* Atomic VS/FS projections from already validated original symbolic links.
 * Output must be disjoint from every input. Unknown bits are retained as a mask,
 * never serialized to a command stream. Failure leaves both outputs unchanged.
 */
int remesa_pds_preview_record_pair(const struct remesa_pds_plan *plans,
 const struct remesa_pds_binding *programs,const struct remesa_pds_use_link *links,
 struct remesa_pds_record_preview *out);
/* Caller-owned accessible HOST DATA windows, never executable GPU mappings.
 * Binding must exactly match the retained symbolic DATA window. The byte
 * pointer starts at that window, not at the allocation base. Caller retains
 * storage and serializes access throughout the call. */
struct remesa_pds_host_data {
 struct remesa_pds_binding binding;
 void *bytes;
 size_t byte_length;
};
/* Borrowed immutable HOST DATA, valid only while its native token is live.
 * Symbolic binding is not an authenticated GPU mapping. */
struct remesa_pds_host_data_view {
 struct remesa_pds_binding binding;
 struct remesa_shader_storage_view storage;
};
/* Apply only the established lower19 CPU entry relocation in little endian.
 * Preserve every other bit and byte supplied by the original host builder.
 * This does not supply/validate task controls, DS routing, source arity, PDS
 * instructions, fetch or termination; the result remains non-executable.
 * Validate both stages before either write, with no allocation or callback.
 * HOST windows must be disjoint from each other and every input descriptor.
 * CODE/DATA may reside in separate bounded symbolic allocations. Entry
 * relocation does not require a T+8 descriptor preview or choose a first PC.
 * Any failure leaves both byte buffers unchanged. */
int remesa_pds_apply_host_entry_fixups(const struct remesa_pds_plan *plans,
 const struct remesa_pds_binding *programs,const struct remesa_pds_use_link *links,
 const struct remesa_pds_host_data *data);
/* Revalidate the exact lower19 entry fields already present in both HOST
 * DATA snapshots against their canonical live links. Read-only; no fixup,
 * output publication, task-control interpretation or hardware qualification.
 * Uses the same binding/bounds/alias checks as apply_host_entry_fixups and
 * permits separate CODE/DATA. Mismatch returns EPROTO; bytes stay unchanged. */
int remesa_pds_validate_host_entry_fixups(const struct remesa_pds_plan *plans,
 const struct remesa_pds_binding *programs,const struct remesa_pds_use_link *links,
 const struct remesa_pds_host_data *data);
/* Original host DATA construction schema, not PDS instructions or DS layout.
 * Every DATA byte is covered by one four-byte little-endian cell. A literal
 * remains opaque. CPU_META6 encodes the observed bounded CPU field. The
 * single USE_ENTRY_LOWER19 cell must be at the validated
 * link offset; value supplies only the upper opaque bits (low19 must be zero).
 * No task-control meanings, DATA->DS transfers or CODE are supplied here. */
enum remesa_pds_data_cell_kind {
 REMESA_PDS_DATA_LITERAL32=1,
 REMESA_PDS_DATA_USE_ENTRY_LOWER19=2,
 /* CPU metadata6 packing observed in Xpsb; no resource/termination meaning. */
 REMESA_PDS_DATA_CPU_META6=3
};
/* Bounded CPU field codec: observed metadata6 values0/1 -> word bit27.
 * This supplies no upper task-control semantics or default stage selection.
 * Pack rejects unobserved values; unpack rejects every other word bit.
 * Outputs stay unchanged on error. CPU_META6 cell.value is metadata6, not
 * an already packed word. No placement in DATA is selected automatically. */
int remesa_pds_cpu_meta6_pack(uint32_t metadata6,uint32_t *word);
int remesa_pds_cpu_meta6_unpack(uint32_t word,uint32_t *metadata6);
struct remesa_pds_data_cell { uint32_t kind,value; };
struct remesa_pds_host_data_recipe {
 const struct remesa_pds_data_cell *cells;
 size_t cell_count;
};
/* Validate both VS/FS recipes, their complete source/destination host ranges,
 * exact retained bindings and live USE links before writing either buffer.
 * Every source recipe and descriptor is disjoint from both destinations;
 * caller serializes access. No allocation, callbacks, CODE writes or GPU
 * publication occur. Any error leaves both buffers unchanged. */
int remesa_pds_build_host_data_pair(const struct remesa_pds_plan *plans,
 const struct remesa_pds_binding *programs,const struct remesa_pds_use_link *links,
 const struct remesa_pds_host_data_recipe *recipes,
 const struct remesa_pds_host_data *data);
/* Read-only complete-byte check against the same canonical recipes and links.
 * EPROTO means stored DATA differs; no executable qualification follows. */
int remesa_pds_validate_host_data_pair(const struct remesa_pds_plan *plans,
 const struct remesa_pds_binding *programs,const struct remesa_pds_use_link *links,
 const struct remesa_pds_host_data_recipe *recipes,
 const struct remesa_pds_host_data *data);
/* Original logical task-input routing. These 64 slots are a bounded HOST
 * namespace, NOT physical DS registers, SGX limits, DATA banks or an ISA.
 * Every DATA cell must have exactly one source-to-slot definition; slots must
 * be unique. Transfer order is immaterial. The USE entry is routed explicitly
 * from its one canonical relocation cell. Caller cannot assign evidence flags.
 * Values are fully known CPU inputs; cpu_relocation_mask identifies
 * only established CPU relocation fields: lower19 for USE, zero for opaque
 * literals. In particular, the upper13 USE bits remain uninterpreted.
 */
#define REMESA_PDS_HOST_TASK_SCHEMA 1u
#define REMESA_PDS_HOST_TASK_SLOTS 64u
struct remesa_pds_host_transfer {
 uint32_t data_cell,logical_slot;
};
struct remesa_pds_host_transfer_request {
 const struct remesa_pds_host_transfer *transfers;
 size_t transfer_count;
};
struct remesa_pds_host_task_slot {
 uint32_t data_cell,logical_slot,data_kind,value;
 uint32_t cpu_relocation_mask;
 uint64_t data_address;
};
struct remesa_pds_host_task_plan {
 uint32_t schema,reserved;
 enum remesa_ir_stage stage;
 struct remesa_pds_use_link link;
 uint32_t slot_count,entry_logical_slot;
 /* Exact independent blockers, never caller-controlled qualifications.
  * DATA->physical DS routing, dispatch task controls and hardware completion
  * are not provided by logical value resolution. All remain ENOTSUP. */
 int data_to_ds_error,dispatch_error,termination_error;
 /* Canonical source-cell order; unused entries are zero. */
 struct remesa_pds_host_task_slot slots[REMESA_PDS_HOST_TASK_SLOTS];
};
/* Validate VS/FS plans, canonical DATA recipes, exact current USE links and
 * explicit complete logical routing before publishing either result. Bounds,
 * undefined/duplicate slots or sources, stale links and alias/wrap are errors.
 * At most64 cells are admitted by this host planner; larger DATA recipes stay
 * valid for the independent DATA builder and return E2BIG here. No CODE is
 * emitted and no hardware DS, task arity, resource count or completion field
 * is invented. Outputs must be disjoint from every input; failure is atomic.
 */
int remesa_pds_prepare_task_pair(const struct remesa_pds_plan *plans,
 const struct remesa_pds_binding *programs,const struct remesa_pds_use_link *links,
 const struct remesa_pds_host_data_recipe *recipes,
 const struct remesa_pds_host_transfer_request *requests,
 struct remesa_pds_host_task_plan *out);
/* Rebuild every resolved value/location from fresh inputs. Changes to opaque
 * controls, source values, mappings, slots or declared blocker states are
 * rejected; a self-consistent plan does not authenticate a GPU allocation. */
int remesa_pds_validate_task_pair(const struct remesa_pds_plan *plans,
 const struct remesa_pds_binding *programs,const struct remesa_pds_use_link *links,
 const struct remesa_pds_host_data_recipe *recipes,
 const struct remesa_pds_host_transfer_request *requests,
 const struct remesa_pds_host_task_plan *tasks);
/* Bounded CPU zero-texture pixel profile reconstructed from scalar CPU
 * producers/relocations. These three CPU values do NOT establish hardware
 * task arity, DS register sources, dispatch or termination semantics. */
#define REMESA_PDS_CPU_PIXEL_TASK_WORDS 3u
#define REMESA_PDS_CPU_PIXEL_DATA_CELLS 12u
#define REMESA_PDS_CPU_PIXEL_TASK_SCHEMA 1u
struct remesa_pds_cpu_pixel_task {
 uint32_t words[REMESA_PDS_CPU_PIXEL_TASK_WORDS];
};
struct remesa_pds_cpu_pixel_task_fields {
 struct remesa_use_entry_fixup_fields use;
 uint32_t metadata6;
};
struct remesa_pds_cpu_pixel_task_source {
 uint32_t data_cell,logical_slot,value;
 uint64_t data_address;
};
struct remesa_pds_cpu_pixel_task_output {
 uint32_t schema,cpu_word_count;
 struct remesa_pds_cpu_pixel_task task;
 struct remesa_pds_cpu_pixel_task_source sources[REMESA_PDS_CPU_PIXEL_TASK_WORDS];
 int data_to_ds_error,dispatch_error,termination_error;
 uint32_t reserved;
};
/* Exact admitted CPU profile: T0 =0x00100000|lower19 USE fixup,
 * T1 =metadata6<<27 (metadata6=0/1), T2 =0x20. Meaning of upper controls
 * and T2 remains unknown. Pixel data master is required. */
int remesa_pds_cpu_pixel_task_pack(const struct remesa_use_entry_plan *,
 uint32_t metadata6,struct remesa_pds_cpu_pixel_task *);
int remesa_pds_cpu_pixel_task_unpack(const struct remesa_pds_cpu_pixel_task *,
 struct remesa_pds_cpu_pixel_task_fields *);
/* Original complete 48-byte HOST recipe: typed fields at cells0,1,8;
 * all remaining cells are zero by explicit host padding policy. Historical
 * CPU writes do not prove the contents or meaning of these other cells. */
int remesa_pds_cpu_pixel_task_recipe(uint32_t metadata6,
 struct remesa_pds_data_cell out[REMESA_PDS_CPU_PIXEL_DATA_CELLS]);
/* Consume the three CPU profile values from an exact canonical FS logical
 * task graph. Fresh mapping ownership is checked by the pipeline caller;
 * this function checks self-consistency only. Sources retain caller logical
 * slots and byte addresses, never physical DS assignments. All execution
 * blockers stay ENOTSUP. Output must be disjoint from input, ranges may not
 * wrap and every failure leaves output unchanged. No CODE is emitted. */
int remesa_pds_cpu_pixel_task_prepare(const struct remesa_pds_host_task_plan *,
 struct remesa_pds_cpu_pixel_task_output *);
void remesa_pds_encoding_status(struct remesa_pds_encoding_status *);
int remesa_pds_encode(const struct remesa_usse_program *,const struct remesa_pds_plan *,void *,size_t,size_t *written);
/* Explicit selected producer profiles, separate from the unchanged scalar
 * schema above. Numeric owned/symbolic bindings are caller-authenticated;
 * producing bytes never grants transport, GPU admission or AF/first-PC facts.
 * Alignment32 is a conservative host storage/entry policy, not an ISA claim. */
#define REMESA_PDS_SELECTED_SCHEMA 1u
enum remesa_pds_selected_profile {
 REMESA_PDS_SELECTED_VS_WHITE_LAMBERT3=1,
 REMESA_PDS_SELECTED_FS_INPUT_COL0=2,
 REMESA_PDS_SELECTED_FS_CONSTANT_DRIVER_BG0=3,
 /* Existing physical positive gears attribute-normal body320, eleven numeric
  * temporaries, input P/N in the retained three-float4 record. Same original
  * compiled producer fields as its archived physical witness, not a generic
  * resource planner or approval of arbitrary programs/temporary counts. */
 REMESA_PDS_SELECTED_VS_GEARS_ATTRIBUTE_PADDED3=4,
 /* Separate CPU variant: existing lighting plus COLOR clamp, CODE344 in a
  * reserved352-byte USE span. No physical launch qualification. */
 REMESA_PDS_SELECTED_VS_GEARS_ATTRIBUTE_CLAMPED_PADDED3=5
};
struct remesa_pds_selected_request {
 uint32_t profile,use_bank;
 uint64_t semantic_identity; /* exact caller program correlation; not crypto */
 struct remesa_pds_request storage;
 /* bytes is the reserved USE span (144/16/8), not committed CODE64/8/8. */
 struct remesa_pds_binding use,vertex_source;
 uint32_t source_records,reserved; /* VS records of48B; both zero for FS */
};
enum remesa_pds_selected_relocation_kind {
 REMESA_PDS_SELECTED_USE_LOWER19=1,
 REMESA_PDS_SELECTED_SOURCE_ADDRESS32=2
};
struct remesa_pds_selected_relocation {
 uint32_t kind,data_byte_offset,mask,background;
 uint64_t allocation_cookie,mapping_generation,address,bytes;
};
struct remesa_pds_selected_plan {
 uint32_t schema;
 enum remesa_ir_stage stage;
 struct remesa_pds_selected_request request;
 struct remesa_use_entry_plan use_entry;
 uint32_t use_address_bits,data_bytes,code_bytes,image_bytes,alignment;
 uint32_t use_committed_bytes,use_reserved_bytes,pds_reserved_bytes;
 uint32_t relocation_count,reserved;
 struct remesa_pds_selected_relocation relocations[2];
};
/* DATA48 followed immediately by CODE16/12/8. PDS reservation192 for the
 * original VS producer;64 for FS is explicit host rounding policy, not an
 * inferred original FS reservation. Requires contiguous same-allocation
 * DATA/CODE and32B block start. SOURCE/USE fixups are calculated from retained
 * input bindings; relocations/masks preserve their producer distinctions. */
int remesa_pds_selected_prepare(const struct remesa_pds_selected_request *,
 struct remesa_pds_selected_plan *);
int remesa_pds_selected_validate(const struct remesa_pds_selected_plan *,
 const struct remesa_pds_selected_request *current);
int remesa_pds_selected_pair_validate(const struct remesa_pds_selected_plan plans[2]);
/* Resolved LE DATA+CODE and conservative byte known-mask, transactionally.
 * Mask0 remains unknown CPU padding even when initialized to zero. Full USE
 * word mask is known after canonical lower19 fixup, with exact profile bg.
 * All outputs/input spans must be disjoint; every failure leaves bytes,
 * mask and written unchanged. Unknown profiles remain ENOTSUP. */
int remesa_pds_selected_encode(const struct remesa_pds_selected_plan *,void *bytes,
 uint8_t *known_mask,size_t capacity,size_t *written);
/* Selected pixel constant-loader candidate: reuses the existing EE final
 * control encoder (original decoder: nop.end), canonical USE fixup and the
 * constant PDS constructor. Bindings are retained trusted caller metadata,
 * not independently authenticated mappings or acquired GEM references.
 * source.bytes must be the exact second-image transfer; auxiliary.bytes=8.
 * pds.bytes is reserved capacity, not effective program size. Caller also
 * retains indirect texture/constant dependencies and the bank lease.
 */
struct remesa_pds_constant_loader_request {
 uint64_t operation,generation,owner_cookie;
 struct remesa_pds_binding source,auxiliary,pds;
 uint32_t constant_dwords,compiler_attribute_dwords,use_bank;
};
struct remesa_pds_constant_loader_plan {
 struct remesa_pds_constant_loader_request request;
 struct remesa_use_entry_plan auxiliary_entry;
 uint32_t use_address_bits,program_bytes,attribute_dwords,attribute_units;
 uint32_t attribute_mask,attribute_bits;
 uint8_t auxiliary_code[8],pds_image[64],pds_known_mask[64];
 int execution_error;
};
/* Observed auxiliary start alignment128 and PDS start alignment32; no CODE
 * filler. Attribute allocation=max(compiler requirement, transfer words),
 * distinct from source backing/transfer bytes. Field representation alone
 * is not a hardware attribute-capacity approval. All failures transactional.
 */
int remesa_pds_constant_loader_prepare(const struct remesa_pds_constant_loader_request *,
 struct remesa_pds_constant_loader_plan *);
int remesa_pds_constant_loader_revalidate(const struct remesa_pds_constant_loader_plan *,
 const struct remesa_pds_constant_loader_request *current);
#endif
