/* SPDX-License-Identifier: MIT. Wire/layout/envelope tests only; GPU0. */
#include "remesa/gears_scene_probe_envelope.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
 _Static_assert(sizeof(struct remesa_scene_probe_request)==144,"old request unchanged");
 _Static_assert(offsetof(struct remesa_scene_probe_request,operation)==88,"old prefix unchanged");
 _Static_assert(sizeof(struct remesa_scene_probe_trace)==4256,"old trace unchanged");
 _Static_assert(sizeof(struct remesa_scene_trace_request)==4272,"old query unchanged");
 _Static_assert(sizeof(struct remesa_gears_scene_probe_request)==152,"new request");
 _Static_assert(offsetof(struct remesa_gears_scene_probe_request,operation)==96,"new prefix");
 _Static_assert(sizeof(struct remesa_gears_scene_trace)==4280,"new trace");
 _Static_assert(offsetof(struct remesa_gears_scene_trace,last_binding)==4256,"complete last binding");
 _Static_assert(sizeof(struct remesa_gears_scene_trace_request)==4296,"new query");
 assert(DRM_IOCTL_REMESA_GEARS_SCENE_PROBE!=DRM_IOCTL_REMESA_SCENE_PROBE&&DRM_IOCTL_REMESA_GEARS_SCENE_TRACE!=DRM_IOCTL_REMESA_SCENE_TRACE);
 struct remesa_gears_scene_probe_request r={.version=2,.profile=1,.context=77,.budget_ms=1};
 for(unsigned i=0;i<18;i++)r.ids[i]=32+i;
 assert(remesa_gears_scene_probe_request_valid(&r));r.budget_ms=10000;for(r.profile=1;r.profile<=4;r.profile++)assert(remesa_gears_scene_probe_request_valid(&r));r.profile=4;
#define BAD(field,value) do{struct remesa_gears_scene_probe_request bad=r;bad.field=value;assert(!remesa_gears_scene_probe_request_valid(&bad));}while(0)
 BAD(version,1);BAD(profile,0);BAD(profile,5);BAD(context,0);BAD(budget_ms,0);BAD(budget_ms,10001);BAD(reserved_input,1);
 BAD(operation,1);BAD(cookie,1);BAD(core,1);BAD(revision,1);BAD(pds_base,1);BAD(phase,1);BAD(ledger,1);BAD(error,-1);BAD(retained,1);BAD(stages,1);BAD(reserved[0],1);BAD(reserved[1],1);
#undef BAD
 for(unsigned i=0;i<18;i++){struct remesa_gears_scene_probe_request bad=r;bad.ids[i]=0;assert(!remesa_gears_scene_probe_request_valid(&bad));for(unsigned j=0;j<18;j++)if(i!=j){bad=r;bad.ids[i]=r.ids[j];assert(!remesa_gears_scene_probe_request_valid(&bad));}}
 assert(!remesa_gears_scene_probe_request_valid(NULL));
 struct remesa_gears_scene_trace_request q={.version=2,.context=77};assert(remesa_gears_scene_trace_request_valid(&q));
 q.version=1;assert(!remesa_gears_scene_trace_request_valid(&q));q.version=2;q.context=0;assert(!remesa_gears_scene_trace_request_valid(&q));q.context=77;q.reserved=1;assert(!remesa_gears_scene_trace_request_valid(&q));q.reserved=0;
 unsigned char *bytes=(unsigned char *)&q.trace;
 for(unsigned i=0;i<sizeof q.trace;i++){bytes[i]=1;assert(!remesa_gears_scene_trace_request_valid(&q));bytes[i]=0;}
 assert(remesa_gears_scene_trace_request_valid(&q)&&!remesa_gears_scene_trace_request_valid(NULL));
 puts("PASS old/new layouts separated, all18 zero/duplicate IDs and response/reserved fields refused, every4280 trace input bytes must be zero; GPU0");
}
