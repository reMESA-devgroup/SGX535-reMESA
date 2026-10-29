/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_PRIVATE_PROBE_REQUEST_H
#define REMESA_PRIVATE_PROBE_REQUEST_H
#include "remesa_private_probe_uapi.h"
/* Pure wire admission before endpoint access, allocation or device reads. */
static inline int remesa_private_probe_request_valid(const struct remesa_private_probe_request *r)
{
 unsigned i,j;
 if(!r||r->version!=REMESA_PRIVATE_PROBE_VERSION||r->flags||!r->context||
    !r->budget_ms||r->budget_ms>10000||r->operation||r->cookie||r->core||
    r->revision||r->phase||r->ledger||r->error||r->retained||r->stages||r->reserved)return 0;
 for(i=0;i<11;i++){
  if(!r->ids[i])return 0;
  for(j=0;j<i;j++)if(r->ids[i]==r->ids[j])return 0;
 }
 return 1;
}
#endif
