/* SPDX-License-Identifier: MIT. Owned dynamic draw state, fake provider only. */
int state_persistent_regressions(void);
#define main state_persistent_regressions
#include "test_persistent_shader.c"
#undef main
int state_linkage_regressions(void);
#define main state_linkage_regressions
#include "test_shader_linkage.c"
#undef main
static void state_endpoint(struct fixture *f,struct remesa_device *d,uint64_t context,
 void **code,void **geometry,void **pds)
{
 struct remesa_persistent_config cfg={.ops=&ops,.priv=f,.context=context,
  .va_start=0x20000000,.va_end=0x20100000,.byte_budget=8*4096,.resource_only_backend=true};
 cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};
 CHECK(!remesa_persistent_open(d,&cfg));
 struct remesa_shader_program v=vertex(2),fs=fragment(1);struct remesa_shader_linkage link;
 CHECK(!remesa_shader_linkage_prepare(&v,&fs,&link));
 const uint32_t banks[2]={4,3};unsigned char vertices[144];
 for(unsigned n=0;n<144;n++)vertices[n]=(unsigned char)(n*41+5);
 const struct remesa_linked_geometry_input in={.vertices=vertices,.vertex_bytes=144,.vertex_count=3};
 CHECK(!remesa_persistent_linked_program_prepare(d,&link,banks,code));
 CHECK(!remesa_persistent_linked_geometry_prepare(d,&in,geometry));
 CHECK(!remesa_persistent_linked_pds_prepare(d,*code,*geometry,pds));
}
static struct remesa_persistent_linked_state_request state_request(unsigned w,unsigned h,unsigned matrix)
{
 return (struct remesa_persistent_linked_state_request){
  .isp={.depth_bits_present=true,.depth_test=true,.depth_write=true,.compare=REMESA_DEPTH_ALWAYS,.rgba_mask=15},
  .viewport={.framebuffer_width=w,.framebuffer_height=h,.width=w,.height=h,.depth_bits=24,.far_z=1,
   .path=matrix?REMESA_TA_VIEWPORT_MATRIX_WINDOW:REMESA_TA_VIEWPORT_INITIAL_ZERO}};
}
int main(void)
{
 CHECK(!state_persistent_regressions());CHECK(!state_linkage_regressions());
 const unsigned sizes[3][2]={{1280,800},{640,640},{641,479}};
 for(unsigned size=0;size<3;size++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  void *code=NULL,*geometry=NULL,*pds=NULL,*state=NULL,*close=NULL,*body=NULL,*second=NULL;
  state_endpoint(&f,&d,2300+size,&code,&geometry,&pds);
  struct remesa_persistent_linked_state_request request=state_request(sizes[size][0],sizes[size][1],0);
  CHECK(!remesa_persistent_linked_state_prepare(&d,pds,&request,4,&state));
  CHECK(!remesa_persistent_scene_close_prepare(&d,sizes[size][0],sizes[size][1],4,&close));
  struct remesa_persistent_linked_state_backing sb;
  struct remesa_persistent_scene_close_backing cb;
  CHECK(!remesa_persistent_linked_state_snapshot(&d,state,&sb));
  CHECK(!remesa_persistent_scene_close_snapshot(&d,close,&cb));
  struct remesa_linked_body_draw draws[16];for(unsigned i=0;i<16;i++)draws[i]=(struct remesa_linked_body_draw){state,0,3};
  /* Caller may release raw PDS/CODE/geometry owners before body construction. */
  d.ops->destroy_bo(d.priv,code);d.ops->destroy_bo(d.priv,geometry);d.ops->destroy_bo(d.priv,pds);
  CHECK(!remesa_persistent_linked_body_prepare(&d,draws,16,close,&body));
  struct remesa_persistent_linked_body_backing b;
  CHECK(!remesa_persistent_linked_body_snapshot(&d,body,&b));
  CHECK(b.draw_count==16&&b.command_bytes==460&&b.close_offset==448&&b.dependency_count==2);
  const uint8_t *bytes=get(&f,b.handle)->bytes;
  for(unsigned i=0;i<16;i++){
   CHECK(shader_pds_le32(bytes+28*i)==(0x40000000|((sb.image.gpu_va+sb.image.pds_offset)>>4)));
   CHECK(shader_pds_le32(bytes+28*i+4)==0x0c02a204);
   CHECK(shader_pds_le32(bytes+28*i+8)==0x81400003);
   CHECK(shader_pds_le32(bytes+28*i+12)==sb.dependencies.geometry.gpu_va+sb.dependencies.geometry.index_offset);
   CHECK(shader_pds_le32(bytes+28*i+24)==0x10000303);
  }
  CHECK(!memcmp(bytes+448,get(&f,cb.image.handle)->bytes+cb.image.ta_offset,12));
  CHECK(shader_pds_le32(bytes+456)==0xc0000000);
  for(unsigned i=460;i<4096;i++)CHECK(!bytes[i]);
  unsigned maps=f.cpu_maps;for(unsigned i=0;i<100;i++)CHECK(!remesa_persistent_linked_body_snapshot(&d,body,&b));
  CHECK(f.cpu_maps==maps);
  CHECK(!remesa_persistent_linked_body_prepare(&d,draws,1,close,&second));
  d.ops->destroy_bo(d.priv,state);d.ops->destroy_bo(d.priv,close);
  CHECK(!f.destroys&&!remesa_persistent_linked_body_snapshot(&d,body,&b));
  void *cpu=NULL;CHECK(d.ops->map(d.priv,body,true,&cpu)==ENOTSUP&&!cpu);
  CHECK(!d.ops->map(d.priv,body,false,&cpu));d.ops->destroy_bo(d.priv,body);
  CHECK(!f.destroys&&remesa_persistent_close(&d)==EBUSY);
  d.ops->unmap(d.priv,body,false);CHECK(f.destroys==1);
  CHECK(!remesa_persistent_linked_body_snapshot(&d,second,&b)&&b.command_bytes==40);
  d.ops->destroy_bo(d.priv,second);
  CHECK(f.creates==7&&f.destroys==7&&f.pins==f.unpins&&!f.forbidden);
  CHECK(!remesa_persistent_close(&d));
 }
 /* Distinct state owners stay distinct; mismatched extents and bank words
  * must reject before allocating a command body. */
 {
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  void *code=NULL,*geometry=NULL,*pds=NULL,*state=NULL,*other=NULL,*close=NULL,*body=NULL;
  state_endpoint(&f,&d,2310,&code,&geometry,&pds);
  struct remesa_persistent_linked_state_request req=state_request(1280,800,0);
  CHECK(!remesa_persistent_linked_state_prepare(&d,pds,&req,4,&state));
  req.isp.compare=REMESA_DEPTH_LESS;
  CHECK(!remesa_persistent_linked_state_prepare(&d,pds,&req,4,&other));
  struct remesa_linked_body_draw draws[2]={{state,0,3},{other,0,3}};
  CHECK(!remesa_persistent_scene_close_prepare(&d,640,640,4,&close));
  unsigned created=f.creates;
  CHECK(remesa_persistent_linked_body_prepare(&d,draws,2,close,&body)==EINVAL&&!body&&f.creates==created);
  d.ops->destroy_bo(d.priv,close);close=NULL;
  CHECK(!remesa_persistent_scene_close_prepare(&d,1280,800,3,&close));
  created=f.creates;
  CHECK(remesa_persistent_linked_body_prepare(&d,draws,2,close,&body)==EBUSY&&!body&&f.creates==created);
  d.ops->destroy_bo(d.priv,close);close=NULL;
  CHECK(!remesa_persistent_scene_close_prepare(&d,1280,800,4,&close));
  CHECK(!remesa_persistent_linked_body_prepare(&d,draws,2,close,&body));
  struct remesa_persistent_linked_body_backing b;
  struct remesa_persistent_linked_state_backing a,z;
  CHECK(!remesa_persistent_linked_body_snapshot(&d,body,&b)&&b.dependency_count==3&&b.command_bytes==68);
  CHECK(!remesa_persistent_linked_state_snapshot(&d,state,&a));
  CHECK(!remesa_persistent_linked_state_snapshot(&d,other,&z));
  const uint8_t *bytes=get(&f,b.handle)->bytes;
  CHECK(shader_pds_le32(bytes)!=shader_pds_le32(bytes+28));
  CHECK(shader_pds_le32(bytes)==(0x40000000|((a.image.gpu_va+a.image.pds_offset)>>4)));
  CHECK(shader_pds_le32(bytes+28)==(0x40000000|((z.image.gpu_va+z.image.pds_offset)>>4)));
  d.ops->destroy_bo(d.priv,code);d.ops->destroy_bo(d.priv,geometry);d.ops->destroy_bo(d.priv,pds);
  d.ops->destroy_bo(d.priv,state);d.ops->destroy_bo(d.priv,other);d.ops->destroy_bo(d.priv,close);
  CHECK(!remesa_persistent_linked_body_snapshot(&d,body,&b));
  d.ops->destroy_bo(d.priv,body);
  CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden&&!remesa_persistent_close(&d));
 }
 const enum failure failures[]={FAIL_NONE,FAIL_CREATE,FAIL_PIN,FAIL_MAP,FAIL_READ_BODY,
  FAIL_READ_TAIL,FAIL_MAP_UNCERTAIN,FAIL_UNMAP_UNCERTAIN,FAIL_UNPIN};
 for(unsigned i=0;i<sizeof(failures)/sizeof(failures[0]);i++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  void *code=NULL,*geometry=NULL,*pds=NULL,*state=NULL,*close=NULL,*body=NULL;
  state_endpoint(&f,&d,2320+i,&code,&geometry,&pds);
  struct remesa_persistent_linked_state_request request=state_request(1280,800,0);
  CHECK(!remesa_persistent_linked_state_prepare(&d,pds,&request,4,&state));
  CHECK(!remesa_persistent_scene_close_prepare(&d,1280,800,4,&close));
  struct remesa_linked_body_draw draw={state,0,3};
  if(failures[i]==FAIL_NONE){
   draw.index_count=6;CHECK(remesa_persistent_linked_body_prepare(&d,&draw,1,close,&body)==ERANGE&&!body&&f.creates==5);
   draw.index_count=4;CHECK(remesa_persistent_linked_body_prepare(&d,&draw,1,close,&body)==ENOTSUP&&!body);
   draw.index_count=3;CHECK(remesa_persistent_linked_body_prepare(&d,&draw,17,close,&body)==EINVAL&&!body);
   CHECK(remesa_persistent_linked_body_prepare(&d,&draw,1,close,(void *)&draw)==EINVAL);
  }else if(failures[i]==FAIL_UNPIN){
   CHECK(!remesa_persistent_linked_body_prepare(&d,&draw,1,close,&body));f.fail=FAIL_UNPIN;d.ops->destroy_bo(d.priv,body);
  }else{f.fail=failures[i];CHECK(remesa_persistent_linked_body_prepare(&d,&draw,1,close,&body)!=0);}
  d.ops->destroy_bo(d.priv,code);d.ops->destroy_bo(d.priv,geometry);d.ops->destroy_bo(d.priv,pds);
  d.ops->destroy_bo(d.priv,state);d.ops->destroy_bo(d.priv,close);
  if(failures[i]==FAIL_MAP_UNCERTAIN||failures[i]==FAIL_UNMAP_UNCERTAIN||failures[i]==FAIL_UNPIN){
   CHECK(f.creates==6&&!f.destroys&&!f.unpins&&remesa_persistent_close(&d)==EBUSY);
  }else{f.fail=FAIL_NONE;CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!remesa_persistent_close(&d));}
  CHECK(!f.forbidden);
 }
 puts("PASS owned dynamic TA body: 1/16draw ordering, exact effective460/40bytes vs4096capacity, close12/noextraCODE, deduplicated shared dependency closure, leases, rollback and uncertain retention; GPU0/API0; initial bounds/clear absent");
 return 0;
}
