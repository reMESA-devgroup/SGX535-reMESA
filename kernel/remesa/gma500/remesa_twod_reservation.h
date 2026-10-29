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
 /* Kernel-only borrower token; prevents release or issue during MMU work. */
 const void *mmu_owner;
};
/* Synchronize with the original FIFO producer under lock_2d. Reserve requires
 * an idle engine. No register is written. Release additionally requires the
 * saved request base already restored. Native caller MUST first prove owned
 * completion, visibility and retirement; idle alone does not prove those.
 * Every failure preserves the live reservation. No timeout/reset fallback.
 */
int remesa_twod_reserve(struct drm_psb_private *,struct remesa_twod_reservation *);
int remesa_twod_release(struct drm_psb_private *,struct remesa_twod_reservation *);
/* Claim an already-held, idle/unissued reservation for a stable kernel owner.
 * Caller retains device/module/power and serializes reservation/owner storage.
 * No root publication, MMU-writer exclusion, GPU completion or retirement is
 * implied. Guard blocks reservation release and 2D IRQ issue until unclaimed.
 * Both operations use the original lock_2d -> shared IRQ lock order. */
int remesa_twod_mmu_guard_acquire(struct drm_psb_private *,
 struct remesa_twod_reservation *,const void *owner);
int remesa_twod_mmu_guard_release(struct drm_psb_private *,
 struct remesa_twod_reservation *,const void *owner);
/* Native retire only, with lock_2d AND shared IRQ lock already held, in that
 * order. Allows final quiet/restore/drop to be one uninterrupted interval. */
int remesa_twod_release_locked(struct drm_psb_private *,struct remesa_twod_reservation *);
#endif
