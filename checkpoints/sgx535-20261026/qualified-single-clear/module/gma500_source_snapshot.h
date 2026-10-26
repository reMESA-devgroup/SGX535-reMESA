/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SGX535_GMA500_SOURCE_SNAPSHOT_H
#define SGX535_GMA500_SOURCE_SNAPSHOT_H
#include "psb_drv.h"
#include "psb_reg.h"
#include "power.h"
#include "frozen_source_guard.h"

/* Only reads. No drain, ACK, reset, command, retry or wake. Nine observations
 * corroborate a retained lifecycle; they do not create it. Caller serializes
 * against the IRQ read/ACK interval when the device is registered. */
static inline void sgx535_gma500_source_snapshot(struct drm_psb_private *dev_priv,
    struct sgx535_source_facts *facts)
{
    *facts = (struct sgx535_source_facts){0};
    if (!dev_priv || !dev_priv->sgx_reg || !gma_power_is_on(dev_priv->dev)) return;
    facts->pending = (PSB_RSGX32(PSB_CR_EVENT_STATUS) &
        ~((1U << 31) | (1U << 27))) | PSB_RSGX32(PSB_CR_EVENT_STATUS2);
    facts->busy = PSB_RSGX32(PSB_CR_2D_SOCIF) != _PSB_C2_SOCIF_EMPTY ||
        (PSB_RSGX32(PSB_CR_2D_BLIT_STATUS) & _PSB_C2B_STATUS_BUSY);
    facts->reset = PSB_RSGX32(PSB_CR_SOFT_RESET);
    facts->bif_reads = PSB_RSGX32(SGX535_SOURCE_BIF_READS_REG) &
        SGX535_SOURCE_BIF_READS_MASK;
    facts->bif_fault = PSB_RSGX32(PSB_CR_BIF_INT_STAT) & SGX535_SOURCE_BIF_FAULT_MASK;
    facts->autonomous = (PSB_RSGX32(SGX535_SOURCE_EVENT_TIMER_REG) &
        SGX535_SOURCE_EVENT_TIMER_ENABLE) |
        (PSB_RSGX32(SGX535_SOURCE_EVENT_KICK_REG) & SGX535_SOURCE_EVENT_KICK_NOW);
    facts->sample_valid = 1;
}
#endif
