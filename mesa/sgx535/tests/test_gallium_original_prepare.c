/* SPDX-License-Identifier: MIT
 * Actual Mesa/Gallium callbacks through original compiler preparation.
 * CPU-only backend; this is not an OpenGL context or rendered pixels.
 * Reuse the existing allocator, ownership and NIR fixtures; no fake rasterizer.
 */
int inherited_gallium_driver_fixture(void);
#define main inherited_gallium_driver_fixture
#include "test_driver_nir.c"
#undef main
#undef remesa_bind_diagnostic_pipeline
#include "remesa_pds_structure.h"
#include "remesa_linux.h"
#include "remesa/shader_request.h"

struct smoke {
 struct recorder base;
 unsigned prepare_attempts,prepared,destroyed,submit_attempts;
 int requested_error;
 size_t vertex_bytes,fragment_bytes,pds_bytes;
};
static int prepare_original(void *priv,const struct remesa_shader_draw_programs *programs,void **out)
{
 struct smoke *state=priv;state->prepare_attempts++;
 assert(out&&!*out&&!remesa_shader_draw_programs_validate(programs));
 struct remesa_pds_data_cell cells[2][12];
 struct remesa_shader_host_pipeline_request request={.banks={1,2},.data_byte_offsets={4,0}};
 for(unsigned c=0;c<4;c++) {
  request.vertex_mapping.input[c]=(struct remesa_usse_cpu_register){2,c};
  request.vertex_mapping.output[c]=(struct remesa_usse_cpu_register){1,c};
 }
 for(unsigned stage=0;stage<2;stage++) {
  request.use[stage]=(struct remesa_pds_binding){.allocation_cookie=1,.owner_cookie=7,
   .mapping_generation=3,.address=UINT64_C(0x20000000),.offset=stage?32:0,
   .bytes=stage?8:16,.allocation_bytes=4096};
  request.pds[stage]=(struct remesa_pds_block_request){.operation=9,.generation=4,
   .allocation={.allocation_cookie=2+stage,.owner_cookie=7,.mapping_generation=5,
    .address=UINT64_C(0x21000000)+stage*UINT64_C(0x01000000),.bytes=256,.allocation_bytes=4096},
   .data_bytes=48,.code_bytes=stage?8:4,.code_alignment=16};
  for(unsigned c=0;c<12;c++)cells[stage][c]=(struct remesa_pds_data_cell){
   REMESA_PDS_DATA_LITERAL32,UINT32_C(0x12340000)+stage*256+c};
  cells[stage][request.data_byte_offsets[stage]/4]=(struct remesa_pds_data_cell){
   REMESA_PDS_DATA_USE_ENTRY_LOWER19,UINT32_C(0xa0000000)};
  request.data[stage]=(struct remesa_pds_host_data_recipe){cells[stage],12};
 }
 struct remesa_pds_structure_request source={.code_policy=REMESA_PDS_STRUCTURE_CODE_CPU_COMPLETE};
 struct remesa_shader_host_pipeline *pipeline=NULL;
 int error=remesa_shader_host_pipeline_create_pds_structure(programs,&request,&source,&pipeline);
 assert(!error&&pipeline);
 struct remesa_shader_host_pipeline_view view;
 assert(!remesa_shader_host_pipeline_inspect(pipeline,&view));
 assert(view.structure_error==0&&view.pds_code_error==ENOTSUP&&view.execution_error==ENOTSUP);
 assert(view.image.vertex.byte_length==16&&view.image.fragment.byte_length==8);
 assert(view.structure_code.byte_length==8&&view.data[1].storage.byte_length==48);
 for(size_t i=0;i<view.structure_code_mask.byte_length;i++)
  assert(((const unsigned char *)view.structure_code_mask.bytes)[i]==255);
 /* Relocated host DATA is still not qualified physical read/dispatch state. */
 for(unsigned i=0;i<4;i++)assert(!((const unsigned char *)view.structure_data_mask.bytes)[i]);
 assert(!remesa_shader_host_pipeline_revalidate_pds_structure(pipeline,programs,&request,&source));
 /* Production staging now carries the generated combined image, not only
  * standalone body records. No ioctl, BO upload or execution is performed. */
 assert(view.image_owner&&view.image.fragment_pds_error==0);
 assert(view.image.fragment_pds.byte_length==56&&view.image.fragment_pds_mask.byte_length==56);
 assert(!memcmp(view.image.fragment_pds.bytes,view.data[1].storage.bytes,48));
 assert(!memcmp((const unsigned char *)view.image.fragment_pds.bytes+48,view.structure_code.bytes,8));
 assert(!memcmp(view.image.fragment_pds_mask.bytes,view.structure_data_mask.bytes,48));
 struct remesa_shader_request staged,saved;
 assert(!remesa_linux_shader_request_build(view.image_owner,7,11,&staged));
 assert(!remesa_shader_image_validate_request(view.image_owner,&staged));
 assert(remesa_shader_request_validate(&staged,7,11)==REMESA_SHADER_REQUEST_OK);
 const struct remesa_shader_resource *pds=&staged.resources[REMESA_SHADER_FRAGMENT_PDS-1];
 assert(pds->length==56&&pds->state==REMESA_SHADER_CPU_CANDIDATE&&pds->alignment==16);
 assert(staged.resource_mask==((1u<<(REMESA_SHADER_VERTEX_USSE-1))|
  (1u<<(REMESA_SHADER_FRAGMENT_USSE-1))|(1u<<(REMESA_SHADER_FRAGMENT_PDS-1))));
 assert(!memcmp(staged.data+pds->offset,view.image.fragment_pds.bytes,56));
 saved=staged;staged.data[pds->offset+48]^=1;
 assert(remesa_shader_image_validate_request(view.image_owner,&staged)==EPROTO);staged=saved;
 staged.resources[REMESA_SHADER_FRAGMENT_PDS-1].length=52;
 assert(remesa_shader_image_validate_request(view.image_owner,&staged)==EPROTO);staged=saved;
 staged.owner=8;assert(remesa_shader_request_validate(&staged,7,11)==REMESA_SHADER_REQUEST_INVALID);staged=saved;
 staged.action=REMESA_SHADER_EXECUTE;
 assert(remesa_shader_request_validate(&staged,7,11)==REMESA_SHADER_REQUEST_UNSUPPORTED);
 assert(remesa_shader_image_validate_execution(view.image_owner)==ENOTSUP);
 memset(&staged,0xa5,sizeof(staged));saved=staged;
 assert(remesa_linux_shader_request_build(view.image_owner,0,11,&staged)==EINVAL);
 assert(!memcmp(&staged,&saved,sizeof(staged)));
 unsigned char *evidence=(unsigned char *)(uintptr_t)view.image.fragment_pds_mask.bytes;
 evidence[48]^=1;
 assert(remesa_linux_shader_request_build(view.image_owner,7,11,&staged)==EPROTO);
 assert(!memcmp(&staged,&saved,sizeof(staged)));evidence[48]^=1;
 unsigned char *program_bytes=(unsigned char *)(uintptr_t)view.image.fragment_pds.bytes;
 program_bytes[48]^=1;
 assert(remesa_linux_shader_request_build(view.image_owner,7,11,&staged)==EPROTO);
 assert(!memcmp(&staged,&saved,sizeof(staged)));program_bytes[48]^=1;
 assert(!remesa_shader_host_pipeline_validate(pipeline));
 /* Sparse/default profile keeps its old body-only staging. Texture structural
  * analysis remains supported, but unresolved texture addresses reject staging. */
 struct remesa_shader_host_pipeline *other=NULL;
 struct remesa_pds_structure_request legacy={0};
 assert(!remesa_shader_host_pipeline_create_pds_structure(programs,&request,&legacy,&other));
 struct remesa_shader_host_pipeline_view other_view;
 assert(!remesa_shader_host_pipeline_inspect(other,&other_view));
 assert(other_view.image.fragment_pds_error==ENODATA&&!other_view.image.fragment_pds.byte_length);
 assert(!remesa_linux_shader_request_build(other_view.image_owner,7,11,&staged));
 assert(staged.resource_mask==3);remesa_shader_host_pipeline_destroy(other);other=NULL;
 struct remesa_pds_structure_request textured=source;
 textured.entry_count=1;textured.entries[0].kind=REMESA_PDS_STRUCTURE_T;textured.entries[0].id=1;
 request.pds[1].code_bytes=16;
 assert(!remesa_shader_host_pipeline_create_pds_structure(programs,&request,&textured,&other));
 assert(!remesa_shader_host_pipeline_inspect(other,&other_view));
 assert(other_view.image.fragment_pds_error==ENOTSUP&&!other_view.image.fragment_pds.byte_length);
 memset(&staged,0xa5,sizeof(staged));saved=staged;
 assert(remesa_linux_shader_request_build(other_view.image_owner,7,11,&staged)==ENOTSUP);
 assert(!memcmp(&staged,&saved,sizeof(staged)));remesa_shader_host_pipeline_destroy(other);other=NULL;
 /* A valid64-bit symbolic block may align CODE at2^32. It must never narrow
  * tosize_t0 then silently clamp4 on i386. The constructor stays transactional. */
 struct remesa_shader_host_pipeline_request oversized=request;
 oversized.pds[1].code_bytes=8;
 oversized.pds[1].allocation.address=UINT64_C(0x100000000)-48u;
 oversized.pds[1].code_alignment=UINT64_C(0x100000000);
 assert(remesa_shader_host_pipeline_create_pds_structure(programs,&oversized,&source,&other)==
  (sizeof(size_t)==4?EOVERFLOW:E2BIG));
 assert(!other);
 state->vertex_bytes=view.image.vertex.byte_length;state->fragment_bytes=view.image.fragment.byte_length;
 state->pds_bytes=view.structure_code.byte_length;state->prepared++;
 *out=pipeline;return state->requested_error;
}
static void destroy_original(void *priv,void *token)
{
 struct smoke *state=priv;assert(token);
 assert(!remesa_shader_host_pipeline_validate(token));
 remesa_shader_host_pipeline_destroy(token);state->destroyed++;
}
static int forbidden_submit(void *priv,void *target,const struct remesa_draw_packet *packet,void **fence)
{
 struct smoke *state=priv;(void)target;(void)packet;(void)fence;
 state->submit_attempts++;return ENOTSUP;
}
static int forbidden_shader_submit(void *priv,void *target,const struct remesa_draw_packet *packet,
 const struct remesa_shader_draw_inputs *inputs,void *token,void **fence)
{
 (void)inputs;(void)token;return forbidden_submit(priv,target,packet,fence);
}
static void read_target(struct pipe_context *ctx,struct pipe_resource *target,unsigned char bytes[4096])
{
 struct pipe_box box={.width=32,.height=32,.depth=1};struct pipe_transfer *transfer=NULL;
 const void *p=ctx->texture_map(ctx,target,0,PIPE_MAP_READ,&box,&transfer);
 assert(p&&transfer&&transfer->stride==128);memcpy(bytes,p,4096);ctx->texture_unmap(ctx,transfer);
}
static void exercise(uint32_t color,int error)
{
 struct smoke state={.requested_error=error};struct remesa_device_ops backend=ops;
 backend.prepare_shader_programs=prepare_original;backend.destroy_shader_programs=destroy_original;
 backend.submit=forbidden_submit;backend.submit_shader_programs=forbidden_shader_submit;
 struct remesa_device device={.ops=&backend,.priv=&state,.offline_test_backend=true,
  .framebuffer_mode=REMESA_FRAMEBUFFER_PRESERVE};
 struct pipe_screen *screen=remesa_screen_create(&device);assert(screen);
 struct pipe_context *ctx=screen->context_create(screen,NULL,0);assert(ctx);
 assert(strstr(screen->get_name(screen),"NO SGX"));
 const float vertices[3][4]={{-.5f,-.5f,0,1},{.5f,-.5f,0,1},{-.5f,.5f,0,1}};
 struct pipe_resource *vb=buffer(screen,sizeof(vertices),PIPE_BIND_VERTEX_BUFFER);assert(vb);
 struct pipe_box box={.width=sizeof(vertices),.height=1,.depth=1};struct pipe_transfer *transfer=NULL;
 void *p=ctx->buffer_map(ctx,vb,0,PIPE_MAP_WRITE,&box,&transfer);assert(p&&transfer);
 memcpy(p,vertices,sizeof(vertices));ctx->buffer_unmap(ctx,transfer);
 struct pipe_resource rt={.target=PIPE_TEXTURE_2D,.format=PIPE_FORMAT_B8G8R8A8_UNORM,
  .width0=32,.height0=32,.depth0=1,.array_size=1,.bind=PIPE_BIND_RENDER_TARGET};
 struct pipe_resource *target=screen->resource_create(screen,&rt);assert(target);
 struct pipe_framebuffer_state fb={.width=32,.height=32,.nr_cbufs=1};
 fb.cbufs[0].texture=target;fb.cbufs[0].format=target->format;ctx->set_framebuffer_state(ctx,&fb);
 struct pipe_vertex_buffer vertex={0};++vb->reference.count;vertex.buffer.resource=vb;ctx->set_vertex_buffers(ctx,1,&vertex);
 struct pipe_vertex_element element={.src_format=PIPE_FORMAT_R32G32B32A32_FLOAT,.src_stride=16};
 void *ve=ctx->create_vertex_elements_state(ctx,1,&element);assert(ve);ctx->bind_vertex_elements_state(ctx,ve);
 assert(!bind_nir(ctx,color));assert(!remesa_context_set_framebuffer_load(ctx,true));
 union pipe_color_union clear_color={.f={.25f,.5f,.75f,1}};
 ctx->clear(ctx,PIPE_CLEAR_COLOR0,NULL,&clear_color,0,0);assert(!remesa_context_error(ctx));
 assert(remesa_context_cpu_clear_count(ctx)==1);
 unsigned char before[4096],after[4096];read_target(ctx,target,before);
 /* Check actual clear storage, not a canned readback from a draw fixture. */
 assert(before[0]&&before[1]&&before[2]&&before[3]==255);
 for(unsigned i=4;i<sizeof(before);i++)assert(before[i]==before[i%4]);
 struct pipe_draw_info info={.mode=MESA_PRIM_TRIANGLES,.instance_count=1};
 struct pipe_draw_start_count_bias draw={.count=3};
 ctx->draw_vbo(ctx,&info,0,NULL,&draw,1);
 assert(remesa_context_error(ctx)==error);
 assert(state.prepare_attempts==1&&state.prepared==1&&state.destroyed==1);
 assert(!state.submit_attempts&&!state.base.draws&&!state.base.shader_submits);
 struct pipe_fence_handle *fence=NULL;ctx->flush(ctx,&fence,0);assert(!fence);
 read_target(ctx,target,after);assert(!memcmp(before,after,sizeof(before)));
 assert(remesa_context_cpu_clear_count(ctx)==1);
 ctx->bind_vertex_elements_state(ctx,NULL);ctx->delete_vertex_elements_state(ctx,ve);ctx->destroy(ctx);
 resource_drop(vb);resource_drop(target);assert(state.base.allocs==state.base.frees);
 assert(state.base.maps==state.base.unmaps);screen->destroy(screen);
 printf("{\"classification\":\"FAILED_ACCELERATION_%s\",\"constant_argb\":%u,"
  "\"actual_Gallium_context\":true,\"OpenGL_context\":false,\"CPU_clears\":1,"
  "\"draw_vbo_calls\":1,\"original_prepare_calls\":1,\"VS_bytes\":%zu,\"FS_bytes\":%zu,"
  "\"FS_PDS_CODE_bytes\":%zu,\"staged_FS_PDS_bytes\":56,\"owned_tokens_destroyed\":1,\"clear_preserved\":true,"
  "\"submit_calls\":0,\"GPU_submissions\":0}\n",error==ENOTSUP?"ENOTSUP":"EIO",
  color,state.vertex_bytes,state.fragment_bytes,state.pds_bytes);
}
int main(void)
{
 exercise(UINT32_C(0xff00ff00),ENOTSUP);
 exercise(UINT32_C(0xffff00ff),ENOTSUP);
 exercise(UINT32_C(0xffffff00),ENOTSUP);
 exercise(UINT32_C(0xff00ff00),EIO);
 return 0;
}
