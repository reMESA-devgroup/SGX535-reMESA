/* SPDX-License-Identifier: MIT */
#ifndef REMESA_PDS_PREFETCH_H
#define REMESA_PDS_PREFETCH_H
#include <stddef.h>
#include <stdint.h>
/* Selected EMGD primary CPU-constructor profile, not execution admission.
 * Input task/control/state words must come from separately validated bindings.
 * Numeric construction authenticates no GPU address or resource allocation. */
#define REMESA_PDS_PREFETCH_ENTRIES 16u
#define REMESA_PDS_PREFETCH_SAMPLES 16u
#define REMESA_PDS_PREFETCH_NO_SAMPLE UINT32_MAX
#define REMESA_PDS_PREFETCH_MAX_BYTES (128u*4u+34u*4u)
struct remesa_pds_prefetch_entry {uint32_t control,sample;};
struct remesa_pds_prefetch_request {
 uint32_t task[3],entry_count,sample_count,reserved;
 struct remesa_pds_prefetch_entry entries[REMESA_PDS_PREFETCH_ENTRIES];
 uint32_t samples[REMESA_PDS_PREFETCH_SAMPLES][3];
};
struct remesa_pds_prefetch_layout {size_t data_bytes,code_bytes,total_bytes;};
/* Serializes relative to a caller-provided aligned GPU entry; host output need
 * not have the GPU address. Original writer aligns entry to16 and DATA to16.
 * This is not a GPU fetch-size claim. Unknown DATA is zero/mask0, never CODE
 * filler. Bytes beyond total_bytes remain untouched. All outputs transactional
 * and disjoint from each other/input; no mappings, task validation or upload.
 * Adjacent/nonadjacent eligible R controls retain original merge semantics. */
int remesa_pds_prefetch_encode(const struct remesa_pds_prefetch_request *,
 uint8_t *bytes,uint8_t *known_mask,size_t capacity,
 struct remesa_pds_prefetch_layout *);
#endif
