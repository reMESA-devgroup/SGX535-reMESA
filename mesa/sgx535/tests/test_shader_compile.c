/* SPDX-License-Identifier: MIT
 * Durable compiler diagnostics and owned Gallium VS/FS host preparation.
 * Recording backend only: no GPU instructions, execution or pixels asserted.
 */
#include "nir_fixture.h"
#include "vertex_nir_fixture.h"
int inherited_shader_compile_fixture(void);
#define main inherited_shader_compile_fixture
#include "test_driver.c"
#undef main

static unsigned checks;
#define CHECK(x) do {assert(x);++checks;} while(0)
static const struct remesa_shader_host_pipeline_request *backend_request;
static unsigned host_prepares;
static int host_preparation_error;
static int prepare_host_shaders(void *priv,const struct remesa_shader_draw_programs *programs,void **out)
{
 struct remesa_shader_host_pipeline *pipeline=NULL;++host_prepares;
 host_preparation_error=remesa_shader_host_pipeline_create(programs,backend_request,&pipeline);
 if(host_preparation_error)return host_preparation_error;
 struct remesa_shader_host_pipeline_view view;
 CHECK(!remesa_shader_host_pipeline_inspect(pipeline,&view)&&view.pds_code_error==ENOTSUP&&
       view.execution_error==ENOTSUP&&view.data[0].storage.byte_length==48&&view.data[1].storage.byte_length==48);
 CHECK(!remesa_shader_host_pipeline_revalidate(pipeline,programs,backend_request));
 struct remesa_pds_host_transfer routes[2][12];struct remesa_pds_host_transfer_request requests[2];
 for(unsigned stage=0;stage<2;stage++) {
  for(unsigned cell=0;cell<12;cell++)routes[stage][cell]=(struct remesa_pds_host_transfer){cell,cell};
  requests[stage]=(struct remesa_pds_host_transfer_request){routes[stage],12};
 }
 struct remesa_shader_execution_contract contract;
 CHECK(!remesa_shader_host_pipeline_prepare_contract(pipeline,requests,&contract));
 CHECK(!remesa_shader_host_pipeline_validate_contract(pipeline,requests,&contract));
 CHECK(contract.vertex.route_count==4&&contract.task[1].slot_count==12&&contract.execution_error==ENOTSUP);
 remesa_shader_host_pipeline_destroy(pipeline);
 /* Exercise the explicit profile during real draw_vbo preparation, while
  * retaining the generic recipe failure and bound-state regressions above.
  * metadata6=0 is an explicit CPU test input, not a shader-derived rule. */
 pipeline=NULL;
 struct remesa_shader_host_pipeline_request pixel=*backend_request;
 pixel.data_byte_offsets[1]=0;pixel.data[1]=(struct remesa_pds_host_data_recipe){0};
 CHECK(!remesa_shader_host_pipeline_create_pixel_task(programs,&pixel,0,&pipeline));
 struct remesa_pds_cpu_pixel_task_output output;
 CHECK(!remesa_shader_host_pipeline_prepare_pixel_task_output(pipeline,requests,&output));
 CHECK(!remesa_shader_host_pipeline_validate_pixel_task_output(pipeline,requests,&output));
 CHECK(output.cpu_word_count==3&&output.dispatch_error==ENOTSUP&&output.termination_error==ENOTSUP);
 remesa_shader_host_pipeline_destroy(pipeline);
 return prepare_shaders(priv,programs,out);
}

static struct remesa_shader_host_pipeline_request host_request(struct remesa_pds_data_cell cells[2][12])
{
 struct remesa_shader_host_pipeline_request request={.banks={1,2},.data_byte_offsets={4,8}};
 for(unsigned i=0;i<4;i++) {
  request.vertex_mapping.input[i]=(struct remesa_usse_cpu_register){0,8+i};
  request.vertex_mapping.output[i]=(struct remesa_usse_cpu_register){2,24+i};
 }
 for(unsigned stage=0;stage<2;stage++) {
  request.use[stage]=(struct remesa_pds_binding){.allocation_cookie=1,.owner_cookie=7,
   .mapping_generation=3,.address=UINT64_C(0x20000000),.offset=stage?32:0,
   .bytes=stage?8:32,.allocation_bytes=4096};
  request.pds[stage]=(struct remesa_pds_block_request){.operation=9,.generation=4,
   .allocation={.allocation_cookie=2+stage,.owner_cookie=7,.mapping_generation=5,
    .address=UINT64_C(0x21000000)+stage*UINT64_C(0x01000000),.bytes=256,.allocation_bytes=4096},
   .data_bytes=48,.code_bytes=4,.code_alignment=16};
  for(unsigned i=0;i<12;i++)cells[stage][i]=(struct remesa_pds_data_cell){
   .kind=REMESA_PDS_DATA_LITERAL32,.value=UINT32_C(0x43210000)+stage*256+i};
  cells[stage][request.data_byte_offsets[stage]/4]=(struct remesa_pds_data_cell){
   .kind=REMESA_PDS_DATA_USE_ENTRY_LOWER19,.value=UINT32_C(0xa0000000)};
  request.data[stage]=(struct remesa_pds_host_data_recipe){.cells=cells[stage],.cell_count=12};
 }
 return request;
}
int main(void)
{
 struct recorder rec={0};struct remesa_device_ops selected=ops;
 selected.prepare_shader_programs=prepare_host_shaders;
 struct remesa_device device={.ops=&selected,.priv=&rec,.offline_test_backend=true};
 struct pipe_screen *screen=remesa_screen_create(&device);CHECK(screen);
 struct pipe_context *ctx=screen->context_create(screen,NULL,0);CHECK(ctx);
 struct remesa_shader_compile_status status,before;
 memset(&status,0xa5,sizeof(status));before=status;
 CHECK(remesa_context_shader_compile_status(ctx,PIPE_SHADER_VERTEX,&status)==ENODATA&&
       !memcmp(&status,&before,sizeof(status)));
 CHECK(remesa_context_shader_compile_status(ctx,PIPE_SHADER_FRAGMENT,&status)==ENODATA);
 CHECK(remesa_context_shader_compile_status(ctx,PIPE_SHADER_GEOMETRY,&status)==ENOTSUP);
 CHECK(remesa_context_shader_compile_status(NULL,PIPE_SHADER_VERTEX,&status)==EINVAL);
 CHECK(remesa_context_shader_compile_status(ctx,PIPE_SHADER_VERTEX,NULL)==EINVAL);
 CHECK(remesa_context_shader_compile_status(ctx,PIPE_SHADER_VERTEX,(void *)ctx)==EINVAL);
 CHECK(remesa_context_shader_compile_status(ctx,PIPE_SHADER_VERTEX,(void *)screen)==EINVAL);
 CHECK(remesa_context_shader_compile_status(ctx,PIPE_SHADER_VERTEX,(void *)(UINTPTR_MAX-3))==EINVAL);
 struct remesa_pds_data_cell cells[2][12];
 struct remesa_shader_host_pipeline_request request=host_request(cells);
 backend_request=&request;
 struct remesa_shader_host_pipeline *pipeline=NULL;
 CHECK(remesa_context_prepare_host_shader_pipeline(ctx,&request,&pipeline)==ENODATA&&!pipeline);
 CHECK(remesa_context_prepare_host_shader_pipeline(ctx,&request,(void *)ctx)==EINVAL);
 CHECK(remesa_context_prepare_host_shader_pipeline(ctx,&request,(void *)screen)==EINVAL);
 CHECK(remesa_context_prepare_host_shader_pipeline(ctx,&request,(void *)(UINTPTR_MAX-3))==EINVAL);
 struct pipe_shader_state shader={.type=PIPE_SHADER_IR_NIR,.ir.nir=vertex_fixture()};
 void *vs=ctx->create_vs_state(ctx,&shader);CHECK(vs&&!remesa_context_error(ctx));
 CHECK(!remesa_context_shader_compile_status(ctx,PIPE_SHADER_VERTEX,&status)&&status.attempts==1&&
       !status.error&&status.binary_error==ENOTSUP&&!status.binary_bytes&&status.launch_error==ENOTSUP);
 ctx->bind_vs_state(ctx,vs);
 shader.ir.nir=fixture(UINT32_C(0xff00ff00));void *fs=ctx->create_fs_state(ctx,&shader);
 CHECK(fs&&!remesa_context_error(ctx));ctx->bind_fs_state(ctx,fs);
 CHECK(remesa_context_shader_compile_status(ctx,PIPE_SHADER_VERTEX,(void *)vs)==EINVAL);
 CHECK(remesa_context_shader_compile_status(ctx,PIPE_SHADER_FRAGMENT,(void *)fs)==EINVAL);
 CHECK(remesa_context_prepare_host_shader_pipeline(ctx,&request,(void *)vs)==EINVAL);
 CHECK(remesa_context_prepare_host_shader_pipeline(ctx,&request,(void *)fs)==EINVAL);
 CHECK(!remesa_context_shader_compile_status(ctx,PIPE_SHADER_FRAGMENT,&status)&&status.attempts==1&&
       !status.error&&!status.binary_error&&status.binary_bytes==8&&status.launch_error==ENOTSUP&&
       !status.uniform_specialization);
 CHECK(!remesa_context_prepare_host_shader_pipeline(ctx,&request,&pipeline)&&pipeline);
 CHECK(!remesa_context_revalidate_host_shader_pipeline(ctx,pipeline,&request));
 struct remesa_shader_host_pipeline_view view;
 CHECK(!remesa_shader_host_pipeline_inspect(pipeline,&view)&&view.execution_error==ENOTSUP&&
       view.pds_code_error==ENOTSUP&&view.image.fragment.byte_length==8&&view.image.vertex.byte_length==32);
 CHECK(!rec.allocs&&!rec.shader_prepares&&!rec.shader_submits&&!rec.draws);
 struct remesa_shader_pipeline bound_pair;CHECK(!remesa_context_shader_pipeline_report(ctx,&bound_pair));
 /* Both failed creates consume their NIR and retain the previously bound pair.
  * Only the stage's latest compile record changes; no implicit bind occurs. */
 shader.ir.nir=fixture(UINT32_C(0xffff0000));CHECK(!ctx->create_fs_state(ctx,&shader));
 CHECK(remesa_context_error(ctx)==ENOTSUP);
 shader.ir.nir=vertex_fixture();shader.stream_output.num_outputs=1;
 CHECK(!ctx->create_vs_state(ctx,&shader)&&remesa_context_error(ctx)==ENOTSUP);
 shader.stream_output.num_outputs=0;
 CHECK(!remesa_context_shader_compile_status(ctx,PIPE_SHADER_FRAGMENT,&status)&&status.attempts==2&&
       status.error==ENOTSUP&&status.binary_error==ENOTSUP&&!status.binary_bytes);
 CHECK(!remesa_context_shader_compile_status(ctx,PIPE_SHADER_VERTEX,&status)&&status.attempts==2&&status.error==ENOTSUP);
 CHECK(remesa_context_error(ctx)==ENOTSUP); /* Getter never clears the error. */
 struct remesa_shader_pipeline current;
 CHECK(!remesa_context_shader_pipeline_report(ctx,&current)&&!memcmp(&current,&bound_pair,sizeof(current)));
 CHECK(!remesa_context_revalidate_host_shader_pipeline(ctx,pipeline,&request));
 struct pipe_viewport_state viewport={.scale={16,16,.5},.translate={16,16,.5}};
 ctx->set_viewport_states(ctx,0,1,&viewport);CHECK(!remesa_context_error(ctx));
 CHECK(!remesa_context_shader_compile_status(ctx,PIPE_SHADER_FRAGMENT,&status)&&status.error==ENOTSUP);
 struct pipe_resource *vb=buffer(screen,48,PIPE_BIND_VERTEX_BUFFER);CHECK(vb);
 struct pipe_resource rt={.target=PIPE_TEXTURE_2D,.format=PIPE_FORMAT_B8G8R8A8_UNORM,
  .width0=32,.height0=32,.depth0=1,.array_size=1,.bind=PIPE_BIND_RENDER_TARGET};
 struct pipe_resource *target=screen->resource_create(screen,&rt);CHECK(target);
 const float positions[3][4]={{-.5,-.5,0,1},{.5,-.5,0,1},{-.5,.5,0,1}};
 struct pipe_box box={.width=sizeof(positions),.height=1,.depth=1};struct pipe_transfer *transfer=NULL;
 void *data=ctx->buffer_map(ctx,vb,0,PIPE_MAP_WRITE,&box,&transfer);CHECK(data&&transfer);
 memcpy(data,positions,sizeof(positions));ctx->buffer_unmap(ctx,transfer);
 struct pipe_framebuffer_state fb={.width=32,.height=32,.nr_cbufs=1};
 fb.cbufs[0].texture=target;fb.cbufs[0].format=target->format;ctx->set_framebuffer_state(ctx,&fb);
 struct pipe_vertex_buffer binding={.buffer.resource=vb};++vb->reference.count;ctx->set_vertex_buffers(ctx,1,&binding);
 struct pipe_vertex_element element={.src_format=PIPE_FORMAT_R32G32B32A32_FLOAT,.src_stride=16};
 void *ve=ctx->create_vertex_elements_state(ctx,1,&element);CHECK(ve);ctx->bind_vertex_elements_state(ctx,ve);
 struct pipe_blend_state blend={0};blend.rt[0].colormask=PIPE_MASK_RGBA;
 struct pipe_depth_stencil_alpha_state depth={0};
 struct pipe_rasterizer_state raster={.fill_front=PIPE_POLYGON_MODE_FILL,.fill_back=PIPE_POLYGON_MODE_FILL};
 void *bs=ctx->create_blend_state(ctx,&blend),*ds=ctx->create_depth_stencil_alpha_state(ctx,&depth),
      *rs=ctx->create_rasterizer_state(ctx,&raster);CHECK(bs&&ds&&rs);
 ctx->bind_blend_state(ctx,bs);ctx->bind_depth_stencil_alpha_state(ctx,ds);ctx->bind_rasterizer_state(ctx,rs);
 struct pipe_draw_info info={.mode=MESA_PRIM_TRIANGLES,.instance_count=1};
 struct pipe_draw_start_count_bias draw={.count=3};
 /* A bad FS DATA recipe propagates through draw_vbo before BO maps or submit;
  * restoring it permits the same owned VS/FS pair and framebuffer to proceed. */
 unsigned maps=rec.maps;
 uint32_t saved_kind=cells[1][0].kind;cells[1][0].kind=UINT32_MAX;
 ctx->draw_vbo(ctx,&info,0,NULL,&draw,1);
 CHECK(host_prepares==1&&host_preparation_error&&remesa_context_error(ctx)==host_preparation_error&&
       !rec.shader_prepares&&!rec.shader_submits&&!rec.draws&&rec.maps==maps);
 CHECK(!remesa_context_shader_pipeline_report(ctx,&current)&&!memcmp(&current,&bound_pair,sizeof(current)));
 cells[1][0].kind=saved_kind;
 ctx->draw_vbo(ctx,&info,0,NULL,&draw,1);
 CHECK(host_prepares==2&&!host_preparation_error);
 CHECK(!remesa_context_error(ctx)&&rec.shader_prepares==1&&rec.shader_submits==1&&rec.draws==1);
 CHECK(!memcmp(&rec.last->programs->snapshot.pipeline,&bound_pair,sizeof(bound_pair)));
 CHECK(rec.packet.argb==UINT32_C(0xff00ff00)&&rec.maps==rec.unmaps);
 CHECK(!remesa_context_shader_compile_status(ctx,PIPE_SHADER_VERTEX,&status)&&status.error==ENOTSUP);
 CHECK(!remesa_context_shader_compile_status(ctx,PIPE_SHADER_FRAGMENT,&status)&&status.error==ENOTSUP);
 struct pipe_fence_handle *f=NULL;ctx->flush(ctx,&f,0);CHECK(f);
 /* A later successful create updates the durable status, without rebinding.
  * Revalidation detects a real pair change only after an explicit bind. */
 shader.ir.nir=fixture(UINT32_C(0xffff00ff));void *other=ctx->create_fs_state(ctx,&shader);CHECK(other);
 CHECK(!remesa_context_shader_compile_status(ctx,PIPE_SHADER_FRAGMENT,&status)&&status.attempts==3&&
       !status.error&&!status.binary_error&&status.binary_bytes==8);
 CHECK(!remesa_context_revalidate_host_shader_pipeline(ctx,pipeline,&request));
 ctx->bind_fs_state(ctx,other);
 CHECK(remesa_context_revalidate_host_shader_pipeline(ctx,pipeline,&request)==ESTALE);
 ctx->bind_fs_state(ctx,NULL);
 CHECK(remesa_context_revalidate_host_shader_pipeline(ctx,pipeline,&request)==ENODATA);
 CHECK(remesa_context_prepare_host_shader_pipeline(ctx,&request,NULL)==EINVAL);
 CHECK(remesa_context_revalidate_host_shader_pipeline(NULL,pipeline,&request)==EINVAL);
 ctx->delete_vs_state(ctx,vs);ctx->delete_fs_state(ctx,fs);ctx->delete_fs_state(ctx,other);
 ctx->delete_vertex_elements_state(ctx,ve);ctx->delete_blend_state(ctx,bs);
 ctx->delete_depth_stencil_alpha_state(ctx,ds);ctx->delete_rasterizer_state(ctx,rs);
 ctx->destroy(ctx);
 CHECK(!remesa_shader_host_pipeline_validate(pipeline));
 CHECK(!remesa_shader_host_pipeline_inspect(pipeline,&view)&&view.links->launch.semantic.fragment_argb==UINT32_C(0xff00ff00));
 memset(cells,0,sizeof(cells));memset(&request,0,sizeof(request));CHECK(!remesa_shader_host_pipeline_validate(pipeline));
 remesa_shader_host_pipeline_destroy(pipeline);
 rec.last->retired=true;rec.last->target->published=true;
 CHECK(screen->fence_finish(screen,NULL,f,0));screen->fence_reference(screen,&f,NULL);
 resource_drop(vb);resource_drop(target);CHECK(rec.allocs==rec.frees&&rec.shader_allocs==rec.shader_frees);
 screen->destroy(screen);
 printf("PASS %u Gallium compiler diagnostics and bound host-pipeline checks; recording only, zero GPU submissions\n",checks);
 return 0;
}
