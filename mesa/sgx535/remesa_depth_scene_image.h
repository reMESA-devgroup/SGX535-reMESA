/* SPDX-License-Identifier: MIT */
#ifndef REMESA_DEPTH_SCENE_IMAGE_H
#define REMESA_DEPTH_SCENE_IMAGE_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "remesa_render_target.h"
#include "remesa_program_registers.h"
struct remesa_shader_image;
#define REMESA_DEPTH_SCENE_IMAGE_BYTES 4096u
/* One page of independently linked code/data/TA, not a native submit packet.
 * The caller must own the allocation, Z/color/auxiliary backing, bank leases
 * and requester coverage. Numeric VAs here do not authenticate any of them.
 * The three immutable shader images are green, magenta and yellow, with the
 * existing generated POSITION4 VS16/constantFS8 profile. No code templates.
 * Geometry is explicitly window-space: no CPU vertex transform is performed.
 * Dimensions describe CPU encodings, not qualified physical display modes. */
/* CPU producer choice, not a claim that either mode is physically admitted.
 * Zero preserves the existing copy/upload profile. The programmable profile
 * selects the independently verified PDS task words and POSITION4 output-byte
 * descriptor units together; mixing the two contracts is not supported. */
enum remesa_depth_vertex_profile {
 REMESA_DEPTH_VERTEX_COPY = 0,
 REMESA_DEPTH_VERTEX_COMPILED_POSITION4 = 1
};
struct remesa_depth_scene_image_request {
 uint64_t gpu_va,allocation_bytes;
 const struct remesa_shader_image *programs[3];
 uint32_t width,height,vertex_bank,pixel_bank,state_bank;
 float triangle[3][2],draw_depth[3],clear_depth;
 enum remesa_depth_compare compare;
 bool depth_write;
 enum remesa_depth_vertex_profile vertex_profile;
};
struct remesa_depth_scene_image {
 uint8_t bytes[REMESA_DEPTH_SCENE_IMAGE_BYTES];
 /* Unknown PDS DATA and host padding retain mask0, including relocated cells
  * for which the structural producer has no complete known-bit evidence. */
 uint8_t known[REMESA_DEPTH_SCENE_IMAGE_BYTES];
 size_t vertex_code,fragment_code[3],vertices[4],indices[4];
 size_t vertex_pds[4],fragment_pds[3],secondary_pds;
 size_t state_data[5],state_bytes[5],state_use[5],state_pds[5];
 size_t ta_offset,ta_bytes,used_bytes;
 enum remesa_depth_vertex_profile vertex_profile;
 /* VS, FS and state-upload bank plans, derived with the same entry planner
  * used for DATA fixups. Values are native preparation, not bank leases or
  * MMIO writes. All FS/state entries must agree on their selected base.
  * VS/state may share a bank only with identical register words. */
 struct remesa_use_entry_plan use_entries[3];
};
/* Transactional, disjoint output; input payload aliases are rejected. Requires
 * a page-aligned32bit span; unsupported shader/control/extent returns errno.
 * Fully generates the source bytes and references anew for the supplied VA.
 * No device, memory mapping, job holds, MMIO or admission is performed. */
int remesa_depth_scene_image_encode(const struct remesa_depth_scene_image_request *,
 struct remesa_depth_scene_image *);
#endif
