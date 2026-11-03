/* SPDX-License-Identifier: MIT */
#ifndef REMESA_MATRIX_SUBMIT_UAPI_H
#define REMESA_MATRIX_SUBMIT_UAPI_H
#include <linux/types.h>
#include <drm/drm.h>
#include "scene_packet.h"
/* Experimental ordinary matrix draw bridge, separate from closed LAB
 * catalogues. Default-disabled/root-only; one consumed source attempt per
 * boot until normal source lifetime is qualified. Not a permanent DRM ABI.
 * Packet contains owned IDs/typed metadata, never pointers, VA/register words
 * or CODE. All response fields enter zero. ioctl0 is a structured receipt,
 * not GPU success; admission/execution/retirement/closure must be verified. */
#define REMESA_MATRIX_SUBMIT_VERSION 1u
/* Optional normal-session lifetime; flags0 keeps its qualified one-shot ABI.
 * READY is job retirement with source/backing retained, never CLOSED. CLOSE
 * has an empty packet/budget and cannot issue a GPU command. */
#define REMESA_MATRIX_KEEP_SESSION 1u
#define REMESA_MATRIX_CLOSE_SESSION 2u
#define REMESA_MATRIX_SESSION_READY 16u
#define REMESA_MATRIX_SUBMIT_MAX_VERTICES 4095u /* existing frontend construction bound, not a hardware limit */
struct remesa_matrix_submit_request {
 __u32 version,budget_ms,flags,reserved;
 __u64 context;
 struct remesa_scene_packet packet;
 __u64 operation,cookie;
 __u32 core,revision,pds_base,phase,ledger;
 __s32 error;
 __u32 retained,stages,response_reserved[2];
};
#define DRM_REMESA_MATRIX_SUBMIT 17
#define DRM_IOCTL_REMESA_MATRIX_SUBMIT DRM_IOWR(DRM_COMMAND_BASE+DRM_REMESA_MATRIX_SUBMIT,struct remesa_matrix_submit_request)
static inline int remesa_matrix_submit_request_valid(const struct remesa_matrix_submit_request *r)
{
 unsigned i;const struct remesa_scene_packet *p;
 if(!r||r->version!=1||!r->context||r->reserved||
    r->operation||r->cookie||r->core||r->revision||r->pds_base||r->phase||r->ledger||r->error||
    r->retained||r->stages||r->response_reserved[0]||r->response_reserved[1])return 0;
 if(r->flags==REMESA_MATRIX_CLOSE_SESSION){
  const unsigned char *bytes=(const unsigned char *)&r->packet;
  if(r->budget_ms)return 0;
  for(i=0;i<sizeof(r->packet);i++)if(bytes[i])return 0;
  return 1;
 }
 if((r->flags&~REMESA_MATRIX_KEEP_SESSION)||!r->budget_ms||r->budget_ms>10000)return 0;
 p=&r->packet;
 if(!remesa_scene_packet_valid(p)||p->owner!=r->context||p->binding_count!=18||p->use_count!=11||p->window_count!=2||
    !p->controls.width||p->controls.width>=2048||!p->controls.height||p->controls.height>=2048||
    p->controls.schema!=2||p->controls.depth_finalize!=1||p->controls.ta_bytes!=104||
    p->windows[0].vertex_count!=3||p->windows[0].vertex_bytes!=48||p->windows[0].index_bytes!=6||
    p->windows[1].vertex_count<3||p->windows[1].vertex_count>REMESA_MATRIX_SUBMIT_MAX_VERTICES||
    p->windows[1].vertex_count%3||p->windows[1].vertex_bytes!=p->windows[1].vertex_count*48||
    p->windows[1].index_bytes!=p->windows[1].vertex_count*2)return 0;
 for(i=0;i<p->use_count;i++){
  if(p->uses[i].data_master!=0&&p->uses[i].data_master!=1)return 0;
  if(p->uses[i].bank!=(p->uses[i].data_master?3U:4U))return 0;
 }
 return 1;
}
#endif
