/* SPDX-License-Identifier: MIT */
#ifndef REMESA_SCENE_PACKET_H
#define REMESA_SCENE_PACKET_H
/* Internal capture-to-native metadata, NOT an ioctl ABI or GPU command list.
 * No address, register word, pointer or CODE. Backing is resolved by held IDs.
 * Native admission still authenticates all actual images and hardware state. */
#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/string.h>
typedef u32 remesa_scene_word;typedef u64 remesa_scene_owner;
#else
#include <stdint.h>
#include <string.h>
typedef uint32_t remesa_scene_word;typedef uint64_t remesa_scene_owner;
#endif
#define REMESA_SCENE_PACKET_BINDINGS 32u
#define REMESA_SCENE_PACKET_USES 55u
#define REMESA_SCENE_PACKET_WINDOWS 17u
struct remesa_scene_packet_actions {remesa_scene_word schema,bytes,width,height,region_id,table_id,parameter_id,reserved;};
struct remesa_scene_packet_controls {
 remesa_scene_word schema,bytes,width,height,color_pitch,depth_pitch,depth_cpp,clear_depth_bits;
 remesa_scene_word color_id,depth_id,ta_id,ta_offset,ta_bytes;
 remesa_scene_word background_id,background_offset,background_bytes,event_id,event_offset,event_bytes,depth_finalize;
};
struct remesa_scene_packet_use {remesa_scene_word id,byte_offset,program_bytes,bank,data_master,reserved;};
struct remesa_scene_packet_window {remesa_scene_word index_id,index_offset,index_bytes,vertex_id,vertex_offset,vertex_bytes,vertex_count,reserved;};
struct remesa_scene_packet {
 remesa_scene_owner owner,serial;
 remesa_scene_word version,binding_count,use_count,window_count;
 struct {remesa_scene_word id,access;} bindings[REMESA_SCENE_PACKET_BINDINGS];
 struct remesa_scene_packet_actions actions;
 struct remesa_scene_packet_controls controls;
 struct remesa_scene_packet_use uses[REMESA_SCENE_PACKET_USES];
 struct remesa_scene_packet_window windows[REMESA_SCENE_PACKET_WINDOWS];
};
#define REMESA_SCENE_PACKET_COMMAND_MAX (32u+80u+16u+24u*REMESA_SCENE_PACKET_USES+16u+32u*REMESA_SCENE_PACKET_WINDOWS)
struct remesa_scene_packet_sections {remesa_scene_word actions,controls,use,vertices;};
static inline int remesa_scene_packet_has_id(const struct remesa_scene_packet *p,remesa_scene_word id)
{unsigned i;for(i=0;i<p->binding_count;i++)if(p->bindings[i].id==id)return 1;return 0;}
/* Boolean metadata validation only. No mutation or initialization certificate. */
static inline int remesa_scene_packet_valid(const struct remesa_scene_packet *p)
{
 unsigned i,k;
 if(!p||!p->owner||!p->serial||p->version!=1||!p->binding_count||p->binding_count>32||
    !p->use_count||p->use_count>55||!p->window_count||p->window_count>17||
    p->actions.schema!=1||p->actions.bytes!=32||p->actions.reserved||
    !((p->controls.schema==1&&p->controls.depth_finalize==0)||
      (p->controls.schema==2&&p->controls.depth_finalize==1))||p->controls.bytes!=80||
    p->actions.width!=p->controls.width||p->actions.height!=p->controls.height)return 0;
 for(i=0;i<32;i++){
  if(i>=p->binding_count){if(p->bindings[i].id||p->bindings[i].access)return 0;continue;}
  if(!p->bindings[i].id||(p->bindings[i].access!=1&&p->bindings[i].access!=3))return 0;
  for(k=0;k<i;k++)if(p->bindings[k].id==p->bindings[i].id)return 0;
 }
 if(!remesa_scene_packet_has_id(p,p->actions.region_id)||!remesa_scene_packet_has_id(p,p->actions.table_id)||
    !remesa_scene_packet_has_id(p,p->actions.parameter_id)||!remesa_scene_packet_has_id(p,p->controls.color_id)||
    !remesa_scene_packet_has_id(p,p->controls.depth_id)||!remesa_scene_packet_has_id(p,p->controls.ta_id)||
    !remesa_scene_packet_has_id(p,p->controls.background_id)||!remesa_scene_packet_has_id(p,p->controls.event_id))return 0;
 for(i=0;i<55;i++){
  const struct remesa_scene_packet_use *e=&p->uses[i];
  if(i>=p->use_count){if(e->id||e->byte_offset||e->program_bytes||e->bank||e->data_master||e->reserved)return 0;continue;}
  if(!remesa_scene_packet_has_id(p,e->id)||!e->program_bytes||e->bank<3||e->bank>15||(e->data_master!=0&&e->data_master!=1&&e->data_master!=3)||e->reserved)return 0;
 }
 for(i=0;i<17;i++){
  const struct remesa_scene_packet_window *e=&p->windows[i];
  if(i>=p->window_count){if(e->index_id||e->index_offset||e->index_bytes||e->vertex_id||e->vertex_offset||e->vertex_bytes||e->vertex_count||e->reserved)return 0;continue;}
  if(!remesa_scene_packet_has_id(p,e->index_id)||!remesa_scene_packet_has_id(p,e->vertex_id)||
     !e->index_bytes||!e->vertex_bytes||!e->vertex_count||e->reserved)return 0;
 }
 return 1;
}
/* Compact the existing kernel-only immutable sections. Returns0 on refusal,
 * leaving output/count/sections untouched. Disjoint output/storage required. */
static inline int remesa_scene_packet_commands(const struct remesa_scene_packet *p,void *out,
 unsigned capacity,unsigned *written,struct remesa_scene_packet_sections *sections)
{
 unsigned use_bytes,vertex_bytes,total;unsigned char *b=out;
 remesa_scene_word header[4];struct remesa_scene_packet_sections s={0,32,112,0};
 if(!p||!out||!written||!sections||!remesa_scene_packet_valid(p))return 0;
 use_bytes=16+24*p->use_count;vertex_bytes=16+32*p->window_count;s.vertices=112+use_bytes;total=s.vertices+vertex_bytes;
 if(capacity<total)return 0;
 memcpy(b,&p->actions,32);memcpy(b+32,&p->controls,80);
 header[0]=1;header[1]=use_bytes;header[2]=p->use_count;header[3]=0;
 memcpy(b+112,header,16);memcpy(b+128,p->uses,24*p->use_count);
 header[1]=vertex_bytes;header[2]=p->window_count;
 memcpy(b+s.vertices,header,16);memcpy(b+s.vertices+16,p->windows,32*p->window_count);
 *written=total;*sections=s;return 1;
}
#endif
