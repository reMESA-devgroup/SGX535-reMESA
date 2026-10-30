/* SPDX-License-Identifier: MIT */
#ifndef REMESA_PRIVATE_SESSION_LAB_UAPI_H
#define REMESA_PRIVATE_SESSION_LAB_UAPI_H
#include <linux/types.h>
#include <drm/drm.h>
#define REMESA_PRIVATE_SESSION_LAB_VERSION 1u
#define REMESA_PRIVATE_SESSION_LAB_OPEN 1u
#define REMESA_PRIVATE_SESSION_LAB_SUBMIT 2u
#define REMESA_PRIVATE_SESSION_LAB_CLOSE 3u
#define REMESA_PRIVATE_SESSION_LAB_ACTIVE 1u
#define REMESA_PRIVATE_SESSION_LAB_POISONED 2u
#define REMESA_PRIVATE_SESSION_LAB_CLOSED 3u
#define REMESA_PRIVATE_SESSION_LAB_OPENED 16u
#define REMESA_PRIVATE_SESSION_LAB_CLOSED_STAGE 32u
/* Root-only bounded qualification of the retained 1280x800 profile. Not a
 * production render ABI. Uses existing memory/private-probe N/N admission.
 * OPEN: server memory context, eleven owned IDs, max_jobs1..64; session/seq0.
 * SUBMIT: returned session, next sequence, budget1..10000ms; IDs/max_jobs0.
 * CLOSE: session and last successful sequence, budget/IDs/max_jobs0. CLOSE is
 * admitted even if execution controls have since been disabled. No user VA,
 * packet, PFN, pointer or fabricated event/retirement field is accepted.
 * All response fields enter zero. OPEN/CLOSE are NOT GPU receipts. SUBMIT
 * requires phase9/ledger7/stages15 plus actual readback for LAB approval.
 * A poisoned result retains backing until a separately established recovery. */
struct remesa_private_session_lab_request {
 __u32 version,action;
 __u64 context,session,sequence;
 __u32 ids[11],budget_ms,max_jobs,flags;
 __u64 operation,cookie;
 __u32 core,revision,phase,ledger;
 __s32 error;
 __u32 state,stages,reserved;
};
#define DRM_REMESA_PRIVATE_SESSION_LAB 10
#define DRM_IOCTL_REMESA_PRIVATE_SESSION_LAB DRM_IOWR(DRM_COMMAND_BASE+DRM_REMESA_PRIVATE_SESSION_LAB,struct remesa_private_session_lab_request)
#endif
