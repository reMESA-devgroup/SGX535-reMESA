/* SPDX-License-Identifier: MIT */
#ifndef REMESA_STREAM_UAPI_H
#define REMESA_STREAM_UAPI_H
#include <linux/types.h>
#define REMESA_STREAM_VERSION 5u
/* Private ABI5: FAST OPEN8 performs NO cryptographic hashing or checkpoints.
 * All hash arrays are zero, proof111 excludes full COLOR+Z readback bit16.
 * Fixed input bytes are validated against immutable canonical images; typed
 * source holes, PTEs, bounds, holds, visible gate, fences and retirement remain.
 * LAB OPEN1 and legacy OPEN7 retain their full-hash policies. ABI4 rejected. */
#define REMESA_STREAM_ROLES 11u
#define REMESA_STREAM_BATCHES 64u
#define REMESA_STREAM_HISTORY 64u
#define REMESA_STREAM_OPEN 1u
#define REMESA_STREAM_OPEN_FAST_BLAKE3 7u
#define REMESA_STREAM_OPEN_FAST_NOHASH 8u
#define REMESA_STREAM_PROOF_NOHASH 0x6fu
#define REMESA_STREAM_OUTPUT_SHA256 1u
#define REMESA_STREAM_OUTPUT_BLAKE3 2u
#define REMESA_STREAM_BEGIN 2u
#define REMESA_STREAM_STAGE 3u
#define REMESA_STREAM_FINISH 4u
#define REMESA_STREAM_ACK 5u
#define REMESA_STREAM_CLOSE 6u
/* All evidence comes from trusted kernel observers, never request flags. */
#define REMESA_STREAM_PROOF_ALL 0x7fu
/* Algorithm marker, NOT an additional hardware proof. All lower7 proofs remain. */
#define REMESA_STREAM_PROOF_BLAKE3 0x80u
static inline __u32 remesa_stream_proof_for_open(__u32 action){return action==REMESA_STREAM_OPEN_FAST_NOHASH?REMESA_STREAM_PROOF_NOHASH:REMESA_STREAM_PROOF_ALL|(action==REMESA_STREAM_OPEN_FAST_BLAKE3?REMESA_STREAM_PROOF_BLAKE3:0u);}
/* identity121, submitted, completed, retired, fullreadback, buffers, holdssettled */
struct remesa_stream_binding { __u32 id, reserved; __u64 va, bytes; };
struct remesa_stream_batch {
 /* normal_variant:1=normal attribute/stride32/source160;0=state normal/stride16/source176. */
 __u32 normal_variant, vertex_count, index_count, vertex_stride;
 __u64 vertex_offset, vertex_bytes, index_offset, index_bytes;
 __u64 source_offset, source_capacity;
 __u32 source_effective_dwords, source_dma_dwords;
 __u64 image_generation;
};
struct remesa_stream_receipt {
 __u64 session, sequence, operation, generation;
 __u32 proof, phase, ledger, error;
 unsigned char color_hash[32], depth_hash[32];
 unsigned char previous_sha256[32], receipt_sha256[32];
 unsigned char source_sha256[32], images_sha256[32];
};
/* OPEN8 has no cryptographic transcript or readback digests. */
static inline __u32 remesa_stream_receipt_hashes_zero(const struct remesa_stream_receipt *r) {
 unsigned i;for(i=0;i<32;i++)if(r->color_hash[i]||r->depth_hash[i]||r->previous_sha256[i]||r->receipt_sha256[i]||r->source_sha256[i]||r->images_sha256[i])return 0;return 1;
}
/* No userspace pointers, command stream, MMIO register, completion injection,
 * or reset operation. Session/profile digest are authorization bindings, not proof. */
struct remesa_stream_request {
 __u32 version, action;
 __u64 context, session, sequence;
 __u32 width, height;
 unsigned char profile_sha256[32];
 struct remesa_stream_binding bindings[REMESA_STREAM_ROLES];
 struct remesa_stream_batch batch;
 struct remesa_stream_receipt receipt;
};
#ifdef __KERNEL__
#include <drm/drm.h>
#define DRM_REMESA_STREAM 5
#define DRM_IOCTL_REMESA_STREAM DRM_IOWR(DRM_COMMAND_BASE+DRM_REMESA_STREAM,struct remesa_stream_request)
#endif
#endif
