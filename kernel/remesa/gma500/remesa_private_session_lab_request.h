/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_PRIVATE_SESSION_LAB_REQUEST_H
#define REMESA_PRIVATE_SESSION_LAB_REQUEST_H
#include "remesa_private_session_lab_uapi.h"
static inline int remesa_private_session_lab_request_valid(const struct remesa_private_session_lab_request *r)
{
 unsigned i,k;
 if(!r||r->version!=1||!r->context||r->flags||r->reserved||r->operation||r->cookie||
    r->core||r->revision||r->phase||r->ledger||r->error||r->state||r->stages)return 0;
 if(r->action==REMESA_PRIVATE_SESSION_LAB_OPEN){
  if(r->session||r->sequence||r->budget_ms||!r->max_jobs||r->max_jobs>64)return 0;
  for(i=0;i<11;i++){
   if(!r->ids[i])return 0;
   for(k=0;k<i;k++)if(r->ids[k]==r->ids[i])return 0;
  }
 }else{
  if(!r->session||r->max_jobs)return 0;
  for(i=0;i<11;i++)if(r->ids[i])return 0;
  if(r->action==REMESA_PRIVATE_SESSION_LAB_SUBMIT){
   if(!r->sequence||!r->budget_ms||r->budget_ms>10000)return 0;
  }else if(r->action!=REMESA_PRIVATE_SESSION_LAB_CLOSE||r->budget_ms)return 0;
 }
 return 1;
}
#endif
