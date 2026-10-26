/* SPDX-License-Identifier: MIT */
#ifndef REMESA_USSE_FRAGMENT_COLOR_H
#define REMESA_USSE_FRAGMENT_COLOR_H
#include <stddef.h>
#include <stdint.h>
/* Original CPU compiler+linker+whole USE-builder profile, public identity
 * COLOR0/COLOR1 input, default link key0. Numeric class2[0] -> class1[0].
 * Logical code is8 bytes; original requests16 bytes aligned32 and commits8.
 * Bytes8..15 were untouched allocator storage, not a padding instruction.
 * This API writes only the logical bytes. Caller storage remains untouched
 * beyond that end. Allocation extent is metadata, not an upload request.
 * No physical interpolation, executable-memory admission, shader frontend or
 * GPU completion is implied. Key-dependent tests/kill/depth/packing reject.
 * Every output remains unchanged on failure. */
struct remesa_usse_fragment_color_request {
 uint32_t input_slot; /* Public Mesa fragment COLOR0=1, COLOR1=2. */
 uint32_t link_key;   /* Selected profile requires exactly0. */
};
struct remesa_usse_fragment_color_extent {
 size_t logical_bytes, allocation_bytes, alignment_bytes;
 /* Original CPU R descriptor metadata for remesa_pds_structure_entry.
  * Physical attribute/format mapping is not implied. */
 uint32_t r_id, r_components, r_mode, r_flag;
};
int remesa_usse_fragment_color_encode(
 const struct remesa_usse_fragment_color_request *,void *,size_t,
 struct remesa_usse_fragment_color_extent *);
struct nir_shader;
/* Explicit offline NIR->typed request->final USE+R metadata for COLOR0/1.
 * Default shader-program/Gallium admits only the exact COLOR0 graph as
 * COMPILE_OFFLINE. Neither path admits device launch or a shader pair. */
int remesa_usse_fragment_color_encode_nir(const struct nir_shader *,void *,size_t,
 struct remesa_usse_fragment_color_extent *);
#endif
