/* SPDX-License-Identifier: MIT */
#ifndef REMESA_TA_SCENE_H
#define REMESA_TA_SCENE_H
#include <stdint.h>
#include "remesa_render_target.h"
#include "remesa_ta_scalar.h"
/* CPU fragment-binding group from original psb_dri283d8/39ea0/287ff.
 * This is state DATA, NOT PDS CODE, a complete initial state or a GPU job.
 * The verified profile is an authored constant-color FS: primary PDS DATA12,
 * CODE8, descriptor field10=0 and field14=1. The separate original39ac4
 * SCENE_ZCLEAR profile has field14=0. Both have empty secondary DATA0/CODE4,
 * field10=field14=0. Unknown descriptors return ENOTSUP.
 *
 * Original37854 relocation masks the address after shifting by4, retaining
 * only24 bits. address_window_base explicitly accounts for the omitted top
 * nibble. Both BO objects must fit that SAME 256MiB window. This host contract
 * does not prove the architectural bank/window selection: the provider must
 * separately qualify it, ownership, visibility and retirement lifetime.
 * BO VA/bytes and object offset/extent are numeric GPU ranges, not CPU pointers.
 * Fields10/14 keep their observed names; they are NOT CODE/DATA lengths.
 */
struct remesa_ta_fragment_pds_reference {
 uint64_t bo_gpu_va,bo_bytes,byte_offset,object_bytes;
 uint32_t data_dwords,field10,field14;
};
enum remesa_ta_fragment_cpu_profile {
 REMESA_TA_FRAGMENT_AUTHORED_CONSTANT=0,
 /* Original39ac4 scalar-color producer, used by2a794 scene Z-clear. */
 REMESA_TA_FRAGMENT_SCENE_ZCLEAR=1,
 /* Authored MOV COLOR0/COLOR1 FS, original33490/370d8/25970/39ea0/287ff.
  * Primary DATA12/CODE12, field10=1/field14=0; secondary4/0/0/0.
  * CPU binding only: physical R delivery, varying format and full USE
  * finalization are not qualified. Constant draw/clear APIs reject this. */
 REMESA_TA_FRAGMENT_INPUT_COLOR_CPU=2
};
struct remesa_ta_fragment_binding_request {
 uint64_t address_window_base;
 struct remesa_ta_fragment_pds_reference primary,secondary;
 enum remesa_ta_fragment_cpu_profile profile;
};
/* Outputs LE {header40, secondary_reference, control, primary_reference},16B.
 * This updates only group40. A first scene additionally needs its initial
 * ISP/MTE/viewport state; drawing after this prefix alone is unsupported.
 * Positive errno; failures leave out untouched. Request/output may alias.
 * This serializer does not admit AF semantics or enable any execution gate.
 */
int remesa_ta_fragment_binding_encode(
 const struct remesa_ta_fragment_binding_request *,uint8_t out[16]);
/* Restricted original2f317 ->2ee34 background object: two PDS references,
 * producer control/scalars and three packed geometry/depth pairs,68bytes.
 * Background primary is64bytes with DATA12/control10=1; empty secondary is
 *4bytes with DATA0/control10=0. field14 is not consumed by the CPU constructor;
 * this canonical host API requires it zero, without assigning reserved-bit
 * semantics. References must be disjoint, aligned32 and share the specified
 *256MiB address window. No ownership, USE body or physical interpretation is
 * implied. This is a descriptor, not executable shader CODE. */
struct remesa_background_object_request {
 uint64_t address_window_base;
 struct remesa_ta_fragment_pds_reference primary,secondary;
 uint32_t width,height;
 float depth;
};
/* Transactional/disjoint request and output. Unknown metadata/profiles reject.
 * Remaining object bytes after the original68byte GPU view are not emitted. */
int remesa_background_object_encode(const struct remesa_background_object_request *,
 uint8_t out[68]);
/* Original window-framebuffer branch2be59 and public Mesa matrix viewport.
 * CPU group100 DATA (header plus six float32 values), NOT clip vertices or
 * Z texel layout. Only a named window framebuffer (Mesa Name=0), no FBO
 * orientation/renderbuffer branch. near/far are independently clamped public
 * values already: finite[0,1], multiples1/65536 for exact intermediate bounds.
 * depth_bits16/24 determine the public DepthMaxF normalization. This does not
 * select or clear a Z attachment, or declare physical depth support.
 */
enum remesa_ta_viewport_cpu_path {
 REMESA_TA_VIEWPORT_MATRIX_WINDOW=0,
 /* Original init2b493/constructor270c5 plus disabled2be59 predicate. This
  * names the CPU path ONLY; it does not assert physical transform bypass. */
 REMESA_TA_VIEWPORT_INITIAL_ZERO=1
};
struct remesa_ta_window_viewport_request {
 uint32_t framebuffer_width,framebuffer_height,x,y,width,height,depth_bits;
 float near_z,far_z;
 enum remesa_ta_viewport_cpu_path path;
};
/* The six serialized values are Xoffset,Xscale,Yoffset,Yscale,Zoffset,Zscale.
 * Name=0 inverts Y through framebuffer_height, as the original producer does.
 * Positive errno; output unchanged on every failure; request/out may alias.
 */
int remesa_ta_window_viewport_encode(
 const struct remesa_ta_window_viewport_request *,uint8_t out[28]);
/* Restricted explicit-group constant-FS DATA, original serializer headerf41.
 * ISP word0 + fragment group40 + initial-zero group100 + default/scalarE00.
 * This is a draw delta AFTER the initial clear/state groups are established,
 * never a self-contained initial scene, command stream or submitted job.
 * Only POSITION4 layout and INITIAL_ZERO viewport are admitted; matrix window,
 * varying-color fragments and other vertex export profiles remain unsupported.
 * This explicitly retains group400; original cache filtering after54c5 uses
 * the separate52byte API below. Child encoders validate all fields before
 * the56bytes are committed.
 */
struct remesa_ta_constant_draw_state_request {
 struct remesa_isp_opaque_triangle_request isp;
 struct remesa_ta_fragment_binding_request fragment;
 struct remesa_ta_window_viewport_request viewport;
 enum remesa_ta_scalar_profile scalar_profile;
};
int remesa_ta_constant_draw_state_encode(
 const struct remesa_ta_constant_draw_state_request *,uint8_t out[56]);
/* Canonical first constant draw after the exact54c5 scene-Z-clear record
 * encoded below has been successfully published as prior CPU state. Original28fff
 * removes the identical POSITION4 group400, leaving headerb41 and52bytes.
 * The caller owns that predecessor relation; arbitrary/absent cached state
 * is unsupported. This does not establish prior GPU execution/completion.
 * Same request restrictions as the56byte explicit-group encoder. Positive
 * errno; failures preserve output and written. Request/output may alias;
 * written must be aligned and disjoint from the entire output capacity.
 */
int remesa_ta_constant_draw_after_zclear_encode(
 const struct remesa_ta_constant_draw_state_request *,
 uint8_t *out,size_t capacity,size_t *written);
/* Exact original2a794 scene-Z-clear state DATA contract54c5 (44bytes).
 * Distinct from generic opaque/color-write-off ISP state. The fixed ISP
 * values are the original constructor contract, not inferred generic bit
 * semantics. Requires SCENE_ZCLEAR binding profile (original39ac4 metadata).
 * Does NOT supply geometry, Z attachment, initial clear status, indexed TA
 * commands, or completion. No physical depth capability/gate is enabled.
 */
int remesa_ta_scene_zclear_state_encode(
 const struct remesa_ta_fragment_binding_request *,uint8_t out[44]);
#endif
