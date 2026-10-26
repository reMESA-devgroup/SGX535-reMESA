/* SPDX-License-Identifier: GPL-2.0-only */
#include "frozen_capsule.h"
#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

static void invalid(struct sgx535_capsule *c) { c->invalid = 1; }
static int active(struct sgx535_capsule *c, const void *b, const void *s)
{
    if (c->invalid || c->state != SGX535_CAP_ACTIVE || c->backend != b ||
        (s && c->service != s)) { invalid(c); return 0; }
    return 1;
}
void sgx535_capsule_call(struct sgx535_capsule *c, const void *call)
{
    if (c->state != SGX535_CAP_UNUSED || !call) { invalid(c); return; }
    c->call = call; c->state = SGX535_CAP_CALL;
}
void sgx535_capsule_admit(struct sgx535_capsule *c, const void *call,
    const void *b, const void *s, const void *o, sgx535_u32 phase,
    sgx535_u32 events, sgx535_u32 sequence, int fresh)
{
    if (c->invalid || c->state != SGX535_CAP_CALL || c->call != call ||
        !b || !s || !o || !fresh || events || sequence ||
        phase != SGX535_PHASE_HOST_IMAGE_FINAL) { invalid(c); return; }
    c->backend = b; c->service = s; c->owner = o;
    c->state = SGX535_CAP_ACTIVE;
}
void sgx535_capsule_service(struct sgx535_capsule *c, const void *b,
    const void *s, int matched)
{
    if (!active(c,b,s)) return;
    if (!matched || c->service_started) { invalid(c); return; }
    c->service_started = 1;
}
void sgx535_capsule_issued(struct sgx535_capsule *c, const void *b)
{
    /* Conservative possible issuance, never a claim of successful fire. */
    if (!active(c,b,0) || !c->service_started) { invalid(c); return; }
    c->issued = 1;
}
void sgx535_capsule_sample(struct sgx535_capsule *c, const void *b,
    sgx535_u32 q, sgx535_u32 a, sgx535_u32 d, int exclusive)
{
    if (!active(c,b,0)) return;
    if (!c->service_started || !c->issued || !q || !exclusive ||
        (c->sequence && c->sequence != q) || c->pending || c->processing) {
        invalid(c); return;
    }
    c->sequence = q;
    if (!a && !d) return;
    c->sample1 = a; c->sample2 = d; c->pending = 1;
}
void sgx535_capsule_before(struct sgx535_capsule *c, const void *b,
    const void *s, sgx535_u32 q, sgx535_u32 a, sgx535_u32 d, int exclusive)
{
    if (!active(c,b,s)) return;
    if (!c->pending || c->processing || q != c->sequence || !exclusive ||
        a != c->sample1 || d != c->sample2) { invalid(c); return; }
    c->pending = 0; c->processing = 1;
    c->sample1 = c->sample2 = 0; /* No register history retained. */
}
void sgx535_capsule_after(struct sgx535_capsule *c, const void *b,
    const void *s, sgx535_u32 ledger, sgx535_u32 phase)
{
    if (!active(c,b,s)) return;
    if (!c->processing || (ledger & ~7U) ||
        (ledger & c->events) != c->events) { invalid(c); return; }
    /* Acceptance semantics are supplied by the existing qualified contract,
     * not reinterpreted from status bits here. */
    c->events = ledger; c->phase = phase; c->processing = 0;
}
void sgx535_capsule_terminal(struct sgx535_capsule *c, const void *b,
    const void *s, int result, sgx535_u32 phase, sgx535_u32 ledger)
{
    if (!active(c,b,s)) return;
    if (!c->service_started || c->pending || c->processing || ledger != c->events ||
        (!result && (phase != SGX535_PHASE_RETIRED || ledger != 7U))) {
        invalid(c); return;
    }
    c->result = result; c->phase = phase; c->terminal_present = 1;
    c->state = SGX535_CAP_TERMINAL;
}
void sgx535_capsule_color(struct sgx535_capsule *c, const void *o,
    sgx535_u32 phase, const void *bytes, size_t length)
{
    if (c->invalid || c->state != SGX535_CAP_TERMINAL || c->owner != o ||
        c->color_present || c->result || c->events != 7U || !bytes ||
        phase != SGX535_PHASE_RETIRED || c->phase != phase || length != 4096U) {
        invalid(c); return;
    }
    memcpy(c->color,bytes,4096); c->color_present = 1;
}
void sgx535_capsule_close(struct sgx535_capsule *c, const void *call, int result)
{
    if (c->call != call || c->state == SGX535_CAP_UNUSED ||
        c->state == SGX535_CAP_CLOSED) { invalid(c); return; }
    c->syscall_result = result;
    if (c->state != SGX535_CAP_TERMINAL) invalid(c);
    c->state = SGX535_CAP_CLOSED;
}
void sgx535_capsule_release(struct sgx535_capsule *c, const void *o)
{
    if (c->owner != o) return;
    if (!c->terminal_present || (!c->result && !c->color_present)) invalid(c);
}
int sgx535_capsule_success(const struct sgx535_capsule *c)
{
    return c->state == SGX535_CAP_CLOSED && !c->invalid && c->issued &&
        c->terminal_present && !c->result && c->syscall_result >= 0 &&
        c->phase == SGX535_PHASE_RETIRED && c->events == 7 && c->color_present;
}
static void put32(sgx535_u8 *p, sgx535_u32 v)
{
    p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24;
}
int sgx535_capsule_encode(const struct sgx535_capsule *c, void *buffer, size_t n)
{
    sgx535_u8 *p=buffer;
    if (!p || n != SGX535_CAPSULE_BYTES) return -1;
    memset(p,0,n); memcpy(p,"SGXCAPB1",8);
    put32(p+8,1); put32(p+12,SGX535_CAPSULE_BYTES);
    put32(p+16,c->state); put32(p+20,c->invalid);
    put32(p+24,c->issued); put32(p+28,c->terminal_present);
    put32(p+32,(sgx535_u32)c->result); put32(p+36,c->phase);
    put32(p+40,c->events); put32(p+44,c->color_present);
    put32(p+48,(sgx535_u32)c->syscall_result);
    memcpy(p+SGX535_CAPSULE_HEADER,c->color,4096);
    return 0;
}
