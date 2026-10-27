/* SPDX-License-Identifier: MIT. Owned combined TA and read allocation closure. */
int ta_persistent_fixture(void);
#define main ta_persistent_fixture
#include "test_persistent_shader.c"
#undef main
int ta_linkage_fixture(void);
#define main ta_linkage_fixture
#include "test_shader_linkage.c"
#undef main
static void ta_setup(struct fixture *f,struct remesa_device *d,unsigned context,
 unsigned width,unsigned height,void **code,void **geometry,void **pds,void **state,
 void **close,void **body,void **prefix)
{
 struct remesa_persistent_config cfg={.ops=&ops,.priv=f,.context=context,
  .va_start=0x20000000,.va_end=0x20100000,.byte_budget=16*4096,.resource_only_backend=true};
 cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};
 CHECK(!remesa_persistent_open(d,&cfg));
 struct remesa_shader_program vs=vertex(2),fs=fragment(1);struct remesa_shader_linkage link;
 CHECK(!remesa_shader_linkage_prepare(&vs,&fs,&link));const uint32_t banks[2]={4,3};
 unsigned char raw[144];for(unsigned n=0;n<144;n++)raw[n]=(unsigned char)(n*43+7);
 const struct remesa_linked_geometry_input in={.vertices=raw,.vertex_bytes=144,.vertex_count=3};
 CHECK(!remesa_persistent_linked_program_prepare(d,&link,banks,code));
 CHECK(!remesa_persistent_linked_geometry_prepare(d,&in,geometry));
 CHECK(!remesa_persistent_linked_pds_prepare(d,*code,*geometry,pds));
 const struct remesa_persistent_linked_state_request request={
  .isp={.depth_bits_present=true,.depth_test=true,.depth_write=true,.compare=REMESA_DEPTH_LESS,.rgba_mask=15},
  .viewport={.framebuffer_width=width,.framebuffer_height=height,.width=width,.height=height,.depth_bits=24,
   .far_z=1,.path=REMESA_TA_VIEWPORT_INITIAL_ZERO}};
 CHECK(!remesa_persistent_linked_state_prepare(d,*pds,&request,4,state));
 CHECK(!remesa_persistent_scene_close_prepare(d,width,height,4,close));
 struct remesa_linked_body_draw draw={*state,0,3};
 CHECK(!remesa_persistent_linked_body_prepare(d,&draw,1,*close,body));
 CHECK(!remesa_persistent_zclear_prefix_prepare(d,width,height,1,4,3,4,prefix));
}
static void ta_drop_children(struct remesa_device *d,void *code,void *geometry,void *pds,
 void *state,void *close,void *body,void *prefix)
{
 void *objects[]={code,geometry,pds,state,close,body,prefix};
 for(unsigned n=0;n<7;n++)d->ops->destroy_bo(d->priv,objects[n]);
}
int main(void)
{
 CHECK(!ta_persistent_fixture());CHECK(!ta_linkage_fixture());
 const unsigned sizes[3][2]={{1280,800},{640,640},{641,479}};
 for(unsigned size=0;size<3;size++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  void *code=NULL,*geometry=NULL,*pds=NULL,*state=NULL,*close=NULL,*body=NULL,*prefix=NULL,*ta=NULL,*second=NULL;
  ta_setup(&f,&d,2500+size,sizes[size][0],sizes[size][1],&code,&geometry,&pds,&state,&close,&body,&prefix);
  struct remesa_persistent_zclear_prefix_backing p;struct remesa_persistent_linked_body_backing b;
  CHECK(!remesa_persistent_zclear_prefix_snapshot(&d,prefix,&p));
  CHECK(!remesa_persistent_linked_body_snapshot(&d,body,&b));
  CHECK(!remesa_persistent_linked_ta_prepare(&d,prefix,body,&ta));
  struct remesa_persistent_linked_ta_backing t;
  CHECK(!remesa_persistent_linked_ta_snapshot(&d,ta,&t));
  CHECK(t.width==sizes[size][0]&&t.height==sizes[size][1]&&t.draw_count==1&&t.command_bytes==68&&
   t.allocation_bytes==4096&&t.draw_offset==28&&t.close_offset==56&&t.read_allocation_count==7);
  const uint8_t *bytes=get(&f,t.handle)->bytes;
  CHECK(!memcmp(bytes,get(&f,p.handle)->bytes+p.ta_offset,28));
  CHECK(!memcmp(bytes+28,get(&f,b.handle)->bytes,b.command_bytes));
  CHECK(shader_pds_le32(bytes+64)==0xc0000000);
  for(unsigned n=68;n<4096;n++)CHECK(!bytes[n]);
  for(unsigned n=0;n<t.read_allocation_count;n++){
   CHECK(t.reads[n].handle&&t.reads[n].generation&&t.reads[n].gpu_va&&t.reads[n].access==REMESA_ACCESS_READ&&
    t.reads[n].allocation_bytes==4096&&t.reads[n].handle!=b.handle);
   CHECK(get(&f,t.reads[n].handle)->live&&get(&f,t.reads[n].handle)->pinned);
   for(unsigned j=0;j<n;j++)CHECK(t.reads[j].handle!=t.reads[n].handle&&
    t.reads[j].generation!=t.reads[n].generation&&t.reads[j].gpu_va!=t.reads[n].gpu_va);
  }
  unsigned maps=f.cpu_maps;for(unsigned n=0;n<100;n++)CHECK(!remesa_persistent_linked_ta_snapshot(&d,ta,&t));
  CHECK(maps==f.cpu_maps);CHECK(!remesa_persistent_linked_ta_prepare(&d,prefix,body,&second));
  ta_drop_children(&d,code,geometry,pds,state,close,body,prefix);
  CHECK(!f.destroys&&!remesa_persistent_linked_ta_snapshot(&d,ta,&t));
  void *cpu=NULL;CHECK(d.ops->map(d.priv,ta,true,&cpu)==ENOTSUP&&!cpu);
  CHECK(!d.ops->map(d.priv,ta,false,&cpu)&&cpu);d.ops->destroy_bo(d.priv,ta);
  CHECK(!f.destroys&&remesa_persistent_close(&d)==EBUSY);d.ops->unmap(d.priv,ta,false);
  CHECK(f.destroys==1&&!remesa_persistent_linked_ta_snapshot(&d,second,&t));
  d.ops->destroy_bo(d.priv,second);
  CHECK(f.creates==9&&f.destroys==9&&f.pins==f.unpins&&!f.forbidden&&!remesa_persistent_close(&d));
 }
 const enum failure failures[]={FAIL_CREATE,FAIL_PIN,FAIL_MAP,FAIL_READ_BODY,FAIL_READ_TAIL,
  FAIL_MAP_UNCERTAIN,FAIL_UNMAP_UNCERTAIN,FAIL_UNPIN};
 for(unsigned n=0;n<sizeof(failures)/sizeof(failures[0]);n++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  void *code=NULL,*geometry=NULL,*pds=NULL,*state=NULL,*close=NULL,*body=NULL,*prefix=NULL,*ta=NULL;
  ta_setup(&f,&d,2520+n,1280,800,&code,&geometry,&pds,&state,&close,&body,&prefix);
  if(failures[n]==FAIL_UNPIN){CHECK(!remesa_persistent_linked_ta_prepare(&d,prefix,body,&ta));
   f.fail=FAIL_UNPIN;d.ops->destroy_bo(d.priv,ta);
  }else{f.fail=failures[n];CHECK(remesa_persistent_linked_ta_prepare(&d,prefix,body,&ta)!=0);}
  ta_drop_children(&d,code,geometry,pds,state,close,body,prefix);
  if(failures[n]==FAIL_MAP_UNCERTAIN||failures[n]==FAIL_UNMAP_UNCERTAIN||failures[n]==FAIL_UNPIN){
   CHECK(f.creates==8&&!f.destroys&&!f.unpins&&remesa_persistent_close(&d)==EBUSY);
  }else{f.fail=FAIL_NONE;CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!remesa_persistent_close(&d));}
  CHECK(!f.forbidden);
 }
 /* Cross-component dimensions/banks/owner identity are checked before upload. */
 {
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  void *code=NULL,*geometry=NULL,*pds=NULL,*state=NULL,*close=NULL,*body=NULL,*prefix=NULL,*bad=NULL,*ta=NULL;
  ta_setup(&f,&d,2540,1280,800,&code,&geometry,&pds,&state,&close,&body,&prefix);
  CHECK(!remesa_persistent_zclear_prefix_prepare(&d,640,640,1,4,3,4,&bad));
  unsigned before=f.creates;CHECK(remesa_persistent_linked_ta_prepare(&d,bad,body,&ta)==EINVAL&&!ta&&f.creates==before);
  d.ops->destroy_bo(d.priv,bad);bad=NULL;
  CHECK(!remesa_persistent_zclear_prefix_prepare(&d,1280,800,1,3,4,3,&bad));
  before=f.creates;CHECK(remesa_persistent_linked_ta_prepare(&d,bad,body,&ta)==EBUSY&&!ta&&f.creates==before);
  CHECK(remesa_persistent_linked_ta_prepare(&d,body,prefix,&ta)==EINVAL&&!ta&&f.creates==before);
  struct remesa_persistent_linked_ta_backing unchanged,saved;memset(&unchanged,0xa5,sizeof(unchanged));saved=unchanged;
  CHECK(remesa_persistent_linked_ta_snapshot(&d,body,&unchanged)==EINVAL&&!memcmp(&unchanged,&saved,sizeof(saved)));
  d.ops->destroy_bo(d.priv,bad);ta_drop_children(&d,code,geometry,pds,state,close,body,prefix);
  CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden&&!remesa_persistent_close(&d));
 }
 puts("PASS owned combined TA: clear28+body40=68 effective bytes, exact ordering, unique seven-allocation READ closure excluding copied body storage, dimensions/bank/owner gates, no-IO snapshots, shared transitive lease lifetime, rollback and uncertain retention; GPU0/API0/attachments0");return 0;
}
