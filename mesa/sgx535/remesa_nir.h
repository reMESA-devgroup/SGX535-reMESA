/* SPDX-License-Identifier: MIT */
#ifndef REMESA_NIR_H
#define REMESA_NIR_H
#include <stdint.h>
struct nir_shader;
/* Pure admission. No NIR mutation, GPU program generation or default color.
 * Accepts bounded float32 constants, checked channel routing, vector construction
 * and compile-time uint32 bit operations / Boolean selection of constants,
 * plus up to four disjoint stores covering one complete RGBA output. Lowered
 * scalar component stores and variable-deref write masks normalize to the same
 * constant profile; partial, overlapping or mixed-output writes are rejected.
 * Only opaque observed RGB endpoint colors; general UNORM/alpha is unknown.
 * Constant evaluation is CPU-side admission, not native shader ALU execution. */
int remesa_nir_constant_fragment(const struct nir_shader *,uint32_t *argb);
int remesa_constant_rgba32(const uint32_t rgba_bits[4],uint32_t *argb);
/* Restricted lowered or variable-deref POSITION = GENERIC0 float32 vec4 description.
 * Bounded bit-preserving channel copies, vec2/vec3 grouping and identity vec4 reconstruction are
 * admitted in either IO representation. This recognizes semantics only; no
 * native VS ISA is generated. */
int remesa_nir_position_vertex(const struct nir_shader *,unsigned *input_slot);
/* Explicit opt-in CPU profile: one identity COLOR0/1 variable input and one
 * complete float32 vec4 color output. No interpolation/physical admission is
 * implied. Unknown IO, lowered IO, ALU/swizzles or side effects reject. */
int remesa_nir_fragment_color_input(const struct nir_shader *,unsigned *input_slot);
/* Same complete identity graph, preserving explicit FLAT input semantics.
 * flat is0 for NONE/SMOOTH and1 for FLAT. No other interpolation accepted;
 * output variables cannot carry FLAT. Old smooth-only query stays strict.
 * Outputs unchanged on rejection; distinct output references required. */
int remesa_nir_fragment_color_profile(const struct nir_shader *,unsigned *input_slot,unsigned *flat);
/* Strict lowered-IO CPU compilation profile: GENERIC0 POSITION passthrough,
 * independent GENERIC1/2 NORMAL/LIGHT, scalar fdot3 then fsat, splatted into
 * COLOR0.xyzw. Three float4 inputs and two float4 outputs; no constants,
 * uniforms, aliases, extra arithmetic/control flow or launch admission. */
int remesa_nir_white_lambert_vertex(const struct nir_shader *);
/* Complete, strictly ordered ff-vs semantic profile from the two retained
 * gears NIR captures. Pure recognition only: no IR/runtime/GPU admission.
 * PSIZ=1 is checked, and may be unconsumed only for a triangle draw. */
struct remesa_nir_gears_vertex_profile {
 uint32_t normal_from_attribute,uniform_vectors,source_dwords;
 uint32_t input_count,output_dwords,temporary_dwords,point_size_bits;
};
int remesa_nir_gears_vertex(const struct nir_shader *,
 struct remesa_nir_gears_vertex_profile *);
#endif
