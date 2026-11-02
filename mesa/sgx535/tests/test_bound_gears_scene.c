/* SPDX-License-Identifier: MIT. Actual Gallium bindings/common scene owners;
 * fake GEM only. No software vertex/pixel execution or hardware claim. */
#define REMESA_LINKED_SCENE_CONTEXT_FIXTURE
#include "test_linked_scene_context.c"
#define main inherited_bound_gears_nir_tests
#define nir_debug inherited_bound_gears_nir_debug
#include "test_nir_gears_vertex.c"
#undef nir_debug
#undef main
#ifdef REMESA_BOUND_GEARS_HOMOGENEOUS
#include "remesa/state_upload.h"
#define bound_prepare_scene remesa_context_prepare_gears_homogeneous_scene
#define bound_submit_scene remesa_context_submit_gears_homogeneous_scene
static unsigned char bound_expected_isp[8],bound_expected_viewport[28];
static void bound_check_homogeneous(struct fixture *f,const struct remesa_scene_packet *packet)
{
 unsigned found=0;
 for(unsigned i=0;i<packet->binding_count;i++)if(packet->bindings[i].access==1){
  struct backing *b=get(f,packet->bindings[i].id);CHECK(b);
  if(shader_pds_le32(b->bytes)!=0x1f41)continue;
  CHECK(!memcmp(b->bytes+4,bound_expected_isp+4,4));
  CHECK(!memcmp(b->bytes+20,bound_expected_viewport+4,24));
  CHECK(!shader_pds_le32(b->bytes+56));
  struct remesa_state_upload decoded;CHECK(!remesa_state_upload_pds(b->bytes+96,60,15,&decoded));
  struct backing *ta=get(f,packet->controls.ta_id);CHECK(ta&&packet->controls.ta_bytes==104);
  uint32_t pds_address=(shader_pds_le32(ta->bytes+packet->controls.ta_offset+56)&0x0fffffffu)<<4;
  CHECK(decoded.source_bytes==60&&decoded.source_address==pds_address-96);found++;
 }
 CHECK(found==1);
}
#else
#define bound_prepare_scene remesa_context_prepare_gears_attribute_scene
#define bound_submit_scene remesa_context_submit_gears_attribute_scene
#endif
#ifdef REMESA_BOUND_GEARS_SCENE_SUBMIT
static unsigned bound_submit_mode,bound_submit_calls;
static enum remesa_dispatch bound_submit_native(void *priv,const struct remesa_gpu_scene_submit *submit,struct remesa_gpu_scene_receipt *out)
{
 bound_submit_calls++;
#ifdef REMESA_BOUND_GEARS_HOMOGENEOUS
 bound_check_homogeneous(priv,submit->packet);
#else
 (void)priv;
#endif
 CHECK(submit&&remesa_scene_packet_valid(submit->packet)&&submit->packet->binding_count==18&&submit->packet->use_count==11&&submit->packet->controls.ta_bytes==104);
 if(bound_submit_mode==14)return REMESA_NOT_ISSUED;
 if(bound_submit_mode==15)return REMESA_ISSUANCE_AMBIGUOUS;
 *out=(struct remesa_gpu_scene_receipt){.context=submit->context,.operation=submit->operation,.retired=true,.published=true,.acknowledged=bound_submit_mode!=16};return REMESA_ISSUED;
}
#endif
int main(int argc,char **argv)
{
 CHECK(argc==2);unsigned mode=(unsigned)strtoul(argv[1],NULL,10);
#ifdef REMESA_BOUND_GEARS_SCENE_SUBMIT
 CHECK(mode<
#ifdef REMESA_BOUND_GEARS_HOMOGENEOUS
 27
#else
 17
#endif
 );bound_submit_mode=mode;
#else
 CHECK(mode<
#ifdef REMESA_BOUND_GEARS_HOMOGENEOUS
 27
#else
 14
#endif
 );
#endif
 unsigned width=mode==1?640:mode==2?641:mode==3?1600:1280,height=mode==1?640:mode==2?479:mode==3?768:800;
 glsl_type_singleton_init_or_ref();struct fixture f={.allow_gpu_map=true,.allow_sync=true};target_ops=ops;target_ops.create=builder_create;
 struct remesa_device d={0};struct remesa_persistent_config cfg={.ops=&target_ops,.priv=&f,.context=9400,.va_start=0x20000000,.va_end=0x45000000,.byte_budget=96u*1024u*1024u,.resource_only_backend=true,.resource_limits={2048,2048,128,16u*1024u*1024u}};
 cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};cfg.placement[REMESA_VA_RASTER]=(struct remesa_persistent_va_range){0x30000000,0x34000000,1048576};cfg.placement[REMESA_VA_SURFACE]=(struct remesa_persistent_va_range){0x40001000,0x45000000,4096};
 CHECK(!remesa_persistent_open(&d,&cfg));
#ifdef REMESA_BOUND_GEARS_SCENE_SUBMIT
 CHECK(!remesa_persistent_set_native_scene(&d,bound_submit_native));
#endif
 struct pipe_screen *screen=remesa_screen_create(&d);CHECK(screen);CHECK(!remesa_screen_enable_linked_attachment_storage(screen));
 struct pipe_resource *color=attachment(screen,width,height,false),*depth=attachment(screen,width,height,true);CHECK(color&&depth);
 struct pipe_context *ctx=screen->context_create(screen,NULL,0);CHECK(ctx);
 const struct pipe_framebuffer_state fb={.width=width,.height=height,.nr_cbufs=1,.cbufs={{.texture=color,.format=PIPE_FORMAT_B8G8R8A8_UNORM}},.zsbuf={.texture=depth,.format=PIPE_FORMAT_Z24X8_UNORM}};
 ctx->set_framebuffer_state(ctx,&fb);CHECK(!remesa_context_error(ctx));
#ifdef REMESA_BOUND_GEARS_HOMOGENEOUS
 struct pipe_blend_state blend={.rt={{.colormask=PIPE_MASK_RGBA}}};
 struct pipe_depth_stencil_alpha_state dsa={.depth_enabled=mode!=23,.depth_writemask=mode!=23,.depth_func=mode==23?PIPE_FUNC_GREATER:PIPE_FUNC_LESS};
 void *bs=ctx->create_blend_state(ctx,&blend),*ds=ctx->create_depth_stencil_alpha_state(ctx,&dsa);CHECK(bs&&ds);
 ctx->bind_blend_state(ctx,mode==21?NULL:bs);ctx->bind_depth_stencil_alpha_state(ctx,mode==22?NULL:ds);
 struct remesa_ta_window_viewport_request expected_view={.framebuffer_width=width,.framebuffer_height=height,.x=mode==24?7:0,.y=mode==24?9:0,.width=mode==24?width-14:width,.height=mode==24?height-18:height,.depth_bits=24,.near_z=mode==24?.25f:0,.far_z=mode==24?.75f:1,.path=REMESA_TA_VIEWPORT_MATRIX_WINDOW};
 struct pipe_viewport_state viewport={.scale={expected_view.width*.5f,-(float)expected_view.height*.5f,(expected_view.far_z-expected_view.near_z)*.5f},.translate={expected_view.x+expected_view.width*.5f,height-(expected_view.y+expected_view.height*.5f),(expected_view.far_z+expected_view.near_z)*.5f}};
 if(mode==18)viewport.scale[1]=-viewport.scale[1];
 if(mode==26)viewport.translate[0]=NAN;
 if(mode!=17)ctx->set_viewport_states(ctx,0,1,&viewport);
 const struct remesa_isp_opaque_triangle_request expected_isp={.depth_bits_present=true,.depth_test=mode!=23,.depth_write=mode!=23,.compare=mode==23?REMESA_DEPTH_ALWAYS:REMESA_DEPTH_LESS,.rgba_mask=15};
 CHECK(!remesa_isp_opaque_triangle_data_encode(&expected_isp,bound_expected_isp));
 CHECK(!remesa_ta_window_viewport_encode(&expected_view,bound_expected_viewport));
#endif
 struct pipe_shader_state sh={.type=PIPE_SHADER_IR_NIR,.ir.nir=mode==6?linked_shader(0):make(true,0)};
 void *vs=ctx->create_vs_state(ctx,&sh);CHECK(vs);ctx->bind_vs_state(ctx,vs);sh.ir.nir=linked_shader(1);void *fs=ctx->create_fs_state(ctx,&sh);CHECK(fs);ctx->bind_fs_state(ctx,fs);
 uint32_t coefficients[36],expected[36];for(unsigned i=0;i<36;i++)coefficients[i]=expected[i]=0x3f800000u+i*37;
 struct pipe_resource *cb=NULL,*ib=NULL;
 const struct pipe_resource buffer={.target=PIPE_BUFFER,.format=PIPE_FORMAT_R8_UNORM,.bind=PIPE_BIND_VERTEX_BUFFER,.width0=96,.height0=1,.depth0=1,.array_size=1};
 if(mode!=7){
  struct pipe_constant_buffer constant={.user_buffer=coefficients,.buffer_size=sizeof coefficients};
  if(mode==9){struct pipe_resource t=buffer;t.bind=PIPE_BIND_CONSTANT_BUFFER;t.width0=sizeof coefficients;cb=screen->resource_create(screen,&t);CHECK(cb);ctx->buffer_subdata(ctx,cb,0,0,sizeof coefficients,coefficients);constant.user_buffer=NULL;constant.buffer=cb;}
  ctx->set_constant_buffer(ctx,PIPE_SHADER_VERTEX,0,false,&constant);CHECK(!remesa_context_error(ctx));
  if(mode==10)memset(coefficients,0,sizeof coefficients); /* Bind snapshot wins. */
 }
 const uint32_t raw[3][8]={{0x80000000,0x3f800000,0x3e000000,0x3f800000,0,0,0x3f800000,0},
  {0x3f800000,0x80000000,0x3e000000,0x3f800000,0,0,0x3f800000,0},
  {0,0x3f000000,0x3e000000,0x3f800000,0,0,0x3f800000,0}};
 struct pipe_resource *vb=screen->resource_create(screen,&buffer);CHECK(vb);ctx->buffer_subdata(ctx,vb,0,0,sizeof raw,raw);
 struct pipe_vertex_buffer binding={.buffer.resource=vb};vb->reference.count++;ctx->set_vertex_buffers(ctx,1,&binding);
 struct pipe_vertex_element elements[2]={0};for(unsigned a=0;a<2;a++){elements[a].src_format=PIPE_FORMAT_R32G32B32A32_FLOAT;elements[a].src_stride=32;elements[a].src_offset=a*16;}
 void *el=ctx->create_vertex_elements_state(ctx,2,elements);CHECK(el);ctx->bind_vertex_elements_state(ctx,el);CHECK(!remesa_context_error(ctx));
 struct pipe_draw_info info={.mode=MESA_PRIM_TRIANGLES,.instance_count=1};struct pipe_draw_start_count_bias draw={.count=3};
 const uint16_t user_indices[]={999,2,0,1};const uint32_t resource_indices[]={3,1,2};
 if(mode==4){info.index_size=2;info.has_user_indices=true;info.index.user=user_indices;draw.start=1;}
 if(mode==5){struct pipe_resource t=buffer;t.bind=PIPE_BIND_INDEX_BUFFER;t.width0=sizeof resource_indices;ib=screen->resource_create(screen,&t);CHECK(ib);ctx->buffer_subdata(ctx,ib,0,0,sizeof resource_indices,resource_indices);info.index_size=4;info.index.resource=ib;draw.index_bias=-1;}
 struct remesa_linked_scene_request options={.state={.isp={.depth_bits_present=true,.depth_test=true,.depth_write=true,.compare=REMESA_DEPTH_LESS,.rgba_mask=15},.viewport={.framebuffer_width=width,.framebuffer_height=height,.width=width,.height=height,.depth_bits=24,.far_z=1,.path=REMESA_TA_VIEWPORT_INITIAL_ZERO}},.vertex_bank=4,.pixel_bank=mode==11?4:3,.state_bank=4,.clear_depth=1};
#ifdef REMESA_BOUND_GEARS_HOMOGENEOUS
 memset(&options.state,0,sizeof options.state);
 if(mode==19)options.state.isp.rgba_mask=15;
 if(mode==20)options.state.viewport.framebuffer_width=width;
 if(mode==25)options.state.viewport.near_z=.25f;
 if((mode>=17&&mode<=22)||mode==25||mode==26){
  unsigned n=f.creates,m=f.cpu_maps;struct remesa_scene_capture out={0},saved=out;
  int expected=mode==18?ENOTSUP:mode==19||mode==20||mode==25?EINVAL:ENODATA;
  CHECK(bound_prepare_scene(ctx,&info,&draw,&options,&out)==expected&&!memcmp(&out,&saved,sizeof out));
#ifdef REMESA_BOUND_GEARS_SCENE_SUBMIT
  struct pipe_fence_handle *fence=NULL;CHECK(bound_submit_scene(ctx,&info,&draw,&options,&fence)==expected&&!fence&&!bound_submit_calls);
#endif
  CHECK(f.creates==n&&f.cpu_maps==m&&!remesa_context_cpu_clear_count(ctx));
  ctx->destroy(ctx);remesa_resource_reference(&color,NULL);remesa_resource_reference(&depth,NULL);remesa_resource_reference(&vb,NULL);remesa_resource_reference(&cb,NULL);remesa_resource_reference(&ib,NULL);screen->destroy(screen);CHECK(f.creates==f.destroys&&!remesa_persistent_close(&d));glsl_type_singleton_decref();
  puts("PASS homogeneous context refuses missing/invalid/overridden bound state before resource IO or dispatch; GPU0");return 0;
 }
#endif
 struct remesa_scene_capture capture={0},saved=capture;unsigned creates=f.creates,maps=f.cpu_maps;
 void *aliases[]={ctx,vs,fs,el,color,depth,vb,&info,&draw,&options};
 for(unsigned a=0;a<sizeof aliases/sizeof aliases[0];a++)CHECK(bound_prepare_scene(ctx,&info,&draw,&options,aliases[a])==EINVAL);
 if(cb)CHECK(bound_prepare_scene(ctx,&info,&draw,&options,(void *)cb)==EINVAL);
#ifdef REMESA_BOUND_GEARS_SCENE_SUBMIT
 for(unsigned a=0;a<sizeof aliases/sizeof aliases[0];a++)CHECK(bound_submit_scene(ctx,&info,&draw,&options,aliases[a])==EINVAL);
 if(cb)CHECK(bound_submit_scene(ctx,&info,&draw,&options,(void *)cb)==EINVAL);
#endif
 CHECK(f.creates==creates&&f.cpu_maps==maps);
 struct pipe_transfer *transfer=NULL;
 if(mode==8||mode==9){const struct pipe_box box={.width=16,.height=1,.depth=1};CHECK(ctx->buffer_map(ctx,mode==8?vb:cb,0,PIPE_MAP_READ,&box,&transfer)&&transfer);}
 if(mode==12||mode==13){fail_at=f.creates+3;fail_uncertain=mode==13;}
#ifdef REMESA_BOUND_GEARS_SCENE_SUBMIT
 struct pipe_fence_handle *fence=NULL;
 if(mode==14){
  CHECK(bound_submit_scene(ctx,&info,&draw,&options,&fence)==EOPNOTSUPP&&!fence&&bound_submit_calls==1);
  CHECK(!remesa_resource_wait_writer(ctx,color,0)&&!remesa_resource_wait_writer(ctx,depth,0));bound_submit_mode=0;
 }
 int error=bound_submit_scene(ctx,&info,&draw,&options,&fence);
 if(mode==13||mode==15||mode==16){
  CHECK(error==EOWNERDEAD&&!fence&&bound_submit_calls==(mode==13?0:1));
  CHECK(remesa_resource_wait_writer(ctx,color,0)==EOWNERDEAD&&remesa_resource_wait_writer(ctx,depth,0)==EOWNERDEAD);
  unsigned calls=bound_submit_calls;CHECK(bound_submit_scene(ctx,&info,&draw,&options,&fence)==EOWNERDEAD&&bound_submit_calls==calls);
  ctx->flush(ctx,&fence,0);CHECK(!fence&&remesa_persistent_close(&d)==EBUSY);
  puts("PASS bound gears uncertain allocation/dispatch/receipt poisons both attachments and suppresses retry/flush; MODEL_GPU0");return 0;
 }
#else
 int error=bound_prepare_scene(ctx,&info,&draw,&options,&capture);
#endif
 if(mode==13){CHECK(error==EOWNERDEAD&&!memcmp(&capture,&saved,sizeof saved)&&remesa_persistent_close(&d)==EBUSY);puts("PASS uncertain bound gears assembly retained; MODEL_GPU0");return 0;}
 if(mode==6||mode==7||mode==8||mode==9||mode==11||mode==12){
  CHECK(error&&!memcmp(&capture,&saved,sizeof saved));
  CHECK(mode!=6||error==ENOTSUP);CHECK(mode!=7||error==ENODATA);CHECK((mode!=8&&mode!=9)||error==EBUSY);CHECK(mode!=12||error==ENOMEM);
  if(transfer)ctx->buffer_unmap(ctx,transfer);
 }else{
#ifdef REMESA_BOUND_GEARS_SCENE_SUBMIT
  CHECK(!error&&fence&&bound_submit_calls==(mode==14?2:1)&&!remesa_context_cpu_clear_count(ctx)&&!screen->caps.graphics&&!screen->caps.accelerated);
  CHECK(screen->fence_finish(screen,ctx,fence,0));struct pipe_fence_handle *flushed=NULL;ctx->flush(ctx,&flushed,0);CHECK(flushed==fence);screen->fence_reference(screen,&flushed,NULL);
  CHECK(!remesa_resource_wait_writer(ctx,color,0));ctx->delete_vs_state(ctx,vs);ctx->delete_fs_state(ctx,fs);ctx->delete_vertex_elements_state(ctx,el);ctx->destroy(ctx);
  remesa_resource_reference(&color,NULL);remesa_resource_reference(&vb,NULL);remesa_resource_reference(&cb,NULL);remesa_resource_reference(&ib,NULL);
  struct pipe_context *other=screen->context_create(screen,NULL,0);CHECK(other);CHECK(!remesa_resource_wait_writer(other,depth,0));other->destroy(other);remesa_resource_reference(&depth,NULL);
  CHECK(remesa_persistent_close(&d)==EBUSY);screen->fence_reference(screen,&fence,NULL);CHECK(!fence);screen->destroy(screen);
  CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!dispatch_calls&&!remesa_persistent_close(&d));glsl_type_singleton_decref();
  puts("PASS bound gears submit shares one COLOR/Z/context/caller fence across waits and teardown, definite nonissue retry preserved; MODEL_GPU0");return 0;
#else
  CHECK(!error);struct remesa_scene_packet packet;struct remesa_persistent_scene_footprint footprint;
  CHECK(!remesa_persistent_scene_packet_snapshot(&d,capture,&packet)&&packet.binding_count==18&&packet.use_count==11&&packet.controls.ta_bytes==104);
  CHECK(!remesa_persistent_scene_capture_snapshot(&d,capture,&footprint));
#ifdef REMESA_BOUND_GEARS_HOMOGENEOUS
  bound_check_homogeneous(&f,&packet);
#endif
  struct backing *vertices=get(&f,packet.windows[1].vertex_id);CHECK(vertices&&packet.windows[1].vertex_bytes==144);
  for(unsigned i=0;i<3;i++){unsigned index=mode==4||mode==5?(i+2)%3:i;const uint8_t *at=vertices->bytes+packet.windows[1].vertex_offset+i*48;CHECK(!memcmp(at,raw[index],32));for(unsigned n=32;n<48;n++)CHECK(!at[n]);}
  unsigned found=0;for(unsigned i=0;i<packet.use_count;i++)if(packet.uses[i].data_master==REMESA_USE_VERTEX&&packet.uses[i].program_bytes==8){struct backing *params=get(&f,packet.uses[i].id);CHECK(params);for(unsigned j=0;j<36;j++)CHECK(shader_pds_le32(params->bytes+4+j*4)==expected[j]);found++;}CHECK(found==1);
  scene_capture_export(&f,&footprint,&packet);CHECK(!remesa_context_cpu_clear_count(ctx)&&!screen->caps.graphics&&!screen->caps.accelerated);
  CHECK(!remesa_persistent_scene_capture_release(&d,capture));
#endif
 }
 CHECK(!dispatch_calls);ctx->delete_vs_state(ctx,vs);ctx->delete_fs_state(ctx,fs);ctx->delete_vertex_elements_state(ctx,el);ctx->destroy(ctx);
 remesa_resource_reference(&color,NULL);remesa_resource_reference(&depth,NULL);remesa_resource_reference(&vb,NULL);remesa_resource_reference(&cb,NULL);remesa_resource_reference(&ib,NULL);
 screen->destroy(screen);CHECK(f.creates==f.destroys&&f.pins==f.unpins);CHECK(!remesa_persistent_close(&d));glsl_type_singleton_decref();
 puts("PASS actual bound NIR/constant/raw vertex/index callbacks -> complete common gears capture, exact P/N bits+unused padding, aliases/failures/cleanup; GPU0");return 0;
}

#undef bound_prepare_scene
#undef bound_submit_scene
