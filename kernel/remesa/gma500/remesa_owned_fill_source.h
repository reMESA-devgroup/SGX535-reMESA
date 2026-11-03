/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_OWNED_FILL_SOURCE_H
#define REMESA_OWNED_FILL_SOURCE_H
struct drm_device;
struct remesa_twod_launch;
struct remesa_twod_retirement;
/* Kernel-only ordinary black clear predecessor. Claim BEFORE publication,
 * while the existing backing owner excludes foreign work. No user credential,
 * guard reset, completion inference, IRQ ACK or new UAPI. Native hooks run
 * under fixed_irq_lock; claim/complete/ready take source_lock internally.
 * Stable owner, launch and PD storage must survive every uncertain return.
 * These entry points alone do not enable this path in the normal endpoint. */
int remesa_owned_fill_claim(struct drm_device *,const void *,u64 destination);
int remesa_owned_fill_publish(const void *,const struct remesa_twod_launch *,u32 root,u32 saved_base,u64 cookie);
int remesa_owned_fill_issued(const struct remesa_twod_launch *,u64 cookie);
int remesa_owned_fill_published(const struct remesa_twod_launch *,int error);
int remesa_owned_fill_restore(const struct remesa_twod_launch *,u64 cookie);
int remesa_owned_fill_restored(const struct remesa_twod_launch *,const struct remesa_twod_retirement *,int error);
int remesa_owned_fill_complete(const void *,const struct remesa_twod_retirement *);
int remesa_owned_fill_ready(struct drm_device *,const void *);
#endif
