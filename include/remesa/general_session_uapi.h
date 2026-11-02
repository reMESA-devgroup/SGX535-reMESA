/* SPDX-License-Identifier: MIT */
#ifndef REMESA_GENERAL_SESSION_UAPI_H
#define REMESA_GENERAL_SESSION_UAPI_H
#include <linux/types.h>
#include <drm/drm.h>
/* Separate bounded root-only LAB transport for the existing CODE344/native
 * catalogues5/6. Same default-off memory/scene controls and consumed source
 * lineage; never reinterpret the old11-ID session or one-shot ABI. OPEN/CLOSE
 * are not GPU receipts. Immutable profile inputs are authenticated per SUBMIT
 * in this LAB endpoint, outside production FAST. No caller CODE/VA/packet. */
#define REMESA_GENERAL_SESSION_VERSION 1u
#define REMESA_GENERAL_SESSION_OPEN 1u
#define REMESA_GENERAL_SESSION_SUBMIT 2u
#define REMESA_GENERAL_SESSION_CLOSE 3u
#define REMESA_GENERAL_SESSION_ACTIVE 1u
#define REMESA_GENERAL_SESSION_POISONED 2u
#define REMESA_GENERAL_SESSION_CLOSED 3u
struct remesa_general_session_request {
 __u32 version,action;
 __u64 context,session,sequence;
 __u32 ids[18],profile,budget_ms,max_jobs,flags;
 __u64 operation,cookie;
 __u32 core,revision,pds_base,phase,ledger;
 __s32 error;
 __u32 state,stages,reserved[2];
};
#define DRM_REMESA_GENERAL_SESSION 15
#define DRM_IOCTL_REMESA_GENERAL_SESSION DRM_IOWR(DRM_COMMAND_BASE+DRM_REMESA_GENERAL_SESSION,struct remesa_general_session_request)
static inline int remesa_general_session_request_valid(const struct remesa_general_session_request *r)
{
 unsigned i,j;
 if(!r||r->version!=1||!r->context||r->action<1||r->action>3||r->flags||
    r->operation||r->cookie||r->core||r->revision||r->pds_base||r->phase||r->ledger||
    r->error||r->state||r->stages||r->reserved[0]||r->reserved[1])return 0;
 if(r->action==1){
  if(r->session||r->sequence||(r->profile!=5&&r->profile!=6)||!r->max_jobs||r->max_jobs>64||r->budget_ms)return 0;
  for(i=0;i<18;i++){
   if(!r->ids[i])return 0;
   for(j=0;j<i;j++)if(r->ids[i]==r->ids[j])return 0;
  }
 }else{
  if(!r->session||r->profile||r->max_jobs)return 0;
  for(i=0;i<18;i++)if(r->ids[i])return 0;
  if(r->action==2){if(!r->sequence||!r->budget_ms||r->budget_ms>10000)return 0;}
  else if(r->budget_ms)return 0;
 }
 return 1;
}
#endif
