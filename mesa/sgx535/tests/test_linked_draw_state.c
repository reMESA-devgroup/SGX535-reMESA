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
 /* Retained original WHITE64 witness's state-use14.bin and F41 fields;
  * only the two fragment references are relocated to these live owners. */
 const unsigned char original_use14[16]={0,0,0,0xa0,1,0xd0,0xa1,0x28,0,7,0x20,0xa0,0,0x40,0x27,0xfb};
 for(unsigned size=0;size<3;size++)for(unsigned matrix=0;matrix<2;matrix++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  void *code=NULL,*geometry=NULL,*pds=NULL,*state=NULL;
  state_endpoint(&f,&d,2100+size*2+matrix,&code,&geometry,&pds);
  struct remesa_persistent_linked_pds_backing pd;
  CHECK(!remesa_persistent_linked_pds_snapshot(&d,pds,&pd));
  CHECK(pd.object_offset[2]==256&&pd.image_bytes[2]==4&&pd.reserved_bytes[2]==32);
  CHECK(shader_pds_le32(get(&f,pd.handle)->bytes+256)==0xaf000000);
  for(unsigned n=256;n<260;n++)CHECK(pd.known_mask[n]==255);
  for(unsigned n=260;n<288;n++)CHECK(!pd.known_mask[n]&&!get(&f,pd.handle)->bytes[n]);
  struct remesa_persistent_linked_state_request request=state_request(sizes[size][0],sizes[size][1],matrix);
  CHECK(!remesa_persistent_linked_state_prepare(&d,pds,&request,4,&state)&&state);
  struct remesa_persistent_linked_state_backing s;
  CHECK(!remesa_persistent_linked_state_snapshot(&d,state,&s));
  CHECK(s.image.state_offset==0&&s.image.state_bytes==56&&s.image.use_offset==64&&s.image.use_bytes==16&&
   s.image.pds_offset==96&&s.image.pds_bytes==60&&s.image.ta_offset==160&&s.image.ta_bytes==8);
  CHECK(s.dependencies.handle==pd.handle&&s.dependencies.program.handle==pd.program.handle&&
   s.dependencies.geometry.handle==pd.geometry.handle);
  unsigned char *bytes=get(&f,s.image.handle)->bytes;
  CHECK(!memcmp(bytes+64,original_use14,16));
  uint32_t expected[14]={0xf41,0x01c00000,0,0x30030001,0,0,0,0,0,0,0,0,0x08001800,0x358637bd};
  expected[2]=(uint32_t)((pd.gpu_va+256)>>4)&0x00ffffff;
  expected[4]=0x0c000000|((uint32_t)((pd.gpu_va+192)>>4)&0x00ffffff);
  if(matrix){
   unsigned char view[28];CHECK(!remesa_ta_window_viewport_encode(&request.viewport,view));
   for(unsigned n=0;n<6;n++)expected[5+n]=shader_pds_le32(view+4+4*n);
   CHECK(expected[6]&&expected[8]);
  }
  for(unsigned n=0;n<14;n++)CHECK(shader_pds_le32(bytes+4*n)==expected[n]);
  CHECK(shader_pds_le32(bytes+160)==(UINT32_C(0x40000000)|((s.image.gpu_va+96)>>4)));
  CHECK(shader_pds_le32(bytes+164)==0x0c02a204);
  for(unsigned n=56;n<4096;n++)if((n<64||n>=80)&&(n<96||n>=156)&&(n<160||n>=168))CHECK(!bytes[n]);
  unsigned maps=f.cpu_maps,reads=f.reads,writes=f.writes;
  for(unsigned n=0;n<100;n++)CHECK(!remesa_persistent_linked_state_snapshot(&d,state,&s));
  CHECK(maps==f.cpu_maps&&reads==f.reads&&writes==f.writes);
  void *cpu=NULL;CHECK(d.ops->map(d.priv,state,true,&cpu)==ENOTSUP&&!cpu);
  d.ops->destroy_bo(d.priv,code);d.ops->destroy_bo(d.priv,geometry);d.ops->destroy_bo(d.priv,pds);
  CHECK(!f.destroys&&!remesa_persistent_linked_state_snapshot(&d,state,&s));
  CHECK(!d.ops->map(d.priv,state,false,&cpu)&&cpu);d.ops->destroy_bo(d.priv,state);
  CHECK(!f.destroys&&remesa_persistent_close(&d)==EBUSY);d.ops->unmap(d.priv,state,false);
  CHECK(f.creates==4&&f.destroys==4&&f.pins==f.unpins&&!f.forbidden);
  CHECK(!remesa_persistent_close(&d));
 }
 const enum failure failure[]={FAIL_NONE,FAIL_CREATE,FAIL_PIN,FAIL_MAP,FAIL_READ_BODY,FAIL_READ_TAIL,
  FAIL_MAP_UNCERTAIN,FAIL_UNMAP_UNCERTAIN,FAIL_UNPIN};
 for(unsigned i=0;i<sizeof(failure)/sizeof(failure[0]);i++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  void *code=NULL,*geometry=NULL,*pds=NULL,*state=NULL;
  state_endpoint(&f,&d,2120+i,&code,&geometry,&pds);
  struct remesa_persistent_linked_state_request request=state_request(1280,800,0);
  if(failure[i]==FAIL_NONE){
   request.viewport.width=1281;
   CHECK(remesa_persistent_linked_state_prepare(&d,pds,&request,4,&state)==EINVAL&&!state&&f.creates==3);
  }else if(failure[i]==FAIL_UNPIN){
   CHECK(!remesa_persistent_linked_state_prepare(&d,pds,&request,4,&state));f.fail=FAIL_UNPIN;
   d.ops->destroy_bo(d.priv,state);
  }else{
   f.fail=failure[i];CHECK(remesa_persistent_linked_state_prepare(&d,pds,&request,4,&state)!=0);
  }
  d.ops->destroy_bo(d.priv,code);d.ops->destroy_bo(d.priv,geometry);d.ops->destroy_bo(d.priv,pds);
  if(failure[i]==FAIL_MAP_UNCERTAIN||failure[i]==FAIL_UNMAP_UNCERTAIN||failure[i]==FAIL_UNPIN){
   CHECK(f.creates==4&&!f.destroys&&!f.unpins&&!f.forbidden);
   CHECK(remesa_persistent_close(&d)==EBUSY); /* Intentional fake-only quarantine. */
  }else{
   f.fail=FAIL_NONE;CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);
   CHECK(!remesa_persistent_close(&d));
  }
 }
 puts("PASS owned linked draw state: 14DWORD ISP/fragment/viewport/scalar + generated USE16/PDS60/TA8; retained original WHITE64 state oracle, three size storage profiles, INITIAL_ZERO/MATRIX_WINDOW encoding, transitive closure/lease lifetime and failure quarantine; GPU0/API0");
 return 0;
}
