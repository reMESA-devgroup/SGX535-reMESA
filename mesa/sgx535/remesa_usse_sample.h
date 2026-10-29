/* SPDX-License-Identifier: MIT */
#ifndef REMESA_USSE_SAMPLE_H
#define REMESA_USSE_SAMPLE_H
#include <stddef.h>
#include <stdint.h>
/* Decoder-derived SGX535/rev121 scalar profile: ordinary 2D, xyzw mask,
 * r destination, pa coordinates, sa sampler operand, explicit matching WDF.
 * Evidence: USSE-DECODER-CATALOG-20261028.md and TEXTURE-SMP-ORACLE-20261028.md.
 * Register names are original decoder spellings, NOT proof of GPU footprints,
 * float format, interpolation, normalized coordinates, state size or residency.
 * Seven-bit indices below express fields, not an allocated register window.
 * No predicate, repeat, LOD/bias/gradient, export, END or padding is added.
 * This is a straight-line BODY, not a complete/executable fragment shader.
 * Caller must establish all register/state/texture resources, cache visibility,
 * PDS/stage/output/termination and retirement before any future GPU admission.
 * No existing shader/descriptor gate calls or admits this new profile.
 */
#define REMESA_USSE_SAMPLE_2D_BODY_BYTES 16u
struct remesa_usse_sample_2d_body {
 uint32_t destination,coordinates,sampler,drc,reserved;
};
/* Transactional: all errors preserve output and *written. Excess capacity is
 * untouched; request/output/written aliases and wrapping host spans rejected.
 * The WDF is an instruction for the requested DRC, not a CPU fence/retirement.
 */
int remesa_usse_sample_2d_body_encode(const struct remesa_usse_sample_2d_body *,
 void *,size_t capacity,size_t *written);
#endif
