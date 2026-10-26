/* SPDX-License-Identifier: MIT */
#ifndef REMESA_STREAM_GRAMMAR_H
#define REMESA_STREAM_GRAMMAR_H
#include "stream_uapi.h"
/* Kernel and host share these integer-only bounds; no floating point or
 * arbitrary writable spans. Only VS MVP[4..7] and LIGHT[8/9].xyz vary. */
int remesa_stream_mask_sources(const struct remesa_stream_batch *,unsigned,
 unsigned char *,__u64,__u64);
int remesa_stream_validate_geometry(const struct remesa_stream_batch *,unsigned,
 const unsigned char *,__u64);
#endif
