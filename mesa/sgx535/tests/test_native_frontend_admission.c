/* SPDX-License-Identifier: MIT. Native frontend lifecycle/admission only.
 * Actual resource/provider contracts, modeled GEM. No pixels or GPU work. */
#define REMESA_LINKED_SCENE_CONTEXT_FIXTURE
#include "test_linked_scene_context.c"

static int unused_fill(void *priv,const struct remesa_gpu_fill *fill)
{(void)priv;(void)fill;CHECK(0);return EIO;}

int main(void)
{
 glsl_type_singleton_init_or_ref();
 struct pipe_screen foreign={0};struct remesa_device foreign_device={0};
 CHECK(remesa_screen_enable_native_draws(NULL)==EINVAL);
 CHECK(remesa_screen_enable_native_draws(&foreign)==EINVAL);
 CHECK(remesa_persistent_native_draw_ready(NULL)==EINVAL);
 CHECK(remesa_persistent_native_draw_ready(&foreign_device)==EINVAL);
 CHECK(remesa_persistent_set_native_draw_scene(NULL,context_native)==EINVAL);
 for(unsigned mode=0;mode<10;mode++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};target_ops=ops;target_ops.create=builder_create;
  struct remesa_device device={0};
  struct remesa_persistent_config cfg={.ops=&target_ops,.priv=&f,.context=9800,
   .va_start=0x20000000,.va_end=0x45000000,.byte_budget=96u*1024u*1024u,
   .resource_only_backend=true,.resource_limits={2048,2048,128,16u*1024u*1024u}};
  cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};
  cfg.placement[REMESA_VA_RASTER]=(struct remesa_persistent_va_range){0x30000000,0x34000000,1048576};
  cfg.placement[REMESA_VA_SURFACE]=(struct remesa_persistent_va_range){0x40001000,0x45000000,4096};
  if(mode==7)cfg.resource_limits.max_width=31;
  if(mode==8){cfg.resource_limits.max_width=1000;cfg.resource_limits.max_height=600;}
  CHECK(!remesa_persistent_open(&device,&cfg));
  CHECK(remesa_persistent_native_draw_ready(&device)==EOPNOTSUPP);
  if(mode==1)CHECK(!remesa_persistent_set_native_scene(&device,context_native));
  else if(mode!=0)CHECK(!remesa_persistent_set_native_draw_scene(&device,context_native));
  if(mode>1){
   CHECK(!remesa_persistent_native_draw_ready(&device));
   CHECK(remesa_persistent_set_native_scene(&device,context_native)==EBUSY);
   CHECK(remesa_persistent_set_native_draw_scene(&device,context_native)==EBUSY);
  }
  CHECK(!remesa_persistent_set_native_fill(&device,unused_fill));
  struct pipe_screen *screen=remesa_screen_create(&device);CHECK(screen);
  if(mode!=2)CHECK(!remesa_screen_enable_linked_attachment_storage(screen));
  if(mode!=3)CHECK(!remesa_screen_enable_native_fill(screen,remesa_persistent_fill_region));
  struct pipe_context *ctx=NULL;struct pipe_resource *color=NULL;
  if(mode==4||mode==6){ctx=screen->context_create(screen,NULL,0);CHECK(ctx);}
  if(mode==5){color=attachment(screen,32,32,false);CHECK(color);}
  if(mode==6)screen->destroy(screen);
  struct pipe_caps before=screen->caps;
  struct pipe_shader_caps vs=screen->shader_caps[PIPE_SHADER_VERTEX];
  unsigned creates=f.creates,maps=f.cpu_maps;
  int error=remesa_screen_enable_native_draws(screen);
  if(mode<8){
   CHECK(error==(mode==0||mode==1?EOPNOTSUPP:mode==4||mode==5?EBUSY:mode==6?ESHUTDOWN:ENOTSUP));
   CHECK(!memcmp(&before,&screen->caps,sizeof before));
   CHECK(!memcmp(&vs,&screen->shader_caps[PIPE_SHADER_VERTEX],sizeof vs));
   if(mode==1)CHECK(remesa_persistent_native_draw_ready(&device)==EOPNOTSUPP);
  }else{
   CHECK(!error&&screen->caps.graphics&&!screen->caps.accelerated);
   CHECK(screen->caps.max_texture_2d_size==(mode==8?512:2048));
   CHECK(remesa_screen_enable_native_draws(screen)==EALREADY);
   CHECK(remesa_screen_enable_homogeneous_frontend_lab(screen)==ENOTSUP);
   ctx=screen->context_create(screen,NULL,0);CHECK(ctx);
   CHECK(remesa_context_enable_homogeneous_draw_lab(ctx)==ENOTSUP);
   CHECK(remesa_context_enable_homogeneous_api_lab(ctx,REMESA_HOMOGENEOUS_API_W2_CLAMPED)==ENOTSUP);
  }
  CHECK(f.creates==creates&&f.cpu_maps==maps&&!dispatch_calls);
  if(color){
   remesa_resource_reference(&color,NULL);
   CHECK(remesa_screen_enable_native_draws(screen)==EBUSY);
   CHECK(remesa_persistent_set_native_draw_scene(&device,context_native)==EBUSY);
  }
  if(ctx)ctx->destroy(ctx);
  if(mode!=6)screen->destroy(screen);
  CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!remesa_persistent_close(&device));
 }
 glsl_type_singleton_decref();
 puts("PASS10 native frontend lifetime/dispatcher separation/failure-immutability cases; GPU0");
 return 0;
}
