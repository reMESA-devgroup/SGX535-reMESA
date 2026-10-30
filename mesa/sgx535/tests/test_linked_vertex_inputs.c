/* SPDX-License-Identifier: MIT. Pre-VS byte transport, never CPU rendering. */
int input_persistent_regression(void);
#define main input_persistent_regression
#include "test_persistent_shader.c"
#undef main
int input_linkage_regression(void);
#define main input_linkage_regression
#include "test_shader_linkage.c"
#undef main
static unsigned char source[3][208];
static struct remesa_linked_attribute attributes[3];
static void seed(void)
{
 for(unsigned k=0;k<3;k++){
  for(unsigned n=0;n<sizeof(source[k]);n++)source[k][n]=(unsigned char)(n*47+k*19);
  attributes[k]=(struct remesa_linked_attribute){source[k],sizeof(source[k]),k+1,24,
   PIPE_FORMAT_R32G32B32A32_FLOAT};
 }
 attributes[2].stride=0; /* Explicit constant input, still copied bit for bit. */
 const uint32_t special[4]={0x80000000,0x7fc01234,0x7f800000,0xff800000};
 memcpy(source[0]+1+24,special,16);
}
static void words(unsigned char *dst,unsigned width,const uint32_t *values,unsigned count)
{for(unsigned i=0;i<count;i++)memcpy(dst+i*width,&values[i],width);}
#ifndef REMESA_LINKED_INPUT_TEST_MAIN
#define REMESA_LINKED_INPUT_TEST_MAIN main
#endif
int REMESA_LINKED_INPUT_TEST_MAIN(void)
{
 CHECK(!input_persistent_regression());CHECK(!input_linkage_regression());
 seed();const uint32_t raw[7]={99,6,2,6,3,6,2};
 struct pipe_draw_info info={.mode=MESA_PRIM_TRIANGLES,.instance_count=1};
 struct pipe_draw_start_count_bias draw={.start=1,.count=7,.index_bias=-1};
 for(unsigned width=1;width<=4;width*=2){
  unsigned char indices[28];words(indices,width,raw,7);info.index_size=width;
  info.index_bounds_valid=true;info.min_index=2;info.max_index=6;info.index.resource=(void *)1;
  struct remesa_linked_vertex_inputs *owned=NULL;
  CHECK(!remesa_linked_vertex_inputs_create(&info,&draw,attributes,indices,7*width,&owned)&&owned);
  struct remesa_linked_vertex_inputs_view view;
  CHECK(!remesa_linked_vertex_inputs_inspect(owned,&view));
  CHECK(view.vertex_count==5&&view.index_count==6&&view.stride_bytes==48&&view.vertex_bytes==240&&
   view.index_bytes==12&&view.min_source_vertex==1&&view.max_source_vertex==5);
  const uint16_t expected[6]={4,0,4,1,4,0};CHECK(!memcmp(view.indices,expected,12));
  for(unsigned v=0;v<5;v++)for(unsigned a=0;a<3;a++)
   CHECK(!memcmp((const unsigned char *)view.vertices+48*v+16*a,
    source[a]+attributes[a].offset+(v+1)*attributes[a].stride,16));
  CHECK(remesa_linked_vertex_inputs_inspect(owned,(void *)view.vertices)==EINVAL);
  /* Actual immutable GEM/PDS/state owners consume the resulting host snapshot. */
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device device={0};
  struct remesa_persistent_config cfg={.ops=&ops,.priv=&f,.context=2200+width,
   .va_start=0x20000000,.va_end=0x20100000,.byte_budget=8*4096,.resource_only_backend=true};
  cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};
  CHECK(!remesa_persistent_open(&device,&cfg));
  struct remesa_shader_program vs=vertex(2),fs=fragment(1);struct remesa_shader_linkage link;
  CHECK(!remesa_shader_linkage_prepare(&vs,&fs,&link));const uint32_t banks[2]={4,3};
  void *code=NULL,*geometry=NULL,*pds=NULL;
  CHECK(!remesa_persistent_linked_program_prepare(&device,&link,banks,&code));
  const struct remesa_linked_geometry_input input={.vertices=view.vertices,.vertex_bytes=view.vertex_bytes,
   .vertex_count=view.vertex_count,.indices=view.indices,.index_bytes=view.index_bytes,.index_count=view.index_count};
  CHECK(!remesa_persistent_linked_geometry_prepare(&device,&input,&geometry));
  CHECK(!remesa_persistent_linked_pds_prepare(&device,code,geometry,&pds));
  struct remesa_persistent_linked_pds_backing backing;
  CHECK(!remesa_persistent_linked_pds_snapshot(&device,pds,&backing));
  CHECK(backing.geometry.vertex_count==5&&backing.geometry.index_count==6);
  CHECK(!memcmp(get(&f,backing.geometry.handle)->bytes,view.vertices,view.vertex_bytes));
  const unsigned char *ib=get(&f,backing.geometry.handle)->bytes+backing.geometry.index_offset;
  for(unsigned n=0;n<6;n++)CHECK(ib[n*2]==expected[n]&&!ib[n*2+1]);
  unsigned char ta[20];CHECK(!remesa_persistent_linked_index_encode(&device,pds,0,6,ta));
  CHECK(shader_pds_le32(ta)==0x81400006&&shader_pds_le32(ta+16)==0x10000303);
  device.ops->destroy_bo(device.priv,code);device.ops->destroy_bo(device.priv,geometry);
  device.ops->destroy_bo(device.priv,pds);CHECK(f.creates==f.destroys&&!f.forbidden);
  CHECK(!remesa_persistent_close(&device));
  unsigned char saved[240];memcpy(saved,view.vertices,240);memset(source,0,sizeof(source));
  CHECK(!memcmp(saved,view.vertices,240));remesa_linked_vertex_inputs_destroy(owned);seed();
 }
 /* Nonindexed draws ignore all indexed hints/bias and incomplete tails. */
 info.index_size=0;info.index.resource=(void *)1;info.primitive_restart=true;
 draw=(struct pipe_draw_start_count_bias){.start=2,.count=8,.index_bias=INT32_MIN};
 struct remesa_linked_vertex_inputs *owned=NULL;struct remesa_linked_vertex_inputs_view view;
 CHECK(!remesa_linked_vertex_inputs_create(&info,&draw,attributes,(void *)1,SIZE_MAX,&owned));
 CHECK(!remesa_linked_vertex_inputs_inspect(owned,&view)&&view.vertex_count==6&&view.index_count==6&&
  view.min_source_vertex==2&&view.max_source_vertex==7);
 for(unsigned i=0;i<6;i++)CHECK(view.indices[i]==i);
 remesa_linked_vertex_inputs_destroy(owned);owned=NULL;
 /* Zero-stride attributes can address a high source range without host overflow. */
 for(unsigned a=0;a<3;a++)attributes[a].stride=0;
 const uint32_t high[3]={UINT32_MAX-2,UINT32_MAX-1,UINT32_MAX};
 info=(struct pipe_draw_info){.mode=MESA_PRIM_TRIANGLES,.instance_count=1,.index_size=4};
 draw=(struct pipe_draw_start_count_bias){.count=3};
 CHECK(!remesa_linked_vertex_inputs_create(&info,&draw,attributes,high,sizeof(high),&owned));
 CHECK(!remesa_linked_vertex_inputs_inspect(owned,&view)&&view.vertex_count==3&&view.max_source_vertex==UINT32_MAX);
 remesa_linked_vertex_inputs_destroy(owned);owned=NULL;seed();
 /* Index capacity is independent of the number of uploaded source records. */
 uint8_t *many=malloc(65535);CHECK(many);for(unsigned n=0;n<65535;n++)many[n]=(uint8_t)(n%3);
 info.index_size=1;draw.count=65535;
 CHECK(!remesa_linked_vertex_inputs_create(&info,&draw,attributes,many,65535,&owned));
 CHECK(!remesa_linked_vertex_inputs_inspect(owned,&view)&&view.vertex_count==3&&view.index_count==65535);
 remesa_linked_vertex_inputs_destroy(owned);owned=NULL;free(many);
 for(unsigned mode=0;mode<13;mode++){
  seed();info=(struct pipe_draw_info){.mode=MESA_PRIM_TRIANGLES,.instance_count=1,.index_size=4};
  draw=(struct pipe_draw_start_count_bias){.count=3};uint32_t idx[3]={0,1,2};int expected_error=EINVAL;
  if(mode==0)draw.index_bias=-1;
  if(mode==1){info.index_bounds_valid=true;info.min_index=1;info.max_index=2;}
  if(mode==2)attributes[1].bytes=40;
  if(mode==3)attributes[0].offset=UINT64_MAX;
  if(mode==4){attributes[2].format=PIPE_FORMAT_R32G32B32_FLOAT;expected_error=ENOTSUP;}
  if(mode==5){draw.count=65538;expected_error=E2BIG;}
  if(mode==6){idx[2]=4095;expected_error=E2BIG;}
  if(mode==7){info.index_size=0;draw.start=UINT32_MAX;}
  if(mode==8)info.index_size=3;
  if(mode==9){draw.count=0;expected_error=ENODATA;}
  if(mode==10){info.primitive_restart=true;expected_error=ENOTSUP;}
  if(mode==11)draw.start=1;
  if(mode==12){info.instance_count=2;expected_error=ENOTSUP;}
  CHECK(remesa_linked_vertex_inputs_create(&info,&draw,attributes,idx,sizeof(idx),&owned)==expected_error&&!owned);
 }
 seed();info=(struct pipe_draw_info){.mode=MESA_PRIM_TRIANGLES,.instance_count=1};draw=(struct pipe_draw_start_count_bias){.count=3};
 CHECK(remesa_linked_vertex_inputs_create(&info,&draw,attributes,NULL,0,(void *)source[0])==EINVAL);
 CHECK(remesa_linked_vertex_inputs_create(&info,&draw,attributes,NULL,0,(void *)attributes)==EINVAL);
 puts("PASS linked pre-VS attributes: bit-exact P/N/L, strided/broadcast sources, u8/u16/u32+bias/bounds, min/max compaction, repeated/provoking order,65535 indices independent of4095 records, overflow/alias rejection and immutable GEM/PDS/TA owner integration; GPU0/API0");
 return 0;
}
