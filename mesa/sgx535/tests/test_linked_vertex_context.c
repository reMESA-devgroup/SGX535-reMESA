/* SPDX-License-Identifier: MIT. Actual Gallium/NIR; fake BOs/fences; GPU0. */
int linked_context_driver_fixture(void);
#define main linked_context_driver_fixture
#include "test_driver.c"
#undef main
#include "nir_builder.h"
uint32_t nir_debug=NIR_DEBUG_EXTENDED_VALIDATION;
static void *last_allocation;
static unsigned fail_map_after;
static bool fail_malloc;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t);
void *__wrap_malloc(size_t n){if(fail_malloc){fail_malloc=false;return NULL;}return __real_malloc(n);}
static int tracked_alloc(void *priv,size_t n,void **out)
{int e=alloc(priv,n,out);last_allocation=*out;return e;}
static int linked_map(void *priv,void *object,bool write,void **out)
{
 if(fail_map_after&&!--fail_map_after){
  struct recorder *rec=priv;unsigned old=rec->map_mode;rec->map_mode=2;
  int e=map_bo(priv,object,write,out);rec->map_mode=old;return e;
 }
 return map_bo(priv,object,write,out);
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
static void check_inputs(const struct remesa_linked_vertex_inputs *owned,const uint32_t values[3][3][4],unsigned separate)
{
 struct remesa_linked_vertex_inputs_view v;assert(!remesa_linked_vertex_inputs_inspect(owned,&v));
 assert(v.vertex_count==3&&v.index_count==3&&v.stride_bytes==48&&v.min_source_vertex==0&&v.max_source_vertex==2);
 const uint16_t expected[3]={2,0,1};assert(!memcmp(v.indices,expected,sizeof(expected)));
 for(unsigned i=0;i<3;i++)for(unsigned a=0;a<3;a++)
  assert(!memcmp((const unsigned char *)v.vertices+i*48+a*16,values[a][separate&&a==2?0:i],16));
}
int main(void)
{
 glsl_type_singleton_init_or_ref();struct recorder rec={0};struct remesa_device_ops tracked=ops;
 tracked.allocate=tracked_alloc;tracked.map=linked_map;
 struct remesa_device dev={.ops=&tracked,.priv=&rec,.resource_only_backend=true};
 struct pipe_screen *s=remesa_screen_create(&dev);assert(s);
 for(unsigned layout=0;layout<3;layout++){
  struct pipe_context *ctx=s->context_create(s,NULL,0);assert(ctx);
  struct pipe_shader_state sh={.type=PIPE_SHADER_IR_NIR,.ir.nir=linked_shader(0)};
  void *vs=ctx->create_vs_state(ctx,&sh);assert(vs);ctx->bind_vs_state(ctx,vs);
  sh.ir.nir=linked_shader(1);void *fs=ctx->create_fs_state(ctx,&sh);assert(fs);ctx->bind_fs_state(ctx,fs);
  uint32_t values[3][3][4];
  for(unsigned a=0;a<3;a++)for(unsigned v=0;v<3;v++)for(unsigned k=0;k<4;k++)values[a][v][k]=0x3f000000+a*113+v*13+k;
  values[0][1][0]=0x80000000;values[0][1][1]=0x7fc01234;
  unsigned char interleaved[160]={0};uint16_t indices[4]={65535,2,0,1};memcpy(interleaved,indices,sizeof(indices));
  for(unsigned v=0;v<3;v++)for(unsigned a=0;a<3;a++)memcpy(interleaved+16+v*48+a*16,values[a][v],16);
  struct pipe_resource *pv=buffer(s,sizeof(interleaved),PIPE_BIND_VERTEX_BUFFER|PIPE_BIND_INDEX_BUFFER);assert(pv);
  struct bo *pv_bo=last_allocation;ctx->buffer_subdata(ctx,pv,0,0,sizeof(interleaved),interleaved);
  struct pipe_resource *nv=pv,*lv=pv,*ib=pv;
  if(layout==1){
   nv=buffer(s,48,PIPE_BIND_VERTEX_BUFFER);lv=buffer(s,16,PIPE_BIND_VERTEX_BUFFER);ib=buffer(s,8,PIPE_BIND_INDEX_BUFFER);
   assert(nv&&lv&&ib);ctx->buffer_subdata(ctx,nv,0,0,48,values[1]);ctx->buffer_subdata(ctx,lv,0,0,16,values[2]);
   ctx->buffer_subdata(ctx,ib,0,0,8,indices);
  }
  struct pipe_vertex_buffer vb[3]={{.buffer.resource=pv,.buffer_offset=16},
   {.buffer.resource=nv},{.buffer.resource=lv}};
  if(layout==2)vb[1].buffer_offset=vb[2].buffer_offset=16;
  unsigned buffers=layout?3:1;
  for(unsigned b=0;b<buffers;b++)++vb[b].buffer.resource->reference.count;
  ctx->set_vertex_buffers(ctx,buffers,vb);assert(!remesa_context_error(ctx));
  struct pipe_vertex_element elements[3]={0};
  for(unsigned a=0;a<3;a++){
   elements[a].src_format=PIPE_FORMAT_R32G32B32A32_FLOAT;
   elements[a].vertex_buffer_index=layout?a:0;
   elements[a].src_stride=layout==1&&a? (a==2?0:16):48;
   elements[a].src_offset=layout==1&&a?0:a*16;
  }
  void *el=ctx->create_vertex_elements_state(ctx,3,elements);assert(el);ctx->bind_vertex_elements_state(ctx,el);
  struct pipe_draw_info info={.mode=MESA_PRIM_TRIANGLES,.instance_count=1,.index_size=2,
   .has_user_indices=layout==0,.index_bounds_valid=true,.min_index=0,.max_index=2};
  if(layout==0)info.index.user=indices;else info.index.resource=ib;
  struct pipe_draw_start_count_bias draw={.start=1,.count=4};struct remesa_linked_vertex_inputs *owned=NULL;
  unsigned maps=rec.maps;
  if(layout==0){
   assert(remesa_context_linked_vertex_inputs_snapshot(ctx,&info,&draw,(void *)indices)==EINVAL&&rec.maps==maps);
   const void *saved=info.index.user;info.index.user=(void *)(uintptr_t)(UINTPTR_MAX-3);
   assert(remesa_context_linked_vertex_inputs_snapshot(ctx,&info,&draw,&owned)==EINVAL&&!owned&&rec.maps==maps);
   info.index.user=saved;
  }
  assert(!remesa_context_linked_vertex_inputs_snapshot(ctx,&info,&draw,&owned));
  assert(rec.maps-maps==(layout==1?4:1)&&rec.maps==rec.unmaps);
  check_inputs(owned,values,layout==1);
  unsigned char zero[160]={0};ctx->buffer_subdata(ctx,pv,0,0,sizeof(zero),zero);check_inputs(owned,values,layout==1);
  remesa_linked_vertex_inputs_destroy(owned);owned=NULL;ctx->buffer_subdata(ctx,pv,0,0,sizeof(interleaved),interleaved);
  for(unsigned mode=1;mode<=2;mode++){
   rec.map_mode=mode;assert(remesa_context_linked_vertex_inputs_snapshot(ctx,&info,&draw,&owned)==(mode==1?EPROTO:EIO)&&!owned);
   assert(rec.maps==rec.unmaps);
  }
  rec.map_mode=0;
  fail_malloc=true;
  assert(remesa_context_linked_vertex_inputs_snapshot(ctx,&info,&draw,&owned)==ENOMEM&&!owned&&!fail_malloc&&rec.maps==rec.unmaps);
  if(layout==1){fail_map_after=3;assert(remesa_context_linked_vertex_inputs_snapshot(ctx,&info,&draw,&owned)==EIO&&!owned&&!fail_map_after&&rec.maps==rec.unmaps);}
  struct pipe_box box={.width=16,.height=1,.depth=1};struct pipe_transfer *transfer=NULL;
  assert(ctx->buffer_map(ctx,pv,0,PIPE_MAP_READ,&box,&transfer)&&transfer);
  maps=rec.maps;assert(remesa_context_linked_vertex_inputs_snapshot(ctx,&info,&draw,&owned)==EBUSY&&!owned&&rec.maps==maps);
  ctx->buffer_unmap(ctx,transfer);
  maps=rec.maps;assert(remesa_context_linked_vertex_inputs_snapshot(ctx,&info,&draw,(void *)pv)==EINVAL&&rec.maps==maps);
  assert(remesa_context_linked_vertex_inputs_snapshot(ctx,&info,&draw,(void *)&info)==EINVAL&&rec.maps==maps);
  /* Imported writer completion stays necessary; no read occurs on timeout. */
  struct fence *pending=calloc(1,sizeof(*pending));assert(pending);pending->target=pv_bo;++pv_bo->refs;pv_bo->pending=true;pv_bo->published=false;
  struct pipe_fence_handle *writer=NULL;assert(!remesa_resource_attach_writer(ctx,pv,pending,&writer));
  maps=rec.maps;assert(remesa_context_linked_vertex_inputs_snapshot(ctx,&info,&draw,&owned)==ETIMEDOUT&&!owned&&rec.maps==maps);
  pending->retired=true;pv_bo->published=true;
  assert(!remesa_context_linked_vertex_inputs_snapshot(ctx,&info,&draw,&owned));check_inputs(owned,values,layout==1);
  remesa_linked_vertex_inputs_destroy(owned);owned=NULL;s->fence_reference(s,&writer,NULL);
  ctx->draw_vbo(ctx,&info,0,NULL,&draw,1);assert(remesa_context_error(ctx)&&!rec.draws&&!rec.shader_prepares);
  ctx->delete_vertex_elements_state(ctx,el);
  assert(remesa_context_linked_vertex_inputs_snapshot(ctx,&info,&draw,&owned)==ENOTSUP&&!owned);
  ctx->delete_vs_state(ctx,vs);ctx->delete_fs_state(ctx,fs);ctx->destroy(ctx);
  resource_drop(pv);if(layout==1){resource_drop(nv);resource_drop(lv);resource_drop(ib);}
 }
 s->destroy(s);assert(rec.allocs==rec.frees&&rec.maps==rec.unmaps&&!rec.draws&&!rec.shader_prepares&&!rec.shader_submits);
 glsl_type_singleton_decref();puts("PASS actual Gallium/NIR linked attributes: three sources/interleaved/broadcast/VB-IB aliases, deduplicated maps, source independence, map rollback/busy/alias rejection, writer timeout then simulated retirement, ref cleanup and unchanged draw gate; GPU0/API0");return 0;
}
