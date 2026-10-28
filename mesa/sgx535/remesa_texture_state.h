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

/* Selected original GLES2 stream-target composition with explicit stride,
 * independent min/mag image filters, clamp-to-edge and no mipmapping. This
 * identifies the CPU producer role, not support for an IMG/OES GL extension.
 * Evidence: TEXTURE-LAYOUT-COMPOSITION-20261028.md. Hardware fetch/upload and
 * normal texture2D admission remain unqualified. Flags must be exactly 0/1.
 */
struct remesa_texture_cpu_stream_sampler_state {
   uint64_t image_address;
   uint32_t width, height, stride_bytes, format_id;
   uint32_t min_filter_linear, mag_filter_linear;
};
int remesa_texture_cpu_stream_sampler_encode(
   const struct remesa_texture_cpu_stream_sampler_state *, void *, size_t,
   size_t *written);

/* Selected original secondary-constant image construction: literal32 uniforms
 * and identity texture triplets. Byte limit matches the existing 4096-byte
 * host shader-storage policy, not an architectural SA register limit.
 * No arbitrary compiler masks/ORs or float conversions are accepted here.
 */
#define REMESA_TEXTURE_CPU_SA_IMAGE_MAX_BYTES 4096u
struct remesa_texture_cpu_sa_uniform {
   uint32_t destination_dword, value;
};
struct remesa_texture_cpu_sa_texture {
   uint32_t destination_dword;
   struct remesa_texture_cpu_stream_sampler_state sampler;
};
/* Exactly uniform_count+3*texture_count DWORDs must be covered once. Checks
 * all destinations, overlap, input/output spans and image encodings before
 * publishing. No allocation, GPU upload, synchronization or admission occurs;
 * caller must retain/revalidate all owned texture mappings before GPU use.
 * ENODATA for an empty image; all errors preserve output and *written.
 */
int remesa_texture_cpu_sa_image_encode(
   const struct remesa_texture_cpu_sa_uniform *, size_t uniform_count,
   const struct remesa_texture_cpu_sa_texture *, size_t texture_count,
   void *, size_t capacity, size_t *written);
#endif
