/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_MMU_SWITCH_H
#define REMESA_MMU_SWITCH_H
struct remesa_mmu_private;
/* TRUSTED KERNEL CALLERS ONLY, not an ioctl or job admission interface.
 * Caller retains device/module/power/default PD, owns exclusive device and
 * producer/IRQ exclusion for the WHOLE bind/job/restore interval, serializes
 * all owner/default mappings, and has proved a retained idle/retired lifecycle.
 * Register samples below only corroborate that proof: they cannot create it.
 * May sleep; caller must not hold driver->sem or a spinlock. Caller exclusion
 * must block console 2D, native 2D/3D, context setters and power transitions.
 * No bank selection, command submission, event ACK, reset or retry occurs.
 * A bind failure may leave an ACTIVE/UNCERTAIN owner; always retain it until
 * release actually succeeds. An uncertain transaction cannot restore or free.
 * Restore additionally requires caller-proven owned completion, visibility and
 * retirement (or proof no command was ever issued), not merely idle samples.
 * Successful restore proves only checked root/control restoration; it does not
 * manufacture GPU completion/retirement. Current normal/LAB job paths do NOT
 * call these functions. A future caller must qualify its exclusion contract.
 */
int remesa_mmu_private_bind_exclusive(struct remesa_mmu_private *);
int remesa_mmu_private_restore_exclusive(struct remesa_mmu_private *);
/* Same root sequences and device/lifecycle contract, with MMU writer semaphore
 * ALREADY held by this task. Neither routine acquires nor releases it, even on
 * failure. Bind includes the complete audit/table cache publication under that
 * same interval. Caller can span bind -> job -> owned retirement -> restore in
 * one synchronous call. No lock may be carried across ioctl/task boundaries.
 * Uncertain failure still retains roots and all original exclusion obligations.
 * The private integration profile also retains a driver owner that defers
 * ordinary queued flushes; that does not exclude context setters, prove
 * retirement, or establish a complete device recovery protocol. */
int remesa_mmu_private_bind_locked(struct remesa_mmu_private *);
int remesa_mmu_private_restore_locked(struct remesa_mmu_private *);
/* Private integration profile only. Caller ALREADY holds driver->sem for
 * writing plus the retained device/producer/IRQ/power contract. For the
 * pre-issue DEVICE_MAINTAIN stage, corroborates the exact bound private root,
 * calls the original driver cache-maintenance body WITHOUT reacquiring sem,
 * then checks unchanged root/control/identity. No PTE data or flags are guessed.
 * Failure retains the active owner and marks uncertainty. No GPU completion,
 * default restore, command issue, ACK, reset or permission is produced. */
int remesa_mmu_private_maintain_locked(struct remesa_mmu_private *);
/* Pre-issue translation publication, ALREADY holding driver->sem for writing.
 * Expected bindings must be VA-sorted and represent the exact retained job.
 * Matches addresses, access/cache policy and held page identities to the
 * owner's immutable copied closure, then checks bound root/context/identity.
 * No data readback/hash, PTE rewrite, cache maintenance, allocation or lock
 * acquisition. This does not authenticate commands or imply GPU admission. */
int remesa_mmu_private_validate_bound_locked(struct remesa_mmu_private *,
 const struct remesa_mmu_allowed_binding *,u32 count);
#endif
