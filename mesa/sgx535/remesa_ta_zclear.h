/* SPDX-License-Identifier: MIT */
#ifndef REMESA_TA_ZCLEAR_H
#define REMESA_TA_ZCLEAR_H
#include <stdint.h>
/* Original psb_scene_zclear(ELF2a794), distinct from generic glClear529cb.
 * Both attachments must have equal dimensions. The caller establishes live
 * owned backing, surface status, Z clear value and scene/resource lifetime. */
struct remesa_ta_zclear_request {
 uint32_t color_width,color_height,depth_width,depth_height;
 float clear_depth;
};
struct remesa_ta_zclear_geometry {
 uint8_t positions[64]; /* four window-space vec4 records, stride16bytes */
 uint8_t indices[12];   /* original two triangles, uint16 little-endian */
};
/* No color records, shader/PDS CODE, physical Z texel layout or depth clear
 * execution is emitted. The original special clear state1e00100 is NOT the
 * ordinary opaque ISP state with rgba_mask0. Geometry alone cannot select it.
 * No outputs change on error; input/output ranges must be disjoint. */
int remesa_ta_zclear_geometry_encode(const struct remesa_ta_zclear_request *,
 struct remesa_ta_zclear_geometry *);
/* Covering single triangle used by the retained physical full-frame depth
 * clear: (0,0),(2W,0),(0,2H). Fourth record and six-index capacity are kept
 * byte-compatible with the quad encoder, but a consumer MUST issue exactly
 * three indices (48 vertex bytes / 6 index bytes referenced). No shader CODE
 * or padding instruction is added. Coverage is of pixel centres inside the
 * requested target; this does not establish clipping or GPU execution. */
int remesa_ta_zclear_single_geometry_encode(const struct remesa_ta_zclear_request *,
 struct remesa_ta_zclear_geometry *);
#endif
