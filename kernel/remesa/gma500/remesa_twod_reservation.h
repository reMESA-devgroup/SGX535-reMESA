/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_TWOD_RESERVATION_H
#define REMESA_TWOD_RESERVATION_H
#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdbool.h>
#include <stdint.h>
typedef uint32_t u32;
typedef uint64_t u64;
#endif
struct drm_psb_private;
/* Zero-initialized exclusive kernel storage, retained until successful release.
 * Caller retains device/module/power and serializes this token's lifetime.
 * No userspace ABI, GPU permission or hardware identity proof is provided.
 */
struct remesa_twod_reservation {
 struct drm_psb_private *device;
 u32 saved_base;
 bool held;
 /* Shared IRQ lock protects these fields and the global IRQ owner. */
 u64 irq_cookie;
 u32 pending1,pending2;
 bool irq_issued,irq_retired;
};
/* Synchronize with the original FIFO producer under lock_2d. Reserve requires
 * an idle engine. No register is written. Release additionally requires the
 * saved request base already restored. Native caller MUST first prove owned
 * completion, visibility and retirement; idle alone does not prove those.
 * Every failure preserves the live reservation. No timeout/reset fallback.
 */
int remesa_twod_reserve(struct drm_psb_private *,struct remesa_twod_reservation *);
int remesa_twod_release(struct drm_psb_private *,struct remesa_twod_reservation *);
#endif
