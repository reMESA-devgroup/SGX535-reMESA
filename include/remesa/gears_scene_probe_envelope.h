/* SPDX-License-Identifier: MIT */
#ifndef REMESA_GEARS_SCENE_PROBE_ENVELOPE_H
#define REMESA_GEARS_SCENE_PROBE_ENVELOPE_H
#include "gears_scene_probe_uapi.h"
/* Pure envelope checks only, not address/image/native execution admission.
 * Kernel callers must retain capability, owner, default-off and source gates. */
static inline int remesa_gears_scene_probe_request_valid(const struct remesa_gears_scene_probe_request *r)
{
 unsigned i,j;
 if(!r||r->version!=2||!r->profile||r->profile>REMESA_GEARS_SCENE_PROFILE_COUNT||
    !r->context||!r->budget_ms||r->budget_ms>10000||r->reserved_input||
    r->operation||r->cookie||r->core||r->revision||r->pds_base||r->phase||r->ledger||
    r->error||r->retained||r->stages||r->reserved[0]||r->reserved[1])return 0;
 for(i=0;i<18;i++){
  if(!r->ids[i])return 0;
  for(j=0;j<i;j++)if(r->ids[i]==r->ids[j])return 0;
 }
 return 1;
}
static inline int remesa_gears_scene_trace_request_valid(const struct remesa_gears_scene_trace_request *r)
{
 unsigned i;const unsigned char *bytes;
 if(!r||r->version!=2||r->reserved||!r->context)return 0;
 bytes=(const unsigned char *)&r->trace;
 for(i=0;i<sizeof(r->trace);i++)if(bytes[i])return 0;
 return 1;
}
#endif
