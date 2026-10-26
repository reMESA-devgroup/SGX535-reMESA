/* SPDX-License-Identifier: MIT
 * Original immutable host storage for candidate shader records. Neither these
 * limits nor this format identify SGX instruction width or GPU alignment.
 */
#ifndef REMESA_SHADER_STORAGE_H
#define REMESA_SHADER_STORAGE_H
#include "remesa_shader_ir.h"

#define REMESA_SHADER_STORAGE_MAX_BYTES 4096u
#define REMESA_SHADER_STORAGE_MAX_ALIGNMENT 4096u
#define REMESA_SHADER_STORAGE_CPU_RECORD_BYTES 8u

/* Bounded host identity storage, unrelated to a GPU resource/register limit.
 * Tiny cookies remain allocated until the endpoint closes successfully, so
 * freeing a shader payload cannot make a stale token identify a new shader.
 * Serialized by the endpoint; cookies are never dereferenced by callers and
 * have no validity after endpoint close. No executable state is represented. */
#define REMESA_SHADER_COOKIE_LIMIT 4096u
struct remesa_shader_cookie;
struct remesa_shader_cookie_pool {
 struct remesa_shader_cookie *head;
 unsigned count;
};
int remesa_shader_cookie_create(struct remesa_shader_cookie_pool *,void **);
int remesa_shader_cookie_output_check(const struct remesa_shader_cookie_pool *,
                                     const void *,size_t);
void remesa_shader_cookie_pool_finish(struct remesa_shader_cookie_pool *);

enum remesa_shader_storage_format {
 REMESA_SHADER_STORAGE_HOST_CPU_RECORDS = 1,
 /* Original host DATA serialization; four-byte words, not instructions. */
 REMESA_SHADER_STORAGE_HOST_PDS_DATA = 2,
 /* Original CPU descriptor-class CODE fragments and their known-bit masks.
  * Missing initial/final instructions prohibit executable qualification. */
 REMESA_SHADER_STORAGE_HOST_PDS_STRUCTURE = 3
};
enum remesa_shader_storage_status {
 REMESA_SHADER_STORAGE_CANDIDATE = 1
};
struct remesa_shader_storage;
struct remesa_shader_storage_request {
 enum remesa_ir_stage stage;
 enum remesa_shader_storage_format format;
 const void *bytes;
 size_t byte_length;
 size_t byte_alignment;
};
struct remesa_shader_storage_view {
 enum remesa_ir_stage stage;
 enum remesa_shader_storage_format format;
 enum remesa_shader_storage_status status;
 const void *bytes;
 size_t byte_length;
 size_t byte_alignment;
};

/* Copies caller bytes into immutable, aligned host storage. HOST_PDS_DATA uses
 * four-byte CPU data cells and conveys no launch qualification. The eight-byte
 * CPU-record framing is this candidate's serializer convention, not evidence
 * of hardware instruction framing. No address, GEM object, pin or GPU upload
 * is created. Output must be an empty reference, so an existing allocation
 * cannot be overwritten or leaked. The output reference slot must be disjoint
 * from the request and source bytes; pointer-range wrap is rejected before
 * reading the slot or copying bytes. Output is unchanged on all errors. */
int remesa_shader_storage_create(const struct remesa_shader_storage_request *,
                                struct remesa_shader_storage **);
/* Mesa-style reference replacement: increments source before releasing the
 * previous destination. NULL releases; identical references are unchanged.
 * Callers serialize access. UINT32_MAX references are rejected, never wrapped.
 * Destination must hold an owned valid reference or NULL; it cannot point
 * into the source/old object or its backing allocation. Arbitrary byte buffers
 * and stale pointers are not reference slots. Destination is unchanged on error. */
int remesa_shader_storage_reference(struct remesa_shader_storage **,
                                   struct remesa_shader_storage *);
int remesa_shader_storage_validate(const struct remesa_shader_storage *);
/* Borrowed const bytes stay valid until the caller releases its reference.
 * Inspection/range output cannot alias the object or its backing allocation
 * and is unchanged on error. */
int remesa_shader_storage_inspect(const struct remesa_shader_storage *,
                                 struct remesa_shader_storage_view *);
int remesa_shader_storage_range(const struct remesa_shader_storage *,
                               size_t offset,size_t length,const void **);
/* Candidate CPU records are NEVER launchable. No caller-controlled evidence
 * flag can promote this host allocation into an executable GPU image. */
int remesa_shader_storage_validate_execution(const struct remesa_shader_storage *);
#endif
