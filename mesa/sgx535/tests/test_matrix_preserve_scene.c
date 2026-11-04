/* SPDX-License-Identifier: MIT. Constructor/ownership test, no GPU execution. */
#define REMESA_GEARS_SCENE_BUILDER_FIXTURE
int inherited_gears_scene_nir_tests(void);
#include "test_gears_scene_builder.c"
#include "remesa/matrix_preserve_ta.h"
#include "remesa/matrix_submit_uapi.h"
int main(void)
{
#if defined(REMESA_WITH_NIR) && defined(REMESA_TEST_REAL_NIR_BUILDER)
 glsl_type_singleton_init_or_ref();
#endif
 const unsigned sizes[][2]={{640,640},{1280,800},{1600,768}};
 for(unsigned i=0;i<3;i++){
  unsigned width=sizes[i][0],height=sizes[i][1];
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};
  target_ops=ops;target_ops.create=builder_create;struct remesa_device d={0};
  struct remesa_persistent_config cfg={.ops=&target_ops,.priv=&f,.context=12000+i,
   .va_start=0x20000000,.va_end=0x45000000,.byte_budget=96u*1024u*1024u,
   .resource_only_backend=true,.resource_limits={2048,2048,128,16u*1024u*1024u}};
  cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};
  cfg.placement[REMESA_VA_RASTER]=(struct remesa_persistent_va_range){0x30000000,0x34000000,1048576};
  cfg.placement[REMESA_VA_SURFACE]=(struct remesa_persistent_va_range){0x40001000,0x45000000,4096};
  CHECK(!remesa_persistent_open(&d,&cfg));void *color=NULL,*depth=NULL,*parameters=NULL;
  CHECK(!remesa_persistent_allocate_placed(&d,(size_t)width*height*4,REMESA_VA_SURFACE,&color));
  CHECK(!remesa_persistent_depth_allocate(&d,width,height,4,&depth));
  struct remesa_shader_program original=vertex(1),vs,fs=fragment(1);struct remesa_shader_linkage link;
  CHECK(!remesa_shader_program_clamp_vertex_color(&original,&vs));
  CHECK(!remesa_shader_linkage_prepare(&vs,&fs,&link));
  uint32_t coefficients[36]={0};coefficients[0]=coefficients[5]=coefficients[10]=coefficients[15]=0x3f800000;
  coefficients[16]=coefficients[21]=coefficients[26]=0x3f800000;
  coefficients[31]=0x40000000;coefficients[34]=0x3f800000;
  CHECK(!remesa_persistent_vertex_parameter_block_prepare(&d,coefficients,36,40,4,&parameters));
  const float raw[3][12]={{-.5f,-.5f,0,1,0,0,1,0},{.5f,-.5f,0,1,0,0,1,0},{-.5f,.5f,0,1,0,0,1,0}};
  struct remesa_linked_scene_request request={.color=color,.depth=depth,.width=width,.height=height,
   .color_pitch=width,.linkage=&link,.geometry={.vertices=raw,.vertex_bytes=sizeof raw,.vertex_count=3},
   .state={.isp={.depth_bits_present=true,.depth_test=true,.depth_write=true,.compare=REMESA_DEPTH_LESS,.rgba_mask=15},
    .viewport={.framebuffer_width=width,.framebuffer_height=height,.width=width,.height=height,
     .depth_bits=24,.far_z=1,.path=REMESA_TA_VIEWPORT_MATRIX_WINDOW}},
   .vertex_bank=4,.pixel_bank=3,.state_bank=4,.clear_depth=1};
  struct remesa_scene_capture capture={0};
  CHECK(!remesa_persistent_gears_homogeneous_preserve_scene_prepare(&d,&request,parameters,&capture));
  struct remesa_persistent_scene_ta ta;struct remesa_persistent_scene_vertices vertices;
  struct remesa_persistent_scene_use use;struct remesa_persistent_scene_controls controls;
  CHECK(!remesa_persistent_scene_ta_snapshot(&d,capture,&ta));
  CHECK(ta.draw_count==1&&ta.reference_count==8);
  CHECK(ta.references[0].kind==REMESA_TA_REF_INITIAL_STATE);
  for(unsigned k=0;k<ta.reference_count;k++)CHECK(ta.references[k].kind!=REMESA_TA_REF_PREFIX_STATE);
  CHECK(!remesa_persistent_scene_vertices_snapshot(&d,capture,&vertices));
  CHECK(vertices.draw_count==1&&vertices.window_count==2&&vertices.windows[0].vertex_bytes==48&&vertices.windows[1].vertex_count==3&&vertices.windows[1].vertex_bytes==144);
  CHECK(!remesa_persistent_scene_use_snapshot(&d,capture,&use)&&use.entry_count==10);
  CHECK(!remesa_persistent_scene_controls_snapshot(&d,capture,&controls));
  CHECK(shader_pds_le32(controls.raster+80)==0x480);
  CHECK((shader_pds_le32(controls.raster+84)&0x00200026)==0x00200026);
  struct remesa_scene_packet packet;CHECK(!remesa_persistent_scene_packet_snapshot(&d,capture,&packet));
  CHECK(packet.controls.schema==3&&packet.controls.ta_bytes==76&&packet.window_count==2&&packet.use_count==10);
  struct remesa_persistent_scene_footprint footprint;
  CHECK(!remesa_persistent_scene_capture_snapshot(&d,capture,&footprint));
  scene_capture_export(&f,&footprint,&packet);
  struct remesa_matrix_submit_request submit={.version=1,.context=cfg.context,.budget_ms=1000,.flags=1,.packet=packet};
  CHECK(!remesa_matrix_submit_request_valid(&submit)); /* CLEAR mode cannot carry a no-clear graph. */
  submit.flags=REMESA_MATRIX_KEEP_SESSION|REMESA_MATRIX_PRESERVE_DEPTH;
  CHECK(remesa_matrix_submit_request_valid(&submit)); /* Metadata only; not retired-content/source authority. */
  CHECK(!remesa_persistent_scene_capture_release(&d,capture));
  void *objects[]={parameters,depth,color};for(unsigned k=0;k<3;k++)d.ops->destroy_bo(d.priv,objects[k]);
  CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden&&!remesa_persistent_close(&d));
 }
 puts("no-clear candidate composition/closure/cleanup PASS; retired-content/source admission still required; GPU0");
}
