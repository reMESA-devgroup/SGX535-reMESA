/* SPDX-License-Identifier: MIT. Host input storage, real Gallium callbacks;
 * modeled GEM attachments, no rendering or physical GPU claim. */
#define REMESA_LINKED_SCENE_CONTEXT_FIXTURE
#include "test_linked_scene_context.c"

static unsigned native_copy_calls;
static int reject_native_copy(void *priv,const struct remesa_copy_region *request)
{(void)priv;(void)request;native_copy_calls++;return ENOTSUP;}

int main(void)
{
 glsl_type_singleton_init_or_ref();
 struct fixture f={.allow_gpu_map=true,.allow_sync=true};target_ops=ops;target_ops.create=builder_create;
 struct remesa_device device={0};
 struct remesa_persistent_config cfg={.ops=&target_ops,.priv=&f,.context=9950,
  .va_start=0x20000000,.va_end=0x45000000,.byte_budget=16*4096,
  .resource_only_backend=true,.resource_limits={32,32,128,4096}};
 cfg.placement[REMESA_VA_SURFACE]=(struct remesa_persistent_va_range){0x40001000,0x45000000,4096};
 CHECK(!remesa_persistent_open(&device,&cfg));
 CHECK(!remesa_persistent_set_native_scene(&device,context_native));
 struct pipe_screen *screen=remesa_screen_create(&device);CHECK(screen);
 CHECK(!remesa_screen_enable_linked_attachment_storage(screen));
 CHECK(!remesa_screen_enable_homogeneous_frontend_lab(screen));
 CHECK(!remesa_screen_enable_native_copy(screen,reject_native_copy));
 struct pipe_context *ctx=screen->context_create(screen,NULL,0);CHECK(ctx);
 struct pipe_resource t={.target=PIPE_BUFFER,.format=PIPE_FORMAT_R8_UNORM,
  .bind=PIPE_BIND_VERTEX_BUFFER|PIPE_BIND_INDEX_BUFFER,.width0=128,.height0=1,.depth0=1,.array_size=1};
 struct pipe_resource *a=screen->resource_create(screen,&t),*b=screen->resource_create(screen,&t);CHECK(a&&b);
 CHECK(!f.creates&&!f.cpu_maps); /* No GPU allocation or lease for input storage. */
 struct pipe_box box={.width=128,.height=1,.depth=1};struct pipe_transfer *transfer=NULL;
 unsigned char *bytes=ctx->buffer_map(ctx,a,0,PIPE_MAP_READ,&box,&transfer);CHECK(bytes&&transfer);
 for(unsigned i=0;i<128;i++)CHECK(!bytes[i]);ctx->buffer_unmap(ctx,transfer);
 bytes=ctx->buffer_map(ctx,a,0,PIPE_MAP_WRITE|PIPE_MAP_FLUSH_EXPLICIT,&box,&transfer);CHECK(bytes&&transfer);
 memset(bytes,0x5a,128);const struct pipe_box dirty={.x=32,.width=16,.height=1,.depth=1};
 ctx->transfer_flush_region(ctx,transfer,&dirty);CHECK(!remesa_context_error(ctx));ctx->buffer_unmap(ctx,transfer);
 bytes=ctx->buffer_map(ctx,a,0,PIPE_MAP_READ,&box,&transfer);CHECK(bytes&&transfer);
 for(unsigned i=0;i<128;i++)CHECK(bytes[i]==(i>=32&&i<48?0x5a:0));
 CHECK(remesa_context_window_viewport_parameters_snapshot(ctx,(void *)bytes)==EINVAL);
 ctx->buffer_unmap(ctx,transfer);
 ctx->resource_copy_region(ctx,b,0,0,0,0,a,0,&box);CHECK(!remesa_context_error(ctx));
 bytes=ctx->buffer_map(ctx,b,0,PIPE_MAP_READ,&box,&transfer);CHECK(bytes&&transfer);
 for(unsigned i=0;i<128;i++)CHECK(bytes[i]==(i>=32&&i<48?0x5a:0));ctx->buffer_unmap(ctx,transfer);
 CHECK(!f.creates&&!f.cpu_maps&&!dispatch_calls&&!native_copy_calls&&!remesa_context_cpu_clear_count(ctx));

 t.width0=4096;CHECK(!screen->resource_create(screen,&t)); /* Total host budget. */
 remesa_resource_reference(&a,NULL);CHECK(!screen->resource_create(screen,&t));
 remesa_resource_reference(&b,NULL);a=screen->resource_create(screen,&t);CHECK(a);
 t.width0=1;CHECK(!screen->resource_create(screen,&t));
 remesa_resource_reference(&a,NULL);a=screen->resource_create(screen,&t);CHECK(a);
 t.bind=PIPE_BIND_RENDER_TARGET;CHECK(!screen->resource_create(screen,&t));
 struct pipe_resource *color=attachment(screen,32,32,false),*depth=attachment(screen,32,32,true);
 CHECK(color&&depth&&f.creates==2&&!f.cpu_maps); /* Render storage remains GEM. */
 remesa_resource_reference(&color,NULL);remesa_resource_reference(&depth,NULL);
 screen->destroy(screen);box.width=1;
 CHECK(!ctx->buffer_map(ctx,a,0,PIPE_MAP_READ,&box,&transfer)&&!transfer);
 CHECK(remesa_context_error(ctx)==ESHUTDOWN);
 ctx->destroy(ctx);remesa_resource_reference(&a,NULL);
 CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden&&!remesa_persistent_close(&device));
 glsl_type_singleton_decref();
 puts("PASS native host-input zero initialization, explicit flush/copy/alias guards, total budget reuse, GEM-only attachments and closing-screen lifetime; GPU0");
 return 0;
}
