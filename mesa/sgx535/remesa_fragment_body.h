/* SPDX-License-Identifier: MIT
 * Original composition of research CPU-format field codecs. These records
 * have NOT been qualified as an executable SGX535 shader. Numeric classes,
 * PACK behavior, instruction fetch and bounded termination remain unknown.
 */
#ifndef REMESA_FRAGMENT_BODY_H
#define REMESA_FRAGMENT_BODY_H
#include "remesa_usse.h"

#define REMESA_FRAGMENT_BODY_SCHEMA 1u
#define REMESA_FRAGMENT_BODY_BYTES 56u
enum remesa_fragment_body_status {
 REMESA_FRAGMENT_BODY_CPU_CANDIDATE = 1
};
/* Numeric operands of selected CPU formats, not physical register banks.
 * Both literal values and the packed destination are simultaneously live. */
struct remesa_fragment_registers {
 uint32_t literals[2];
 uint32_t packed;
};
struct remesa_fragment_body {
 uint32_t schema;
 enum remesa_fragment_body_status status;
 uint32_t literal_payloads[2];
 uint32_t channel_sources[4]; /* Payload indices, not physical lane semantics. */
 struct remesa_fragment_registers registers;
 unsigned char records[REMESA_FRAGMENT_BODY_BYTES];
};
/* Only the existing endpoint-color fragment profiles are admitted. The body
 * consists of two literal records, four one-mask packing records and one
 * numeric output binding; it has NO terminal record or launch descriptor.
 * Success means CPU_CANDIDATE, never hardware-ready. All outputs are unchanged
 * on error, and overlapping inputs/outputs are rejected. */
int remesa_fragment_body_prepare(const struct remesa_usse_program *,
 struct remesa_fragment_body *);
int remesa_fragment_body_prepare_at(const struct remesa_usse_program *,
 const struct remesa_fragment_registers *,struct remesa_fragment_body *);
/* Decode the exact supported CPU record grammar and recover its numeric
 * dataflow. Unknown/unmodeled bits are rejected, not named hardware reserved. */
int remesa_fragment_body_decode(const void *,size_t,struct remesa_fragment_body *);
int remesa_fragment_body_validate(const struct remesa_fragment_body *);
/* Smaller original CPU-format candidate. The selected historical CPU writer
 * placed a packed32 payload directly in numeric destination class1/number0,
 * with fixed control bits18 and23. Green, magenta and yellow are the only
 * admitted semantic profiles. Original CPU initializer/writer/finalizer replay
 * independently qualifies all three payload encodings; retained hardware
 * evidence remains contextual and does not qualify a generic color export.
 * It does NOT establish generic output-bank names, END semantics or a PDS
 * launch contract. This candidate needs no float-to-color PACK or temporaries.
 */
#define REMESA_FRAGMENT_CONSTANT_SCHEMA 1u
#define REMESA_FRAGMENT_CONSTANT_BYTES 8u
struct remesa_fragment_constant {
 uint32_t schema;
 enum remesa_fragment_body_status status;
 uint32_t argb;
 unsigned char record[REMESA_FRAGMENT_CONSTANT_BYTES];
};
int remesa_fragment_constant_prepare(const struct remesa_usse_program *,
 struct remesa_fragment_constant *);
int remesa_fragment_constant_decode(const void *,size_t,
 struct remesa_fragment_constant *);
int remesa_fragment_constant_validate(const struct remesa_fragment_constant *);
#endif
