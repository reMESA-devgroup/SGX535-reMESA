/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_SCENE_PROBE_REQUEST_H
#define REMESA_SCENE_PROBE_REQUEST_H
#include "remesa_scene_probe_uapi.h"
static inline int remesa_scene_probe_request_valid(const struct remesa_scene_probe_request *r)
{
 unsigned i,j;
 if(!r||r->version!=1||r->profile<1||r->profile>2||!r->context||!r->budget_ms||r->budget_ms>10000||
    r->operation||r->cookie||r->core||r->revision||r->pds_base||r->phase||r->ledger||r->error||
    r->retained||r->stages||r->reserved[0]||r->reserved[1])return 0;
 for(i=0;i<17;i++){
  if(!r->ids[i])return 0;
  for(j=0;j<i;j++)if(r->ids[i]==r->ids[j])return 0;
 }
 return 1;
}
#endif
