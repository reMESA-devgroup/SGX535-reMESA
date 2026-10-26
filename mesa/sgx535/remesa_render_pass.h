/* SPDX-License-Identifier: MIT */
#ifndef REMESA_RENDER_PASS_H
#define REMESA_RENDER_PASS_H
#include <stdbool.h>
#include <stdint.h>

/* Frontend bookkeeping only, not command encodings or kernel operation IDs.
 * Caller serializes access and owns backing/fences. A resource cookie must be
 * unique for its lifetime; never use a reusable kernel handle as the cookie.
 * PRESENT means bytes need preserving, not that every pixel is initialized. */
enum remesa_pass_content { REMESA_PASS_UNDEFINED, REMESA_PASS_PRESENT,
                           REMESA_PASS_UNCERTAIN };
enum remesa_pass_phase { REMESA_PASS_READY, REMESA_PASS_RESERVED,
                         REMESA_PASS_PENDING, REMESA_PASS_QUARANTINED };
enum remesa_pass_load { REMESA_PASS_DISCARD, REMESA_PASS_PRESERVE };
enum remesa_pass_store { REMESA_PASS_STORE, REMESA_PASS_DONT_STORE };
struct remesa_pass_ticket { uint64_t resource_cookie, serial; };
struct remesa_render_pass {
 uint64_t resource_cookie, serial;
 /* Frontend content identity, not a GPU counter or visibility assertion.
  * Advance on published CPU writes and verified completion, including an
  * intentional store discard. Reservation/cancellation never changes it. */
 uint64_t content_epoch;
 enum remesa_pass_content content;
 enum remesa_pass_phase phase;
 enum remesa_pass_store store;
 int error;
};
/* Proof must come from the actual backend verifier/wait contract. Offline
 * fixtures may supply simulated proof but cannot establish SGX behavior. */
struct remesa_pass_completion {
 bool authenticated, cpu_visible, retired;
};
struct remesa_pass_snapshot {
 uint64_t resource_cookie, content_epoch;
 enum remesa_pass_content content;
};

int remesa_render_pass_init(struct remesa_render_pass *, uint64_t cookie);
/* Before acquiring a writable CPU lease or changing backing bytes. This is
 * only the frontend gate; backend mapping/coherency admission still applies. */
int remesa_render_pass_admit_cpu_write(const struct remesa_render_pass *);
int remesa_render_pass_cpu_write(struct remesa_render_pass *);
/* Explicit whole-resource discard, after the owner has synchronized writers
 * and excluded CPU leases. Does not erase memory, release backing or establish
 * completion. Pending and quarantined passes remain protected. */
int remesa_render_pass_invalidate(struct remesa_render_pass *);
/* Obtain ready content identity without authenticating physical memory.
 * Reserved/pending/quarantined resources cannot produce a ready snapshot.
 * UNDEFINED remains explicit; callers must not treat it as initialized data.
 * Output may not alias the pass. Errors leave the output unchanged. Access is
 * externally serialized. */
int remesa_render_pass_snapshot(const struct remesa_render_pass *,
 struct remesa_pass_snapshot *);
/* No mutation on rejection. The output ticket may not alias the pass.
 * Explicit PRESERVE is never changed to DISCARD.
 * preserve_supported must reflect independently admitted backend behavior. */
int remesa_render_pass_admit(const struct remesa_render_pass *,
 enum remesa_pass_load, bool preserve_supported);
int remesa_render_pass_begin(struct remesa_render_pass *,
 enum remesa_pass_load, enum remesa_pass_store, bool preserve_supported,
 struct remesa_pass_ticket *);
/* Cancel only before dispatch. Cancellation consumes its local serial and
 * preserves prior content knowledge. No resubmission happens in this API. */
int remesa_render_pass_cancel(struct remesa_render_pass *,
 const struct remesa_pass_ticket *);
int remesa_render_pass_submitted(struct remesa_render_pass *,
 const struct remesa_pass_ticket *);
/* Attach an externally verified writer using the same local lifetime gate.
 * Does not validate that writer or manufacture completion. */
int remesa_render_pass_import_writer(struct remesa_render_pass *,
 struct remesa_pass_ticket *);
/* Success requires authenticated completion, CPU visibility AND retirement;
 * incomplete proof leaves the writer pending. Wrong tickets never mutate. */
int remesa_render_pass_finish(struct remesa_render_pass *,
 const struct remesa_pass_ticket *, const struct remesa_pass_completion *);
/* Called after potentially issued work fails. No CPU write, new draw or
 * DISCARD may heal uncertain resource contents; backend owns cleanup. */
int remesa_render_pass_fail(struct remesa_render_pass *,
 const struct remesa_pass_ticket *, int error);
#endif
