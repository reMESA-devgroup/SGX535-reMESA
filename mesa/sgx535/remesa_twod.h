/* SPDX-License-Identifier: MIT */
#ifndef REMESA_TWOD_H
#define REMESA_TWOD_H
#include <stdint.h>
#define REMESA_TWOD_ARGB8888 1u
#define REMESA_TWOD_COPY_BYTES 40u
struct remesa_twod_surface {
 uint64_t allocation_id,gpu_address,bytes;
 uint32_t width,height,stride,format;
};
struct remesa_twod_copy {
 struct remesa_twod_surface source,destination;
 uint32_t request_base,source_x,source_y,destination_x,destination_y;
 uint32_t width,height,reserved;
};
struct remesa_twod_copy_plan {
 unsigned char command[REMESA_TWOD_COPY_BYTES];
 uint64_t source_id,destination_id;
 /* Inclusive/exclusive bounding spans, not a claim that row padding is read. */
 uint64_t source_first,source_end,destination_first,destination_end;
 uint32_t request_base,dwords;
};
/* Prepare original gma500 FIFO SRCCOPY only: no software pixel copies,
 * scaling, conversion, PDS, USSE or MMIO. Transactional output, no aliasing.
 * BOs must fit one aligned256MiB 2D request window. Same-allocation overlap
 * requires identical surface layout; different allocations cannot overlap VA.
 * The original eight-pixel erratum is rejected, not bypassed by a CPU fallback.
 * A kernel backend must authenticate allocation ownership/PFNs/permissions,
 * hold buffers, serialize the shared 2D engine and request-base register,
 * publish CPU writes and prove completion/retirement before exposing pixels.
 * The packet's FENCE header is not an application completion fence/receipt.
 * Success grants no GPU execution permission or advertised acceleration.
 */
int remesa_twod_copy_prepare(const struct remesa_twod_copy *,
 struct remesa_twod_copy_plan *);
#endif
