/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_PRIVATE_SOURCE_MMIO_H
#define REMESA_PRIVATE_SOURCE_MMIO_H
struct sgx535_gma500_fixed_backend;
/* Trusted fixed-action caller only; no ioctl, raw packet or retry interface.
 * Source authority and one bounded register write share source_lock. */
int remesa_private_source_write(struct sgx535_gma500_fixed_backend *,unsigned,unsigned);
#endif
