/* SPDX-License-Identifier: MIT. Pre-VS byte ownership/assembly, ZERO GPU. */
#include "gears_nir_fixture.h"
int inherited_gears_geometry_driver_main(void);
#define main inherited_gears_geometry_driver_main
#include "test_driver.c"
#undef main
#include <limits.h>
static bool fail_malloc;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t);
void *__wrap_malloc(size_t n){if(fail_malloc){fail_malloc=false;return NULL;}return __real_malloc(n);}
static const uint32_t positions[3][4]={
 {0x80000000,0x7fc01234,0x00000001,0x3f800000},
 {0x40000000,0x40400000,0x40800000,0x3f000000},
 {0xc0000000,0xc0400000,0xc0800000,0x40000000}};
static const uint32_t normals[3][4]={
 {0x3f800000,0,0,0x3f800000},
 {0,0x3f800000,0,0x3f800000},
 {0,0,0xbf800000,0x3f800000}};
static unsigned fail_map_call;
static int geometry_map(void *priv,void *object,bool write,void **out){
 if(fail_map_call&&!--fail_map_call){struct recorder*rec=priv;rec->map_mode=2;int e=map_bo(priv,object,write,out);rec->map_mode=0;return e;}
 return map_bo(priv,object,write,out);
}
static void check(const struct remesa_gears_geometry *g,unsigned attr,const unsigned ids[3]){
 struct remesa_gears_geometry_view v;assert(!remesa_gears_geometry_inspect(g,&v));
 assert(v.vertex_count==3&&v.stride_dwords==(attr?8:4)&&v.normal_from_attribute==attr&&v.bytes==3*v.stride_dwords*4);
 for(unsigned i=0;i<3;i++){
  const unsigned char*p=(const unsigned char*)v.vertices+i*v.stride_dwords*4;
  assert(!memcmp(p,positions[ids[i]],16));if(attr)assert(!memcmp(p+16,normals[ids[i]],16));
 }
 assert(remesa_gears_geometry_inspect(g,(void*)v.vertices)==EINVAL);
}
static void raw_tests(void){
 unsigned char p[64]={0},n[64]={0};
 for(unsigned i=0;i<3;i++){memcpy(p+1+i*20,positions[i],16);memcpy(n+3+i*16,normals[i],12);}
 struct remesa_gears_attribute pa={p,57,1,20,PIPE_FORMAT_R32G32B32A32_FLOAT};
 struct remesa_gears_attribute na={n,47,3,16,PIPE_FORMAT_R32G32B32_FLOAT};
 struct pipe_draw_info info={.mode=MESA_PRIM_TRIANGLES,.instance_count=1,.index_bounds_valid=true,.min_index=1,.max_index=3};
 struct pipe_draw_start_count_bias draw_info={.start=1,.count=5,.index_bias=-1};
 const unsigned expected[3]={2,0,1};
 for(unsigned width=1;width<=4;width*=2){
  unsigned char indices[16]={0};unsigned raw[4]={255,3,1,2};
  for(unsigned i=0;i<4;i++){if(width==1){indices[i]=raw[i];}else if(width==2){uint16_t x=raw[i];memcpy(indices+2*i,&x,2);}else memcpy(indices+4*i,&raw[i],4);}
  info.index_size=width;struct remesa_gears_geometry*g=NULL;
  assert(!remesa_gears_geometry_create(&info,&draw_info,1,&pa,&na,indices,4*width,&g));check(g,1,expected);
  struct remesa_gears_geometry*same=g;assert(remesa_gears_geometry_create(&info,&draw_info,1,&pa,&na,indices,4*width,&g)==EBUSY&&g==same);
  remesa_gears_geometry_destroy(g);g=NULL;
  /* Repeated sparse references must retain their original primitive order. */
  if(width==1)indices[3]=3;else if(width==2){uint16_t x=3;memcpy(indices+6,&x,2);}else{uint32_t x=3;memcpy(indices+12,&x,4);}
  const unsigned repeated[3]={2,0,2};assert(!remesa_gears_geometry_create(&info,&draw_info,1,&pa,&na,indices,4*width,&g));check(g,1,repeated);remesa_gears_geometry_destroy(g);g=NULL;
  assert(remesa_gears_geometry_create(&info,&draw_info,1,&pa,&na,indices,4*width-1,&g)==EINVAL&&!g);
  info.min_index=2;assert(remesa_gears_geometry_create(&info,&draw_info,1,&pa,&na,indices,4*width,&g)==EINVAL&&!g);info.min_index=1;
  draw_info.index_bias=-4;assert(remesa_gears_geometry_create(&info,&draw_info,1,&pa,&na,indices,4*width,&g)==EINVAL&&!g);draw_info.index_bias=-1;
  na.bytes=46;assert(remesa_gears_geometry_create(&info,&draw_info,1,&pa,&na,indices,4*width,&g)==EINVAL&&!g);na.bytes=47;
  info.primitive_restart=true;assert(remesa_gears_geometry_create(&info,&draw_info,1,&pa,&na,indices,4*width,&g)==ENOTSUP&&!g);info.primitive_restart=false;
 }
 info.index_size=0;info.index.resource=(void*)(uintptr_t)1;info.min_index=UINT_MAX;info.max_index=0;
 draw_info=(struct pipe_draw_start_count_bias){.start=0,.count=3,.index_bias=INT_MIN};
 struct remesa_gears_geometry*g=NULL;const unsigned seq[3]={0,1,2};
 assert(!remesa_gears_geometry_create(&info,&draw_info,1,&pa,&na,(void*)(uintptr_t)1,SIZE_MAX,&g));
 memset(p,0,sizeof(p));memset(n,0,sizeof(n));check(g,1,seq);remesa_gears_geometry_destroy(g);g=NULL;
 pa=(struct remesa_gears_attribute){positions,sizeof(positions),0,16,PIPE_FORMAT_R32G32B32A32_FLOAT};
 assert(!remesa_gears_geometry_create(&info,&draw_info,0,&pa,NULL,NULL,0,&g));check(g,0,seq);remesa_gears_geometry_destroy(g);g=NULL;
 fail_malloc=true;assert(remesa_gears_geometry_create(&info,&draw_info,0,&pa,NULL,NULL,0,&g)==ENOMEM&&!g&&!fail_malloc);
 struct remesa_gears_attribute bad=pa;bad.offset=UINT64_MAX;
 assert(remesa_gears_geometry_create(&info,&draw_info,0,&bad,NULL,NULL,0,&g)==EINVAL&&!g);
 bad=pa;bad.stride=UINT_MAX;assert(remesa_gears_geometry_create(&info,&draw_info,0,&bad,NULL,NULL,0,&g)==EINVAL&&!g);
 bad=pa;bad.format=PIPE_FORMAT_R8_UNORM;assert(remesa_gears_geometry_create(&info,&draw_info,0,&bad,NULL,NULL,0,&g)==ENOTSUP&&!g);
 assert(remesa_gears_geometry_create(&info,&draw_info,2,&pa,NULL,NULL,0,&g)==EINVAL&&!g);
 assert(remesa_gears_geometry_create(&info,&draw_info,0,&pa,NULL,NULL,0,(void*)&positions[0])==EINVAL);
 draw_info.count=2;assert(remesa_gears_geometry_create(&info,&draw_info,0,&pa,NULL,NULL,0,&g)==ENODATA&&!g);
 draw_info.count=4098;assert(remesa_gears_geometry_create(&info,&draw_info,0,&pa,NULL,NULL,0,&g)==E2BIG&&!g);
 pa.stride=0;draw_info.count=4096;
 assert(!remesa_gears_geometry_create(&info,&draw_info,0,&pa,NULL,NULL,0,&g));
 struct remesa_gears_geometry_view v;assert(!remesa_gears_geometry_inspect(g,&v)&&v.vertex_count==4095&&v.bytes==65520);
 for(unsigned i=0;i<v.vertex_count;i++)assert(!memcmp((const unsigned char*)v.vertices+i*16,positions[0],16));remesa_gears_geometry_destroy(g);g=NULL;
 const uint32_t xy[2]={0x3f000000,0xbf000000},xyz[3]={1,2,3};
 pa=(struct remesa_gears_attribute){xy,sizeof(xy),0,0,PIPE_FORMAT_R32G32_FLOAT};
 na=(struct remesa_gears_attribute){xyz,sizeof(xyz),0,0,PIPE_FORMAT_R32G32B32_FLOAT};draw_info.count=3;
 assert(!remesa_gears_geometry_create(&info,&draw_info,1,&pa,&na,NULL,0,&g));assert(!remesa_gears_geometry_inspect(g,&v));
 const uint32_t defaults[8]={0x3f000000,0xbf000000,0,0x3f800000,1,2,3,0x3f800000};
 for(unsigned i=0;i<3;i++)assert(!memcmp((const unsigned char*)v.vertices+i*32,defaults,32));remesa_gears_geometry_destroy(g);g=NULL;
 draw_info.start=UINT_MAX;assert(remesa_gears_geometry_create(&info,&draw_info,1,&pa,&na,NULL,0,&g)==EINVAL&&!g);
 draw_info.start=0;info.instance_count=2;assert(remesa_gears_geometry_create(&info,&draw_info,1,&pa,&na,NULL,0,&g)==ENOTSUP&&!g);
 info.instance_count=1;info.mode=MESA_PRIM_TRIANGLE_STRIP;assert(remesa_gears_geometry_create(&info,&draw_info,1,&pa,&na,NULL,0,&g)==ENOTSUP&&!g);
}
static void context_tests(void){
 struct recorder rec={0};struct remesa_device_ops geometry_ops=ops;geometry_ops.map=geometry_map;
 struct remesa_device dev={.ops=&geometry_ops,.priv=&rec,.offline_test_backend=true};struct pipe_screen*s=remesa_screen_create(&dev);assert(s);
 for(unsigned attr=0;attr<2;attr++)for(unsigned separate=0;separate<3;separate++){
  struct pipe_context*c=s->context_create(s,NULL,0);assert(c);
  struct pipe_shader_state sh={.type=PIPE_SHADER_IR_NIR,.ir.nir=gears_nir_fixture(attr)};void*vs=c->create_vs_state(c,&sh);assert(vs);c->bind_vs_state(c,vs);
  uint32_t interleaved[3][8];for(unsigned i=0;i<3;i++){memcpy(interleaved[i],positions[i],16);memcpy(interleaved[i]+4,normals[i],16);}
  /* Actual Mesa display-list current_bo is INDEX_BUFFER|VERTEX_STATE and
   * is also the VAO vertex source. It remains outside legacy GPU admission. */
  struct pipe_resource*pv=buffer(s,sizeof(interleaved),PIPE_BIND_INDEX_BUFFER|
   (separate==2?PIPE_BIND_VERTEX_STATE:PIPE_BIND_VERTEX_BUFFER));assert(pv);c->buffer_subdata(c,pv,0,0,sizeof(interleaved),interleaved);
  struct pipe_resource*nv=separate==1?buffer(s,sizeof(normals),PIPE_BIND_VERTEX_BUFFER):pv;assert(nv);if(separate==1)c->buffer_subdata(c,nv,0,0,sizeof(normals),normals);
  struct pipe_vertex_buffer vb[2]={{.buffer.resource=pv},{.buffer.resource=nv}};
  unsigned buffers=separate?2:1;for(unsigned i=0;i<buffers;i++)++vb[i].buffer.resource->reference.count;
  c->set_vertex_buffers(c,buffers,vb);assert(!remesa_context_error(c));
  struct pipe_vertex_element el[2]={{.src_stride=32,.src_format=PIPE_FORMAT_R32G32B32A32_FLOAT},
   {.src_stride=separate==1?16:32,.src_offset=separate==1?0:16,.vertex_buffer_index=separate?1:0,.src_format=PIPE_FORMAT_R32G32B32_FLOAT}};
  void*elements=c->create_vertex_elements_state(c,1+attr,el);assert(elements);c->bind_vertex_elements_state(c,elements);
  uint16_t indices[4]={65535,2,0,1};struct pipe_draw_info info={.mode=MESA_PRIM_TRIANGLES,.instance_count=1,.index_size=2,.has_user_indices=true,.index.user=indices};
  struct pipe_draw_start_count_bias dr={.start=1,.count=4};struct remesa_gears_geometry*g=NULL;const unsigned order[3]={2,0,1};
  unsigned maps=rec.maps;assert(!remesa_context_gears_geometry_snapshot(c,&info,&dr,&g));assert(rec.maps-maps==(attr&&separate==1?2:1)&&rec.maps==rec.unmaps);check(g,attr,order);
  memset(interleaved,0,sizeof(interleaved));c->buffer_subdata(c,pv,0,0,sizeof(interleaved),interleaved);check(g,attr,order);
  remesa_gears_geometry_destroy(g);g=NULL;c->buffer_subdata(c,pv,0,0,16,positions[0]);
  for(unsigned mode=1;mode<=2;mode++){rec.map_mode=mode;assert(remesa_context_gears_geometry_snapshot(c,&info,&dr,&g)==(mode==1?EPROTO:EIO)&&!g&&rec.maps==rec.unmaps);}rec.map_mode=0;
  if(attr&&separate==1){fail_map_call=2;assert(remesa_context_gears_geometry_snapshot(c,&info,&dr,&g)==EIO&&!g&&!fail_map_call&&rec.maps==rec.unmaps);}
  struct pipe_box box={.width=16,.height=1,.depth=1};struct pipe_transfer*t=NULL;void*m=c->buffer_map(c,pv,0,PIPE_MAP_READ,&box,&t);assert(m&&t);assert(remesa_context_gears_geometry_snapshot(c,&info,&dr,&g)==EBUSY&&!g);c->buffer_unmap(c,t);
  /* Same BO as POSITION and index source is mapped only once. */
  c->buffer_subdata(c,pv,0,0,sizeof(indices),indices);info.has_user_indices=false;info.index.resource=pv;
  maps=rec.maps;assert(!remesa_context_gears_geometry_snapshot(c,&info,&dr,&g));assert(rec.maps-maps==(attr&&separate==1?2:1));remesa_gears_geometry_destroy(g);g=NULL;
  /* Legacy draw must not accept expanded elements/bindings. */
  if(attr||separate){c->draw_vbo(c,&info,0,NULL,&dr,1);assert(remesa_context_error(c)==ENOTSUP&&!rec.draws&&!rec.shader_prepares);}
  c->delete_vertex_elements_state(c,elements);assert(remesa_context_gears_geometry_snapshot(c,&info,&dr,&g)==ENOTSUP&&!g);
  c->delete_vs_state(c,vs);c->destroy(c);resource_drop(pv);if(separate==1)resource_drop(nv);
 }
 s->destroy(s);assert(rec.allocs==rec.frees&&rec.maps==rec.unmaps&&!rec.draws&&!rec.shader_prepares&&!rec.shader_submits);
}
int main(void){raw_tests();context_tests();puts("PASS raw P/N triangle assembly + owned context snapshots: 8/16/32 indices, bias/start/bounds, sparse/repeated order, defaults/bit preservation, broadcast/capacity, rollback/OOM, two VB/alias mapping, updates/busy/failures/teardown; ZERO GPU");return 0;}
