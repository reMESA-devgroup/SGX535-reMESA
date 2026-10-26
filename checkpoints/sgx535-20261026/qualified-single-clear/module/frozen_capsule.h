/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SGX535_FROZEN_CAPSULE_H
#define SGX535_FROZEN_CAPSULE_H
#include "frozen_kernel_contract.h"

#define SGX535_CAPSULE_HEADER 56U
#define SGX535_CAPSULE_BYTES (SGX535_CAPSULE_HEADER + 4096U)
enum sgx535_capsule_state { SGX535_CAP_UNUSED, SGX535_CAP_CALL,
    SGX535_CAP_ACTIVE, SGX535_CAP_TERMINAL, SGX535_CAP_CLOSED };

/* Private producer state. No pointers or transient raw samples are exported.
 * The adapter serializes every access. No reset/rearm API exists. */
struct sgx535_capsule {
    sgx535_u32 state, invalid, issued, terminal_present;
    int result, syscall_result;
    sgx535_u32 phase, events, color_present;
    const void *call, *backend, *service, *owner;
    sgx535_u32 service_started, sequence, pending, processing;
    sgx535_u32 sample1, sample2;
    sgx535_u8 color[4096];
};
void sgx535_capsule_call(struct sgx535_capsule *, const void *);
void sgx535_capsule_admit(struct sgx535_capsule *, const void *, const void *,
    const void *, const void *, sgx535_u32, sgx535_u32, sgx535_u32, int);
void sgx535_capsule_service(struct sgx535_capsule *, const void *, const void *, int);
void sgx535_capsule_issued(struct sgx535_capsule *, const void *);
void sgx535_capsule_sample(struct sgx535_capsule *, const void *, sgx535_u32,
    sgx535_u32, sgx535_u32, int);
void sgx535_capsule_before(struct sgx535_capsule *, const void *, const void *,
    sgx535_u32, sgx535_u32, sgx535_u32, int);
void sgx535_capsule_after(struct sgx535_capsule *, const void *, const void *,
    sgx535_u32, sgx535_u32);
void sgx535_capsule_terminal(struct sgx535_capsule *, const void *, const void *,
    int, sgx535_u32, sgx535_u32);
void sgx535_capsule_color(struct sgx535_capsule *, const void *, sgx535_u32,
    const void *, size_t);
void sgx535_capsule_close(struct sgx535_capsule *, const void *, int);
void sgx535_capsule_release(struct sgx535_capsule *, const void *);
int sgx535_capsule_success(const struct sgx535_capsule *);
int sgx535_capsule_encode(const struct sgx535_capsule *, void *, size_t);
#endif
