/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_GEM_DPM_H
#define REMESA_GEM_DPM_H
#include "remesa_gem_memory.h"
/* Kernel-only cold DPM ownership. Prepared/unattached job, distinct retained
 * IDs, two32MiB pools, original SURFACE table/1MiB-aligned RASTER parameter.
 * Requires fresh zero-on-pin provenance and no writable alias/exposure. Never
 * scans, clears, maps or flushes pools. Repetition returns EALREADY. No ioctl,
 * root publication, execution or post-render reuse contract is granted.
 */
int remesa_gem_job_claim_zero_dpm(struct remesa_gem_memory *,
 struct remesa_memory_ticket,u32 table_id,u32 parameter_id);
/* Full metadata with CPU views only for selected low count bits. DPM claimed
 * pools may not be selected. Returns count; no partial output on failure.
 * Selected views are writable borrowed kernel aliases, so zero provenance is
 * conservatively consumed. GPU-only views have cpu=NULL and retain page holds.
 * Legacy snapshot checker requires all CPU views; a private consumer uses the
 * existing full loan/translation validator for these optional-CPU views.
 * External kernel consumer serialization and no writes through GPU-only pages
 * are required. This is not cache publication or permission to execute.
 */
int remesa_gem_job_snapshot_selected(struct remesa_gem_memory *,
 struct remesa_memory_ticket,struct remesa_memory_snapshot *,
 struct remesa_memory_view *,unsigned capacity,u32 cpu_mask);
#endif
