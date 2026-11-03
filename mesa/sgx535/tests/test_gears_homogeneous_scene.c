/* SPDX-License-Identifier: MIT. Whole original-state15 scene closure, GPU0. */
#define REMESA_GEARS_SCENE_BUILDER_FIXTURE
#include "test_gears_scene_builder.c"
int main(int argc,char **argv)
{
 CHECK(argc==2);unsigned size=(unsigned)strtoul(argv[1],NULL,10);CHECK(size<4);
 const unsigned sizes[][2]={{1280,800},{640,640},{641,479},{1600,768}};
 unsigned width=sizes[size][0],height=sizes[size][1],pitch=(width+31)&~31u;
#if defined(REMESA_WITH_NIR) && defined(REMESA_TEST_REAL_NIR_BUILDER)
 glsl_type_singleton_init_or_ref();
#endif
 struct fixture f={.allow_gpu_map=true,.allow_sync=true};target_ops=ops;target_ops.create=builder_create;struct remesa_device d={0};
 struct remesa_persistent_config cfg={.ops=&target_ops,.priv=&f,.context=12100+size,.va_start=0x20000000,.va_end=0x45000000,.byte_budget=96u*1024u*1024u,.resource_only_backend=true,.resource_limits={2048,2048,128,16u*1024u*1024u}};
 cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};cfg.placement[REMESA_VA_RASTER]=(struct remesa_persistent_va_range){0x30000000,0x34000000,1048576};cfg.placement[REMESA_VA_SURFACE]=(struct remesa_persistent_va_range){0x40001000,0x45000000,4096};
 CHECK(!remesa_persistent_open(&d,&cfg));void *color=NULL,*depth=NULL,*parameters=NULL;
 CHECK(!remesa_persistent_allocate_placed(&d,(size_t)pitch*height*4,REMESA_VA_SURFACE,&color));CHECK(!remesa_persistent_depth_allocate(&d,width,height,4,&depth));
 struct remesa_shader_program vs=vertex(1),fs=fragment(1);struct remesa_shader_linkage link;
#if defined(REMESA_WITH_NIR) && defined(REMESA_TEST_REAL_NIR_BUILDER)
 nir_shader *nir=make(true,0);struct remesa_shader_ir semantic;
 CHECK(!remesa_shader_ir_lower(nir,&semantic)&&!remesa_shader_program_prepare(&semantic,&vs));ralloc_free(nir);
#endif
 CHECK(!remesa_shader_linkage_prepare(&vs,&fs,&link));
 const float raw[3][12]={{0,0,0,2,0,0,1,0},{.25f,0,0,2,0,0,1,0},{0,.25f,0,2,0,0,1,0}};
 uint32_t coefficients[36]={0};for(unsigned i=0;i<36;i++)coefficients[i]=0x3f800000u;
 CHECK(!remesa_persistent_vertex_parameter_block_prepare(&d,coefficients,36,40,4,&parameters));
 struct remesa_linked_scene_request request={.color=color,.depth=depth,.width=width,.height=height,.color_pitch=pitch,.linkage=&link,.geometry={.vertices=raw,.vertex_bytes=sizeof raw,.vertex_count=3},.state={.isp={.depth_bits_present=true,.depth_test=true,.depth_write=true,.compare=REMESA_DEPTH_LESS,.rgba_mask=15},.viewport={.framebuffer_width=width,.framebuffer_height=height,.width=width,.height=height,.depth_bits=24,.far_z=1,.path=REMESA_TA_VIEWPORT_INITIAL_ZERO}},.vertex_bank=4,.pixel_bank=3,.state_bank=4,.clear_depth=1};
 struct remesa_scene_capture capture={0},zero=capture;unsigned before=f.creates;
 CHECK(remesa_persistent_gears_homogeneous_scene_prepare(&d,&request,parameters,&capture)==ENOTSUP&&!memcmp(&capture,&zero,sizeof zero)&&f.creates==before);
 request.state.viewport.path=REMESA_TA_VIEWPORT_MATRIX_WINDOW;
 CHECK(!remesa_persistent_gears_homogeneous_scene_prepare(&d,&request,parameters,&capture));
 struct remesa_persistent_scene_footprint footprint;struct remesa_scene_packet packet;
 CHECK(!remesa_persistent_scene_capture_snapshot(&d,capture,&footprint));CHECK(!remesa_persistent_scene_packet_snapshot(&d,capture,&packet));
 CHECK(footprint.allocation_count==18&&packet.use_count==11&&packet.window_count==2&&packet.controls.ta_bytes==104);
 struct remesa_persistent_scene_use use;struct remesa_persistent_scene_ta ta;struct remesa_persistent_scene_vertices vertices;
 CHECK(!remesa_persistent_scene_use_snapshot(&d,capture,&use)&&use.entry_count==11);
 CHECK(!remesa_persistent_scene_ta_snapshot(&d,capture,&ta));CHECK(!remesa_persistent_scene_vertices_snapshot(&d,capture,&vertices));
 struct remesa_persistent_scene_parameters params;
 unsigned maps=f.cpu_maps,reads=f.reads,writes=f.writes,creates=f.creates;
 CHECK(!remesa_persistent_scene_parameters_snapshot(&d,capture,&params));
 CHECK(params.owner==capture.owner&&params.serial==capture.serial&&params.draw_count==1&&params.window_count==1);
 const struct remesa_parameter_window *window=&params.windows[0].window;
 CHECK(params.windows[0].draw==1&&remesa_parameter_window_valid(window)&&window->coefficient_dwords==36&&
  window->effective_source_dwords==37&&window->transfer_dwords==40&&window->allocation_bytes==4096&&
  window->source_offset==0&&window->use_offset==160&&window->pds_offset==192&&window->ta_offset==256);
 CHECK(get(&f,window->id)->bytes&&shader_pds_le32(get(&f,window->id)->bytes)==window->gpu_va);
 CHECK(!memcmp(get(&f,window->id)->bytes+4,coefficients,sizeof coefficients));
 for(unsigned off=0;off<320;off++)CHECK(remesa_parameter_window_patch_valid(window,window->id,off,4)==
  (off>=4&&off<=144&&!(off&3)));
 struct remesa_persistent_scene_parameters untouched,saved_params;memset(&untouched,0xa5,sizeof untouched);saved_params=untouched;
 struct remesa_scene_capture stale=capture;stale.serial++;
 CHECK(remesa_persistent_scene_parameters_snapshot(&d,stale,&untouched)==ESTALE&&!memcmp(&untouched,&saved_params,sizeof untouched));
 CHECK(remesa_persistent_scene_parameters_snapshot(&d,capture,(void *)((unsigned char *)&untouched+1))==EINVAL);
 CHECK(remesa_persistent_scene_parameters_snapshot(&d,capture,color)==EINVAL);
 CHECK(remesa_persistent_scene_parameters_snapshot(&d,capture,d.priv)==EINVAL);
 CHECK(f.cpu_maps==maps&&f.reads==reads&&f.writes==writes&&f.creates==creates);
 unsigned states=0;
 for(unsigned i=0;i<ta.reference_count;i++)if(ta.references[i].kind==REMESA_TA_REF_DRAW_STATE){
  const unsigned char *pds=get(&f,ta.references[i].handle)->bytes+ta.references[i].byte_offset;
  CHECK(shader_pds_le32(pds+4)==(0x80000000u|(14u<<21)|14u));states++;
 }
 CHECK(states==1);
 for(unsigned i=0;i<18;i++)if(footprint.allocations[i].access==3){struct backing *b=get(&f,footprint.allocations[i].handle);for(size_t j=0;j<b->length;j++)CHECK(b->bytes[j]==0xa5);}
 scene_capture_export(&f,&footprint,&packet);
 d.ops->destroy_bo(d.priv,parameters);
 CHECK(!remesa_persistent_scene_parameters_snapshot(&d,capture,&untouched)&&untouched.owner==params.owner&&
  untouched.serial==params.serial&&untouched.draw_count==1&&untouched.window_count==1&&
  untouched.windows[0].draw==1&&!memcmp(&untouched.windows[0].window,window,sizeof(*window)));
 CHECK(!remesa_persistent_scene_capture_release(&d,capture));
 CHECK(remesa_persistent_scene_parameters_snapshot(&d,capture,&untouched)==ESTALE);
 d.ops->destroy_bo(d.priv,color);d.ops->destroy_bo(d.priv,depth);
 CHECK(f.creates==f.destroys&&f.pins==f.unpins);CHECK(!remesa_persistent_close(&d));
#if defined(REMESA_WITH_NIR) && defined(REMESA_TEST_REAL_NIR_BUILDER)
 glsl_type_singleton_decref();
#endif
 puts("PASS complete homogeneous scene: 18 owned bindings/11 USE, state15/PARAM40, unchanged RW targets, full capture closure/cleanup; GPU0");return 0;
}
