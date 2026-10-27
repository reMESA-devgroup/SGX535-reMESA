/* SPDX-License-Identifier: MIT
 * Original scalar, virtual-register scheduling IR. The name USSE denotes the
 * intended future backend, NOT established SGX instruction encodings. All
 * enum IDs, sizes and limits below are host schema definitions, not hardware.
 * Evidence: licensed Mesa/NIR float32 semantics and remesa_shader_ir profile
 * validation. Hardware bit fields, register banks, widths and alignments UNKNOWN.
 */
#ifndef REMESA_USSE_H
#define REMESA_USSE_H
#include "remesa_shader_ir.h"
#define REMESA_USSE_SCHEMA 1u
#define REMESA_USSE_VALUES 64u
#define REMESA_USSE_INSTRUCTIONS 128u
#define REMESA_USSE_NO_VALUE UINT32_MAX
/* Selected CPU selectorEE, producer psb ELF252ceb. Fixed word0=0 and
 * flags bit2 clear; mode8 mirrors input flags bit3, record_last mirrors
 * flags bit16/the separate CPU finalizer. Only the three independently
 * observed combinations are emitted. No NOP/phase/termination/SA-load
 * architectural interpretation is assigned. In particular this codec alone
 * does not prove physical parameter delivery. All failures are transactional. */
struct remesa_usse_cpu_control_ee {
 uint32_t mode8,record_last,reserved;
};
int remesa_usse_cpu_control_ee_encode(const struct remesa_usse_cpu_control_ee *,
 void *,size_t,size_t *written);
/* Original PSB event auxiliary CPU producers, distinct from selector EE.
 * end: one zero-initialized record from ELF30422 + finalizer30fb7, eight
 * effective bytes. transfer: ELF391e0, sixteen effective bytes; admits only
 * the caller392ab modes 0/1/3 (surface tags other/2/1 respectively).
 * These names identify constructor roles, not inferred ISA semantics.
 * No reservation/fetch-width, execution permission or new GPU qualification
 * follows from successful encoding. No copied executable template is used.
 * Output and written remain unchanged on all errors; padding is untouched. */
#define REMESA_USSE_PSB_EVENT_END_BYTES 8u
#define REMESA_USSE_PSB_EVENT_TRANSFER_BYTES 16u
int remesa_usse_psb_event_end_encode(void *,size_t,size_t *written);
int remesa_usse_psb_event_transfer_encode(uint32_t mode,void *,size_t,
 size_t *written);
#define REMESA_USSE_HOST_HEADER_BYTES 16u
#define REMESA_USSE_HOST_INSTRUCTION_BYTES 24u

enum remesa_usse_op {
 REMESA_USSE_INPUT=1, REMESA_USSE_CONSTANT, REMESA_USSE_MOVE,
 REMESA_USSE_EXPORT_POSITION, REMESA_USSE_EXPORT_COLOR
};
struct remesa_usse_instruction {
 uint32_t op, dst, src, channel, bits, reserved;
};
struct remesa_usse_program {
 uint32_t schema;
 enum remesa_ir_stage stage;
 uint32_t count;
 struct remesa_usse_instruction instructions[REMESA_USSE_INSTRUCTIONS];
};
enum remesa_encoding_evidence { REMESA_ENCODING_UNKNOWN=0 };
struct remesa_usse_encoding_status {
 enum remesa_encoding_evidence opcode, instruction_width, alignment;
 enum remesa_encoding_evidence register_banks, register_addressing, predicate;
 enum remesa_encoding_evidence execution_control, input_output, immediate;
};
/* Validation admits only identical float4 POSITION or existing endpoint COLOR
 * semantic profiles. Dense SSA, forward refs, unused nodes/storage rejected. */
int remesa_usse_validate(const struct remesa_usse_program *);
/* Pure COPY/COMPOSE4 aliases and exact-bit constants are shared after strict
 * IR validation. Equivalent chains produce identical scalar plans without
 * new values; repeated color channels share one virtual constant definition.
 * Existing host MOVE records remain valid; no hardware MOV is encoded. */
int remesa_usse_lower(const struct remesa_shader_ir *,struct remesa_usse_program *);
/* Original LE host format. Never submit these bytes as hardware program data.
 * Serialization/deserialization is transactional; every output unchanged on error. */
int remesa_usse_serialize_host(const struct remesa_usse_program *,void *,size_t,size_t *written);
int remesa_usse_deserialize_host(const void *,size_t,struct remesa_usse_program *);
/* Noncryptographic semantic identity, useful only for local correlation.
 * NOT authenticity, operation authorization or evidence of GPU completion. */
int remesa_usse_identity(const struct remesa_usse_program *,uint64_t *identity);
void remesa_usse_encoding_status(struct remesa_usse_encoding_status *);
/* Size/encoding of the existing original packed constant fragment body:
 * opaque magenta/green/yellow only, eight bytes, no caller-selected registers or
 * execution controls. Reuses remesa_fragment_constant_prepare; it does not
 * manufacture vertex, PDS or task/termination semantics. Success encodes that
 * bounded body, not a complete executable pipeline or permission to submit.
 * Unsupported stages/profiles return ENOTSUP. All outputs stay unchanged on
 * failure, including ENOSPC; size query never writes program bytes. */
int remesa_usse_encoded_size(const struct remesa_usse_program *,size_t *bytes);
int remesa_usse_encode(const struct remesa_usse_program *,void *,size_t,size_t *written);
/* Restricted CPU-writer format, independently reconstructed in the original
 * candidate-mov-codec-session. These numeric classes are NOT qualified hardware
 * banks. Eight serialized bytes are not a proven GPU fetch width. Numeric
 * controls reproduce independently traced CPU field contributions, not a
 * qualified repeat, output or termination contract. The exact numeric VS body
 * profile below uses compiler-demonstrated controls; other candidates use zero.
 * control12 occupies bits15:12; the others name single bits.
 * No function below authorizes upload to executable memory or submission. */
#define REMESA_USSE_CPU_MOVE_BYTES 8u
/* Maximum candidate size. The qualified numeric class2[0..3] to class1[0..3]
 * position profile uses one shared eight-byte body plus the eight-byte tail
 * demonstrated by the original CPU builder. Physical export remains unproved. */
#define REMESA_USSE_VERTEX_CANDIDATE_BYTES (4u * REMESA_USSE_CPU_MOVE_BYTES)
struct remesa_usse_cpu_register { uint32_t class_id, number; };
struct remesa_usse_cpu_move {
 struct remesa_usse_cpu_register source, destination;
 uint32_t control11, control18;
 uint32_t control12, control21, control23;
};
/* Additionally accepts only the original-writer-proven gears source-class3
 * tuples: vec3 source5/33 -> class0/base4, and scalar source4 -> class1/base7.
 * This is CPU field construction, not parameter/task or GPU admission. */
int remesa_usse_cpu_move_encode(const struct remesa_usse_cpu_move *,
 void *,size_t,size_t *written);
int remesa_usse_cpu_move_decode(const void *,size_t,struct remesa_usse_cpu_move *);
/* Projection and original encoding of the selected manual three-source CPU-writer form.
 * Source slots identify CPU field positions only: they do not establish GPU
 * reads, physical banks, vertex export, implicit outputs or termination.
 * The fixed subgroup/controls and zero-initialized reserved fields are checked;
 * other family31 forms remain unsupported. Encoding is CPU-form only and
 * does not qualify vertex exports, execution controls or task termination.
 * Input must contain exactly eight bytes. Output is unchanged on any failure. */
#define REMESA_USSE_CPU_THREE_SOURCE_BYTES 8u
struct remesa_usse_cpu_three_source {
 struct remesa_usse_cpu_register source[3];
 uint32_t source_extension[3];
 uint32_t word0_control21,word0_control22_27;
 uint32_t word1_control0_1,word1_control12_13,word1_control18;
};
int remesa_usse_cpu_three_source_decode(const void *,size_t,
 struct remesa_usse_cpu_three_source *);
/* Fields reproduce selected original CPU writer 0x30a09 (psb ELF).
 * All outputs are transactional. Numeric class limits describe field widths,
 * not hardware register-bank admission. No execution permission is granted. */
int remesa_usse_cpu_three_source_encode(const struct remesa_usse_cpu_three_source *,
 void *,size_t,size_t *written);
/* Explicit numeric input/output assignments, supplied for offline candidates
 * only. No implicit class-to-attribute/output mapping is made. All destinations
 * must be distinct and disjoint from all inputs: the straight-line copy order
 * must not destroy an unread input. Hardware aliasing across classes UNKNOWN. */
struct remesa_usse_vertex_mapping {
 struct remesa_usse_cpu_register input[4], output[4];
};
int remesa_usse_vertex_mapping_validate(const struct remesa_usse_vertex_mapping *);
/* Original host export contract for the admitted identical POSITION float4.
 * A route follows all semantic MOVE aliases back to its input definition and
 * records exactly which caller-selected numeric CPU fields the candidate uses.
 * Numeric resource masks/spans describe indices in the CPU-writer namespace,
 * NOT physical register banks, task resource counts or an output allocation.
 * The three hardware errors remain ENOTSUP: no input/output bank assignment,
 * vertex consumer handoff or task termination has been qualified. A successful
 * plan is not executable code or evidence of GPU completion. Plans are
 * canonical, immutable snapshots tied to the exact program and mapping.
 * Preparation/validation do not mutate their inputs; output is unchanged on
 * any failure, including overlap and pointer wrap. */
#define REMESA_USSE_VERTEX_EXPORT_SCHEMA 1u
#define REMESA_USSE_CPU_REGISTER_CLASSES 3u
#define REMESA_USSE_CPU_REGISTER_MASK_WORDS 4u
struct remesa_usse_vertex_export_route {
 uint32_t channel, bit_size, export_instruction, source_value;
 uint32_t input_value, input_instruction, input_channel, candidate_offset;
 uint64_t dependencies;
 struct remesa_usse_cpu_register source, destination;
};
struct remesa_usse_vertex_export_resource {
 uint32_t read_mask[REMESA_USSE_CPU_REGISTER_MASK_WORDS];
 uint32_t write_mask[REMESA_USSE_CPU_REGISTER_MASK_WORDS];
 uint32_t read_max_index_plus_one, write_max_index_plus_one;
};
struct remesa_usse_vertex_export_plan {
 uint32_t schema;
 enum remesa_ir_stage stage;
 uint32_t route_count, candidate_bytes;
 uint64_t semantic_identity, used_values;
 struct remesa_usse_vertex_export_route route[4];
 struct remesa_usse_vertex_export_resource resource[REMESA_USSE_CPU_REGISTER_CLASSES];
 int input_bank_error, export_bank_error, termination_error;
 uint32_t reserved;
};
int remesa_usse_vertex_export_plan(const struct remesa_usse_program *,
 const struct remesa_usse_vertex_mapping *,struct remesa_usse_vertex_export_plan *);
int remesa_usse_vertex_export_validate(const struct remesa_usse_program *,
 const struct remesa_usse_vertex_mapping *,const struct remesa_usse_vertex_export_plan *);
int remesa_usse_vertex_candidate(const struct remesa_usse_program *,
 const struct remesa_usse_vertex_mapping *,void *,size_t,size_t *written);
#endif
