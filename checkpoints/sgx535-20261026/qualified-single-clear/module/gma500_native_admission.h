/* SPDX-License-Identifier: GPL-2.0-only
 * Native call freshness predicate. This does not authorize SGX execution.
 */
#ifndef SGX535_GMA500_NATIVE_ADMISSION_H
#define SGX535_GMA500_NATIVE_ADMISSION_H
#include "frozen_capsule.h"
#include "frozen_source_guard.h"
static inline int sgx535_native_fresh(const struct sgx535_capsule *c,
    const struct sgx535_source_guard *g, int device_match, int module_match,
    int task_unused, int operation_unused)
{
    return c && g && c->state == SGX535_CAP_UNUSED && !c->invalid &&
        g->state == SGX535_SOURCE_UNUSED && g->prepared == 1 &&
        sgx535_source_boot_valid(g) && !g->producers_active &&
        device_match == 1 && module_match == 1 && task_unused == 1 &&
        operation_unused == 1;
}
#endif
