/* SPDX-License-Identifier: MIT */
#ifndef REMESA_DRAW_H
#define REMESA_DRAW_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "pipe/p_state.h"
#include "remesa/draw_packet.h"
/* Supported host position fetch widths. Missing FLOAT2 z/w are 0/1;
 * FLOAT3 w is 1. Unsupported formats return zero; no GPU ISA is generated. */
unsigned remesa_position_format_bytes(enum pipe_format);
/* Owned pre-VS triangle stream for the complete gears POSITION/NORMAL profile.
 * Caller holds immutable source leases. Attribute offsets include VB + element
 * offsets; stride zero broadcasts. FLOAT2/3 position defaults are z=0,w=1;
 * FLOAT3 normal defaults w=1. All supplied components are copied bit for bit.
 * No matrix, normalization, lighting, clipping or viewport arithmetic runs.
 * Indexed assembly preserves primitive and provoking-vertex order, including
 * repeated references. Incomplete trailing primitives are ignored. This host
 * allocation bound is not an SGX capacity or execution qualification.
 */
#define REMESA_GEARS_GEOMETRY_MAX_VERTICES 4095u
struct remesa_gears_attribute {
 const void *data; size_t bytes;
 uint64_t offset; uint32_t stride;
 enum pipe_format format;
};
struct remesa_gears_geometry;
struct remesa_gears_geometry_view {
 const void *vertices; size_t bytes;
 uint32_t vertex_count, stride_dwords, normal_from_attribute;
};
/* Borrowed index storage begins at resource byte zero. The explicit snapshots
 * are authoritative; index union/ownership hints are never dereferenced or
 * adopted. Nonindexed draws ignore index data/bias/bounds entirely. Exactly
 * one instance and triangle lists are supported; restart is rejected.
 * Empty output required; outputs unchanged on error. */
int remesa_gears_geometry_create(const struct pipe_draw_info *,
 const struct pipe_draw_start_count_bias *,unsigned normal_from_attribute,
 const struct remesa_gears_attribute *,const struct remesa_gears_attribute *,
 const void *indices,size_t index_bytes,struct remesa_gears_geometry **);
int remesa_gears_geometry_inspect(const struct remesa_gears_geometry *,
 struct remesa_gears_geometry_view *);
void remesa_gears_geometry_destroy(struct remesa_gears_geometry *);
/* Raw attributes for the current three-float4 compiled vertex layout.
 * Copies every component bit for bit, including NaN/zero representations;
 * performs no shader, lighting, normalization, clipping or viewport work.
 * Each source has its own offset/stride (zero stride broadcasts one record).
 * Source mappings must remain immutable for the constructor call. */
#define REMESA_LINKED_VERTEX_ATTRIBUTES 3u
struct remesa_linked_attribute {
 const void *data;size_t bytes;
 uint64_t offset;uint32_t stride;
 enum pipe_format format;
};
struct remesa_linked_vertex_inputs;
struct remesa_linked_vertex_inputs_view {
 const void *vertices;const uint16_t *indices; /* owned, host-native u16 */
 size_t vertex_bytes,index_bytes;
 uint32_t vertex_count,index_count,stride_bytes,min_source_vertex,max_source_vertex;
};
/* Triangle lists, one instance, u8/u16/u32 explicit index snapshots or
 * nonindexed draws. Ignore incomplete trailing primitives; reject restart.
 * Copy the referenced min..max source span once and rebase indices, preserving
 * primitive/provoking order and repeated references. Indexed bounds are checked
 * before bias; all effective source addresses after bias. No resource/index
 * union pointer is adopted. Bounded preparation policy: 4095 source records,
 * 65535 indices; not advertised SGX or OpenGL limits. Empty output required.
 * Errors leave output untouched; output cannot overlap descriptors/payloads. */
int remesa_linked_vertex_inputs_create(const struct pipe_draw_info *,
 const struct pipe_draw_start_count_bias *,const struct remesa_linked_attribute[3],
 const void *indices,size_t index_bytes,struct remesa_linked_vertex_inputs **);
/* General linked-input preparation for TRIANGLES, TRIANGLE_STRIP and
 * TRIANGLE_FAN. Output is always a triangle list with the same first/last
 * provoking convention, winding and degenerate primitives. This is index
 * assembly only; attributes remain byte-exact before GPU vertex processing.
 * The original triangle-only constructor above is unchanged. Restart and
 * instancing stay unsupported; the expanded list is bounded to65535 indices.
 */
struct remesa_linked_topology {uint32_t input_count,index_count;};
int remesa_linked_topology_prepare(unsigned mode,uint32_t count,
 struct remesa_linked_topology *);
int remesa_linked_vertex_inputs_create_topology(const struct pipe_draw_info *,
 const struct pipe_draw_start_count_bias *,const struct remesa_linked_attribute[3],
 const void *indices,size_t index_bytes,bool first_provoking,
 struct remesa_linked_vertex_inputs **);
int remesa_linked_vertex_inputs_inspect(const struct remesa_linked_vertex_inputs *,
 struct remesa_linked_vertex_inputs_view *);
void remesa_linked_vertex_inputs_destroy(struct remesa_linked_vertex_inputs *);
/* Original shader inputs, borrowed only during the backend submit call.
 * These are synchronized VB/IB snapshots before vertex shader evaluation,
 * clipping or viewport transformation. The diagnostic window-space packet
 * is a separate preparation reference, never the input to the actual VS.
 * A backend must validate ranges and copy all consumed bytes/state into its
 * own retained storage before returning. No resource pointer or GPU address
 * is supplied here. Inactive nonindexed fields and disabled state are zero.
 * Current admission is one float position attribute, triangle lists, one
 * BGRA8 color attachment, disabled blend/depth/stencil, no sampling. */
struct remesa_shader_draw_inputs {
    const void *vertex_data, *index_data;
    size_t vertex_bytes, index_bytes, color_bytes;
    uint32_t vertex_offset, element_offset, vertex_stride, position_input_slot;
    uint32_t start, count, index_size;
    int32_t index_bias;
    enum pipe_format position_format, color_format;
    uint32_t color_width, color_height, color_stride;
    struct pipe_viewport_state viewport;
    struct pipe_scissor_state scissor;
    unsigned cull_face;
    bool front_ccw, depth_clip_near, depth_clip_far, scissor_enabled, preserve_color;
    /* Explicit FS-only screen mode, never a hardware instruction/control.
     * A screen created before laboratory configuration cannot acquire the
     * retained VS/PDS route retroactively. */
    bool retained_fragment_carrier;
};
struct remesa_draw_plan;
/* Check the original, pre-VS input contract independently of a remapped
 * diagnostic packet. Returns errno; writes plan only on success. This checks
 * borrowed host ranges and the current one-attribute triangle subset, never
 * proves pointer ownership, GPU addresses, or hardware admission. count must
 * already exclude incomplete final primitives. Zero-stride broadcasts remain
 * outside this subset. The caller must hold immutable mapping leases while
 * validating and snapshotting the identified index/vertex bytes. */
int remesa_shader_draw_inputs_validate(const struct remesa_shader_draw_inputs *,
                                      struct remesa_draw_plan *);
/* Original native preparation owns the consumed pre-VS VB/IB byte ranges.
 * The source mapping leases may end after creation. Stride, start, bias and
 * raw attribute bytes are preserved; no VS, viewport or rasterization runs.
 * This bounded host policy is not an SGX memory/layout limit. */
#define REMESA_SHADER_DRAW_SNAPSHOT_MAX_BYTES (1024u * 1024u)
struct remesa_shader_draw_snapshot;
/* Empty output required; unchanged on error. Caller holds immutable source
 * leases for the call. Snapshot lifetime is independent of the source buffers.
 * It neither owns COLOR backing nor permits native/hardware execution. */
int remesa_shader_draw_snapshot_create(const struct remesa_shader_draw_inputs *,
                                      struct remesa_shader_draw_snapshot **);
/* Borrowed const bytes stay valid until snapshot destruction. Outputs cannot
 * alias the snapshot/its backing and are unchanged on failure. */
int remesa_shader_draw_snapshot_inspect(const struct remesa_shader_draw_snapshot *,
 struct remesa_shader_draw_inputs *,struct remesa_draw_plan *);
void remesa_shader_draw_snapshot_destroy(struct remesa_shader_draw_snapshot *);
/* Resolution-independent CPU primitive preparation, not an SGX packet.
 * Triangle vertices are duplicated in primitive order after clipping; only
 * constant fragment output is admitted (no varying interpolation). */
struct remesa_prepared_triangles {
    uint32_t width, height, argb, vertex_count;
    float positions[REMESA_DRAW_MAX_INDICES][4];
};

/* CPU-only validation, not a Gallium draw callback or an SGX submission.
 * Borrowed resources and the explicit index snapshot must remain immutable
 * throughout the call. index_snapshot starts at index-resource byte zero,
 * not at draw->start. No reference is adopted, retained or released.
 * TRIANGLES ignore the final one or two vertices/indices that cannot form a
 * primitive. They do not contribute to fetches, ranges, bias or index bounds.
 * VALIDATED means position/index ranges pass this diagnostic subset only;
 * it does not establish Gallium feature support or hardware correctness. */
enum remesa_draw_status {
    REMESA_DRAW_VALIDATED, REMESA_DRAW_EMPTY,
    REMESA_DRAW_INVALID, REMESA_DRAW_UNSUPPORTED, REMESA_DRAW_NOMEM
};
/* Output unchanged on rejection or capacity exhaustion. Empty success sets
 * vertex_count=0. Hardware capabilities and submission must be admitted by a
 * separate backend, which may reject all but its historical 32x32 profile. */
enum remesa_draw_status remesa_prepare_constant_draw(
    const struct pipe_draw_info *, const struct pipe_draw_start_count_bias *,
    const struct pipe_vertex_buffer *, const struct pipe_vertex_element *,
    const void *, size_t, const void *, size_t, uint32_t,
    const struct pipe_viewport_state *, uint32_t, uint32_t,
    struct remesa_prepared_triangles *);
/* Compact a CPU-clipped constant-color triangle list into the existing semantic
 * 32x32 packet. No SGX stream is generated. Production backends must retain
 * their own geometry/profile admission. Output unchanged on failure. */
enum remesa_draw_status remesa_pack_prepared_triangles(
    const struct remesa_prepared_triangles *, struct remesa_draw_packet *);
struct remesa_draw_plan {
    uint32_t primitive_count, min_vertex, max_vertex;
    uint64_t index_first_byte, index_end_byte, vertex_end_byte;
};
enum remesa_draw_status remesa_validate_indexed_triangles(
    const struct pipe_draw_info *info,
    const struct pipe_draw_start_count_bias *draw,
    const struct pipe_vertex_buffer *vertices,
    const struct pipe_vertex_element *position,
    const void *index_snapshot, size_t snapshot_bytes,
    struct remesa_draw_plan *plan);
/* Gather referenced positions from strided/offset buffer snapshots. Indexed
 * draws accept native 8/16/32-bit indices and signed index bias. Nonindexed
 * draws never read the index union or index bias. Sparse source indices are
 * remapped by first occurrence; primitive order and duplicate references are
 * preserved. Incomplete final primitives are ignored, including their index
 * storage and vertex references. Only active positions are inspected. Output is unchanged on
 * failure. This translates host data, not shaders or SGX command encodings.
 * The backend must still enforce its independently qualified hardware subset. */
enum remesa_draw_status remesa_pack_triangle_draw(
    const struct pipe_draw_info *info,
    const struct pipe_draw_start_count_bias *draw,
    const struct pipe_vertex_buffer *vertices,
    const struct pipe_vertex_element *position,
    const void *vertex_snapshot, size_t vertex_bytes,
    const void *index_snapshot, size_t index_bytes,
    uint32_t argb, struct remesa_draw_packet *packet);
/* Optional restricted host clip-to-window preparation for an admitted identity
 * position VS. Canonical 32x32 viewport only; w=1, finite xyz within [-1,1].
 * No clipping, perspective interpolation or CPU rasterization is supplied. */
enum remesa_draw_status remesa_pack_triangle_draw_vertex(
    const struct pipe_draw_info *,const struct pipe_draw_start_count_bias *,
    const struct pipe_vertex_buffer *,const struct pipe_vertex_element *,
    const void *,size_t,const void *,size_t,uint32_t,
    const struct pipe_viewport_state *,struct remesa_draw_packet *);
#endif
