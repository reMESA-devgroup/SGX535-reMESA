/* SPDX-License-Identifier: MIT. Real Gallium/NIR/persistent owners, fake BOs;
 * no device, kernel, rasterizer or physical-completion claim. */
#define REMESA_SCENE_BUILDER_FIXTURE
#include "test_scene_builder.c"
#include "remesa_screen.h"
#include "remesa_resource_ref.h"
#include "pipe/p_context.h"
#include "pipe/p_screen.h"
#include "pipe/p_state.h"
#include "nir_builder.h"
#include "util/u_inlines.h"
uint32_t nir_debug=NIR_DEBUG_EXTENDED_VALIDATION;
static unsigned dispatch_mode,dispatch_calls;
static enum remesa_dispatch context_native(void *priv,const struct remesa_gpu_scene_submit *scene,struct remesa_gpu_scene_receipt *receipt)
{
 dispatch_calls++;
 if(dispatch_mode==1)return REMESA_NOT_ISSUED;
 if(dispatch_mode==2)return REMESA_ISSUANCE_AMBIGUOUS;
 enum remesa_dispatch result=complete(priv,scene,receipt);
 if(dispatch_mode==3)receipt->acknowledged=false;
 return result;
}
static nir_shader *linked_shader(unsigned fragment)
{
 static const nir_shader_compiler_options options={0};
 nir_builder b=nir_builder_init_simple_shader(fragment?MESA_SHADER_FRAGMENT:MESA_SHADER_VERTEX,&options,"bound three-input transport");
 if(fragment){
  nir_variable *in=nir_variable_create(b.shader,nir_var_shader_in,glsl_vec4_type(),"color");
  nir_variable *out=nir_variable_create(b.shader,nir_var_shader_out,glsl_vec4_type(),"result");
  in->data.location=VARYING_SLOT_COL0;out->data.location=FRAG_RESULT_DATA0;
  nir_store_deref(&b,nir_build_deref_var(&b,out),nir_load_deref(&b,nir_build_deref_var(&b,in)),15);
 }else{
  b.shader->info.io_lowered=true;nir_def *z=nir_imm_int(&b,0),*in[3];
  for(unsigned k=0;k<3;k++)in[k]=nir_load_input(&b,4,32,z,.base=k,.dest_type=nir_type_float32,
   .io_semantics={.location=VERT_ATTRIB_GENERIC0+k,.num_slots=1});
  nir_def *light=nir_fsat(&b,nir_fdot3(&b,in[1],in[2]));
  nir_store_output(&b,in[0],z,.write_mask=15,.src_type=nir_type_float32,
   .io_semantics={.location=VARYING_SLOT_POS,.num_slots=1});
  nir_store_output(&b,nir_vec4(&b,light,light,light,light),z,.base=1,.write_mask=15,.src_type=nir_type_float32,
   .io_semantics={.location=VARYING_SLOT_COL0,.num_slots=1});
 }
 nir_validate_shader(b.shader,"bound raw attribute transport");return b.shader;
}

static struct pipe_resource *attachment(struct pipe_screen *s,unsigned w,unsigned h,bool depth)
{
 const struct pipe_resource t={.target=PIPE_TEXTURE_2D,.format=depth?PIPE_FORMAT_Z24X8_UNORM:PIPE_FORMAT_B8G8R8A8_UNORM,
  .bind=depth?PIPE_BIND_DEPTH_STENCIL:PIPE_BIND_RENDER_TARGET,.width0=w,.height0=h,.depth0=1,.array_size=1};
 return s->resource_create(s,&t);
}
static struct pipe_surface *surface(struct pipe_context *ctx,struct pipe_resource *res)
{const struct pipe_surface t={.format=res->format};return ctx->create_surface(ctx,res,&t);}
int main(int argc,char **argv)
{
 CHECK(argc==2);unsigned mode=(unsigned)strtoul(argv[1],NULL,10);CHECK(mode<22);
 unsigned width=mode==1||mode==14?640:mode==2||mode==15?641:mode==3?1600:1280,height=mode==1||mode==14?640:mode==2||mode==15?479:mode==3?768:800;
 glsl_type_singleton_init_or_ref();struct fixture f={.allow_gpu_map=true,.allow_sync=true};target_ops=ops;target_ops.create=builder_create;
 struct remesa_device d={0};struct remesa_persistent_config cfg={.ops=&target_ops,.priv=&f,.context=9200,.va_start=0x20000000,.va_end=0x45000000,.byte_budget=96u*1024u*1024u,.resource_only_backend=true,.resource_limits={2048,2048,128,16u*1024u*1024u}};
 cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};cfg.placement[REMESA_VA_RASTER]=(struct remesa_persistent_va_range){0x30000000,0x34000000,1048576};cfg.placement[REMESA_VA_SURFACE]=(struct remesa_persistent_va_range){0x40001000,0x45000000,4096};
 if(mode==7)cfg.resource_limits.row_alignment=1;
 if(mode==10){cfg.resource_only_backend=false;cfg.offline_test_backend=true;}
 CHECK(!remesa_persistent_open(&d,&cfg));CHECK(!remesa_persistent_set_native_scene(&d,context_native));
 struct remesa_device front=d;struct remesa_device_ops foreign=*d.ops;
 if(mode==8){foreign.validate_attachment_storage=NULL;front.ops=&foreign;}
 if(mode==9)front.ops=&foreign;
 struct pipe_screen *screen=remesa_screen_create(&front);CHECK(screen);
 if(mode>=7&&mode<=10){
  CHECK(remesa_screen_enable_linked_attachment_storage(screen)==(mode==9?EINVAL:ENOTSUP));
  CHECK(!screen->is_format_supported(screen,PIPE_FORMAT_Z24X8_UNORM,PIPE_TEXTURE_2D,0,0,PIPE_BIND_DEPTH_STENCIL));
  CHECK(!f.creates);screen->destroy(screen);CHECK(!remesa_persistent_close(&d));glsl_type_singleton_decref();puts("PASS incompatible allocation/stride/device-hook owner mode refused before storage or draw; GPU0");return 0;
 }
 CHECK(!screen->caps.graphics&&!screen->caps.accelerated);
 CHECK(!screen->is_format_supported(screen,PIPE_FORMAT_Z24X8_UNORM,PIPE_TEXTURE_2D,0,0,PIPE_BIND_DEPTH_STENCIL));
 CHECK(!attachment(screen,width,height,false)&&!attachment(screen,width,height,true)&&!f.creates);
 if(mode==4){
  struct pipe_context *ctx=screen->context_create(screen,NULL,0);CHECK(ctx);CHECK(remesa_screen_enable_linked_attachment_storage(screen)==EBUSY);
  struct remesa_isp_opaque_triangle_request isp,saved;memset(&isp,0xa5,sizeof isp);saved=isp;
  CHECK(remesa_context_linked_isp_snapshot(ctx,&isp)==ENOTSUP&&!memcmp(&isp,&saved,sizeof isp));ctx->destroy(ctx);
 }
 CHECK(!remesa_screen_enable_linked_attachment_storage(screen));CHECK(remesa_screen_enable_linked_attachment_storage(screen)==EBUSY);
 CHECK(!screen->caps.graphics&&!screen->caps.accelerated);CHECK(screen->is_format_supported(screen,PIPE_FORMAT_Z24X8_UNORM,PIPE_TEXTURE_RECT,0,0,PIPE_BIND_DEPTH_STENCIL));
 struct pipe_resource *color=attachment(screen,width,height,false),*depth=attachment(screen,width,height,true);CHECK(color&&depth&&f.creates==2);
 struct pipe_context *ctx=screen->context_create(screen,NULL,0);CHECK(ctx);
 struct pipe_surface *cs=surface(ctx,color),*zs=surface(ctx,depth);CHECK(cs&&zs);
 const struct pipe_framebuffer_state fb={.width=width,.height=height,.nr_cbufs=1,.cbufs={{.texture=color,.format=PIPE_FORMAT_B8G8R8A8_UNORM}},.zsbuf={.texture=depth,.format=PIPE_FORMAT_Z24X8_UNORM}};
 ctx->set_framebuffer_state(ctx,&fb);CHECK(!remesa_context_error(ctx));
 struct pipe_shader_state sh={.type=PIPE_SHADER_IR_NIR,.ir.nir=linked_shader(0)};void *vs=ctx->create_vs_state(ctx,&sh);CHECK(vs);ctx->bind_vs_state(ctx,vs);sh.ir.nir=linked_shader(1);void *fs=ctx->create_fs_state(ctx,&sh);CHECK(fs);ctx->bind_fs_state(ctx,fs);
 const float raw[3][12]={{width*.25f,height*.25f,.125f,1,0,0,1,0,0,0,1,0},{width*.75f,height*.25f,.125f,1,0,0,1,0,0,0,1,0},{width*.25f,height*.75f,.125f,1,0,0,1,0,0,0,1,0}};
 const struct pipe_resource vb_template={.target=PIPE_BUFFER,.format=PIPE_FORMAT_R8_UNORM,.bind=PIPE_BIND_VERTEX_BUFFER,.width0=sizeof raw,.height0=1,.depth0=1,.array_size=1};
 struct pipe_resource *vb=screen->resource_create(screen,&vb_template);CHECK(vb);ctx->buffer_subdata(ctx,vb,0,0,sizeof raw,raw);CHECK(!remesa_context_error(ctx));
 struct pipe_vertex_buffer binding={.buffer.resource=vb};vb->reference.count++;ctx->set_vertex_buffers(ctx,1,&binding);CHECK(!remesa_context_error(ctx));
 struct pipe_vertex_element elements[3]={0};for(unsigned a=0;a<3;a++){elements[a].src_format=PIPE_FORMAT_R32G32B32A32_FLOAT;elements[a].src_stride=48;elements[a].src_offset=a*16;}
 void *el=ctx->create_vertex_elements_state(ctx,3,elements);CHECK(el);ctx->bind_vertex_elements_state(ctx,el);
 struct pipe_draw_info info={.mode=MESA_PRIM_TRIANGLES,.instance_count=1};struct pipe_draw_start_count_bias draw={.count=3};
 struct remesa_linked_scene_request options={.state={.isp={.depth_bits_present=true,.depth_test=true,.depth_write=true,.compare=REMESA_DEPTH_LESS,.rgba_mask=15},.viewport={.framebuffer_width=width,.framebuffer_height=height,.width=width,.height=height,.depth_bits=24,.far_z=1,.path=REMESA_TA_VIEWPORT_INITIAL_ZERO}},.vertex_bank=4,.pixel_bank=3,.state_bank=4,.clear_depth=width==640?.25f:1};
 struct remesa_scene_capture capture={0},before=capture;
 unsigned oldcreates=f.creates,oldmaps=f.cpu_maps;
 CHECK(remesa_context_prepare_linked_scene(ctx,&info,&draw,&options,(void *)&info)==EINVAL);
 CHECK(remesa_context_prepare_linked_scene(ctx,&info,&draw,&options,(void *)&draw)==EINVAL);
 CHECK(remesa_context_prepare_linked_scene(ctx,&info,&draw,&options,(void *)&options)==EINVAL);
 CHECK(remesa_context_prepare_linked_scene(ctx,&info,&draw,&options,(void *)color)==EINVAL);
 CHECK(remesa_context_prepare_linked_scene(ctx,&info,&draw,&options,(void *)vb)==EINVAL);
 struct remesa_linked_scene_request bad=options;bad.width=width;CHECK(remesa_context_prepare_linked_scene(ctx,&info,&draw,&bad,&capture)==EINVAL&&!memcmp(&capture,&before,sizeof before));
 CHECK(f.creates==oldcreates&&f.cpu_maps==oldmaps);
 if(mode==5){
  struct pipe_box box={.width=16,.height=1,.depth=1};struct pipe_transfer *transfer=NULL;CHECK(ctx->buffer_map(ctx,vb,0,PIPE_MAP_READ,&box,&transfer)&&transfer);
  CHECK(remesa_context_prepare_linked_scene(ctx,&info,&draw,&options,&capture)==EBUSY&&!memcmp(&capture,&before,sizeof before));ctx->buffer_unmap(ctx,transfer);
 }
 if(mode==6){fail_at=f.creates+6;CHECK(remesa_context_prepare_linked_scene(ctx,&info,&draw,&options,&capture)==ENOMEM&&!memcmp(&capture,&before,sizeof before));CHECK(f.creates-f.destroys==3);fail_at=0;}
 if(mode>=18){
  struct remesa_isp_opaque_triangle_request isp,saved;memset(&isp,0xa5,sizeof isp);saved=isp;
  CHECK(remesa_context_linked_isp_snapshot(ctx,&isp)==ENODATA&&!memcmp(&isp,&saved,sizeof isp));
  const struct pipe_blend_state blend={.rt={{.colormask=PIPE_MASK_RGBA}}};
  const struct pipe_depth_stencil_alpha_state less={.depth_enabled=true,.depth_writemask=true,.depth_func=PIPE_FUNC_LESS};
  void *bs=ctx->create_blend_state(ctx,&blend),*ds=ctx->create_depth_stencil_alpha_state(ctx,&less);CHECK(bs&&ds);
  ctx->bind_depth_stencil_alpha_state(ctx,ds);
  CHECK(remesa_context_linked_isp_snapshot(ctx,&isp)==ENODATA&&!memcmp(&isp,&saved,sizeof isp));ctx->bind_blend_state(ctx,bs);
  CHECK(remesa_context_linked_isp_snapshot(ctx,(void *)bs)==EINVAL);
  CHECK(remesa_context_linked_isp_snapshot(ctx,(void *)color)==EINVAL);
  CHECK(remesa_context_linked_isp_snapshot(ctx,(void *)vb)==EINVAL);
  CHECK(remesa_context_linked_isp_snapshot(ctx,(void *)((unsigned char *)&isp+1))==EINVAL);
  CHECK(remesa_context_linked_isp_snapshot(ctx,(void *)(UINTPTR_MAX-3))==EINVAL);
  unsigned maps=f.cpu_maps,creates=f.creates;CHECK(!remesa_context_linked_isp_snapshot(ctx,&isp));
  CHECK(isp.depth_bits_present&&isp.depth_test&&isp.depth_write&&isp.compare==REMESA_DEPTH_LESS&&isp.rgba_mask==15);
  unsigned char encoded[8];CHECK(!remesa_isp_opaque_triangle_data_encode(&isp,encoded));
  const unsigned char less_bytes[8]={1,0,0,0,0,0,0x40,0};CHECK(!memcmp(encoded,less_bytes,8));
  if(mode==19){
   struct pipe_depth_stencil_alpha_state bad=less;bad.depth_func=PIPE_FUNC_GREATER;
   CHECK(!ctx->create_depth_stencil_alpha_state(ctx,&bad));ctx->bind_vs_state(ctx,vs);CHECK(!remesa_context_error(ctx));saved=isp;
   CHECK(remesa_context_linked_isp_snapshot(ctx,&isp)==EINVAL&&!memcmp(&isp,&saved,sizeof isp));ctx->bind_depth_stencil_alpha_state(ctx,ds);
  }
  if(mode==20){
   const struct pipe_depth_stencil_alpha_state disabled={.depth_writemask=true,.depth_func=PIPE_FUNC_GREATER};
   void *off=ctx->create_depth_stencil_alpha_state(ctx,&disabled);CHECK(off);ctx->bind_depth_stencil_alpha_state(ctx,off);
   CHECK(!remesa_context_linked_isp_snapshot(ctx,&isp)&&!isp.depth_test&&!isp.depth_write&&isp.compare==REMESA_DEPTH_ALWAYS);
   CHECK(!remesa_isp_opaque_triangle_data_encode(&isp,encoded));const unsigned char off_bytes[8]={1,0,0,0,0,0,0xd0,1};CHECK(!memcmp(encoded,off_bytes,8));
   ctx->bind_depth_stencil_alpha_state(ctx,ds);ctx->delete_depth_stencil_alpha_state(ctx,off);
  }
  if(mode==21){
   saved=isp;ctx->delete_depth_stencil_alpha_state(ctx,ds);
   CHECK(remesa_context_linked_isp_snapshot(ctx,&isp)==EINVAL&&!memcmp(&isp,&saved,sizeof isp));
   ds=ctx->create_depth_stencil_alpha_state(ctx,&less);CHECK(ds);ctx->bind_depth_stencil_alpha_state(ctx,ds);
  }
  CHECK(!remesa_context_linked_isp_snapshot(ctx,&isp));options.state.isp=isp;
  CHECK(f.creates==creates&&f.cpu_maps==maps);
 }
 if(mode>=11){
  struct pipe_fence_handle *fence=NULL;
  CHECK(remesa_context_submit_linked_scene(ctx,&info,&draw,&options,(void *)&info)==EINVAL);
  CHECK(remesa_context_submit_linked_scene(ctx,&info,&draw,&options,(void *)&options)==EINVAL);
  CHECK(remesa_context_submit_linked_scene(ctx,&info,&draw,&options,(void *)color)==EINVAL);
  CHECK(remesa_context_submit_linked_scene(ctx,&info,&draw,&options,(void *)vb)==EINVAL);
  CHECK(!dispatch_calls&&f.creates==oldcreates);
  if(mode==12){
   dispatch_mode=1;
   CHECK(remesa_context_submit_linked_scene(ctx,&info,&draw,&options,&fence)==EOPNOTSUPP&&!fence&&dispatch_calls==1);
   CHECK(f.creates-f.destroys==3);
   CHECK(!remesa_resource_wait_writer(ctx,color,0)&&!remesa_resource_wait_writer(ctx,depth,0));
   dispatch_mode=0;
  }
  if(mode==13)dispatch_mode=2;
  if(mode==16){fail_at=f.creates+6;fail_uncertain=1;}
  if(mode==17)dispatch_mode=3;
  int error=remesa_context_submit_linked_scene(ctx,&info,&draw,&options,&fence);
  if(mode==13||mode==16||mode==17){
   CHECK(error==EOWNERDEAD&&!fence&&dispatch_calls==(mode==16?0:1));
   struct remesa_isp_opaque_triangle_request isp,saved;memset(&isp,0xa5,sizeof isp);saved=isp;
   CHECK(remesa_context_linked_isp_snapshot(ctx,&isp)==EOWNERDEAD&&!memcmp(&isp,&saved,sizeof isp));
   CHECK(remesa_resource_wait_writer(ctx,color,0)==EOWNERDEAD&&remesa_resource_wait_writer(ctx,depth,0)==EOWNERDEAD);
   unsigned calls=dispatch_calls;
   CHECK(remesa_context_submit_linked_scene(ctx,&info,&draw,&options,&fence)==EOWNERDEAD&&dispatch_calls==calls);
   ctx->flush(ctx,&fence,0);CHECK(!fence);
   unsigned destroyed=f.destroys,unpinned=f.unpins;
   ctx->surface_destroy(ctx,cs);ctx->surface_destroy(ctx,zs);ctx->destroy(ctx);
   remesa_resource_reference(&color,NULL);remesa_resource_reference(&depth,NULL);remesa_resource_reference(&vb,NULL);
   screen->destroy(screen);
   if(mode==16){
    /* CREATE failed before pin/dispatch. Only that uncertain new handle is
     * quarantined; the three never-submitted frontend BOs remain releasable. */
    CHECK(f.backing[fail_at-1].live&&f.destroys==destroyed+3&&f.unpins==unpinned+3);
   }else CHECK(f.destroys==destroyed&&f.unpins==unpinned);
   CHECK(remesa_persistent_close(&d)==EBUSY);
   glsl_type_singleton_decref();puts("PASS uncertain context scene submission quarantines BOTH attachments, suppresses stale flush fence and retry; MODELED held backing/GPU0");return 0;
  }
  CHECK(!error&&fence&&dispatch_calls==(mode==12?2:1));
  CHECK(screen->fence_finish(screen,ctx,fence,0));
  struct pipe_fence_handle *flushed=NULL;ctx->flush(ctx,&flushed,0);CHECK(flushed==fence);screen->fence_reference(screen,&flushed,NULL);
  CHECK(!remesa_resource_wait_writer(ctx,color,0));
  CHECK(!remesa_context_cpu_clear_count(ctx)&&!screen->caps.graphics&&!screen->caps.accelerated);
  ctx->delete_vs_state(ctx,vs);ctx->delete_fs_state(ctx,fs);ctx->delete_vertex_elements_state(ctx,el);
  ctx->surface_destroy(ctx,cs);ctx->surface_destroy(ctx,zs);ctx->destroy(ctx);
  /* The remaining Z writer and returned fence independently survive context
   * teardown. Drop COLOR and the context before the final shared references. */
  remesa_resource_reference(&color,NULL);remesa_resource_reference(&vb,NULL);
  struct pipe_context *other=screen->context_create(screen,NULL,0);CHECK(other);
  CHECK(!remesa_resource_wait_writer(other,depth,0));other->destroy(other);
  remesa_resource_reference(&depth,NULL);
  CHECK(remesa_persistent_close(&d)==EBUSY); /* caller still owns the sole fence */
  screen->fence_reference(screen,&fence,NULL);CHECK(!fence);
  screen->destroy(screen);CHECK(f.creates==f.destroys&&f.pins==f.unpins);CHECK(!remesa_persistent_close(&d));
  glsl_type_singleton_decref();puts("PASS actual bound scene submit shares ONE fence across COLOR/Z/context/caller, independent waits and owner teardown; no CPU clear/draw caps; MODEL_GPU0");return 0;
 }
 CHECK(!remesa_context_prepare_linked_scene(ctx,&info,&draw,&options,&capture));
 struct remesa_scene_packet packet;CHECK(!remesa_persistent_scene_packet_snapshot(&d,capture,&packet));CHECK(packet.controls.width==width&&packet.controls.height==height&&packet.controls.depth_finalize==1);
 struct backing *z=get(&f,packet.controls.depth_id);CHECK(z);for(size_t i=0;i<z->length;i++)CHECK(z->bytes[i]==0xa5);
 struct backing *col=get(&f,packet.controls.color_id);CHECK(col);for(size_t i=0;i<col->length;i++)CHECK(col->bytes[i]==0xa5);
 struct backing *vertices=get(&f,packet.windows[1].vertex_id);CHECK(vertices);CHECK(!memcmp(vertices->bytes+packet.windows[1].vertex_offset,raw,sizeof raw));
 struct remesa_persistent_scene_footprint footprint;CHECK(!remesa_persistent_scene_capture_snapshot(&d,capture,&footprint));
 scene_capture_export(&f,&footprint,&packet);
 CHECK(!remesa_context_cpu_clear_count(ctx));
 unsigned creates=f.creates;ctx->draw_vbo(ctx,&info,0,NULL,&draw,1);CHECK(remesa_context_error(ctx)==ENOTSUP&&f.creates==creates); /* normal dispatch remains refused */
 ctx->delete_vs_state(ctx,vs);ctx->delete_fs_state(ctx,fs);ctx->delete_vertex_elements_state(ctx,el);ctx->surface_destroy(ctx,cs);ctx->surface_destroy(ctx,zs);ctx->destroy(ctx);
 remesa_resource_reference(&color,NULL);remesa_resource_reference(&depth,NULL);remesa_resource_reference(&vb,NULL);
 CHECK(!remesa_persistent_scene_packet_snapshot(&d,capture,&packet));void *fence=NULL;CHECK(!remesa_persistent_scene_capture_submit(&d,capture,&fence)&&fence);CHECK(!d.ops->wait(d.priv,fence,0,true));d.ops->destroy_fence(d.priv,fence);
 screen->destroy(screen);CHECK(f.creates==f.destroys&&f.pins==f.unpins);CHECK(!remesa_persistent_close(&d));glsl_type_singleton_decref();
 puts("PASS bound Gallium NIR/VB/FBO -> typed persistent COLOR/Z -> library scene -> retained provider fence; raw vertices exact, CPU depth path unused, normal draw/caps unchanged; MODEL_GPU0");
}
