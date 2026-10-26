/* SPDX-License-Identifier: MIT */
#include "remesa_depth_scene_image.h"
#include "remesa_shader_program.h"
#include "remesa_fragment_body.h"
#include "remesa_pds_structure.h"
#include "remesa_ta_scene.h"
#include "remesa_ta_index.h"
#include "remesa_ta_zclear.h"
#include <errno.h>
#include <math.h>
#include <string.h>
static int span(const void *p,size_t n)
{return p&&n<=UINTPTR_MAX-(uintptr_t)p;}
static int overlap(const void *a,size_t n,const void *b,size_t m)
{return (uintptr_t)a<(uintptr_t)b+m&&(uintptr_t)b<(uintptr_t)a+n;}
static size_t reserve(size_t *cursor,size_t n,size_t alignment)
{size_t at=(*cursor+alignment-1)&~(alignment-1);*cursor=at+n;return at;}
static void put32(uint8_t *p,uint32_t n)
{for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static void putfloat(uint8_t *p,float f)
{uint32_t n;memcpy(&n,&f,4);put32(p,n);}
static int entry(const struct remesa_depth_scene_image_request *r,size_t offset,
 size_t length,uint32_t bank,enum remesa_use_data_master master,uint32_t *fixup,struct remesa_use_entry_plan *out)
{
 struct remesa_use_entry_request q={.allocation_address=r->gpu_va,
  .allocation_bytes=r->allocation_bytes,.entry_offset=offset,.program_bytes=length,
  .bank=bank,.data_master=master};
 struct remesa_use_entry_plan plan;int e=remesa_use_entry_prepare(&q,&plan);
 if(e)return e;
 e=remesa_use_entry_preview_fixup(&plan,fixup);if(!e&&out)*out=plan;return e;
}
int remesa_depth_scene_image_encode(const struct remesa_depth_scene_image_request *request,
 struct remesa_depth_scene_image *out)
{
 if(!span(request,sizeof(*request))||!span(out,sizeof(*out))||
    (uintptr_t)request%_Alignof(struct remesa_depth_scene_image_request)||
    (uintptr_t)out%_Alignof(struct remesa_depth_scene_image)||
    overlap(request,sizeof(*request),out,sizeof(*out)))return EINVAL;
 const struct remesa_depth_scene_image_request r=*request;
 if(!r.gpu_va||(r.gpu_va&4095)||r.gpu_va>UINT32_MAX||
    r.allocation_bytes>UINT64_C(0x100000000)-r.gpu_va)return EINVAL;
 if(r.allocation_bytes<REMESA_DEPTH_SCENE_IMAGE_BYTES)return ENOSPC;
 if(!r.width||!r.height||r.width>2048||r.height>2048)return ENOTSUP;
 if((r.compare!=REMESA_DEPTH_LESS&&r.compare!=REMESA_DEPTH_ALWAYS)||
    r.vertex_bank==r.pixel_bank||r.pixel_bank==r.state_bank)return ENOTSUP;
 if(r.vertex_profile!=REMESA_DEPTH_VERTEX_COPY&&
    r.vertex_profile!=REMESA_DEPTH_VERTEX_COMPILED_POSITION4)return ENOTSUP;
 const bool compiled_vertex=r.vertex_profile==REMESA_DEPTH_VERTEX_COMPILED_POSITION4;
 if(r.vertex_bank>15||r.pixel_bank>15||r.state_bank>15)return ERANGE;
 for(unsigned i=0;i<3;i++) {
  if(!isfinite(r.draw_depth[i])||r.draw_depth[i]<0||r.draw_depth[i]>1)return EINVAL;
  for(unsigned c=0;c<2;c++)if(!isfinite(r.triangle[i][c])||r.triangle[i][c]<0||
    r.triangle[i][c]>(float)(c?r.height:r.width))return EINVAL;
 }
 struct remesa_shader_image_view shaders[3];const uint32_t colors[]={0xff00ff00,0xffff00ff,0xffffff00};
 for(unsigned i=0;i<3;i++) {
  int e=remesa_shader_image_inspect(r.programs[i],&shaders[i]);if(e)return e;
  const struct remesa_shader_image_view *v=&shaders[i];
  if(v->vertex.byte_length!=16||v->fragment.byte_length!=8||v->fragment_form!=REMESA_SHADER_PACKED_CONSTANT)
   return ENOTSUP;
  if(overlap(v->vertex.bytes,16,out,sizeof(*out))||overlap(v->fragment.bytes,8,out,sizeof(*out))||
     overlap(v->programs,sizeof(*v->programs),out,sizeof(*out)))return EINVAL;
  struct remesa_fragment_constant color;
  e=remesa_fragment_constant_prepare(&v->programs->fragment.usse,&color);if(e)return e;
  if(color.argb!=colors[i])return ENOTSUP;
  if(i&&memcmp(v->vertex.bytes,shaders[0].vertex.bytes,16))return ENOTSUP;
 }
 struct remesa_depth_scene_image image={0};image.vertex_profile=r.vertex_profile;size_t cursor=0;
 image.vertex_code=reserve(&cursor,16,16);
 for(unsigned i=0;i<3;i++)image.fragment_code[i]=reserve(&cursor,8,16);
 for(unsigned i=0;i<4;i++)image.vertices[i]=reserve(&cursor,i?48:64,16);
 for(unsigned i=0;i<4;i++)image.indices[i]=reserve(&cursor,i?6:12,4);
 for(unsigned i=0;i<4;i++)image.vertex_pds[i]=reserve(&cursor,64,32);
 for(unsigned i=0;i<3;i++)image.fragment_pds[i]=reserve(&cursor,56,32);
 image.secondary_pds=reserve(&cursor,4,32);
 const size_t lengths[]={44,52,16,16,8};
 for(unsigned i=0;i<5;i++){image.state_bytes[i]=lengths[i];image.state_data[i]=reserve(&cursor,lengths[i],16);}
 for(unsigned i=0;i<5;i++)image.state_use[i]=reserve(&cursor,16,16);
 for(unsigned i=0;i<5;i++)image.state_pds[i]=reserve(&cursor,60,32);
 image.ta_offset=reserve(&cursor,124,16);image.ta_bytes=124;image.used_bytes=cursor;
 if(cursor>sizeof(image.bytes))return ENOSPC;
 #define BYTES(offset) (image.bytes+(offset))
 #define KNOWN(offset,n) memset(image.known+(offset),255,(n))
 memcpy(BYTES(image.vertex_code),shaders[0].vertex.bytes,16);KNOWN(image.vertex_code,16);
 for(unsigned i=0;i<3;i++){memcpy(BYTES(image.fragment_code[i]),shaders[i].fragment.bytes,8);KNOWN(image.fragment_code[i],8);}
 const struct remesa_ta_zclear_request clear={r.width,r.height,r.width,r.height,r.clear_depth};
 struct remesa_ta_zclear_geometry geometry;int e=remesa_ta_zclear_geometry_encode(&clear,&geometry);if(e)return e;
 memcpy(BYTES(image.vertices[0]),geometry.positions,64);KNOWN(image.vertices[0],64);
 memcpy(BYTES(image.indices[0]),geometry.indices,12);KNOWN(image.indices[0],12);
 for(unsigned i=1;i<4;i++)for(unsigned v=0;v<3;v++) {
  size_t at=image.vertices[i]+v*16;
  putfloat(BYTES(at),r.triangle[v][0]);putfloat(BYTES(at+4),r.triangle[v][1]);
  putfloat(BYTES(at+8),r.draw_depth[i-1]);putfloat(BYTES(at+12),1);
  image.bytes[image.indices[i]+2*v]=(uint8_t)v;
 }
 for(unsigned i=1;i<4;i++){KNOWN(image.vertices[i],48);KNOWN(image.indices[i],6);}
 uint32_t vs_fixup,fs_fixup[3];
 e=entry(&r,image.vertex_code,16,r.vertex_bank,REMESA_USE_VERTEX,&vs_fixup,&image.use_entries[0]);if(e)return e;
 for(unsigned i=0;i<3;i++) {
  struct remesa_use_entry_plan plan;
  e=entry(&r,image.fragment_code[i],8,r.pixel_bank,REMESA_USE_PIXEL,&fs_fixup[i],&plan);if(e)return e;
  if(!i)image.use_entries[1]=plan;
  else if(plan.register_offset!=image.use_entries[1].register_offset||
          plan.register_word!=image.use_entries[1].register_word)return ENOTSUP;
 }
 for(unsigned i=0;i<4;i++) {
  const struct remesa_pds_vertex_image_request q={r.gpu_va,r.allocation_bytes,image.vertices[i],4,vs_fixup};
  size_t n=0;
  if(compiled_vertex)e=remesa_pds_structure_encode_compiled_vertex_image(&q,
    BYTES(image.vertex_pds[i]),image.known+image.vertex_pds[i],64,&n);
  else e=remesa_pds_structure_encode_vertex_image(&q,BYTES(image.vertex_pds[i]),image.known+image.vertex_pds[i],64,&n);
  if(e)return e;
 }
 for(unsigned i=0;i<3;i++) {
  const struct remesa_pds_structure_request q={.code_policy=REMESA_PDS_STRUCTURE_CODE_CPU_COMPLETE};
  struct remesa_pds_structure pds;size_t n=0;e=remesa_pds_structure_encode(&q,&pds);if(e)return e;
  e=remesa_pds_structure_serialize(&pds,BYTES(image.fragment_pds[i]),image.known+image.fragment_pds[i],56,&n);if(e)return e;
  if(n!=56||pds.relocation_count!=1||pds.relocations[0].background||pds.relocations[0].data_cell)return EPROTO;
  put32(BYTES(image.fragment_pds[i]),fs_fixup[i]);
 }
 const struct remesa_pds_code_instruction secondary={REMESA_PDS_STRUCTURE_CPU_EMPTY_SECONDARY_AF,
  REMESA_PDS_STRUCTURE_NO_CELL,REMESA_PDS_STRUCTURE_NO_CELL};size_t n=0;
 e=remesa_pds_structure_encode_code(&secondary,1,BYTES(image.secondary_pds),4,&n);if(e)return e;
 KNOWN(image.secondary_pds,4);
 for(unsigned i=0;i<4;i++) {
  unsigned fs=i?i-1:0;
  struct remesa_ta_fragment_binding_request binding={.address_window_base=r.gpu_va&~UINT64_C(0xfffffff),
   .primary={r.gpu_va,r.allocation_bytes,image.fragment_pds[fs],56,12,0,i?1:0},
   .secondary={r.gpu_va,r.allocation_bytes,image.secondary_pds,4,0,0,0},
   .profile=i?REMESA_TA_FRAGMENT_AUTHORED_CONSTANT:REMESA_TA_FRAGMENT_SCENE_ZCLEAR};
  if(!i)e=remesa_ta_scene_zclear_state_encode(&binding,BYTES(image.state_data[i]));
  else if(i==1) {
   const struct remesa_ta_constant_draw_state_request q={
    .isp={true,true,r.depth_write,r.compare,15},.fragment=binding,
    .viewport={r.width,r.height,0,0,r.width,r.height,24,0,1,REMESA_TA_VIEWPORT_INITIAL_ZERO},
    .scalar_profile=REMESA_TA_SCALAR_POSITION4};
   e=remesa_ta_constant_draw_after_zclear_encode(&q,BYTES(image.state_data[i]),52,&n);
  } else e=remesa_ta_fragment_binding_encode(&binding,BYTES(image.state_data[i]));
  if(e)return e;
  KNOWN(image.state_data[i],lengths[i]);
 }
 e=remesa_ta_scene_close_data_encode(r.width,r.height,BYTES(image.state_data[4]));if(e)return e;KNOWN(image.state_data[4],8);
 for(unsigned i=0;i<5;i++) {
  uint32_t words=(uint32_t)(lengths[i]/4),fixup;
  const struct remesa_usse_cpu_move move={.source={2,0},.destination={1,0},.control12=words-1,.control21=1,.control23=1};
  const struct remesa_usse_cpu_three_source tail={.source={{0,0},{2,words},{2,0}},.source_extension={0,1,1},.word0_control21=1,.word1_control18=1};
  e=remesa_usse_cpu_move_encode(&move,BYTES(image.state_use[i]),8,&n);if(e)return e;
  e=remesa_usse_cpu_three_source_encode(&tail,BYTES(image.state_use[i]+8),8,&n);if(e)return e;KNOWN(image.state_use[i],16);
  struct remesa_use_entry_plan plan;
  e=entry(&r,image.state_use[i],16,r.state_bank,REMESA_USE_VERTEX,&fixup,&plan);if(e)return e;
  if(!i)image.use_entries[2]=plan;
  else if(plan.register_offset!=image.use_entries[2].register_offset||
          plan.register_word!=image.use_entries[2].register_word)return ENOTSUP;
  const struct remesa_pds_state_image_request q={.source_allocation_address=r.gpu_va,
   .source_allocation_bytes=r.allocation_bytes,.source_offset=image.state_data[i],.state_dwords=words,.use_lower19=fixup};
  e=remesa_pds_structure_encode_state_image(&q,BYTES(image.state_pds[i]),image.known+image.state_pds[i],60,&n);if(e)return e;
  /* VS and state-upload entries are both VERTEX. Sharing a bank is valid
   * for this CPU plan only when its complete register word agrees; unlike
   * pixel/vertex, no distinct data-master word is silently overwritten. */
  if(r.vertex_bank==r.state_bank&&
     image.use_entries[0].register_word!=plan.register_word)return ENOTSUP;
  size_t ta=image.ta_offset+i*28;
  if(i==4) {
   const struct remesa_ta_scene_close_request close={r.gpu_va,r.allocation_bytes,image.state_pds[i],60};
   e=remesa_ta_scene_close_encode(&close,BYTES(ta));if(e)return e;KNOWN(ta,12);
  } else {
   e=remesa_ta_state_reference_encode(r.gpu_va+image.state_pds[i],words,false,BYTES(ta));if(e)return e;
   const struct remesa_ta_index_request draw={r.gpu_va,r.allocation_bytes,image.indices[i],
    r.gpu_va,r.allocation_bytes,image.vertex_pds[i],64,0,i?3:6,12,4,compiled_vertex?16:4};
   if(compiled_vertex)e=remesa_ta_index_compiled_position_triangles_encode(&draw,BYTES(ta+8));
   else e=remesa_ta_index_triangles_encode(&draw,BYTES(ta+8));
   if(e)return e;
   KNOWN(ta,28);
  }
 }
 memcpy(out,&image,sizeof(image));return 0;
 #undef BYTES
 #undef KNOWN
}
