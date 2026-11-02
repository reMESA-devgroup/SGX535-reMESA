/* SPDX-License-Identifier: MIT. Real Gallium/NIR/persistent ownership; modeled
 * native receipts and CPU bytes. No GPU, server or rendered-pixel claim. */
#define REMESA_LINKED_SCENE_CONTEXT_FIXTURE
#include "test_linked_scene_context.c"
#include "remesa_present.h"
static unsigned map_failure,read_maps,read_unmaps;
static int (*saved_map)(void *,uint32_t,bool,void **);
static int (*saved_unmap)(void *,uint32_t,bool);
static int frontend_read_map(void *p,uint32_t id,bool write,void **out)
{
 CHECK(!write);read_maps++;
 if(map_failure==1)return EOWNERDEAD;
 return saved_map(p,id,false,out);
}
static int frontend_read_unmap(void *p,uint32_t id,bool write)
{
 CHECK(!write);read_unmaps++;
 if(map_failure==2)return EOWNERDEAD;
 return saved_unmap(p,id,false);
}
int main(int argc,char **argv)
{
 CHECK(argc==2);unsigned test_case=(unsigned)strtoul(argv[1],NULL,10);CHECK(test_case<8);unsigned mode=test_case<4?test_case:0;
 bool matrix=mode==22||mode==23;
 unsigned width=mode==1||mode==14||mode==23||mode==25?640:mode==2||mode==15?641:mode==3?1600:1280,height=mode==1||mode==14||mode==23||mode==25?640:mode==2||mode==15?479:mode==3?768:800;
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
 float raw[3][12]={{width*.25f,height*.25f,.125f,1,0,0,1,0,0,0,1,0},{width*.75f,height*.25f,.125f,1,0,0,1,0,0,0,1,0},{width*.25f,height*.75f,.125f,1,0,0,1,0,0,0,1,0}};
 /* Explicit new input contract, never a CPU transform of submitted vertices.
  * The unchanged POSITION program passes these clip coordinates to the TA. */
 if(matrix){
  const float clip[3][4]={{-.5f,.5f,-.75f,1},{.5f,.5f,-.75f,1},{-.5f,-.5f,-.75f,1}};
  for(unsigned i=0;i<3;i++)memcpy(raw[i],clip[i],sizeof clip[i]);
 }
 const struct pipe_resource vb_template={.target=PIPE_BUFFER,.format=PIPE_FORMAT_R8_UNORM,.bind=PIPE_BIND_VERTEX_BUFFER,.width0=sizeof raw,.height0=1,.depth0=1,.array_size=1};
 struct pipe_resource *vb=screen->resource_create(screen,&vb_template);CHECK(vb);ctx->buffer_subdata(ctx,vb,0,0,sizeof raw,raw);CHECK(!remesa_context_error(ctx));
 struct pipe_vertex_buffer binding={.buffer.resource=vb};vb->reference.count++;ctx->set_vertex_buffers(ctx,1,&binding);CHECK(!remesa_context_error(ctx));
 struct pipe_vertex_element elements[3]={0};for(unsigned a=0;a<3;a++){elements[a].src_format=PIPE_FORMAT_R32G32B32A32_FLOAT;elements[a].src_stride=48;elements[a].src_offset=a*16;}
 void *el=ctx->create_vertex_elements_state(ctx,3,elements);CHECK(el);ctx->bind_vertex_elements_state(ctx,el);
 struct pipe_draw_info info={.mode=MESA_PRIM_TRIANGLES,.instance_count=1};struct pipe_draw_start_count_bias draw={.count=3};
 struct remesa_linked_scene_request options={.state={.isp={.depth_bits_present=true,.depth_test=true,.depth_write=true,.compare=REMESA_DEPTH_LESS,.rgba_mask=15},.viewport={.framebuffer_width=width,.framebuffer_height=height,.width=width,.height=height,.depth_bits=24,.far_z=1,.path=REMESA_TA_VIEWPORT_INITIAL_ZERO}},.vertex_bank=4,.pixel_bank=3,.state_bank=4,.clear_depth=width==640?.25f:1};
 if(matrix){
  const struct pipe_viewport_state viewport={.scale={width*.5f,-(float)height*.5f,.5f},.translate={width*.5f,height*.5f,.5f}};
  ctx->set_viewport_states(ctx,0,1,&viewport);CHECK(!remesa_context_error(ctx));
  unsigned creates=f.creates,maps=f.cpu_maps;
  CHECK(!remesa_context_window_viewport_snapshot(ctx,24,&options.state.viewport));
  CHECK(options.state.viewport.path==REMESA_TA_VIEWPORT_MATRIX_WINDOW&&f.creates==creates&&f.cpu_maps==maps);
 }

 struct remesa_device_ops incomplete=*d.ops;struct remesa_device invalid=d;
 incomplete.unmap_retired_color=NULL;invalid.ops=&incomplete;
 CHECK(!remesa_screen_create(&invalid));
 struct pipe_box box={.width=width,.height=height,.depth=1};struct pipe_transfer *transfer=NULL;
 unsigned maps=f.cpu_maps;
 CHECK(!ctx->texture_map(ctx,color,0,PIPE_MAP_READ|PIPE_MAP_DONTBLOCK,&box,&transfer)&&!transfer);
 CHECK(remesa_context_error(ctx)==EAGAIN&&f.cpu_maps==maps); /* unheld fallback, undefined contents */
 struct remesa_scene_capture capture={0};
 CHECK(!remesa_context_prepare_linked_scene(ctx,&info,&draw,&options,&capture));
 maps=f.cpu_maps;
 CHECK(!ctx->texture_map(ctx,color,0,PIPE_MAP_READ|PIPE_MAP_DONTBLOCK,&box,&transfer)&&!transfer);
 CHECK(remesa_context_error(ctx)==EAGAIN&&f.cpu_maps==maps); /* held but no publication, no fallback */
 void *fence=NULL;
 if(test_case!=4){
  CHECK(!remesa_persistent_scene_capture_submit_keep(&d,capture,&fence));
  CHECK(!d.ops->wait(d.priv,fence,0,true));
  saved_map=target_ops.cpu_map;saved_unmap=target_ops.cpu_unmap;
  target_ops.cpu_map=frontend_read_map;target_ops.cpu_unmap=frontend_read_unmap;
  unsigned writes=f.writes;size_t stride=(size_t)width*4+16,bytes=stride*height;
  unsigned char *copy=malloc(bytes);CHECK(copy);memset(copy,0xcc,bytes);
  if(test_case==5)map_failure=1;
  if(test_case==6)map_failure=2;
  if(test_case==7){
   struct pipe_box corner={.x=width-1,.y=height-1,.width=1,.height=1,.depth=1};
   CHECK(ctx->texture_map(ctx,color,0,PIPE_MAP_READ|PIPE_MAP_DONTBLOCK,&corner,&transfer)&&transfer);
   CHECK(transfer->stride>=width*4);
   void *next=NULL;CHECK(remesa_persistent_scene_capture_submit_keep(&d,capture,&next)==EBUSY&&!next);
   CHECK(remesa_persistent_scene_capture_release(&d,capture)==EBUSY);
   ctx->texture_unmap(ctx,transfer);CHECK(!remesa_context_error(ctx));transfer=NULL;
  }
  int error=remesa_present_snapshot(ctx,color,copy,stride,bytes);
  CHECK(f.writes==writes&&!remesa_context_cpu_clear_count(ctx));
  if(test_case==5||test_case==6){
   CHECK(error==EOWNERDEAD&&remesa_context_error(ctx)==EOWNERDEAD);
   CHECK(remesa_resource_wait_writer(ctx,color,0)==EOWNERDEAD);
   unsigned calls=dispatch_calls;void *next=NULL;
   CHECK(remesa_persistent_scene_capture_submit_keep(&d,capture,&next)==EOWNERDEAD&&!next&&dispatch_calls==calls);
   if(test_case==5)for(size_t i=0;i<bytes;i++)CHECK(copy[i]==0xcc);
   free(copy);d.ops->destroy_fence(d.priv,fence);
   unsigned unpins=f.unpins,destroys=f.destroys;
   ctx->delete_vs_state(ctx,vs);ctx->delete_fs_state(ctx,fs);ctx->delete_vertex_elements_state(ctx,el);
   ctx->surface_destroy(ctx,cs);ctx->surface_destroy(ctx,zs);ctx->destroy(ctx);
   remesa_resource_reference(&color,NULL);remesa_resource_reference(&depth,NULL);remesa_resource_reference(&vb,NULL);
   screen->destroy(screen);CHECK(f.unpins==unpins&&f.destroys==destroys&&remesa_persistent_close(&d)==EBUSY);
   glsl_type_singleton_decref();puts("PASS Gallium/presentation propagate retained COLOR lease failure, no cleanup or GPU retry; model GPU0");return 0;
  }
  CHECK(!error&&read_maps==(test_case==7?2u:1u)&&read_unmaps==read_maps);
  for(unsigned y=0;y<height;y++){
   /* Copies only the modeled allocation's initial bytes. This is transfer
    * verification, never a native-render result or physical oracle. */
   for(unsigned x=0;x<width*4;x++)CHECK(copy[y*stride+x]==0xa5);
   for(unsigned x=width*4;x<stride;x++)CHECK(copy[y*stride+x]==0xcc);
  }
  free(copy);d.ops->destroy_fence(d.priv,fence);fence=NULL;
  CHECK(!remesa_persistent_scene_capture_submit_keep(&d,capture,&fence));d.ops->destroy_fence(d.priv,fence);
 }
 CHECK(!remesa_persistent_scene_capture_release(&d,capture));
 ctx->delete_vs_state(ctx,vs);ctx->delete_fs_state(ctx,fs);ctx->delete_vertex_elements_state(ctx,el);
 ctx->surface_destroy(ctx,cs);ctx->surface_destroy(ctx,zs);ctx->destroy(ctx);
 remesa_resource_reference(&color,NULL);remesa_resource_reference(&depth,NULL);remesa_resource_reference(&vb,NULL);
 screen->destroy(screen);CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!remesa_persistent_close(&d));
 glsl_type_singleton_decref();puts("PASS Gallium texture READ/presentation uses held retired COLOR lease, layout/padding and repeated native-provider operation; model GPU0");
}
