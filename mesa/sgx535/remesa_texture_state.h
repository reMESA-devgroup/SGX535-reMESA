/* SPDX-License-Identifier: MIT */
#ifndef REMESA_TEXTURE_STATE_H
#define REMESA_TEXTURE_STATE_H
#include <stddef.h>
#include <stdint.h>

/* Selected original SGX535/rev121 CPU state-builder profile (numeric layout3).
 * Evidence: docs/phase9/TEXTURE-STATE-ORACLE-20261028.md. This constructor is
 * deliberately separate from Gallium sampler semantics and GPU admission.
 * The observed CPU write is 12 bytes; neither a 16-byte slot spacing nor this
 * size establishes GPU fetch size/alignment, PDS delivery or safe sampling.
 * Numeric formats20/21 and filters0/1/2 identify the original builder inputs,
 * not Gallium enums. No buffer ownership, coherency or retirement is inferred.
 */
#define REMESA_TEXTURE_CPU_STATE_BYTES 12u
struct remesa_texture_cpu_strided_state {
   uint64_t image_address;
   uint32_t width, height, stride_bytes;
   uint32_t format_id, filter_id;
};
/* Checks field representation, row coverage, address alignment and 32-bit
 * full-row span without wrapping. Wider physical/layout limits and the owned
 * mapping/visibility checks must still be enforced by an admitted backend.
 * Transactional: failures preserve destination and *written; excess capacity
 * is untouched. This helper does not enable descriptor_emit_sgx or drawing.
 */
int remesa_texture_cpu_strided_state_encode(
   const struct remesa_texture_cpu_strided_state *, void *, size_t, size_t *written);
#endif
