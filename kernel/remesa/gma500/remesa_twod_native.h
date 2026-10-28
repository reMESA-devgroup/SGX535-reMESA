/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_TWOD_NATIVE_H
#define REMESA_TWOD_NATIVE_H
#include "remesa_gem_twod_job.h"
#include "remesa_twod_irq.h"
/* Kernel caller zero-initializes and retains this context, job and device
 * until successful retirement. No forced teardown on error. Not a user ABI.
 * Native MMIO implementation is a candidate until physical qualification.
 */
struct remesa_twod_native {
 struct remesa_twod_reservation reservation;
 const struct remesa_twod_launch *launch;
 u64 serial,cookie;
 u32 root,core,revision,stale_status1;
#ifdef REMESA_TWOD_LAB_VENDOR_STREAM
 u32 saved_alpha,expected_alpha;
 bool alpha_saved;
#endif
 bool live,complete,visible,failed;
};
extern const struct remesa_twod_native_ops remesa_twod_native_executor;
#endif
