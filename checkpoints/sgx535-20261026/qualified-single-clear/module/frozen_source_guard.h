/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SGX535_FROZEN_SOURCE_GUARD_H
#define SGX535_FROZEN_SOURCE_GUARD_H
#include "frozen_kernel_contract.h"

enum sgx535_source_state { SGX535_SOURCE_UNUSED, SGX535_SOURCE_OBSERVING,
    SGX535_SOURCE_BOUNDARY, SGX535_SOURCE_CLOSED, SGX535_SOURCE_BLOCKED };
enum sgx535_source_reason {
    SGX535_SOURCE_PENDING = 1U, SGX535_SOURCE_BUSY = 2U,
    SGX535_SOURCE_DELAYED_UNKNOWN = 4U, SGX535_SOURCE_SAMPLE_FAILED = 8U,
    SGX535_SOURCE_COVERAGE_UNKNOWN = 16U, SGX535_SOURCE_INTERFERENCE = 32U,
    SGX535_SOURCE_LOST = 64U, SGX535_SOURCE_REUSE = 128U,
    SGX535_SOURCE_WRONG_LIFETIME = 256U, SGX535_SOURCE_LIFECYCLE = 512U
};
/* SGX535 register definitions; BIF READS counts outstanding reads, not global
 * engine idleness. See the permitted SGX535 definitions/reset reference. */
#define SGX535_SOURCE_RESET_MASK 0x7fU
#define SGX535_SOURCE_BIF_READS_REG 0x0ca8U
#define SGX535_SOURCE_BIF_READS_MASK 0xffU
#define SGX535_SOURCE_BIF_FAULT_MASK 0x3fffU
#define SGX535_SOURCE_EVENT_TIMER_REG 0x0accU
#define SGX535_SOURCE_EVENT_TIMER_ENABLE (1U << 24)
#define SGX535_SOURCE_EVENT_KICK_REG 0x0ac8U
#define SGX535_SOURCE_EVENT_KICK_NOW 1U
enum sgx535_source_boot { SGX535_BOOT_NONE, SGX535_BOOT_RESET_EXPECTED,
    SGX535_BOOT_RESET_ASSERTED, SGX535_BOOT_READY, SGX535_BOOT_INVALID };
/* Observations, never a supplied delayed-exclusion certificate. No single
 * clean sample proves the lifetime relation established by startup reset. */
struct sgx535_source_facts {
    sgx535_u32 sample_valid, pending, busy, reset, bif_reads, bif_fault,
        coverage_complete, autonomous;
};
/* Serialized by the adapter. No rearm, clearing, draining or execution API. */
struct sgx535_source_guard {
    sgx535_u32 state, reasons, producers_active;
    sgx535_u32 boot, prepared, asserted_reset, released_reset, startup_pending,
        startup_reads, startup_fault, startup_autonomous;
    const void *operation;
};
void sgx535_source_boot_start(struct sgx535_source_guard *,
    const struct sgx535_source_facts *);
void sgx535_source_boot_assert(struct sgx535_source_guard *, sgx535_u32);
void sgx535_source_boot_finish(struct sgx535_source_guard *,
    const struct sgx535_source_facts *);
void sgx535_source_lifecycle_lost(struct sgx535_source_guard *);
void sgx535_source_pending(struct sgx535_source_guard *, sgx535_u32);
int sgx535_source_boot_valid(const struct sgx535_source_guard *);
int sgx535_source_probe(struct sgx535_source_guard *,
    const struct sgx535_source_facts *);
int sgx535_source_begin(struct sgx535_source_guard *, const void *);
int sgx535_source_boundary(struct sgx535_source_guard *, const void *,
    const struct sgx535_source_facts *);
void sgx535_source_producer(struct sgx535_source_guard *, int);
void sgx535_source_lost(struct sgx535_source_guard *);
int sgx535_source_valid(const struct sgx535_source_guard *, const void *);
int sgx535_source_close(struct sgx535_source_guard *, const void *);
#endif
