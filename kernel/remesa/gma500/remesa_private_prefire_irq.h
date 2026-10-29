/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_PRIVATE_PREFIRE_IRQ_H
#define REMESA_PRIVATE_PREFIRE_IRQ_H
/* Caller owns fixed_irq_lock. Only the exact DPM load action interval may
 * consume its four status2 bits and INITEND. Master is aggregate. A 2D event
 * never belongs to a private 3D owner retaining the producer reservation. */
static inline bool remesa_private_prefire_unexpected(u32 status1,u32 status2,
                                                    bool dpm_load_active)
{
 u32 allowed1=(1U<<31)|(dpm_load_active?(1U<<22):0);
 u32 allowed2=dpm_load_active?15U:0;
 return (status1&~allowed1)||(status2&~allowed2);
}
#endif
