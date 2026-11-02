/* SPDX-License-Identifier: MIT. Actual screen/provider lifetime admission;
 * GEM is modeled, no ioctl, MMIO, rendering or physical qualification. */
#define REMESA_LINKED_SCENE_CONTEXT_FIXTURE
#include "test_linked_scene_context.c"

int main(void)
{
 glsl_type_singleton_init_or_ref();
 CHECK(remesa_screen_enable_homogeneous_frontend_lab(NULL)==EINVAL);
 struct pipe_screen foreign={0};CHECK(remesa_screen_enable_homogeneous_frontend_lab(&foreign)==EINVAL);
 struct remesa_device foreign_device={0};
 CHECK(remesa_persistent_native_scene_ready(NULL)==EINVAL);
 CHECK(remesa_persistent_native_scene_ready(&foreign_device)==EINVAL);
 for(unsigned mode=0;mode<12;mode++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};target_ops=ops;target_ops.create=builder_create;
  struct remesa_device device={0};
  struct remesa_persistent_config cfg={.ops=&target_ops,.priv=&f,.context=9700,
   .va_start=0x20000000,.va_end=0x45000000,.byte_budget=96u*1024u*1024u,
   .resource_only_backend=true,.resource_limits={2048,2048,128,16u*1024u*1024u}};
  cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};
  cfg.placement[REMESA_VA_RASTER]=(struct remesa_persistent_va_range){0x30000000,0x34000000,1048576};
  cfg.placement[REMESA_VA_SURFACE]=(struct remesa_persistent_va_range){0x40001000,0x45000000,4096};
  if(mode==6){cfg.resource_limits.max_width=1000;cfg.resource_limits.max_height=600;}
  if(mode==5){cfg.resource_only_backend=false;cfg.offline_test_backend=true;}
  if(mode==7)cfg.resource_limits.max_width=cfg.resource_limits.max_height=0;
  if(mode==8)cfg.resource_limits.max_width=31;
  if(mode==9)cfg.resource_limits.max_width=cfg.resource_limits.max_height=0x80000000u;
  CHECK(!remesa_persistent_open(&device,&cfg));
  CHECK(remesa_persistent_native_scene_ready(&device)==EOPNOTSUPP);
  if(mode!=11)CHECK(!remesa_persistent_set_native_scene(&device,context_native)&&!remesa_persistent_native_scene_ready(&device));
  struct pipe_screen *screen=remesa_screen_create(&device);CHECK(screen);
  if(mode&&mode!=5)CHECK(!remesa_screen_enable_linked_attachment_storage(screen));
  struct pipe_caps before=screen->caps;
  struct pipe_shader_caps vertex=screen->shader_caps[PIPE_SHADER_VERTEX];
  struct pipe_context *ctx=NULL;struct pipe_resource *color=NULL;
  if(mode==2||mode==4){ctx=screen->context_create(screen,NULL,0);CHECK(ctx);}
  if(mode==3){color=attachment(screen,32,32,false);CHECK(color);}
  if(mode==4)screen->destroy(screen); /* Child retains a closing screen. */
  unsigned creates=f.creates,maps=f.cpu_maps;
  int error=remesa_screen_enable_homogeneous_frontend_lab(screen);
  if(mode==0||mode==2||mode==3||mode==4||mode==5||mode==8||mode==11){
   CHECK(error==(mode==4?ESHUTDOWN:mode==2||mode==3?EBUSY:mode==11?EOPNOTSUPP:ENOTSUP));
   CHECK(!memcmp(&before,&screen->caps,sizeof before));
   CHECK(!memcmp(&vertex,&screen->shader_caps[PIPE_SHADER_VERTEX],sizeof vertex));
  }else{
   CHECK(!error&&screen->caps.graphics&&!screen->caps.accelerated);
   CHECK(screen->caps.max_texture_2d_size==(mode==6?512:mode==7?32:2048));
   CHECK(screen->caps.viewport_transform_lowered==before.viewport_transform_lowered);
   CHECK(!screen->is_format_supported(screen,PIPE_FORMAT_B8G8R8A8_UNORM,
    PIPE_TEXTURE_2D,0,0,PIPE_BIND_SAMPLER_VIEW));
   CHECK(remesa_screen_enable_homogeneous_frontend_lab(screen)==EALREADY);
   ctx=screen->context_create(screen,NULL,0);CHECK(ctx);
   CHECK(remesa_context_enable_homogeneous_draw_lab(ctx)==EALREADY);
   if(mode==10)CHECK(remesa_screen_enable_homogeneous_frontend_lab(screen)==EALREADY);
  }
  CHECK(f.creates==creates&&f.cpu_maps==maps&&!dispatch_calls);
  if(mode==3){
   remesa_resource_reference(&color,NULL);
   CHECK(remesa_screen_enable_homogeneous_frontend_lab(screen)==EBUSY);
  }
  if(ctx)ctx->destroy(ctx);
  if(mode!=4)screen->destroy(screen);
  CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!remesa_persistent_close(&device));
 }
 glsl_type_singleton_decref();
 puts("PASS12 frontend lifecycle/limits/dispatcher/failure-immutability cases + null/foreign screen; no new sampler/accelerated cap, IO or GPU");
 return 0;
}
