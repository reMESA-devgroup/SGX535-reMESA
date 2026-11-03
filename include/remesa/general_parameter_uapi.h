/* SPDX-License-Identifier: MIT */
#ifndef REMESA_GENERAL_PARAMETER_UAPI_H
#define REMESA_GENERAL_PARAMETER_UAPI_H
#include <linux/types.h>
#include <drm/drm.h>
/* Experimental root-only data transaction on an explicitly typed v2 session.
 * No caller BO ID, VA, policy, CODE, completion or fence is accepted. The
 * target is the draw's kernel-derived PARAM policy. An acknowledgement means
 * a CPU data update, never GPU submission/retirement. Fixed pointer-free ABI. */
#define REMESA_GENERAL_PARAMETER_VERSION 1u
#define REMESA_GENERAL_PARAMETER_MAX_WORDS 127u
struct remesa_general_parameter_request {
 __u32 version,flags,draw,offset,words,reserved0;
 __u64 context,session,retired_operation,sequence;
 __u32 data[128];
 __u64 accepted_sequence;
 __s32 error;
 __u32 state,reserved[2];
};
#define DRM_REMESA_GENERAL_PARAMETER 16
#define DRM_IOCTL_REMESA_GENERAL_PARAMETER DRM_IOWR(DRM_COMMAND_BASE+DRM_REMESA_GENERAL_PARAMETER,struct remesa_general_parameter_request)
static inline int remesa_general_parameter_request_valid(const struct remesa_general_parameter_request *r)
{
 unsigned i;
 if(!r||r->version!=1||r->flags||r->reserved0||!r->draw||r->draw>16||
    (r->offset&3)||!r->words||r->words>127||!r->context||!r->session||
    !r->retired_operation||!r->sequence||r->accepted_sequence||r->error||r->state||
    r->reserved[0]||r->reserved[1])return 0;
 for(i=r->words;i<128;i++)if(r->data[i])return 0;
 return 1;
}
#endif
