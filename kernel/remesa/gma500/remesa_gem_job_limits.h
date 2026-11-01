/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_GEM_JOB_LIMITS_H
#define REMESA_GEM_JOB_LIMITS_H
/* Kernel-only retained allocation capacity, not an ioctl/command schema or
 * permission to execute. Must fit the32 GEM slots and private MMU auditor.
 * Keep every view/ID/rule array at this same bound; never truncate closure. */
#define REMESA_GEM_JOB_BINDINGS 32u
#endif
