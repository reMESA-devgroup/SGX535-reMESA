/* SPDX-License-Identifier: MIT. Actual Gallium/NIR/provider ownership,
 * modeled native completion and bytes only; no hardware/FPS qualification. */
#define REMESA_LINKED_SCENE_CONTEXT_FIXTURE
#include "test_linked_scene_context.c"
#include "remesa_present.h"
int main(int argc,char **argv)
{
 CHECK(argc==2);unsigned test_case=(unsigned)strtoul(argv[1],NULL,10);CHECK(test_case<11);unsigned mode=test_case<4?test_case:0;
 unsigned width=mode==1?640:mode==2?641:mode==3?1600:1280,height=mode==1?640:mode==2?479:mode==3?768:800;
 glsl_type_singleton_init_or_ref();struct fixture f={.allow_gpu_map=true,.allow_sync=true};target_ops=ops;target_ops.create=builder_create;
 struct remesa_device d={0};struct remesa_persistent_config cfg={.ops=&target_ops,.priv=&f,.context=9200,.va_start=0x20000000,.va_end=0x45000000,.byte_budget=96u*1024u*1024u,.resource_only_backend=true,.resource_limits={2048,2048,128,16u*1024u*1024u}};
 cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};cfg.placement[REMESA_VA_RASTER]=(struct remesa_persistent_va_range){0x30000000,0x34000000,1048576};cfg.placement[REMESA_VA_SURFACE]=(struct remesa_persistent_va_range){0x40001000,0x45000000,4096};
 CHECK(!remesa_persistent_open(&d,&cfg));CHECK(!remesa_persistent_set_native_scene(&d,context_native));
 struct pipe_screen *screen=remesa_screen_create(&d);CHECK(screen);
 CHECK(!screen->caps.graphics&&!screen->caps.accelerated);
 CHECK(!screen->is_format_supported(screen,PIPE_FORMAT_Z24X8_UNORM,PIPE_TEXTURE_2D,0,0,PIPE_BIND_DEPTH_STENCIL));
 CHECK(!attachment(screen,width,height,false)&&!attachment(screen,width,height,true)&&!f.creates);
 CHECK(!remesa_screen_enable_linked_attachment_storage(screen));CHECK(remesa_screen_enable_linked_attachment_storage(screen)==EBUSY);
 CHECK(!screen->caps.graphics&&!screen->caps.accelerated);CHECK(screen->is_format_supported(screen,PIPE_FORMAT_Z24X8_UNORM,PIPE_TEXTURE_RECT,0,0,PIPE_BIND_DEPTH_STENCIL));
 struct pipe_resource *color=attachment(screen,width,height,false),*depth=attachment(screen,width,height,true);CHECK(color&&depth&&f.creates==2);
 struct pipe_context *ctx=screen->context_create(screen,NULL,0);CHECK(ctx);
 struct pipe_surface *cs=surface(ctx,color),*zs=surface(ctx,depth);CHECK(cs&&zs);
 const struct pipe_framebuffer_state fb={.width=width,.height=height,.nr_cbufs=1,.cbufs={{.texture=color,.format=PIPE_FORMAT_B8G8R8A8_UNORM}},.zsbuf={.texture=depth,.format=PIPE_FORMAT_Z24X8_UNORM}};
 ctx->set_framebuffer_state(ctx,&fb);CHECK(!remesa_context_error(ctx));
 struct pipe_shader_state sh={.type=PIPE_SHADER_IR_NIR,.ir.nir=linked_shader(0)};void *vs=ctx->create_vs_state(ctx,&sh);CHECK(vs);ctx->bind_vs_state(ctx,vs);sh.ir.nir=linked_shader(1);void *fs=ctx->create_fs_state(ctx,&sh);CHECK(fs);ctx->bind_fs_state(ctx,fs);
 float raw[3][12]={{width*.25f,height*.25f,.125f,1,0,0,1,0,0,0,1,0},{width*.75f,height*.25f,.125f,1,0,0,1,0,0,0,1,0},{width*.25f,height*.75f,.125f,1,0,0,1,0,0,0,1,0}};

 const struct pipe_resource vb_template={.target=PIPE_BUFFER,.format=PIPE_FORMAT_R8_UNORM,.bind=PIPE_BIND_VERTEX_BUFFER,.width0=sizeof raw,.height0=1,.depth0=1,.array_size=1};
 struct pipe_resource *vb=screen->resource_create(screen,&vb_template);CHECK(vb);ctx->buffer_subdata(ctx,vb,0,0,sizeof raw,raw);CHECK(!remesa_context_error(ctx));
 struct pipe_vertex_buffer binding={.buffer.resource=vb};vb->reference.count++;ctx->set_vertex_buffers(ctx,1,&binding);CHECK(!remesa_context_error(ctx));
 struct pipe_vertex_element elements[3]={0};for(unsigned a=0;a<3;a++){elements[a].src_format=PIPE_FORMAT_R32G32B32A32_FLOAT;elements[a].src_stride=48;elements[a].src_offset=a*16;}
 void *el=ctx->create_vertex_elements_state(ctx,3,elements);CHECK(el);ctx->bind_vertex_elements_state(ctx,el);
 struct pipe_draw_info info={.mode=MESA_PRIM_TRIANGLES,.instance_count=1};struct pipe_draw_start_count_bias draw={.count=3};
 struct remesa_linked_scene_request options={.state={.isp={.depth_bits_present=true,.depth_test=true,.depth_write=true,.compare=REMESA_DEPTH_LESS,.rgba_mask=15},.viewport={.framebuffer_width=width,.framebuffer_height=height,.width=width,.height=height,.depth_bits=24,.far_z=1,.path=REMESA_TA_VIEWPORT_INITIAL_ZERO}},.vertex_bank=4,.pixel_bank=3,.state_bank=4,.clear_depth=width==640?.25f:1};



 struct remesa_scene_capture capture={0};
 CHECK(!remesa_context_prepare_linked_scene(ctx,&info,&draw,&options,&capture));
 struct pipe_fence_handle *fence=NULL;
 if(test_case==6){
  struct pipe_resource *other=attachment(screen,width,height,false);CHECK(other);
  struct pipe_framebuffer_state wrong=fb;wrong.cbufs[0].texture=other;
  ctx->set_framebuffer_state(ctx,&wrong);CHECK(!remesa_context_error(ctx));
  CHECK(remesa_context_submit_captured_scene(ctx,capture,&fence)==EACCES&&!fence&&!dispatch_calls);
  ctx->set_framebuffer_state(ctx,&fb);CHECK(!remesa_context_error(ctx));
  remesa_resource_reference(&other,NULL);
 }
 if(test_case==8){
  CHECK(remesa_context_submit_captured_scene(ctx,capture,(void *)ctx)==EINVAL);
  CHECK(remesa_context_submit_captured_scene(ctx,capture,(void *)color)==EINVAL);
  CHECK(remesa_context_submit_captured_scene(ctx,capture,(void *)d.priv)==EINVAL);
  CHECK(!dispatch_calls);
 }
 size_t stride=(size_t)width*4+16,bytes=stride*height;
 unsigned char *copy=malloc(bytes);CHECK(copy);
 unsigned creates=f.creates,writes=f.writes;
 for(unsigned i=0;i<8;i++){
  if(i==1&&test_case==4){
   dispatch_mode=1;
   CHECK(remesa_context_submit_captured_scene(ctx,capture,&fence)==EOPNOTSUPP&&!fence);
   struct pipe_fence_handle *last=NULL;ctx->flush(ctx,&last,0);
   CHECK(last&&screen->fence_finish(screen,ctx,last,0));screen->fence_reference(screen,&last,NULL);
   CHECK(!remesa_context_error(ctx));dispatch_mode=0;
  }
  if(i==1&&(test_case==5||test_case==10)){
   dispatch_mode=2;
   if(test_case==10){
    /* Another caller sharing the serialized device lost ownership first. */
    void *lost=NULL;
    CHECK(remesa_persistent_scene_capture_submit_keep(&d,capture,&lost)==EOWNERDEAD&&lost);
    d.ops->destroy_fence(d.priv,lost);
   }
   CHECK(remesa_context_submit_captured_scene(ctx,capture,&fence)==EOWNERDEAD&&!fence);
   CHECK(remesa_context_error(ctx)==EOWNERDEAD);
   struct pipe_fence_handle *last=NULL;ctx->flush(ctx,&last,0);CHECK(!last);
   CHECK(remesa_resource_wait_writer(ctx,color,0)==EOWNERDEAD);
   unsigned calls=dispatch_calls,unpins=f.unpins,destroys=f.destroys;
   CHECK(remesa_context_submit_captured_scene(ctx,capture,&fence)==EOWNERDEAD&&!fence&&dispatch_calls==calls);
   CHECK(remesa_persistent_scene_capture_release(&d,capture)==EOWNERDEAD);
   CHECK(remesa_persistent_close(&d)==EBUSY&&f.unpins==unpins&&f.destroys==destroys);
   puts("PASS later frontend uncertainty poisons context/COLOR/Z and retains all owners; model GPU0");return 0;
  }
  CHECK(!remesa_context_submit_captured_scene(ctx,capture,&fence)&&fence);
  CHECK(screen->fence_finish(screen,ctx,fence,0));screen->fence_reference(screen,&fence,NULL);
  CHECK(!remesa_resource_wait_writer(ctx,color,0)&&!remesa_resource_wait_writer(ctx,depth,0));
  if(i==0&&test_case==7){
   struct pipe_box corner={.width=1,.height=1,.depth=1};struct pipe_transfer *transfer=NULL;
   CHECK(ctx->texture_map(ctx,color,0,PIPE_MAP_READ|PIPE_MAP_DONTBLOCK,&corner,&transfer)&&transfer);
   unsigned calls=dispatch_calls;
   CHECK(remesa_context_submit_captured_scene(ctx,capture,&fence)==EBUSY&&!fence&&dispatch_calls==calls);
   CHECK(remesa_persistent_scene_capture_release(&d,capture)==EBUSY);
   ctx->texture_unmap(ctx,transfer);CHECK(!remesa_context_error(ctx));
  }
  memset(copy,0xcc,bytes);CHECK(!remesa_present_snapshot(ctx,color,copy,stride,bytes));
  for(unsigned y=0;y<height;y++){
   CHECK(copy[y*stride]==0xa5&&copy[y*stride+width*4-1]==0xa5);
   for(unsigned x=width*4;x<stride;x++)CHECK(copy[y*stride+x]==0xcc);
  }
  CHECK(f.creates==creates&&f.writes==writes&&!remesa_context_cpu_clear_count(ctx));
 }
 free(copy);CHECK(!remesa_persistent_scene_capture_release(&d,capture));
 if(test_case==9){unsigned calls=dispatch_calls;CHECK(remesa_context_submit_captured_scene(ctx,capture,&fence)==ESTALE&&!fence&&dispatch_calls==calls);}
 ctx->delete_vs_state(ctx,vs);ctx->delete_fs_state(ctx,fs);ctx->delete_vertex_elements_state(ctx,el);
 ctx->surface_destroy(ctx,cs);ctx->surface_destroy(ctx,zs);ctx->destroy(ctx);
 remesa_resource_reference(&color,NULL);remesa_resource_reference(&depth,NULL);remesa_resource_reference(&vb,NULL);
 screen->destroy(screen);CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!remesa_persistent_close(&d));
 glsl_type_singleton_decref();puts("PASS eight captured frontend submissions, exact attachments/shared fences/readback and owner cleanup; model GPU0");return 0;
}
