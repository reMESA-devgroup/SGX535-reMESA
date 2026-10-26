/* SPDX-License-Identifier: GPL-2.0-only */
#include "frozen_source_guard.h"

static void observations(struct sgx535_source_guard *g,
    const struct sgx535_source_facts *f)
{
    if (!f || f->sample_valid != 1) g->reasons |= SGX535_SOURCE_SAMPLE_FAILED;
    if (f && f->pending) g->reasons |= SGX535_SOURCE_PENDING;
    if (f && (f->busy || f->reset || f->bif_reads || f->bif_fault || f->autonomous))
        g->reasons |= SGX535_SOURCE_BUSY;
}
void sgx535_source_boot_start(struct sgx535_source_guard *g,
    const struct sgx535_source_facts *f)
{
    if (g->boot != SGX535_BOOT_NONE || g->state != SGX535_SOURCE_UNUSED ||
        g->producers_active) {
        sgx535_source_lifecycle_lost(g);
        return;
    }
    g->boot = SGX535_BOOT_RESET_EXPECTED;
    observations(g, f);
    if (f) {
        g->startup_pending |= f->pending;
        g->startup_reads |= f->bif_reads;
        g->startup_fault |= f->bif_fault;
        g->startup_autonomous |= f->autonomous;
    }
}
void sgx535_source_boot_assert(struct sgx535_source_guard *g, sgx535_u32 reset)
{
    if (g->boot != SGX535_BOOT_RESET_EXPECTED ||
        g->state != SGX535_SOURCE_UNUSED || g->producers_active != 1) {
        sgx535_source_lifecycle_lost(g);
        return;
    }
    g->asserted_reset = reset;
    if (reset != SGX535_SOURCE_RESET_MASK) {
        sgx535_source_lifecycle_lost(g);
        return;
    }
    g->boot = SGX535_BOOT_RESET_ASSERTED;
}
void sgx535_source_boot_finish(struct sgx535_source_guard *g,
    const struct sgx535_source_facts *f)
{
    if (g->boot != SGX535_BOOT_RESET_ASSERTED ||
        g->state != SGX535_SOURCE_UNUSED || g->producers_active != 1) {
        sgx535_source_lifecycle_lost(g);
        return;
    }
    observations(g, f);
    if (f) {
        g->released_reset = f->reset;
        g->startup_pending |= f->pending;
        g->startup_reads |= f->bif_reads;
        g->startup_fault |= f->bif_fault;
        g->startup_autonomous |= f->autonomous;
    }
    g->boot = g->reasons ? SGX535_BOOT_INVALID : SGX535_BOOT_READY;
}
void sgx535_source_lifecycle_lost(struct sgx535_source_guard *g)
{
    if (g->state == SGX535_SOURCE_CLOSED) return;
    g->boot = SGX535_BOOT_INVALID;
    g->reasons |= SGX535_SOURCE_LIFECYCLE;
}
void sgx535_source_pending(struct sgx535_source_guard *g, sgx535_u32 pending)
{
    if (g->state == SGX535_SOURCE_CLOSED || g->state == SGX535_SOURCE_BOUNDARY)
        return; /* Current-operation events use the existing acceptance path. */
    g->startup_pending |= pending;
    if (pending) g->reasons |= SGX535_SOURCE_PENDING;
}
int sgx535_source_boot_valid(const struct sgx535_source_guard *g)
{
    return g->boot == SGX535_BOOT_READY && !g->reasons;
}
int sgx535_source_probe(struct sgx535_source_guard *g,
    const struct sgx535_source_facts *f)
{
    if (g->state != SGX535_SOURCE_UNUSED) return 0;
    if (!sgx535_source_boot_valid(g)) g->reasons |= SGX535_SOURCE_DELAYED_UNKNOWN;
    observations(g, f);
    if (!f || f->coverage_complete != 1) g->reasons |= SGX535_SOURCE_COVERAGE_UNKNOWN;
    if (g->producers_active) g->reasons |= SGX535_SOURCE_INTERFERENCE;
    if (g->reasons) return 0;
    /* This is the pre-authorization source boundary. No rearm: activity
     * after this observation is retained even if it ends before admission. */
    g->prepared = 1;
    return 1;
}

int sgx535_source_begin(struct sgx535_source_guard *g, const void *operation)
{
    if (g->state != SGX535_SOURCE_UNUSED || !operation) {
        g->reasons |= SGX535_SOURCE_REUSE;
        return 0;
    }
    g->operation = operation;
    g->state = SGX535_SOURCE_OBSERVING;
    if (g->producers_active) g->reasons |= SGX535_SOURCE_INTERFERENCE;
    return 1;
}
int sgx535_source_boundary(struct sgx535_source_guard *g, const void *operation,
    const struct sgx535_source_facts *f)
{
    if (g->state != SGX535_SOURCE_OBSERVING || g->operation != operation) {
        g->reasons |= SGX535_SOURCE_WRONG_LIFETIME;
        return 0;
    }
    if (!sgx535_source_boot_valid(g) || !g->prepared)
        g->reasons |= SGX535_SOURCE_DELAYED_UNKNOWN;
    observations(g, f);
    if (!f || f->coverage_complete != 1) g->reasons |= SGX535_SOURCE_COVERAGE_UNKNOWN;
    if (g->producers_active) g->reasons |= SGX535_SOURCE_INTERFERENCE;
    g->state = g->reasons ? SGX535_SOURCE_BLOCKED : SGX535_SOURCE_BOUNDARY;
    return g->state == SGX535_SOURCE_BOUNDARY;
}
void sgx535_source_producer(struct sgx535_source_guard *g, int begin)
{
    if (g->state == SGX535_SOURCE_CLOSED) return;
    if (g->prepared || g->state != SGX535_SOURCE_UNUSED)
        g->reasons |= SGX535_SOURCE_INTERFERENCE;
    if (begin) {
        if (g->producers_active == ~0U) sgx535_source_lost(g);
        else g->producers_active++;
    } else {
        if (!g->producers_active) sgx535_source_lost(g);
        else g->producers_active--;
    }
}
void sgx535_source_lost(struct sgx535_source_guard *g)
{
    g->reasons |= SGX535_SOURCE_LOST;
}
int sgx535_source_valid(const struct sgx535_source_guard *g, const void *operation)
{
    return g->state == SGX535_SOURCE_BOUNDARY && g->operation == operation &&
        !g->reasons && !g->producers_active;
}
int sgx535_source_close(struct sgx535_source_guard *g, const void *operation)
{
    if (g->state != SGX535_SOURCE_BOUNDARY || g->operation != operation) {
        g->reasons |= SGX535_SOURCE_WRONG_LIFETIME;
        return 0;
    }
    if (g->producers_active) g->reasons |= SGX535_SOURCE_INTERFERENCE;
    g->state = SGX535_SOURCE_CLOSED;
    return !g->reasons;
}
