/* SPDX-License-Identifier: MIT */
#ifndef REMESA_RENDER_TARGET_H
#define REMESA_RENDER_TARGET_H
#include <stdint.h>
#include <stdbool.h>
#include "remesa/scene_layout.h"
/* Original encoding of the selected CPU producer's linear cpp4 state.
 * This is not a hardware-capability declaration or an executable packet.
 * The caller supplies an effective GPU VA, never a CPU pointer. Ownership,
 * pinned pages, visibility and retirement remain the existing provider's job.
 * The current hardware path still rejects larger surfaces and depth. */
struct remesa_render_target_request {
 uint32_t width,height,pitch_pixels;
 uint64_t gpu_va,allocation_bytes;
};
struct remesa_render_target_state {
 uint8_t target_data[16]; /* original four DWORDs, little endian */
 uint32_t background_data_extent;
 uint32_t raster_410,termination_extent;
 uint32_t stride_bytes,image_bytes;
};
/* Scene subset: origin0, linear BGRA8, pitch multiple32, dimensions <=2048.
 * Bounds are an intentionally restricted encoding profile, not a claim about
 * the maximum SGX surface. No unknown mode, MSAA, depth or cropped rectangle.
 * All outputs are committed only on success. Padding is part of image_bytes.
 * The 4-byte address alignment follows the observed relocation mask/shifts;
 * it does not override page alignment required by the actual mapping service. */
int remesa_render_target_encode_linear_bgra8(const struct remesa_render_target_request *,
 struct remesa_render_target_state *);
/* Xpsb_scene_info CPU small-dimension branch. Only DWORDs0..14 are written
 * by that producer. Word15 is deliberately absent: callers must not invent
 * it or submit this host record as a complete hardware cookie. option64 is
 * the explicitly selected CPU context+0x64 branch, not an architectural mode.
 * The allocation/clear extents describe what the CPU producer requests, not
 * proof that scene tables have been generated or made visible to the GPU. */
int remesa_scene_layout_encode(uint32_t width,uint32_t height,uint32_t option64,
 struct remesa_scene_layout *);
/* Original2f317 ->2ee34 background geometry contribution only (object+2c).
 * Source surface+0c/+10 are supplied as width/height, but the original caller
 * routes width into the high packed coordinate field and height into the low
 * field. This does not assign physical x/y axes. Produces three packed-word /
 * float-depth pairs, not a complete background descriptor or shader program.
 * Restricted origin0 and 1..2048 extents, finite depth0..1. The corresponding
 * PDS/surface state, object header, ownership and visibility are separate.
 * Input/output must be disjoint; errors preserve all output bytes. */
struct remesa_background_geometry_request { uint32_t width,height;float depth; };
int remesa_background_geometry_encode(const struct remesa_background_geometry_request *,
 uint8_t out[24]);
enum remesa_depth_compare {
 REMESA_DEPTH_NEVER,REMESA_DEPTH_LESS,REMESA_DEPTH_EQUAL,REMESA_DEPTH_LEQUAL,
 REMESA_DEPTH_GREATER,REMESA_DEPTH_NOTEQUAL,REMESA_DEPTH_GEQUAL,REMESA_DEPTH_ALWAYS
};
#define REMESA_DEPTH_CONTROL_MASK UINT32_C(0x01d00000)
/* Only the depth contribution to an ISP word, as built by psb_dri0x2bfdf.
 * Does not emit its stencil/shader/reserved bits or supply a Z attachment.
 * GPU depth and the existing Gallium depth admission remain unsupported. */
int remesa_depth_control_encode(bool depth_bits_present,bool test,bool write,
 enum remesa_depth_compare,uint32_t *);
/* Actual state DATA prefix from original2bfdf ->28712 ->287ff, not a
 * complete TA job or PDS program. Restricted triangle, no stencil/offset,
 * alpha/blend/kill/depth-export, original context+13c84/+13c80 both zero;
 * mask is full RGBA or zero only. Those context fields remain unnamed.
 * The CPU producer's other two ISP words are omitted because their presence
 * bits are clear. This is a word0-only state update, NOT a self-contained
 * initial state: other ISP/MTE/PDS groups are not reset or supplied here.
 * Output is LE {presence=1, ISP word0}, exactly8bytes.
 * LESS/ALWAYS and disabled-test cases are independently replayed. It is not
 * safe to patch an arbitrary carrier with this prefix: state-upload PDS,
 * state-object relocation, index ordering and job ownership are still needed.
 * No Gallium capability or physical depth admission is enabled.
 * Errors preserve the output. request/out may overlap; request is snapshotted.
 */
struct remesa_isp_opaque_triangle_request {
 bool depth_bits_present,depth_test,depth_write;
 enum remesa_depth_compare compare;
 uint32_t rgba_mask;
};
int remesa_isp_opaque_triangle_data_encode(
 const struct remesa_isp_opaque_triangle_request *,uint8_t out[8]);
/* Original3b73b8byte TA reference for the287ff state-upload constructor,
 * whose PDS DATA bank is12DWORDs. state_data_dwords instead counts the
 * separately committed ISP/MTE source DATA, NOT PDS CODE. bit13 names only
 * that original source-header predicate; its architectural meaning is unknown.
 * VA is the resolved BO+offset, encoded by the independently replayed
 * original relocation (mask0fffffff,right4,left0,background40000000/60000000).
 * Caller must separately validate ownership, full object extents and relocation
 * lifetime. This emits a command reference only, no PDS program or complete
 * scene/job; physical depth/multi-draw admission remains unchanged.
 */
int remesa_ta_state_reference_encode(uint64_t effective_pds_gpu_va,
 uint32_t state_data_dwords,bool state_header_bit13,uint8_t out[8]);
/* Distinct original3b606 input/parameter-state reference, called by29385.
 * PDS DATA length is fixed12DWORDs. coefficient_dwords counts committed
 * source words INCLUDING its prefix, not PDS CODE/USE bytes. Bounds2..128
 * retain the observed source profile. This command is not interchangeable
 * with the3b73b ISP/MTE state reference or an FS-secondary binding.
 * Ownership, full PDS/USE extents and execution qualification remain caller
 * obligations; output is unchanged on failure. No GPU gate is changed. */
int remesa_ta_parameter_reference_encode(uint64_t effective_pds_gpu_va,
 uint32_t coefficient_dwords,uint8_t out[8]);
/* Original ISP producer2bfdf, restricted opaque/unblended/no-kill branch:
 * state+69/+38/+2c, shader+20 bit2/+11dc and context+13c84 must all be zero.
 * mask is RGBA (R bit0), while the original intermediate packs R at bit3.
 * Returns ONLY bits0x02200000 of the first ISP word. This CPU branch does not
 * prove physical color preservation, nor implement masked fragment exports.
 * Unknown producer branches must use a different encoder, never this one. */
int remesa_isp_opaque_color_mask_encode(uint32_t rgba_mask,uint32_t *);
/* CPU ISP routing in psb_dri2c03b..2c22d. These are producer predicates,
 * not a declaration of physical blend, alpha, discard or fragment-depth
 * support. Channel flags map to the public Mesa7.4 Color state; depth-written
 * maps to OutputsWritten bitFRAG_RESULT_DEPR and uses-kill to UsesKill.
 * context_13c84 is deliberately unnamed beyond its original field: its
 * semantic/architectural meaning has not been demonstrated. A caller must
 * supply actual program metadata, never infer these fields from color masks.
 * Returns only bits0x0e200000 of ISP word0; other fields are not generated. */
struct remesa_isp_color_control_request {
 uint32_t rgba_mask;
 bool blend_enabled,color_logic_op_enabled,alpha_test_enabled;
 bool fragment_depth_written,fragment_uses_kill,context_13c84;
};
int remesa_isp_color_control_encode(const struct remesa_isp_color_control_request *,
 uint32_t *);
/* Names from the original CPU surface-state string selector, not inferred
 * meanings of GPU bits. CLEAR does not mean the backing has been CPU-filled. */
enum remesa_depth_surface_status {
 REMESA_DEPTH_SURFACE_UNDEFINED,REMESA_DEPTH_SURFACE_CLEAR,REMESA_DEPTH_SURFACE_DEFINED
};
struct remesa_depth_surface_request {
 uint32_t width,height,pitch_pixels,cpp;
 enum remesa_depth_surface_status status;
 uint64_t gpu_va,allocation_bytes;
};
struct remesa_depth_surface_state {
 uint32_t control_480,minimum_backing_bytes,address_count;
 struct {uint32_t reg,address_mask,word;} addresses[5];
};
/* CPU raster-prefix construction only: original surface ctor/setter and
 * psb_dri0x2ab70. NULL request selects the original no-depth descriptor.
 * Non-NULL requires cpp2/4, pitch multiple32, 16-byte aligned effective VA,
 * and backing for pitch * align32(height) texels. Relocations have zero
 * offset/background/shifts; only masked contributions are emitted here.
 * No clear payload, texel ordering, complete packet or GPU Z support is
 * implied. The five contributions remain separate; never treat them as
 * five independent backing allocations. Outputs are transactional. */
int remesa_depth_surface_encode(const struct remesa_depth_surface_request *,
 struct remesa_depth_surface_state *);
/* CPU clear-draw payload recovered from psb_dri ELF529cb and the depth-only
 * state setter51c9d. Four ordered window-space corners, each vec4 POSITION
 * followed by vec4 COLOR, serialized as IEEE float32 little endian. Color is
 * not written by a depth-only operation; its input must nevertheless be valid.
 * This is vertex data, NOT shader/PDS CODE or a packed depth image. The depth
 * contribution uses ALWAYS with writes enabled; other ISP bits are absent.
 * No live surface state is changed here. A consumer must establish color-write
 * disable, primitive assembly, depth backing, visibility and retirement before
 * using it. General Gallium/GPU depth admission remains unchanged. */
struct remesa_depth_clear_request {
 uint32_t width,height,x0,y0,x1,y1;
 float depth,color[4];
};
struct remesa_depth_clear_geometry {
 uint8_t vertices[128];
 /* Two triangles for the original ordered-corner fan, uint16 little endian.
  * Original CPU helper27eb2 independently confirms this assembly. */
 uint8_t indices[12];
 /* Color routing is separate: it depends on the selected fragment program
  * and must not be inferred from this geometry or a zero color-write mask. */
 uint32_t depth_control;
};
int remesa_depth_clear_geometry_encode(const struct remesa_depth_clear_request *,
 struct remesa_depth_clear_geometry *);
/* Original finalization2a39a->2a3c8. Inputs name the observed CPU predicates,
 * not guessed hardware modes. Operates only on an already encoded depth
 * surface contribution. No packet emission, state transition or admission. */
int remesa_depth_surface_finalize(const struct remesa_depth_surface_state *,
 bool caller_flag,bool context_depth_present,struct remesa_depth_surface_state *);
/* Bounded CPU Z attachment contribution from original2ab70: eight LE
 * register/value pairs, in original relative order:480,cb0,484,488,48c,490,
 *4bc,4b8. These bytes are CPU command DATA, never direct MMIO writes or a
 * complete raster prefix. The five split-address contributions stay distinct.
 * color/depth dimensions must agree for the scene Z-clear profile. The clear
 * values come from current context state, not a CPU fill or proof of content.
 * caller_finalize_flag is the observed2a39a predicate; it is not renamed as
 * a demonstrated hardware preserve/load operation. Status DEFINED requires
 * external evidence; owned fresh depth storage must start UNDEFINED.
 */
#define REMESA_DEPTH_PREFIX_BYTES 64u
struct remesa_depth_prefix_request {
 struct remesa_depth_surface_request depth;
 uint32_t color_width,color_height,clear_stencil;
 float clear_depth;
 bool caller_finalize_flag;
};
/* Source VA must come from the live resource owner's snapshot, not a guessed
 * surface address. This numeric codec validates ranges, not ownership or TLB
 * coverage. Request/output must be disjoint; error leaves output unchanged. */
int remesa_depth_prefix_encode(const struct remesa_depth_prefix_request *,
 uint8_t out[REMESA_DEPTH_PREFIX_BYTES]);
/* Complete original2ab70 CPU raster-control prefix,26 LE register/value pairs.
 * This is command DATA, never direct MMIO or an admitted native job. Numeric
 * object references do not authenticate ownership, CODE or requester coverage.
 * Restricted background RAST window30000000 and event PDS window20000000;
 * original descriptor field0c=16 is the only event metadata profile admitted.
 * The architectural meaning of field0c and fixed register controls is not
 * inferred. Explicit caller finalization reuses the depth-prefix predicate
 * only with has_depth=true; it does not mark the resource initialized/DEFINED.
 * Unknown event profiles and finalization without depth reject.
 * No historical program/packet bytes are embedded. */
#define REMESA_RASTER_CONTROL_BYTES 208u
struct remesa_raster_object_reference {
 uint64_t bo_gpu_va,bo_bytes,byte_offset,object_bytes;
};
struct remesa_raster_control_request {
 struct remesa_render_target_request color;
 bool has_depth;
 struct remesa_depth_prefix_request depth;
 struct remesa_raster_object_reference background,event;
 uint32_t event_field0c;
};
/* background range covers the original68-byte object. event range is supplied
 * explicitly by its producer; it is not inferred as the complete fetch extent
 * from field0c. Both references must be16-byte aligned and in bounds. Caller
 * separately verifies all executable input images. Transactional, disjoint. */
int remesa_raster_control_encode(const struct remesa_raster_control_request *,
 uint8_t out[REMESA_RASTER_CONTROL_BYTES]);
/* Original2b258 TA control constructor: seven LE register/value pairs.
 * Width/height arguments are unused by that producer. The source is the
 * effective TA command-stream address, not a PDS entry point or CPU pointer.
 * Uses an owned-range-shaped numeric reference,4-byte alignment; ownership,
 * stream validity and lifetime require separate native qualification. */
#define REMESA_TA_CONTROL_BYTES 56u
int remesa_ta_control_encode(const struct remesa_raster_object_reference *,
 uint8_t out[REMESA_TA_CONTROL_BYTES]);
#endif
