/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_STREAM_LIFECYCLE_H
#define REMESA_STREAM_LIFECYCLE_H
#include "stream_uapi.h"
enum remesa_stream_state { STREAM_FRESH, STREAM_IDLE, STREAM_STAGING,
 STREAM_ISSUED, STREAM_WAIT_ACK, STREAM_QUARANTINED, STREAM_CLOSED };
struct remesa_stream;
/* Serialized by caller. Every callback is kernel trusted, not userspace supplied.
 * claim excludes legacy/other source owners and authenticates SGX535 rev121.
 * freeze must acquire provider/registry holds BEFORE validating images/PTEs,
 * immutable CODE, mapping cookies, bounds and zero initialized DMA tails.
 * execute uses independent per-operation source observer, genuine completion,
 * retirement and full COLOR/Z readback. settle ACKs provider, verifies all holds
 * and producers settled. Any error after freeze permanently quarantines owner.
 * seal hashes canonical receipt with previous receipt digest; no fake proof.
 */
struct remesa_stream_ops {
 int (*claim)(void *,const struct remesa_stream_request *);
 int (*freeze)(void *,const struct remesa_stream *);
 int (*execute)(void *,struct remesa_stream_receipt *);
 int (*settle)(void *);
 int (*seal)(void *,struct remesa_stream_receipt *);
 void (*quarantine)(void *);
 int (*close)(void *);
};
struct remesa_stream {
 enum remesa_stream_state state;
 struct remesa_stream_request profile;
 struct remesa_stream_batch batches[REMESA_STREAM_BATCHES];
 struct remesa_stream_receipt history[REMESA_STREAM_HISTORY];
 unsigned count, history_count;
 __u64 next_sequence, next_generation, completed, acknowledged;
 struct remesa_stream_ops ops;
 void *backend;
};
int remesa_stream_open(struct remesa_stream *,const struct remesa_stream_request *,
 const struct remesa_stream_ops *,void *);
int remesa_stream_action(struct remesa_stream *,struct remesa_stream_request *);
#endif
