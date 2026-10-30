/* SPDX-License-Identifier: MIT. Index assembly only, not CPU/GPU rendering. */
int triangle_input_regression(void);
#define REMESA_LINKED_INPUT_TEST_MAIN triangle_input_regression
#include "test_linked_vertex_inputs.c"
#undef REMESA_LINKED_INPUT_TEST_MAIN
#include "mesa_topology_oracle.inc"

static void check_topology(const struct remesa_linked_vertex_inputs *owned,
 const struct pipe_draw_info *info,const struct pipe_draw_start_count_bias *draw,
 const uint32_t *raw,bool first)
{
 struct remesa_linked_topology plan;CHECK(!remesa_linked_topology_prepare(info->mode,draw->count,&plan));
 struct remesa_linked_vertex_inputs_view v;CHECK(!remesa_linked_vertex_inputs_inspect(owned,&v));
 uint32_t sequence[16],min=UINT32_MAX,max=0;CHECK(plan.input_count<=16);
 for(unsigned i=0;i<plan.input_count;i++){
  sequence[i]=info->index_size?(uint32_t)((int64_t)raw[draw->start+i]+draw->index_bias):draw->start+i;
  if(sequence[i]<min)min=sequence[i];
  if(sequence[i]>max)max=sequence[i];
 }
 for(unsigned i=0;i<plan.input_count;i++)sequence[i]-=min;
 uint16_t expected[42];mesa_topology_expected(info->mode,first,sequence,plan.input_count,plan.index_count,expected);
 CHECK(v.min_source_vertex==min&&v.max_source_vertex==max&&v.vertex_count==max-min+1&&
       v.index_count==plan.index_count&&v.index_bytes==plan.index_count*2&&v.stride_bytes==48);
 CHECK(!memcmp(v.indices,expected,v.index_bytes));
 for(unsigned i=0;i<v.vertex_count;i++)for(unsigned a=0;a<3;a++)
  CHECK(!memcmp((const unsigned char *)v.vertices+i*48+a*16,
   source[a]+attributes[a].offset+(min+i)*attributes[a].stride,16));
}
static void check_gpu_storage(const struct remesa_linked_vertex_inputs *owned)
{
 struct remesa_linked_vertex_inputs_view v;CHECK(!remesa_linked_vertex_inputs_inspect(owned,&v));
 struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device device={0};
 struct remesa_persistent_config cfg={.ops=&ops,.priv=&f,.context=2900,
  .va_start=0x20000000,.va_end=0x20100000,.byte_budget=8*4096,.resource_only_backend=true};
 cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};
 CHECK(!remesa_persistent_open(&device,&cfg));
 struct remesa_shader_program vs=vertex(2),fs=fragment(1);struct remesa_shader_linkage link;
 CHECK(!remesa_shader_linkage_prepare(&vs,&fs,&link));const uint32_t banks[2]={4,3};
 void *code=NULL,*geometry=NULL,*pds=NULL;
 CHECK(!remesa_persistent_linked_program_prepare(&device,&link,banks,&code));
 const struct remesa_linked_geometry_input input={.vertices=v.vertices,.vertex_bytes=v.vertex_bytes,
  .vertex_count=v.vertex_count,.indices=v.indices,.index_bytes=v.index_bytes,.index_count=v.index_count};
 CHECK(!remesa_persistent_linked_geometry_prepare(&device,&input,&geometry));
 CHECK(!remesa_persistent_linked_pds_prepare(&device,code,geometry,&pds));
 struct remesa_persistent_linked_pds_backing backing;
 CHECK(!remesa_persistent_linked_pds_snapshot(&device,pds,&backing)&&backing.geometry.index_count==9);
 const unsigned char *ib=get(&f,backing.geometry.handle)->bytes+backing.geometry.index_offset;
 for(unsigned i=0;i<v.index_count;i++)CHECK(ib[2*i]==v.indices[i]&&!ib[2*i+1]);
 unsigned char ta[20];CHECK(!remesa_persistent_linked_index_encode(&device,pds,0,v.index_count,ta));
 CHECK(shader_pds_le32(ta)==0x81400009&&shader_pds_le32(ta+16)==0x10000303);
 device.ops->destroy_bo(device.priv,code);device.ops->destroy_bo(device.priv,geometry);
 device.ops->destroy_bo(device.priv,pds);CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);
 CHECK(!remesa_persistent_close(&device));
}
int main(void)
{
 CHECK(!triangle_input_regression());seed();unsigned cases=0;
 const unsigned modes[]={MESA_PRIM_TRIANGLES,MESA_PRIM_TRIANGLE_STRIP,MESA_PRIM_TRIANGLE_FAN};
 uint32_t raw[16];for(unsigned i=0;i<16;i++)raw[i]=1+i%6;
 for(unsigned m=0;m<3;m++)for(unsigned first=0;first<2;first++)for(unsigned w=0;w<4;w++)
  for(unsigned count=3;count<=6;count++)for(unsigned start=0;start<3;start++){
   struct pipe_draw_info info={.mode=modes[m],.instance_count=1,.index_size=w?(1u<<(w-1)):0,
    .index_bounds_valid=true,.min_index=1,.max_index=6};
   struct pipe_draw_start_count_bias draw={.start=start,.count=count,.index_bias=info.index_size?-1:INT32_MIN};
   unsigned char ib[64];if(info.index_size)words(ib,info.index_size,raw,16);
   struct remesa_linked_vertex_inputs *owned=NULL;
   CHECK(!remesa_linked_vertex_inputs_create_topology(&info,&draw,attributes,ib,sizeof(ib),first,&owned));
   check_topology(owned,&info,&draw,raw,first);
   if(m&&count==5&&start==0&&w==2)check_gpu_storage(owned);
   remesa_linked_vertex_inputs_destroy(owned);owned=NULL;
   if(m)CHECK(remesa_linked_vertex_inputs_create(&info,&draw,attributes,ib,sizeof(ib),&owned)==ENOTSUP&&!owned);
   cases++;
  }
 /* Degenerate input triangles are retained, including their provoking IDs. */
 const uint32_t degenerate[]={2,2,4,4,2,3};
 for(unsigned m=1;m<3;m++)for(unsigned first=0;first<2;first++){
  struct pipe_draw_info info={.mode=modes[m],.instance_count=1,.index_size=4};
  struct pipe_draw_start_count_bias draw={.count=6,.index_bias=-1};struct remesa_linked_vertex_inputs *owned=NULL;
  CHECK(!remesa_linked_vertex_inputs_create_topology(&info,&draw,attributes,degenerate,sizeof(degenerate),first,&owned));
  check_topology(owned,&info,&draw,degenerate,first);remesa_linked_vertex_inputs_destroy(owned);
 }
 struct remesa_linked_topology p={91,92},saved=p;
 CHECK(remesa_linked_topology_prepare(MESA_PRIM_LINES,7,&p)==ENOTSUP&&!memcmp(&p,&saved,sizeof(p)));
 for(unsigned m=0;m<3;m++)for(unsigned n=0;n<3;n++)
  CHECK(remesa_linked_topology_prepare(modes[m],n,&p)==ENODATA&&!memcmp(&p,&saved,sizeof(p)));
 CHECK(remesa_linked_topology_prepare(MESA_PRIM_TRIANGLE_STRIP,UINT32_MAX,&p)==E2BIG&&!memcmp(&p,&saved,sizeof(p)));
 CHECK(!remesa_linked_topology_prepare(MESA_PRIM_TRIANGLE_FAN,21847,&p)&&p.input_count==21847&&p.index_count==65535);
 p=saved;CHECK(remesa_linked_topology_prepare(MESA_PRIM_TRIANGLE_FAN,21848,&p)==E2BIG&&!memcmp(&p,&saved,sizeof(p)));
 CHECK(!remesa_linked_topology_prepare(MESA_PRIM_TRIANGLES,65537,&p)&&p.input_count==65535&&p.index_count==65535);
 struct pipe_draw_info info={.mode=MESA_PRIM_TRIANGLE_STRIP,.instance_count=1,.index_size=4};
 struct pipe_draw_start_count_bias draw={.count=6};struct remesa_linked_vertex_inputs *owned=NULL;
 CHECK(remesa_linked_vertex_inputs_create_topology(&info,&draw,attributes,raw,5*4,false,&owned)==EINVAL&&!owned);
 info.primitive_restart=true;CHECK(remesa_linked_vertex_inputs_create_topology(&info,&draw,attributes,raw,sizeof(raw),false,&owned)==ENOTSUP&&!owned);
 info.primitive_restart=false;draw.index_bias=-2;CHECK(remesa_linked_vertex_inputs_create_topology(&info,&draw,attributes,raw,sizeof(raw),false,&owned)==EINVAL&&!owned);
 draw.index_bias=0;info.min_index=2;info.max_index=6;info.index_bounds_valid=true;
 CHECK(remesa_linked_vertex_inputs_create_topology(&info,&draw,attributes,raw,sizeof(raw),false,&owned)==EINVAL&&!owned);
 for(unsigned a=0;a<3;a++)attributes[a].stride=0;
 uint8_t *many=malloc(21848);CHECK(many);for(unsigned i=0;i<21848;i++)many[i]=(uint8_t)(i%3);
 info=(struct pipe_draw_info){.mode=MESA_PRIM_TRIANGLE_FAN,.instance_count=1,.index_size=1};draw=(struct pipe_draw_start_count_bias){.count=21847};
 CHECK(!remesa_linked_vertex_inputs_create_topology(&info,&draw,attributes,many,21848,false,&owned));
 struct remesa_linked_vertex_inputs_view v;CHECK(!remesa_linked_vertex_inputs_inspect(owned,&v)&&v.index_count==65535&&v.vertex_count==3);
 remesa_linked_vertex_inputs_destroy(owned);owned=NULL;draw.count++;
 CHECK(remesa_linked_vertex_inputs_create_topology(&info,&draw,attributes,many,21848,false,&owned)==E2BIG&&!owned);free(many);
 printf("PASS %u topology cases against pinned Mesa-generated oracle: lists/strips/fans, first/last provoking, winding, indexed/bias/start, byte-exact attributes, degenerates and expanded bounds; legacy inputs preserved; GPU0\n",cases);
 return 0;
}
