/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_TWOD_IRQ_H
#define REMESA_TWOD_IRQ_H
#include "remesa_twod_reservation.h"
struct drm_device;
struct remesa_twod_irq_snapshot {u64 cookie;u32 status1,status2;};
/* All entry points require the existing fixed IRQ lock. Lock order is
 * lock_2d -> fixed IRQ lock, never the reverse. No MMIO or IRQ ACK here.
 * Kernel-only; no ioctl, user cookie, registration knob or completion injection.
 */
bool remesa_twod_irq_busy_locked(void);
bool remesa_twod_irq_available_locked(void);
int remesa_twod_irq_claim_locked(struct remesa_twod_reservation *);
int remesa_twod_irq_drop_locked(struct remesa_twod_reservation *);
/* Native caller MUST establish idle/stale-event-ACK boundary under this SAME
 * lock, then mark possible issuance BEFORE the first FIFO write. Each claim
 * admits one nonzero cookie; errors/uncertainty must retain it, not rearm.
 */
int remesa_twod_irq_issue_locked(struct remesa_twod_reservation *,u64);
void remesa_twod_irq_capture_locked(struct drm_device *,u32,u32);
int remesa_twod_irq_snapshot_locked(struct remesa_twod_reservation *,u64,
 struct remesa_twod_irq_snapshot *);
/* Only after native adapter proves device visibility, DMA quiescence and
 * request-base restoration. This validates the retained event ledger; it
 * does not itself prove hardware retirement. Later unexpected events still
 * prevent drop. Caller may not free token/device before successful release.
 */
int remesa_twod_irq_mark_retired_locked(struct remesa_twod_reservation *,u64);
/* Shared IRQ lock held: confirm this exact unissued reservation ledger. */
int remesa_twod_irq_guard_locked(struct remesa_twod_reservation *);
#endif
