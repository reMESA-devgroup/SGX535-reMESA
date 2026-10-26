/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_BIF_DEBUG_H
#define REMESA_BIF_DEBUG_H
#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/string.h>
typedef u32 remesa_bif_u32;
#else
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
typedef uint32_t remesa_bif_u32;
#endif
/* Public psb_reg.h: PSB_CR_BIF_INT_STAT,PSB_CR_BIF_FAULT and
 * _PSB_CE2_BIF_REQUESTER_FAULT. Kernel integration asserts equality. */
#define REMESA_BIF_INT_STAT_OFFSET 0x0c04u
#define REMESA_BIF_FAULT_OFFSET 0x0c08u
#define REMESA_BIF_REQUESTER_EVENT (1u<<4)
#define REMESA_BIF_DEBUG_VERSION 1u
#define REMESA_BIF_DEBUG_BYTES 96u
#define REMESA_BIF_DEBUG_RESPONSE_OFFSET 4220u
#define REMESA_BIF_DEBUG_RESPONSE_BYTES 4316u
#define REMESA_BIF_PRE 0u
#define REMESA_BIF_IRQ 1u
#define REMESA_BIF_POST 2u
struct remesa_bif_sample {
 remesa_bif_u32 irq_status1,irq_status2; /* Valid as IRQ observations ONLY. */
 remesa_bif_u32 bif_int_stat,bif_fault,session_phase,ledger;
};
struct remesa_bif_debug_record {
 remesa_bif_u32 version,bytes,present_mask,irq_fault_observations,sequence,error_mask;
 struct remesa_bif_sample samples[3];
};
typedef int (*remesa_bif_read)(void *,remesa_bif_u32,remesa_bif_u32 *);
static inline void remesa_bif_debug_init(struct remesa_bif_debug_record *r,
 remesa_bif_u32 sequence)
{
 memset(r,0,sizeof(*r));r->version=REMESA_BIF_DEBUG_VERSION;
 r->bytes=sizeof(*r);r->sequence=sequence;
}
/* Caller establishes power/exclusivity and serialization; this helper does
 * NOT acquire them or grant admission. At most one read pair per slot,
 * including failed reads. IRQ count is hook observations, not unique faults.
 * Zero BIF values are raw observations, never proof that no task executed. */
static inline int remesa_bif_debug_sample(struct remesa_bif_debug_record *r,
 unsigned slot,remesa_bif_u32 sequence,bool qualified,remesa_bif_read read,
 void *opaque,remesa_bif_u32 irq1,remesa_bif_u32 irq2,
 remesa_bif_u32 phase,remesa_bif_u32 ledger)
{
 struct remesa_bif_sample sample;
 remesa_bif_u32 bit;int e;
 if(!r||!read||slot>REMESA_BIF_POST)return -EINVAL;
 if(!qualified||!sequence||r->sequence!=sequence||
    r->version!=REMESA_BIF_DEBUG_VERSION||r->bytes!=sizeof(*r))return -EACCES;
 bit=1u<<slot;
 if(slot==REMESA_BIF_IRQ) {
  if(!(irq2&REMESA_BIF_REQUESTER_EVENT))return 0;
  if(r->irq_fault_observations!=~(remesa_bif_u32)0)r->irq_fault_observations++;
 }
 if((r->present_mask|r->error_mask)&bit)return 0;
 memset(&sample,0,sizeof(sample));sample.session_phase=phase;sample.ledger=ledger;
 if(slot==REMESA_BIF_IRQ){sample.irq_status1=irq1;sample.irq_status2=irq2;}
 e=read(opaque,REMESA_BIF_INT_STAT_OFFSET,&sample.bif_int_stat);
 if(!e)e=read(opaque,REMESA_BIF_FAULT_OFFSET,&sample.bif_fault);
 if(e){r->error_mask|=bit;return e;}
 r->samples[slot]=sample;r->present_mask|=bit;return 0;
}
#ifdef __KERNEL__
struct sgx535_gma500_fixed_backend;
int sgx535_gma500_fixed_bif_pre(struct sgx535_gma500_fixed_backend *,u32);
int sgx535_gma500_fixed_bif_post(struct sgx535_gma500_fixed_backend *);
int sgx535_gma500_fixed_bif_snapshot(struct sgx535_gma500_fixed_backend *,
 struct remesa_bif_debug_record *);
#endif
#endif
