/* SPDX-License-Identifier: MIT */
/* Reuse the injected-provider fixture, never the hardware backend. This builds
 * the complete ordered TA bytes and fetched allocation set from live owners.
 * It does not supply native admission, PFNs, requester coverage or execution. */
int persistent_fixture_regressions(void);
#define main persistent_fixture_regressions
#include "test_persistent_shader.c"
#undef main
#include "remesa_ta_zclear.h"
#include "remesa_ta_index.h"
static void use_once(uint32_t id,uint32_t *ids,unsigned *count)
{
 CHECK(id);
 for(unsigned i=0;i<*count;i++)if(ids[i]==id)return;
 CHECK(*count<32);ids[(*count)++]=id;
}
int main(void)
{
 struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
 struct remesa_persistent_config cfg={.ops=&ops,.priv=&f,.context=1500,
  .va_start=0x40000000,.va_end=0x40100000,.byte_budget=32*4096,.resource_only_backend=true};
 CHECK(!remesa_persistent_open(&d,&cfg));
 void *z=NULL,*close=NULL,*tokens[4]={0},*states[4]={0};
 CHECK(!remesa_persistent_depth_allocate(&d,32,32,4,&z));
 struct remesa_persistent_depth_backing zb;CHECK(!remesa_persistent_depth_snapshot(&d,z,&zb));
 const struct remesa_depth_prefix_request prefix_request={
  .depth={32,32,zb.pitch_pixels,4,REMESA_DEPTH_SURFACE_CLEAR,zb.gpu_va,zb.allocation_bytes},
  .color_width=32,.color_height=32,.clear_depth=1};
 uint8_t prefix[64];CHECK(!remesa_depth_prefix_encode(&prefix_request,prefix));
 struct remesa_ta_zclear_geometry clear_geometry;
 const struct remesa_ta_zclear_request clear_request={32,32,32,32,1};
 CHECK(!remesa_ta_zclear_geometry_encode(&clear_request,&clear_geometry));
 const uint32_t colors[]={0xff00ff00,0xff00ff00,0xffff00ff,0xffffff00};
 const uint32_t headers[]={0x54c5,0xb41,0x40,0x40};
 const size_t state_lengths[]={44,52,16,16};
 const float depths[]={.75f,.25f,.75f};
 float vertices[3][3][4];const uint16_t indices[3]={0,1,2};
 uint8_t stream[124]={0};size_t cursor=0;uint32_t uses[32]={0};unsigned use_count=0;
 use_once(zb.handle,uses,&use_count);
 for(unsigned i=0;i<4;i++) {
  struct remesa_shader_draw_programs pair=programs(colors[i]);
  struct remesa_usse_vertex_mapping mapping={0};
  for(unsigned c=0;c<4;c++) {mapping.input[c]=(struct remesa_usse_cpu_register){2,c};
   mapping.output[c]=(struct remesa_usse_cpu_register){1,c};}
  CHECK(remesa_persistent_prepare_vertex_candidate(&d,&pair,&mapping,&tokens[i])==ENOTSUP&&tokens[i]);
  const uint32_t banks[2]={2,3};CHECK(!remesa_persistent_shader_prepare_gpu(&d,tokens[i],banks));
  struct remesa_shader_draw_inputs in;
  if(!i) {
   in=geometry_inputs(clear_geometry.positions,sizeof(clear_geometry.positions),NULL);
   in.index_data=clear_geometry.indices;in.index_bytes=sizeof(clear_geometry.indices);in.index_size=2;in.count=6;
  } else {
   const float xy[3][2]={{4,4},{28,4},{4,27}};
   for(unsigned v=0;v<3;v++){vertices[i-1][v][0]=xy[v][0];vertices[i-1][v][1]=xy[v][1];
    vertices[i-1][v][2]=depths[i-1];vertices[i-1][v][3]=1;}
   in=geometry_inputs(vertices[i-1],sizeof(vertices[i-1]),NULL);
   in.index_data=indices;in.index_bytes=sizeof(indices);in.index_size=2;
  }
  CHECK(!remesa_persistent_shader_stage_draw(&d,tokens[i],&in));
  CHECK(!remesa_persistent_shader_prepare_draw_gpu(&d,tokens[i]));
  CHECK(!remesa_persistent_shader_prepare_pds_gpu(&d,tokens[i]));
  struct remesa_persistent_constant_state_request state=constant_state_request();
  int e=!i?remesa_persistent_zclear_state_prepare(&d,tokens[i],4,&states[i]):i==1?
   remesa_persistent_after_zclear_state_prepare(&d,tokens[i],&state,4,&states[i]):
   remesa_persistent_fragment_state_prepare(&d,tokens[i],4,&states[i]);
  CHECK(!e);
  struct remesa_persistent_scene_state_backing sb;CHECK(!remesa_persistent_scene_state_snapshot(&d,states[i],&sb));
  CHECK(sb.image.state_bytes==state_lengths[i]);
  const uint8_t *sbytes=get(&f,sb.image.handle)->bytes;
  CHECK(shader_pds_le32(sbytes)==headers[i]);
  memcpy(stream+cursor,sbytes+sb.image.ta_offset,8);cursor+=8;
  struct remesa_persistent_draw_gpu_backing raw;
  struct remesa_persistent_shader_pds_gpu_backing pds;
  CHECK(!remesa_persistent_shader_draw_gpu_backing(&d,tokens[i],&raw));
  CHECK(!remesa_persistent_shader_pds_gpu_backing(&d,tokens[i],&pds));
  const struct remesa_ta_index_request draw={
   .index_bo_gpu_va=raw.gpu_va,.index_bo_bytes=raw.allocation_bytes,.index_byte_offset=raw.index_offset,
   .pds_bo_gpu_va=pds.gpu_va,.pds_bo_bytes=pds.allocation_bytes,.pds_byte_offset=pds.object_offset[0],
   .pds_object_bytes=pds.object_bytes[0],.index_count=in.count,.pds_data_dwords=12,
   .descriptor_a_field10=4,.descriptor_b_field38=4};
  CHECK(!remesa_ta_index_triangles_encode(&draw,stream+cursor));cursor+=20;
  use_once(sb.shader_code_handle,uses,&use_count);use_once(sb.shader_pds_handle,uses,&use_count);
  use_once(raw.handle,uses,&use_count);use_once(sb.image.handle,uses,&use_count);
 }
 CHECK(cursor==112);
 CHECK(!remesa_persistent_scene_close_prepare(&d,32,32,4,&close));
 struct remesa_persistent_scene_close_backing end;CHECK(!remesa_persistent_scene_close_snapshot(&d,close,&end));
 memcpy(stream+cursor,get(&f,end.image.handle)->bytes+end.image.ta_offset,12);cursor+=12;
 use_once(end.image.handle,uses,&use_count);
 CHECK(cursor==sizeof(stream)&&use_count==18&&shader_pds_le32(stream+120)==0xc0000000);
 for(unsigned i=0;i<4;i++) {
  CHECK(shader_pds_le32(stream+i*28+8)==(0x81400000u|(i?3u:6u)));
  d.ops->destroy_shader_programs(d.priv,tokens[i]);
  struct remesa_persistent_scene_state_backing retained;
  CHECK(!remesa_persistent_scene_state_snapshot(&d,states[i],&retained));
 }
 CHECK(remesa_persistent_close(&d)==EBUSY);
 for(unsigned i=4;i;i--)d.ops->destroy_bo(d.priv,states[i-1]);
 d.ops->destroy_bo(d.priv,close);d.ops->destroy_bo(d.priv,z);
 CHECK(!f.forbidden&&f.creates==f.destroys&&f.pins==f.unpins);
 CHECK(!remesa_persistent_close(&d));
 printf("PASS: owned depth scene CPU composition: clear+3draw+close, TA124 bytes,18 fetched owners, deferred destruction; NO GPU/native job admission (%u checks)\n",checks);
 return 0;
}
