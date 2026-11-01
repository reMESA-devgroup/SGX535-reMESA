/* SPDX-License-Identifier: MIT
 * Original reconstruction of CPU descriptor/index construction, not an ISA
 * semantic decoder or executable shader. Evidence: psb_dri.so ELF 0x25970,
 * descriptor paths 0x25bd4..0x260d0, markers 0x2612c/0x26163 and CODE stores
 * 0x26345..0x263c4. The separate fixed psb event profile below reconstructs
 * its original CPU stores; it is not a general event instruction encoder.
 */
#ifndef REMESA_PDS_STRUCTURE_H
#define REMESA_PDS_STRUCTURE_H
#include <stddef.h>
#include <stdint.h>

/* Bounded offline reconstruction policy, not hardware limits. */
#define REMESA_PDS_STRUCTURE_INPUTS 16u
#define REMESA_PDS_STRUCTURE_DATA_CELLS 128u
#define REMESA_PDS_STRUCTURE_CODE_SLOTS 35u
#define REMESA_PDS_STRUCTURE_DESCRIPTORS 17u
#define REMESA_PDS_STRUCTURE_RELOCATIONS 17u
#define REMESA_PDS_ALLOCATOR_LOGICAL_WORDS (REMESA_PDS_STRUCTURE_INPUTS * 4u)
#define REMESA_PDS_STRUCTURE_NO_CELL UINT32_MAX
#define REMESA_PDS_STRUCTURE_IMAGE_BYTES \
 ((REMESA_PDS_STRUCTURE_DATA_CELLS + REMESA_PDS_STRUCTURE_CODE_SLOTS) * 4u)

enum remesa_pds_structure_kind {
 REMESA_PDS_STRUCTURE_R=1, REMESA_PDS_STRUCTURE_T=2
};
struct remesa_pds_structure_entry {
 uint32_t kind,id;
 /* R fields: component count, equality-to-3 mode, nonzero flag. id 10/11
  * selects explicit_format instead of the observed default 0x0c000000.
  * Byte-sized ids are deliberately not narrowed to hypothetical nibbles:
  * overlapping OR contributions remain exactly overlapping CPU fields.
  * All id10/11 entries share one explicit_format in one request. */
 uint32_t components,mode,flag_r,format_present,explicit_format;
 /* T fields: nonzero flag_a; flag_b selects 0x200 rather than 0x100.
  * state_words are the two CPU values preceding the texture relocation.
  * reloc_arg3/arg4 retain original helper arguments, not authenticated VAs. */
 uint32_t flag_a,flag_b,state_words[2],reloc_arg3,reloc_arg4;
};
enum remesa_pds_structure_code_policy {
 REMESA_PDS_STRUCTURE_CODE_LEGACY=0,
 REMESA_PDS_STRUCTURE_CODE_CPU_COMPLETE=1
};
struct remesa_pds_structure_request {
 uint32_t entry_count,extra_r,task_word1,temp_count;
 struct remesa_pds_structure_entry entries[REMESA_PDS_STRUCTURE_INPUTS];
 /* Opt-in complete psb primary CPU grammar; not physical ISA admission. */
 uint32_t code_policy;
};
enum remesa_pds_structure_slot_kind {
 REMESA_PDS_STRUCTURE_INITIAL_HOLE=1,
 REMESA_PDS_STRUCTURE_A2=2,
 REMESA_PDS_STRUCTURE_CLASS64=3,
 REMESA_PDS_STRUCTURE_FINAL_HOLE=4,
 REMESA_PDS_STRUCTURE_PAIR_SINGLE=5,
 REMESA_PDS_STRUCTURE_CPU_TRAILER_AF=6,
 REMESA_PDS_STRUCTURE_CPU_EMPTY_SECONDARY_AF=7,
 REMESA_PDS_STRUCTURE_CPU_TRANSFER_PAIR=8,
 REMESA_PDS_STRUCTURE_CPU_VERTEX_SETUP=9,
 REMESA_PDS_STRUCTURE_CPU_VERTEX_PAIR=10,
 REMESA_PDS_STRUCTURE_CPU_VERTEX_TASK=11,
 REMESA_PDS_STRUCTURE_CPU_FORWARD_INDEX=12,
 REMESA_PDS_STRUCTURE_CPU_RECORD_LAST_E6=13
};
/* Restricted original CPU construction IR. These are names for observed
 * word classes, not assertions about a hardware instruction mnemonic.
 * A2 has only ds1_index; ds0_index must be NO_CELL. Class64 has both.
 * CPU_TRAILER_AF has no CPU operands (both must be NO_CELL). It is only
 * admitted at the end of the psb primary profile beginning PAIR_SINGLE(0,0)
 * or the exact three-word state CPU profile documented below.
 * It describes original CPU grammar, not HALT/dispatch/termination semantics.
 * CPU_TRANSFER_PAIR uses ds0_index 0 or 2 and NO_CELL ds1_index. These
 * identify the two original CPU DATA pairs; no hardware transfer mnemonic
 * or destination is established.
 * CPU_EMPTY_SECONDARY_AF is the separate empty-secondary constructor: no
 * operands, no DATA, and exactly one CODE word. This is a CPU construction
 * contract, not an architectural HALT or no-read assertion.
 * CPU_VERTEX_SETUP has no operands; CPU_VERTEX_PAIR has ds0 0/2 and no
 * ds1; CPU_VERTEX_TASK admits only (4,1). These reconstruct the original
 * one/two-descriptor vertex constructor, not hardware instruction semantics.
 * CPU_FORWARD_INDEX uses ds0_index as a forward word delta (3, 4, or 13)
 * and NO_CELL ds1_index. encode_code derives its current index from the IR
 * position and requires the resulting target to lie inside the same CODE.
 * It reconstructs CPU index arithmetic, not a hardware branch instruction.
 * CPU_RECORD_LAST_E6 uses ds0_index as the original constructor's even
 * record cursor (0..66, even), and NO_CELL ds1_index. The producer emits
 * 0x170000e6 | ((cursor+1)<<9) only for the last record of its last group.
 * This is a numerical CPU field, not identification of a DS register or
 * architectural operation. Other class17 low control forms are unresolved.
 * encode_code does not grant DATA capacity or physical execution admission.
 * Unresolved holes and unknown classes remain non-encodable. */
struct remesa_pds_code_instruction {
 uint32_t kind,ds0_index,ds1_index;
};
/* Original psb3967b/396b2 and independent Windows100a71d6 arithmetic.
 * Only the observed forward deltas are admitted; total_words is bounded by
 * CODE_SLOTS. The output remains unchanged on any error. */
int remesa_pds_structure_forward_index(uint32_t current_word,
 uint32_t forward_delta,uint32_t total_words,uint32_t *word);
/* CPU DATA-reference grammar shared by the two audited texture producers.
 * These names describe CPU writes/fixup sites, not a physical PDS read.
 * A state pair occupies two consecutive logical bank-0 indices starting
 * at an even index. Texture and descriptor occupy logical bank-1 neighbours;
 * a physical DATA-row boundary makes their cell offsets non-consecutive.
 * Xpsb places descriptor after address, psb_dri places it before address. */
enum remesa_pds_texture_data_order {
 REMESA_PDS_TEXTURE_DESCRIPTOR_BEFORE_ADDRESS=1,
 REMESA_PDS_TEXTURE_DESCRIPTOR_AFTER_ADDRESS=2
};
struct remesa_pds_texture_output {
 uint32_t order,state_cells[2],texture_cell,descriptor_cell;
};
/* CPU writer references recovered from OGL ELF13f470, not physical DS reads.
 * pair_cells are two bank-0 cells; single_cell is one bank-1 cell. */
struct remesa_pds_pair_single_output {
 uint32_t pair_cells[2],single_cell;
};
struct remesa_pds_structure_slot {
 uint32_t kind,word,known_mask,byte_offset,ds1_index,ds0_index;
};
struct remesa_pds_structure_descriptor {
 uint32_t first_input,has_r,has_t,ds1_index,data_cell,ds0_index;
};
enum remesa_pds_structure_relocation_kind {
 REMESA_PDS_STRUCTURE_USE=1, REMESA_PDS_STRUCTURE_TEXTURE=2
};
struct remesa_pds_structure_relocation {
 uint32_t kind,data_cell,arg3,arg4,background;
};
struct remesa_pds_structure {
 /* Values are native uint32_t analysis results, never a submission buffer.
  * DATA known masks distinguish untouched cells and symbolic relocations
  * from fully determined CPU writes. No zero padding is claimed historical.
  * Default initial/final slots remain holes; explicit CPU_COMPLETE calculates
 * all CODE words without resolving DATA addresses. No upload API is supplied. */
 uint32_t data[REMESA_PDS_STRUCTURE_DATA_CELLS];
 uint32_t data_known_mask[REMESA_PDS_STRUCTURE_DATA_CELLS];
 uint32_t data_dwords,code_slot_count,descriptor_count,relocation_count;
 uint32_t a2_count,class64_count,use_background,last_r_cell,last_t_cell;
 struct remesa_pds_structure_slot code[REMESA_PDS_STRUCTURE_CODE_SLOTS];
 struct remesa_pds_structure_descriptor descriptors[REMESA_PDS_STRUCTURE_DESCRIPTORS];
 struct remesa_pds_structure_relocation relocations[REMESA_PDS_STRUCTURE_RELOCATIONS];
};
/* Restricted original CPU allocator input, AFTER liveness analysis. A bit k
 * in live_components[i] means the original four-bit liveness nibble for that
 * entry's logical word k was nonzero. This API does not infer NIR liveness.
 * logical_width is the original 32-byte allocator record's +0x1c field.
 * flags must be zero: pinned prefixes and other allocator modes are unknown
 * to this implementation and are rejected rather than approximated. */
struct remesa_pds_allocator_request {
 struct remesa_pds_structure_request source;
 uint32_t flags;
 uint32_t logical_width[REMESA_PDS_STRUCTURE_INPUTS];
 uint32_t live_components[REMESA_PDS_STRUCTURE_INPUTS];
};
struct remesa_pds_allocation {
 struct remesa_pds_structure_request request;
 struct remesa_pds_structure structure;
 uint32_t logical_words_before,logical_words_after;
 /* Source indices refer to allocator_request.source; entry indices refer
  * to the surviving, stably reordered request. Unmapped entries/words use
  * NO_CELL. Retained prefixes preserve interior dead words. */
 uint32_t source_to_entry[REMESA_PDS_STRUCTURE_INPUTS];
 uint32_t entry_to_source[REMESA_PDS_STRUCTURE_INPUTS];
 uint32_t source_base[REMESA_PDS_STRUCTURE_INPUTS];
 uint32_t entry_base[REMESA_PDS_STRUCTURE_INPUTS];
 uint32_t entry_width[REMESA_PDS_STRUCTURE_INPUTS];
 uint32_t entry_live_components[REMESA_PDS_STRUCTURE_INPUTS];
 uint32_t logical_map[REMESA_PDS_ALLOCATOR_LOGICAL_WORDS];
};
/* CPU bit expressions only. Index 0..63 is an explicit bounded field policy,
 * not proof of DS register addressing or an architectural register count.
 * Outputs are preserved on error; errno values are returned, never negated. */
/* Original381a0 single-descriptor CPU control, for1..16 sourceDWORDs.
 * Source BO/offset and relocations are deliberately separate. No physical
 * DMA/read destination is inferred. Zero is invalid, larger inputs ENOTSUP;
 * output is preserved on error. */
int remesa_pds_structure_dma_control(uint32_t word_count,uint32_t *word);
/* Original381a0 CPU descriptor partition for a bounded1..128-DWORD source.
 * Larger coefficient blocks (e.g.9vec4 plus prefix =37DWORDs) can require
 * two descriptors. Offsets are relative source DWORDs, NOT DS register IDs.
 * Controls reconstruct CPU output only; this does not qualify DMA delivery,
 * a secondary CODE recipe or a GPU launch. Zero/unsupported sizes preserve
 * output. The legacy single-control API retains its1..16 limit. */
struct remesa_pds_dma_partition {
 uint32_t count,source_word_offset[2],control[2];
};
int remesa_pds_structure_dma_partition(uint32_t word_count,
 struct remesa_pds_dma_partition *);
int remesa_pds_structure_a2(uint32_t ds1_index,uint32_t *word);
int remesa_pds_structure_class64(uint32_t ds0_index,uint32_t ds1_index,uint32_t *word);
/* Parameterized CPU word producer ELF13f5b3..13f5f4. The admitted constructor
 * domain is a=0/2,b=0/1, a=2,b=2 (state-upload producer), or a=4,b=0..3. This is not an ISA register limit,
 * MOVS/DOUTU identification, or proof of the physical number of reads. */
int remesa_pds_structure_pair_single(uint32_t a,uint32_t b,uint32_t *word);
int remesa_pds_structure_encode_pair_single_output(
 const struct remesa_pds_pair_single_output *,uint32_t data_dwords,
 uint8_t *bytes,size_t capacity,size_t *written);
/* Opt-in CPU structural refinement: calculate the initial word from the
 * existing task DATA cells0,1,8. Preserve symbolic fixups, all body slots and
 * the unresolved AF suffix. Output remains unchanged on failure. This does
 * not authorize a complete/executable program or alter the legacy default. */
int remesa_pds_structure_resolve_initial(const struct remesa_pds_structure *,
 struct remesa_pds_structure *);
/* Emit a bounded sequence of calculated CPU word classes in little endian.
 * CPU_TRAILER_AF requires the canonical prefix and must occur exactly once,
 * at the final position. This is not a physical execution/termination rule.
 * The separately proven three-word state CPU profile also admits a final
 * CPU_TRAILER_AF after CPU_TRANSFER_PAIR(0) then PAIR_SINGLE(2,2).
 * The exact four/five-word vertex CPU profile (SETUP,PAIR0,[PAIR2],TASK4/1)
 * also admits a final trailer. The original constant-loader secondary profile
 * additionally admits one/two CPU_TRANSFER_PAIR sources0/[2], followed by
 * PAIR_SINGLE(2/4,0) and trailer. Other vertex-prefixed combinations are rejected.
 * CPU_EMPTY_SECONDARY_AF is admitted only as a sole operand-free word,
 * matching the original empty-secondary producer (ELF3faa3).
 * No input word or historical byte sequence is accepted. Unknown classes
 * return ENOTSUP, out-of-policy operands ERANGE, insufficient capacity ENOSPC.
 * count zero accepts NULL instructions/bytes with capacity zero. On failure
 * bytes and written are unchanged. Input, bytes and written cannot overlap. */
int remesa_pds_structure_encode_code(
 const struct remesa_pds_code_instruction *instructions,size_t count,
 uint8_t *bytes,size_t capacity,size_t *written);
/* Derive and emit exactly A2 then class64 from bounded DATA cell references.
 * CPU bank maps are cell0(k)=16*(k/8)+k%8, cell1(k)=cell0(k)+8.
 * DATA has 12..128 DWORDs, a multiple of four; reserved task cells cannot
 * serve as these sources. Only the two observed descriptor/address orders
 * are admitted. Logical indices remain bounded to 0..63 by encoder policy.
 * The instruction emitter above performs byte emission; no historical CODE
 * word is accepted here. Output and written remain unchanged on any error.
 * This is CPU_DESCRIPTOR_ENCODING_CONFIRMED, not verified GPU semantics. */
int remesa_pds_structure_encode_texture_output(
 const struct remesa_pds_texture_output *output,uint32_t data_dwords,
 uint8_t *bytes,size_t capacity,size_t *written);
/* Serialize a structural result as a sparse little-endian DATA+CODE image.
 * Each mask byte is 0xff for determined bits or zero for an unresolved byte.
 * Unknown DATA, relocation sites and unresolved initial/final slots emit zero
 * with mask zero; they are NOT executable padding. Symbolic fixups remain in
 * structure.relocations and are not resolved by this API. The validator checks
 * allocation indices, slot fields, masks, counts and fixup bounds before any
 * publication. Neither buffer nor written changes on error. All output/input
 * ranges must be disjoint. This API does not authorize GPU execution. */
int remesa_pds_structure_serialize(const struct remesa_pds_structure *structure,
 uint8_t *bytes,uint8_t *known_mask,size_t capacity,size_t *written);
/* Adjacent R then T share one descriptor and A2. All other pairs remain
 * separate. extra_r appends the arithmetic R(id14,components3,mode3,flag0)
 * decomposition of the observed extra descriptor; its physical meaning is
 * not established. This is DATA reconstruction, not a shader bytecode blob.
 * USE background and texture fixup sites are retained without applying
 * addresses. task_word1 is caller-provided CPU input and must retain
 * (temp_count<<27); data[0] stays symbolic.
 * Unused per-kind fields must be zero; booleans must be 0/1.
 * Complete output is published transactionally; overlap with input rejected.
 * This does not call or alter remesa_pds_encode(), which remains ENOTSUP. */
int remesa_pds_structure_encode(const struct remesa_pds_structure_request *,
 struct remesa_pds_structure *);
/* Reconstruct the bounded mode-zero CPU allocator subset, then invoke the
 * existing DATA/CODE structure encoder. T and R(id10/11) retain their entire
 * width if any component is live. Other R trim trailing dead components only.
 * Empty entries are removed; survivors alternate R/T, initially preferring R,
 * with stable first-remaining selection and fallback when a group is empty.
 * R widths must equal components; widths 1..4 are a bounded API policy.
 * R modes other than zero, flags, and unknown kinds return ENOTSUP. Invalid
 * liveness/widths return EINVAL/ERANGE. Output is unchanged on every failure;
 * input/output overlap, unaligned structures and wrapped ranges are rejected.
 * request.extra_r is passed to the existing PDS encoder, not assigned a new
 * logical input range here. This does not reconstruct compiler-feature USE
 * slot reservations or their mappings. This API does not enable execution
 * unless the explicit CPU_COMPLETE construction policy is selected; physical
 * execution remains unsupported for either policy. */
int remesa_pds_structure_allocate(const struct remesa_pds_allocator_request *,
 struct remesa_pds_allocation *);
/* Dedicated original CPU background producer ELF39c5c. surface_tag names
 * the numeric surface->object+0c input of4b710 (1/2/4 only); no Gallium format
 * or architectural sampling meaning is assigned. width/height correspond to
 * source+0c/+10 extents. 1..4096 is a lossless CPU-field policy, not a silicon
 * limit. color_address is a checked32bit BYTE address, not authenticated here.
 * use_lower19 must come from the existing canonical USE relocation planner.
 * Emits48 DATA bytes plus4 existing semantic CODE classes, transactionally.
 * Unwritten DATA cells4..7/11 emit zero as an explicit host padding policy,
 * with zero known masks; all other cells/CODE are numerically determined.
 * This does not authorize mapping, upload, GPU execution or arbitrary textures.
 */
#define REMESA_PDS_STRUCTURE_BACKGROUND_BYTES 64u
struct remesa_pds_background_request {
 uint64_t color_address;
 uint32_t use_lower19,surface_tag,width,height;
};
int remesa_pds_structure_encode_background(
 const struct remesa_pds_background_request *,uint8_t *bytes,uint8_t *known_mask,
 size_t capacity,size_t *written);
/* Original287ff single-source state-upload image:48 DATA +12 CODE bytes.
 * Source STATE allocation and the per-count auxiliary USE program are distinct
 * objects. use_lower19 must be derived by the canonical USE entry planner for
 * that auxiliary, never the fragment shader. Source extent/offset are checked
 * here; ownership, visibility and auxiliary correspondence to state_dwords
 * remain the caller's responsibility. This CPU codec does not validate the
 * contents of the ISP/MTE source state or authorize a complete TA/depth job.
 * The default profile preserves Original287ff and its1..16DWORD limit.
 * The PARAM profile also accepts factored1..128-DWORD lengths when the
 * original partition uses exactly one descriptor; two-descriptor CODE is
 * still unsupported. Larger spans require a separately generated matching
 * auxiliary USE program, not reuse of a16-DWORD copy task. The PARAM profile
 * additionally models Original29385 direct stores DATA3=0x04000000, DATA10=0;
 * it does not supply the corresponding auxiliary USE instruction or establish
 * physical coefficient delivery. Other unknown/unwritten DATA cells are zero
 * with known_mask=0;
 * zero is a host initialization policy, not a demonstrated hardware value.
 * All buffers and written must be disjoint; failure changes no outputs. */
#define REMESA_PDS_STRUCTURE_STATE_BYTES 60u
enum remesa_pds_state_task_profile {
 REMESA_PDS_STATE_TASK_ISP=0,
 REMESA_PDS_STATE_TASK_PARAMETERS=1
};
struct remesa_pds_state_image_request {
 uint64_t source_allocation_address,source_allocation_bytes,source_offset;
 uint32_t state_dwords,use_lower19,task_profile;
};
int remesa_pds_structure_encode_state_image(
 const struct remesa_pds_state_image_request *,uint8_t *bytes,uint8_t *known_mask,
 size_t capacity,size_t *written);
/* Selected original GLES2 constant-loader CPU profile, distinct from PARAM.
 * 1..128 source DWORDs produce one/two DMA descriptors, then a fallback USE
 * task (canonical lower19, other two task values zero) and trailer. DATA48,
 * CODE12/16; padding is zero with mask0, not recovered contents or CODE fill.
 * Source capacity, effective DWORD count and returned program size are separate.
 * This does not establish hardware transfer extent, destination SA, task
 * execution, mapping ownership or permission to submit. All outputs are
 * disjoint/transactional. Source and canonical USE binding stay caller-owned.
 */
#define REMESA_PDS_CONSTANT_DATA_BYTES 48u
#define REMESA_PDS_CONSTANT_MAX_IMAGE_BYTES 64u
struct remesa_pds_constant_image_request {
 uint64_t source_allocation_address,source_allocation_bytes,source_offset;
 uint32_t constant_dwords,use_lower19;
};
int remesa_pds_structure_encode_constant_image(
 const struct remesa_pds_constant_image_request *,uint8_t *bytes,
 uint8_t *known_mask,size_t capacity,size_t *written);
/* Host source construction policy, not a recovered GPU instruction. Prefix one
 * self-address DWORD to coefficient_bytes (already little endian). The caller
 * explicitly chooses transfer_dwords: either the exact effective length
 * (1+coefficient_dwords), or that length rounded up to four DWORDs. Every byte
 * of the chosen transfer is initialized; trailing data is zero, not CODE.
 * Only lengths supported by the existing one-descriptor PARAM encoder pass.
 * source_allocation_bytes is backing capacity, NOT the DMA/TA count. Use
 * transfer_dwords in both state_image and ta_parameter_reference encoders.
 * TA rounds that committed transfer count to 16-byte units; it does not prove
 * shader consumption of the extra lanes. This function proves no GPU delivery,
 * matrix arithmetic, bank capacity or auxiliary correspondence. The caller
 * owns allocation/lifetime/visibility and a matching auxiliary USE program.
 * Outputs, request, coefficient bytes and written must be disjoint. Failure
 * changes no output. Bytes beyond transfer_dwords*4 remain untouched. */
struct remesa_pds_parameter_source_request {
 uint64_t source_allocation_address,source_allocation_bytes,source_offset;
 uint32_t coefficient_dwords,transfer_dwords;
};
int remesa_pds_structure_encode_parameter_source(
 const struct remesa_pds_parameter_source_request *,
 const uint8_t *coefficient_bytes,size_t coefficient_bytes_size,
 uint8_t *bytes,size_t capacity,size_t *written);
/* Original40355 restricted single-DMA primary vertex image:48 DATA+16 CODE.
 * vertex_stride_dwords1..16 controls the source record extent and DATAword8
 * byte stride. It is NOT a vertex count or an attribute format. Source offset
 * addresses one complete record within owned backing; max index, draw start,
 * bias, resource lifetime and complete fetch bounds remain caller contracts.
 * use_lower19 identifies the corresponding vertex auxiliary via canonical USE
 * entry planning, not FS code. Unknown DATA cells are zero with mask0; DATA
 * cells0,1,4,8 and CODE are known. No hardware fetch or DS claim is made.
 * Request/output/mask/written must be disjoint; errors leave all outputs intact.
 */
#define REMESA_PDS_STRUCTURE_VERTEX_BYTES 64u
struct remesa_pds_vertex_image_request {
 uint64_t source_allocation_address,source_allocation_bytes,source_offset;
 uint32_t vertex_stride_dwords,use_lower19;
};
int remesa_pds_structure_encode_vertex_image(
 const struct remesa_pds_vertex_image_request *,uint8_t *bytes,uint8_t *known_mask,
 size_t capacity,size_t *written);
/* Main compiled-VS single-DMA producer4252d. Reuses source/stride/CODE
 * encoding but independently supplies DATA+0x14=e0000000 and DATA+0x24=c.
 * This is distinct from legacy upload/copy producer40355, whose API/masks
 * remain unchanged. No task resources, hardware mode or execution admission
 * are inferred from these CPU stores. All output/mask/written transactional. */
int remesa_pds_structure_encode_compiled_vertex_image(
 const struct remesa_pds_vertex_image_request *,uint8_t *bytes,uint8_t *known_mask,
 size_t capacity,size_t *written);
/* Original Xpsb ELF7eb0 event DATA stores, excluding unresolved event CODE.
 * source identifies the separate 16-byte render-target state. use_lower19 is
 * the canonical relocation for its event USE auxiliary (background bit21),
 * not the FS shader. Known CPU fields are marked ff; untouched original
 * cells become policy zero with mask0. No DS/preload or dispatch claim.
 * Does not produce a complete event program or change execution admission.
 * All arguments/output regions must be disjoint; failure is transactional. */
#define REMESA_PDS_STRUCTURE_EVENT_DATA_BYTES 64u
struct remesa_pds_event_data_request {
 uint64_t source_allocation_address,source_allocation_bytes,source_offset;
 uint32_t use_lower19;
};
int remesa_pds_structure_encode_event_data(
 const struct remesa_pds_event_data_request *,uint8_t *bytes,uint8_t *known_mask,
 size_t capacity,size_t *written);
/* Separate original psb_dri392ab event DATA64 profile (CODE84 follows it).
 * Unlike Xpsb7eb0 above, this has TWO USE links: end-task at DATA0 and
 * transfer auxiliary at DATA4; target DATA16 is addressed from DATA2.
 * Recovered direct stores3959a..3965a put8 at DATA8, not zero. The producer
 * leaves DATA6/7/15 unwritten: policy zero/mask0, not consumed-size evidence.
 * Both lower19 inputs must derive from owned canonical USE entry plans;
 * numeric ranges alone authenticate no mapping or code. No CODE is emitted,
 * and this DATA-only API does not emit a body. The separate fixed-profile
 * API below preserves the original CODE84; Xpsb remains unchanged.
 */
struct remesa_pds_psb_event_data_request {
 uint64_t source_allocation_address,source_allocation_bytes,source_offset;
 uint32_t end_use_lower19,transfer_use_lower19;
};
int remesa_pds_structure_encode_psb_event_data(
 const struct remesa_pds_psb_event_data_request *,uint8_t *bytes,uint8_t *known_mask,
 size_t capacity,size_t *written);
/* Exact fixed392ab CPU construction, not a general event ISA encoder.
 * Reuses the DATA request above and reproduces the original392ab
 * CODE stores, including the two code-base-relative indices. The same CODE
 * is preserved in the physically qualified glxgears profile. Its remaining
 * instruction/operand semantics are NOT inferred from numerical agreement.
 * Original reservation400, committed DATA64+CODE84=148, alignment32 are
 * separate contracts. No extra CODE or padding is emitted to fill capacity;
 * neither148 nor400 is a measured GPU fetch extent. CODE bytes are known
 * CPU stores; DATA6/7/15 remain policy-zero/mask0. No upload/admission follows.
 * All output regions/request must be disjoint; errors preserve all outputs.
 */
#define REMESA_PDS_PSB_EVENT_PROFILE_BYTES 148u
#define REMESA_PDS_PSB_EVENT_PROFILE_RESERVATION_BYTES 400u
int remesa_pds_structure_encode_psb_event_profile(
 const struct remesa_pds_psb_event_data_request *,uint8_t *bytes,uint8_t *known_mask,
 size_t capacity,size_t *written);
#endif
