/* SPDX-License-Identifier: MIT */
#ifndef REMESA_TA_INDEX_H
#define REMESA_TA_INDEX_H
#include <stdint.h>
/* Original psb_dri3b890 CPU serialization, restricted triangle list/u16
 * indices/word2=0, original40355 single-DMA producer only.
 * Thus PDS DATA=12DWORDs, A.field10=B.field38 in1..16, object>=64bytes
 * (48DATA+16CODE). The state-upload PDS from287ff is a DIFFERENT object;
 * never pass that descriptor here, even if its DATA bank is also48bytes. No job, topology admission or GPU capability is enabled.
 * Addresses are resolved GPU BO bases, never host pointers. The provider must
 * independently prove ownership, pinning, visibility and retirement lifetime.
 * These bounds describe byte extents only; they do not authenticate a BO.
 *
 * DescriptorA.field10 comes from render+5c4; descriptorB.field0c/38 and its
 * BO reference come from render+3f4. field0c counts PDS DATA DWORDs; the
 * architectural meanings of field10/38 remain UNKNOWN, so retain raw names.
 * They are NOT interchangeable, and neither is an instruction count.
 * This raw CPU ABI encoder cannot qualify arbitrary descriptors for execution.
 */
struct remesa_ta_index_request {
 uint64_t index_bo_gpu_va,index_bo_bytes,index_byte_offset;
 uint64_t pds_bo_gpu_va,pds_bo_bytes,pds_byte_offset,pds_object_bytes;
 uint32_t first_index,index_count,pds_data_dwords;
 uint32_t descriptor_a_field10,descriptor_b_field38;
};
/* Emits LE20bytes including the two resolved references. Does not write either
 * source BO. The referenced index/PDS byte ranges must not overlap.
 * No truncation/wrapping is accepted. On error output is untouched;
 * request/output may overlap because the request is snapshotted first.
 * Returns positive errno, matching the existing encoder APIs.
 * Caller supplies valid C objects; address-wrap checks do not authenticate
 * host mappings.
 * Restricted count <=65535 is a software encoding profile, not a hardware max.
 */
int remesa_ta_index_triangles_encode(const struct remesa_ta_index_request *,
 uint8_t out[20]);
/* Original2a57b ->287ff source DATA: header bit13 plus rounded scene extent.
 * This final-state source is distinct from the ISP word0-update source.
 * Width/height1..2048 are this software profile, not demonstrated GPU limits.
 * Emission is CPU-contract evidence, not architectural termination semantics.
 * Returns positive errno; output untouched on failure.
 */
int remesa_ta_scene_close_data_encode(uint32_t width,uint32_t height,
 uint8_t out[8]);
struct remesa_ta_scene_close_request {
 uint64_t pds_bo_gpu_va,pds_bo_bytes,pds_byte_offset,pds_object_bytes;
};
/* Original2a39a CPU close stream: TA reference8 (source DATA2, headerbit13)
 * followed by the original invariant emission literal0xc0000000, exactly4B.
 * Original tail allocator reserves8 but commits4: NO padding DWORD is emitted.
 * Input must refer to the separately constructed final-state-upload PDS60,
 * never the primary vertex PDS64 used by the index command above. Provider
 * must prove its source DATA header2000/extent, auxiliary USE and ownership,
 * pinning, visibility/lifetime. This byte-span API does not authenticate them.
 * Existing pending indices must already have been closed and earlier state/
 * draw commands serialized, as original27f03 precedes this close. No initial
 * scene state, final-state PDS, submission job or GPU admission is generated.
 * This is not a claim that C0 itself causes hardware completion.
 * Host request/output must be valid objects; wrapping addresses are rejected;
 * overlap is supported by snapshot and all errors preserve output.
 */
int remesa_ta_scene_close_encode(const struct remesa_ta_scene_close_request *,
 uint8_t out[12]);
/* Main programmable-VS CPU descriptor profile: POSITION float4 only, no
 * extra outputs/clip planes/fog/point-size; source stride4 or8 DWORDs.
 * Original29dd9 stores the same source descriptor as A and the compiled PDS
 * descriptor as B. Main4252d copies ctx14120 (output BYTES,16 here) into B+38,
 * unlike copy40355 which stores source DWORD count there. Original2daad
 * computes the16-byte output format for this restricted profile. Existing
 * copy API/equality checks are unchanged. CPU encoding only, no execution
 * qualification or authentication of the shader/BO payloads. */
int remesa_ta_index_compiled_position_triangles_encode(const struct remesa_ta_index_request *,
 uint8_t out[20]);
/* Explicit original programmable-VS POSITION4+COLOR0 float4 profile, paired
 * with a COLOR0-input fragment shader. Original whole2daad with valid authored
 * VS/FS metadata selects output bytes32 at2dc33/2dc52 and writes ctx14120 at
 *2e18c; whole4252d copies that value into B+38 at428a2/428ab. Two distinct
 * source inputs give A.field10=8, shared input gives4. The selected separate
 * POSITION/NORMAL/LIGHT input layout additionally admits stride12 DWORDs;
 * its POSITION4+COLOR4 output remains32 bytes. No extra output is introduced.
 * This profile does not admit COLOR1, extra outputs, clip/fog/point-size or
 * arbitrary source layouts. It reuses all index/PDS extent and transactional
 * checks above. The raw request cannot authenticate the paired shader or its
 * PDS task/export configuration. CPU serialization only; no execution/default
 * admission or physical varying/interpolation qualification is implied.
 * Existing copy and POSITION-only APIs retain their original restrictions. */
int remesa_ta_index_compiled_position_color_triangles_encode(const struct remesa_ta_index_request *,
 uint8_t out[20]);
#endif
