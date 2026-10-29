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
#endif
