/* SPDX-License-Identifier: MIT. Real Gallium/NIR/PARAM encoders and owners;
 * native writes/retirement modeled. No physical GPU or pixel claim. */
#define REMESA_LINKED_SCENE_CONTEXT_FIXTURE
#include "test_linked_scene_context.c"
#include "authored_gears_vertex_nir_fixture.h"
static unsigned data_calls,data_fault,gpu_calls;
static uint64_t data_sequence,last_gpu;
static enum remesa_dispatch parameter_gpu(void *priv,const struct remesa_gpu_scene_submit *s,
 struct remesa_gpu_scene_receipt *r)
{
 (void)priv;CHECK(remesa_scene_packet_valid(s->packet)&&s->packet->binding_count==18);
 CHECK(s->operation>last_gpu);last_gpu=s->operation;gpu_calls++;
 *r=(struct remesa_gpu_scene_receipt){.context=s->context,.operation=s->operation,
  .retired=true,.published=true,.acknowledged=true};return REMESA_ISSUED;
}
static int parameter_write(void *priv,const struct remesa_gpu_scene_parameters *u,
 struct remesa_gpu_scene_parameter_receipt *r)
{
 struct fixture *f=priv;data_calls++;
 CHECK(u&&u->context==9200&&u->serial&&u->published_operation==last_gpu&&
  u->sequence==data_sequence+1&&u->draw==1&&u->words==36&&remesa_parameter_window_valid(&u->window));
 if(data_fault==1)return ENOMEM;
 if(data_fault==2)return EOWNERDEAD;
 struct backing *b=get(f,u->window.id);CHECK(b->length==u->window.allocation_bytes);
 /* This is a modeled native data write, not a general map/write exception. */
 for(unsigned i=0;i<u->words;i++)for(unsigned j=0;j<4;j++)b->bytes[4+4*i+j]=(uint8_t)(u->bits[i]>>(8*j));
 data_sequence=u->sequence;
 *r=(struct remesa_gpu_scene_parameter_receipt){u->context,u->serial,u->published_operation,u->sequence};
 if(data_fault==3)r->sequence++;
 if(data_fault==4)r->context++;
 return 0;
}
int main(int argc,char **argv)
{
 CHECK(argc==3);unsigned mode=(unsigned)strtoul(argv[1],NULL,10),size=(unsigned)strtoul(argv[2],NULL,10);
 CHECK(mode<10&&size<4);const unsigned sizes[][2]={{1280,800},{640,640},{641,479},{1600,768}};
 unsigned width=sizes[size][0],height=sizes[size][1];
 glsl_type_singleton_init_or_ref();struct fixture f={.allow_gpu_map=true,.allow_sync=true};
 target_ops=ops;target_ops.create=builder_create;
 struct remesa_device d={0};struct remesa_persistent_config cfg={.ops=&target_ops,.priv=&f,.context=9200,
  .va_start=0x20000000,.va_end=0x45000000,.byte_budget=96u*1024u*1024u,
  .resource_only_backend=true,.resource_limits={2048,2048,128,16u*1024u*1024u}};
 cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};
 cfg.placement[REMESA_VA_RASTER]=(struct remesa_persistent_va_range){0x30000000,0x34000000,1048576};
 cfg.placement[REMESA_VA_SURFACE]=(struct remesa_persistent_va_range){0x40001000,0x45000000,4096};
 CHECK(!remesa_persistent_open(&d,&cfg));CHECK(!remesa_persistent_set_native_scene(&d,parameter_gpu));
 if(mode!=8)CHECK(!remesa_persistent_set_native_scene_parameters(&d,parameter_write));
 struct pipe_screen *screen=remesa_screen_create(&d);CHECK(screen);
 CHECK(!remesa_screen_enable_linked_attachment_storage(screen));
 struct pipe_resource *color=attachment(screen,width,height,false),*depth=attachment(screen,width,height,true);
 CHECK(color&&depth);struct pipe_context *ctx=screen->context_create(screen,NULL,0);CHECK(ctx);
 const struct pipe_framebuffer_state fb={.width=width,.height=height,.nr_cbufs=1,
  .cbufs={{.texture=color,.format=PIPE_FORMAT_B8G8R8A8_UNORM}},.zsbuf={.texture=depth,.format=PIPE_FORMAT_Z24X8_UNORM}};
 ctx->set_framebuffer_state(ctx,&fb);
 const struct pipe_viewport_state viewport={.scale={width*.5f,-(float)height*.5f,.5f},.translate={width*.5f,height*.5f,.5f}};
 ctx->set_viewport_states(ctx,0,1,&viewport);CHECK(!remesa_context_error(ctx));
 struct pipe_shader_state sh={.type=PIPE_SHADER_IR_NIR,.ir.nir=make(true,0)};
 void *vs=ctx->create_vs_state(ctx,&sh);CHECK(vs);ctx->bind_vs_state(ctx,vs);
 sh.ir.nir=linked_shader(1);void *fs=ctx->create_fs_state(ctx,&sh);CHECK(fs);ctx->bind_fs_state(ctx,fs);
 uint32_t coefficients[36];for(unsigned i=0;i<36;i++)coefficients[i]=0x3f800000;
 struct pipe_constant_buffer cb={.user_buffer=coefficients,.buffer_size=sizeof coefficients};
 ctx->set_constant_buffer(ctx,PIPE_SHADER_VERTEX,0,false,&cb);CHECK(!remesa_context_error(ctx));
 const struct pipe_blend_state blend={.rt={{.colormask=PIPE_MASK_RGBA}}};
 const struct pipe_depth_stencil_alpha_state less={.depth_enabled=true,.depth_writemask=true,.depth_func=PIPE_FUNC_LESS};
 void *bs=ctx->create_blend_state(ctx,&blend),*ds=ctx->create_depth_stencil_alpha_state(ctx,&less);CHECK(bs&&ds);
 ctx->bind_blend_state(ctx,bs);ctx->bind_depth_stencil_alpha_state(ctx,ds);
 const float raw[3][8]={{0,0,0,2,0,0,1,0},{.25f,0,0,2,0,0,1,0},{0,.25f,0,2,0,0,1,0}};
 const struct pipe_resource vb_template={.target=PIPE_BUFFER,.format=PIPE_FORMAT_R8_UNORM,.bind=PIPE_BIND_VERTEX_BUFFER,
  .width0=sizeof raw,.height0=1,.depth0=1,.array_size=1};
 struct pipe_resource *vb=screen->resource_create(screen,&vb_template);CHECK(vb);
 ctx->buffer_subdata(ctx,vb,0,0,sizeof raw,raw);CHECK(!remesa_context_error(ctx));
 struct pipe_vertex_buffer binding={.buffer.resource=vb};vb->reference.count++;ctx->set_vertex_buffers(ctx,1,&binding);
 struct pipe_vertex_element elements[2]={0};for(unsigned i=0;i<2;i++){
  elements[i].src_format=PIPE_FORMAT_R32G32B32A32_FLOAT;elements[i].src_stride=32;elements[i].src_offset=i*16;
 }
 void *el=ctx->create_vertex_elements_state(ctx,2,elements);CHECK(el);ctx->bind_vertex_elements_state(ctx,el);
 struct pipe_draw_info info={.mode=MESA_PRIM_TRIANGLES,.instance_count=1};struct pipe_draw_start_count_bias draw={.count=3};
 struct remesa_linked_scene_request request={.vertex_bank=4,.pixel_bank=3,.state_bank=4,.clear_depth=1};
 struct remesa_scene_capture capture={0};CHECK(!remesa_context_prepare_gears_homogeneous_scene(ctx,&info,&draw,&request,&capture));
 struct remesa_persistent_scene_parameters windows;CHECK(!remesa_persistent_scene_parameters_snapshot(&d,capture,&windows));
 const struct remesa_parameter_window *window=&windows.windows[0].window;
 unsigned char original[4096];CHECK(window->allocation_bytes==sizeof original);memcpy(original,get(&f,window->id)->bytes,sizeof original);
 struct remesa_scene_packet before,after;CHECK(!remesa_persistent_scene_packet_snapshot(&d,capture,&before));
 CHECK(remesa_persistent_set_native_scene_parameters(&d,parameter_write)==EBUSY);
 CHECK(remesa_context_update_captured_parameters(ctx,capture,1)==(mode==8?EOPNOTSUPP:EAGAIN)&&!data_calls&&!gpu_calls);
 struct pipe_fence_handle *fence=NULL;CHECK(!remesa_context_submit_captured_scene(ctx,capture,&fence));
 CHECK(fence&&screen->fence_finish(screen,ctx,fence,0)&&gpu_calls==1);
 unsigned creates=f.creates,reads=f.reads,writes=f.writes,maps=f.cpu_maps,unpins=f.unpins;
 coefficients[20]=0x3f000000;ctx->set_constant_buffer(ctx,PIPE_SHADER_VERTEX,0,false,&cb);
 coefficients[20]=0x40400000; /* Bind owns a snapshot of 0.5, not this 3.0. */
 if(mode==1){
  struct pipe_box corner={.width=1,.height=1,.depth=1};struct pipe_transfer *transfer=NULL;
  CHECK(ctx->texture_map(ctx,color,0,PIPE_MAP_READ|PIPE_MAP_DONTBLOCK,&corner,&transfer)&&transfer);
  CHECK(remesa_context_update_captured_parameters(ctx,capture,1)==EBUSY&&!data_calls);
  ctx->texture_unmap(ctx,transfer);reads=f.reads;maps=f.cpu_maps;
 }
 if(mode==2||mode==3||mode==9)data_fault=mode==2?2:mode==3?3:4;
 if(mode==4){
  data_fault=1;CHECK(remesa_context_update_captured_parameters(ctx,capture,1)==ENOMEM&&data_calls==1);
  CHECK(!memcmp(original,get(&f,window->id)->bytes,sizeof original)&&!data_sequence);data_fault=0;
 }
 if(mode==5){CHECK(remesa_context_update_captured_parameters(ctx,capture,2)==EINVAL&&!data_calls);}
 if(mode==6){
  struct pipe_constant_buffer empty={0};ctx->set_constant_buffer(ctx,PIPE_SHADER_VERTEX,0,false,&empty);
  CHECK(remesa_context_update_captured_parameters(ctx,capture,1)==ENODATA&&!data_calls);
  coefficients[20]=0x3f000000;ctx->set_constant_buffer(ctx,PIPE_SHADER_VERTEX,0,false,&cb);
 }
 if(mode==7){
  struct remesa_scene_capture stale=capture;stale.serial++;
  CHECK(remesa_context_update_captured_parameters(ctx,stale,1)==ESTALE&&!data_calls);
 }
 int e=remesa_context_update_captured_parameters(ctx,capture,1);
 if(mode==2||mode==3||mode==9){
  CHECK(e==EOWNERDEAD&&data_calls==1&&gpu_calls==1&&remesa_context_error(ctx)==EOWNERDEAD);
  CHECK(remesa_context_update_captured_parameters(ctx,capture,1)==EOWNERDEAD&&data_calls==1);
  struct pipe_fence_handle *next=NULL;CHECK(remesa_context_submit_captured_scene(ctx,capture,&next)==EOWNERDEAD&&!next&&gpu_calls==1);
  CHECK(remesa_persistent_scene_capture_release(&d,capture)==EOWNERDEAD&&f.unpins==unpins);
  CHECK(remesa_persistent_close(&d)==EBUSY);puts("PASS uncertain data update retains whole capture and poisons context/targets; modeled GPU0");return 0;
 }
 CHECK(e==(mode==8?EOPNOTSUPP:0)&&gpu_calls==1);
 if(mode!=8){
  CHECK(data_sequence==1);
  const unsigned char *actual=get(&f,window->id)->bytes;
  for(unsigned i=0;i<sizeof original;i++)if(i<4||i>=148)CHECK(actual[i]==original[i]);
  CHECK(shader_pds_le32(actual+4+20*4)==0x3f000000);
  CHECK(!remesa_persistent_scene_packet_snapshot(&d,capture,&after)&&!memcmp(&before,&after,sizeof before));
  CHECK(f.creates==creates&&f.reads==reads&&f.writes==writes&&f.cpu_maps==maps&&f.unpins==unpins);
  /* Holding the already published fence is safe; update creates no fence. */
  CHECK(!remesa_context_update_captured_parameters(ctx,capture,1)&&data_sequence==2&&gpu_calls==1);
 }
 screen->fence_reference(screen,&fence,NULL);
 CHECK(!remesa_context_submit_captured_scene(ctx,capture,&fence)&&gpu_calls==2);
 CHECK(screen->fence_finish(screen,ctx,fence,0));screen->fence_reference(screen,&fence,NULL);
 CHECK(!remesa_persistent_scene_capture_release(&d,capture));
 ctx->delete_vs_state(ctx,vs);ctx->delete_fs_state(ctx,fs);ctx->delete_vertex_elements_state(ctx,el);
 ctx->delete_blend_state(ctx,bs);ctx->delete_depth_stencil_alpha_state(ctx,ds);ctx->destroy(ctx);
 remesa_resource_reference(&color,NULL);remesa_resource_reference(&depth,NULL);remesa_resource_reference(&vb,NULL);screen->destroy(screen);
 CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!remesa_persistent_close(&d));glsl_type_singleton_decref();
 puts("PASS captured Gallium constants -> existing PARAM encoder -> data ACK -> next distinct submit; backing/packet reused, no map/readback/allocation during update; modeled GPU0");return 0;
}
