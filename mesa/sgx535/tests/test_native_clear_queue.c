/* SPDX-License-Identifier: MIT. Real Gallium owners, modeled GEM; GPU0. */
#define REMESA_LINKED_SCENE_CONTEXT_FIXTURE
#include "test_linked_scene_context.c"

int main(void)
{
 glsl_type_singleton_init_or_ref();
 for(unsigned mode=0;mode<20;mode++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};target_ops=ops;target_ops.create=builder_create;
  struct remesa_device device={0};
  struct remesa_persistent_config cfg={.ops=&target_ops,.priv=&f,.context=9800,
   .va_start=0x20000000,.va_end=0x45000000,.byte_budget=96u*1024u*1024u,
   .resource_only_backend=true,.resource_limits={2048,2048,128,16u*1024u*1024u}};
  cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};
  cfg.placement[REMESA_VA_RASTER]=(struct remesa_persistent_va_range){0x30000000,0x34000000,1048576};
  cfg.placement[REMESA_VA_SURFACE]=(struct remesa_persistent_va_range){0x40001000,0x45000000,4096};
  CHECK(!remesa_persistent_open(&device,&cfg));
  CHECK(!remesa_persistent_set_native_scene(&device,context_native));
  struct pipe_screen *screen=remesa_screen_create(&device);CHECK(screen);
  CHECK(!remesa_screen_enable_linked_attachment_storage(screen));
  struct pipe_resource *color=attachment(screen,640,640,false),*depth=attachment(screen,640,640,true);
  CHECK(color&&depth);
  struct pipe_context *ctx=screen->context_create(screen,NULL,0),*other=screen->context_create(screen,NULL,0);
  CHECK(ctx&&other&&!remesa_context_enable_homogeneous_draw_lab(ctx));
  struct pipe_framebuffer_state fb={.width=640,.height=640,.nr_cbufs=1,
   .cbufs={{.texture=color,.format=PIPE_FORMAT_B8G8R8A8_UNORM}},
   .zsbuf={.texture=depth,.format=PIPE_FORMAT_Z24X8_UNORM}};
  if(mode==7)fb.zsbuf.texture=NULL;
  ctx->set_framebuffer_state(ctx,&fb);CHECK(!remesa_context_error(ctx));
  struct pipe_transfer *transfer=NULL;const struct pipe_box box={.width=1,.height=1,.depth=1};
  if(mode==8||mode==9||mode==19){
   CHECK(ctx->texture_map(ctx,color,0,PIPE_MAP_WRITE|(mode==19?PIPE_MAP_FLUSH_EXPLICIT:0),&box,&transfer));
   if(mode==9||mode==19){ctx->texture_unmap(ctx,transfer);transfer=NULL;}
  }
  union pipe_color_union zero={0};unsigned mask=PIPE_CLEAR_COLOR0|PIPE_CLEAR_DEPTH;
  double z=1;struct pipe_scissor_state partial={.maxx=639,.maxy=640};
  if(mode==2)zero.f[1]=.5f;
  if(mode==3)z=.5;
  if(mode==5)mask|=PIPE_CLEAR_STENCIL;
  if(mode==13)mask=0;
  if(mode==15)mask=PIPE_CLEAR_COLOR0;
  if(mode==16)mask=PIPE_CLEAR_DEPTH;
  if(mode==17)zero.f[0]=NAN;
  if(mode==18)z=NAN;
  unsigned creates=f.creates,maps=f.cpu_maps;
  ctx->clear(ctx,mask,mode==4?&partial:NULL,mode==6?NULL:&zero,z,0);
  bool rejected=(mode>=2&&mode<=9)||mode==17||mode==18;
  CHECK(remesa_context_error(ctx)==(rejected?(mode==6?EINVAL:mode==8?EBUSY:ENOTSUP):0));
  CHECK(f.creates==creates&&f.cpu_maps==maps&&!dispatch_calls&&!remesa_context_cpu_clear_count(ctx));
  if(transfer)ctx->texture_unmap(ctx,transfer);
  if(!rejected&&mask){
   /* Neither a CPU map nor another context may observe unissued pixels. */
   CHECK(remesa_resource_wait_writer(other,color,0)==((mask&PIPE_CLEAR_COLOR0)?EBUSY:0));
   CHECK(remesa_resource_wait_writer(other,depth,0)==((mask&PIPE_CLEAR_DEPTH)?EBUSY:0));
   struct pipe_resource *pending=(mask&PIPE_CLEAR_COLOR0)?color:depth;
   CHECK(!other->texture_map(other,pending,0,PIPE_MAP_READ,&box,&transfer));
   CHECK(remesa_context_error(other)==EBUSY&&!transfer);
   CHECK(!other->texture_map(other,pending,0,PIPE_MAP_WRITE|PIPE_MAP_UNSYNCHRONIZED,&box,&transfer));
   CHECK(remesa_context_error(other)==EBUSY&&!transfer);
   struct pipe_fence_handle *fence=NULL;ctx->flush(ctx,&fence,0);
   CHECK(!fence&&remesa_context_error(ctx)==ENOTSUP);
   if(mode==1){
    ctx->clear(ctx,PIPE_CLEAR_COLOR0,NULL,&zero,1,0);CHECK(!remesa_context_error(ctx));
    ctx->clear(ctx,PIPE_CLEAR_DEPTH,NULL,NULL,1,0);CHECK(!remesa_context_error(ctx));
   }
   if(mode==10){
    other->set_framebuffer_state(other,&fb);CHECK(!remesa_context_error(other));
    CHECK(!remesa_context_enable_homogeneous_draw_lab(other));
    other->clear(other,mask,NULL,&zero,1,0);CHECK(remesa_context_error(other)==EBUSY);
   }
   if(mode==11){
    struct pipe_framebuffer_state unbound={0};ctx->set_framebuffer_state(ctx,&unbound);
    CHECK(!remesa_context_error(ctx));ctx->flush(ctx,&fence,0);
    CHECK(!fence&&remesa_context_error(ctx)==ENOTSUP);
   }
   if(mode==12){
    screen->destroy(screen);ctx->clear(ctx,mask,NULL,&zero,1,0);
    CHECK(remesa_context_error(ctx)==ESHUTDOWN);
   }
   if(mode==14){
    struct pipe_surface templ={.format=PIPE_FORMAT_B8G8R8A8_UNORM};
    struct pipe_surface *surface=ctx->create_surface(ctx,color,&templ);CHECK(surface);
    ctx->clear_render_target(ctx,surface,&zero,0,0,640,640,false);
    CHECK(remesa_context_error(ctx)==ENOTSUP);ctx->surface_destroy(ctx,surface);
   }
  }
  CHECK(f.cpu_maps==maps&&!dispatch_calls&&!remesa_context_cpu_clear_count(ctx));
  ctx->destroy(ctx);
  if(!rejected&&mask){
   CHECK(remesa_resource_wait_writer(other,color,0)==((mask&PIPE_CLEAR_COLOR0)?ECANCELED:0));
   CHECK(remesa_resource_wait_writer(other,depth,0)==((mask&PIPE_CLEAR_DEPTH)?ECANCELED:0));
  }
  other->destroy(other);remesa_resource_reference(&color,NULL);remesa_resource_reference(&depth,NULL);
  if(mode!=12)screen->destroy(screen);
  CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!remesa_persistent_close(&device));
 }
 glsl_type_singleton_decref();
 puts("PASS20 native clear queue ownership/atomic refusal/CPU-map exclusion/cancel/teardown cases; GPU0");
 return 0;
}
