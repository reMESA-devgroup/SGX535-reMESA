/* SPDX-License-Identifier: MIT. Owned combined TA and read allocation closure. */
int ta_persistent_fixture(void);
#define main ta_persistent_fixture
#include "test_persistent_shader.c"
#undef main
int ta_linkage_fixture(void);
#define main ta_linkage_fixture
#include "test_shader_linkage.c"
#undef main
/* Existing small-object fixture intentionally caps at1MiB. Only the new
 * attachment allocations use this larger fake backing; failure injection for
 * the 4KiB immutable binding continues through the original callbacks. */
#ifndef REMESA_LINKED_TEST_CLEAR_DEPTH
#define REMESA_LINKED_TEST_CLEAR_DEPTH(context) 1.0f
#endif
#ifndef REMESA_LINKED_TEST_BYTES
#define REMESA_LINKED_TEST_BYTES (32u*1024u*1024u)
#endif
#ifndef REMESA_LINKED_TEST_RASTER_END
#define REMESA_LINKED_TEST_RASTER_END 0x30010000
#endif
#ifndef REMESA_LINKED_TEST_RASTER_ALIGNMENT
#define REMESA_LINKED_TEST_RASTER_ALIGNMENT 4096
#endif
#ifndef REMESA_LINKED_TEST_ALLOCATION_MAX
#define REMESA_LINKED_TEST_ALLOCATION_MAX (8u*1024u*1024u)
#endif
static int target_create(void *data,size_t n,uint32_t *id)
{
 if(n<=REMESA_SHADER_DRAW_SNAPSHOT_MAX_BYTES)return create(data,n,id);
 struct fixture *f=data;CHECK(n&&!(n&4095)&&n<=REMESA_LINKED_TEST_ALLOCATION_MAX&&f->creates<64&&f->fail==FAIL_NONE);
 struct backing *b=&f->backing[f->creates];b->bytes=malloc(n);CHECK(b->bytes);
 memset(b->bytes,0xa5,n);b->length=n;b->live=true;*id=++f->creates;return 0;
}
static struct remesa_persistent_ops target_ops;
static void ta_setup(struct fixture *f,struct remesa_device *d,unsigned context,
 unsigned width,unsigned height,void **code,void **geometry,void **pds,void **state,
 void **close,void **body,void **prefix)
{
 target_ops=ops;target_ops.create=target_create;
 struct remesa_persistent_config cfg={.ops=&target_ops,.priv=f,.context=context,
  .va_start=0x20000000,.va_end=0x45000000,.byte_budget=REMESA_LINKED_TEST_BYTES,.resource_only_backend=true,
  .resource_limits={2048,2048,128,16u*1024u*1024u}};
 cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};
 cfg.placement[REMESA_VA_RASTER]=(struct remesa_persistent_va_range){0x30000000,REMESA_LINKED_TEST_RASTER_END,REMESA_LINKED_TEST_RASTER_ALIGNMENT};
 cfg.placement[REMESA_VA_SURFACE]=(struct remesa_persistent_va_range){0x40001000,0x45000000,4096};
 CHECK(!remesa_persistent_open(d,&cfg));
#ifdef REMESA_LINKED_TEST_NATIVE_FILL
 if(REMESA_LINKED_TEST_NATIVE_FILL)CHECK(!remesa_persistent_set_native_fill(d,REMESA_LINKED_TEST_NATIVE_FILL));
#endif
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
 CHECK(!remesa_persistent_zclear_prefix_prepare(d,width,height,REMESA_LINKED_TEST_CLEAR_DEPTH(context),4,3,4,prefix));
}
static void ta_drop_children(struct remesa_device *d,void *code,void *geometry,void *pds,
 void *state,void *close,void *body,void *prefix)
{
 void *objects[]={code,geometry,pds,state,close,body,prefix};
 for(unsigned n=0;n<7;n++)d->ops->destroy_bo(d->priv,objects[n]);
}
static void targets_setup(struct fixture *f,struct remesa_device *d,unsigned context,
 unsigned width,unsigned height,void *children[10])
{
 ta_setup(f,d,context,width,height,&children[0],&children[1],&children[2],&children[3],
  &children[4],&children[5],&children[6]);
 CHECK(!remesa_persistent_linked_ta_prepare(d,children[6],children[5],&children[7]));
 unsigned pitch=(width+31u)&~31u;
 CHECK(!remesa_persistent_allocate_placed(d,(size_t)pitch*height*4,REMESA_VA_SURFACE,&children[8]));
 CHECK(!remesa_persistent_depth_allocate(d,width,height,4,&children[9]));
}
static void targets_drop_children(struct remesa_device *d,void *children[10])
{for(unsigned n=0;n<10;n++)d->ops->destroy_bo(d->priv,children[n]);}
static unsigned background_fail_unpin_handle;
static int background_unpin(void *data,uint32_t id)
{return id==background_fail_unpin_handle?EOWNERDEAD:unpin(data,id);}
static void bg_setup(struct fixture *f,struct remesa_device *d,unsigned ctx,unsigned w,unsigned h,
 void *children[11])
{
 targets_setup(f,d,ctx,w,h,children);
 CHECK(!remesa_persistent_linked_targets_prepare(d,children[7],children[8],children[9],(w+31u)&~31u,&children[10]));
}
static void bg_drop_children(struct remesa_device *d,void *children[11])
{for(unsigned n=0;n<11;n++)d->ops->destroy_bo(d->priv,children[n]);}
#ifndef REMESA_LINKED_BACKGROUND_FIXTURE
int main(void)
{
 CHECK(!ta_persistent_fixture());CHECK(!ta_linkage_fixture());
 const unsigned sizes[3][2]={{1280,800},{640,640},{641,479}};
 for(unsigned i=0;i<3;i++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};void *c[11]={0};
  unsigned w=sizes[i][0],h=sizes[i][1];bg_setup(&f,&d,2700+i,w,h,c);
  void *bg=NULL,*second=NULL;unsigned maps=f.cpu_maps;
  CHECK(!remesa_persistent_linked_background_prepare(&d,c[10],3,&bg)&&bg&&f.creates==13&&f.cpu_maps==maps+4);
  struct remesa_persistent_linked_background_backing b;
  CHECK(!remesa_persistent_linked_background_snapshot(&d,bg,&b));
  CHECK(b.gpu_va==0x30000000&&b.object_bytes==68&&b.program_used_bytes==100&&
   b.primary_offset==32&&b.primary_bytes==64&&b.secondary_offset==96&&b.secondary_bytes==4&&b.use_bytes==8);
  CHECK(b.targets.commands.width==w&&b.targets.commands.height==h&&b.entry.bank==3);
  CHECK(b.program_handle!=b.handle&&b.program_gpu_va>=0x20080000&&b.program_gpu_va<0x20090000);
  const uint8_t *object=get(&f,b.handle)->bytes,*program=get(&f,b.program_handle)->bytes;
  uint8_t use[8];size_t n=0;const struct remesa_usse_cpu_move move={.source={2,0},.destination={1,0},
   .control12=1,.control23=1,.control18=1};
  CHECK(!remesa_usse_cpu_move_encode(&move,use,8,&n)&&n==8&&!memcmp(program,use,8));
  /* Exact retained physical role1[0:8], generated rather than transplanted. */
  const uint8_t original_use[8]={0,0,0,0xa0,1,0x10,0x85,0x28};
  CHECK(!memcmp(program,original_use,8));
  uint32_t fixup;CHECK(!remesa_use_entry_preview_fixup(&b.entry,&fixup));
  uint8_t pds[64],known[64];const struct remesa_pds_background_request request={b.targets.color.gpu_va,fixup,4,w,h};
  CHECK(!remesa_pds_structure_encode_background(&request,pds,known,64,&n)&&n==64&&
   !memcmp(pds,program+32,64)&&!memcmp(known,b.primary_known_mask,64));
  CHECK(shader_pds_le32(program+44)==(0x6c000000|((w-1)<<12)|(h-1)));
  CHECK(shader_pds_le32(program+72)==b.targets.color.gpu_va&&shader_pds_le32(program+96)==0xaf000000);
  CHECK(shader_pds_le32(object)==((b.program_gpu_va+96)>>4&0x00ffffff));
  CHECK(shader_pds_le32(object+8)==(0x0c000000|((b.program_gpu_va+32)>>4&0x00ffffff)));
  uint8_t geometry[24];const struct remesa_background_geometry_request g={w,h,1};
  CHECK(!remesa_background_geometry_encode(&g,geometry)&&!memcmp(object+44,geometry,24));
  if(!i){
   /* Retained qualified1280x800 role0 primary4560. DATA0 is the only
    * relocated USE word; COLOR is the same first surface VA40001000. */
   const uint32_t original_pds[16]={0x101203,0,0x1e0092,0x6c4ff31f,0,0,0,0,
    0x20,0xf800,0x40001000,0,0x07000345,0x070418a2,0x07042364,0xaf000000};
   for(unsigned j=1;j<16;j++)CHECK(shader_pds_le32(program+32+j*4)==original_pds[j]);
   const uint32_t original_geometry[6]={0x40004000,0x3f800000,0x90004000,0x3f800000,0x40007200,0x3f800000};
   for(unsigned j=0;j<6;j++)CHECK(shader_pds_le32(object+44+j*4)==original_geometry[j]);
  }
  for(unsigned j=8;j<32;j++)CHECK(!program[j]);
  for(unsigned j=100;j<4096;j++)CHECK(!program[j]);
  for(unsigned j=68;j<4096;j++)CHECK(!object[j]);
  maps=f.cpu_maps;for(unsigned j=0;j<100;j++)CHECK(!remesa_persistent_linked_background_snapshot(&d,bg,&b));
  CHECK(f.cpu_maps==maps);
  void *cpu=NULL;CHECK(d.ops->map(d.priv,c[8],false,&cpu)==EAGAIN&&!cpu); /* No background CPU render. */
  CHECK(!remesa_persistent_linked_background_prepare(&d,c[10],3,&second));
  bg_drop_children(&d,c);CHECK(!f.destroys&&!remesa_persistent_linked_background_snapshot(&d,bg,&b));
  CHECK(d.ops->map(d.priv,bg,true,&cpu)==ENOTSUP&&!cpu);
  CHECK(!d.ops->map(d.priv,bg,false,&cpu));d.ops->destroy_bo(d.priv,bg);
  CHECK(!f.destroys&&remesa_persistent_close(&d)==EBUSY);d.ops->unmap(d.priv,bg,false);
  CHECK(f.destroys==2&&!remesa_persistent_linked_background_snapshot(&d,second,&b));
  d.ops->destroy_bo(d.priv,second);
  CHECK(f.creates==15&&f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden&&!remesa_persistent_close(&d));
 }
 /* Bank conflicts unwind both tentative allocations, without touching targets. */
 {
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};void *c[11]={0},*bg=NULL;
  bg_setup(&f,&d,2710,640,640,c);unsigned before=f.creates;
  CHECK(remesa_persistent_linked_background_prepare(&d,c[10],4,&bg)==EBUSY&&!bg);
  CHECK(f.creates==before+2&&f.destroys==2);
  before=f.creates;CHECK(remesa_persistent_linked_background_prepare(&d,c[10],16,&bg)!=0&&!bg&&f.creates==before);
  bg_drop_children(&d,c);CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden&&!remesa_persistent_close(&d));
 }
 const enum failure failures[]={FAIL_CREATE,FAIL_PIN,FAIL_MAP,FAIL_READ_BODY,FAIL_READ_TAIL,
  FAIL_MAP_UNCERTAIN,FAIL_UNMAP_UNCERTAIN,FAIL_UNPIN};
 for(unsigned i=0;i<sizeof(failures)/sizeof(failures[0]);i++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};void *c[11]={0},*bg=NULL;
  bg_setup(&f,&d,2720+i,640,640,c);
  if(failures[i]==FAIL_UNPIN){CHECK(!remesa_persistent_linked_background_prepare(&d,c[10],3,&bg));
   f.fail=FAIL_UNPIN;d.ops->destroy_bo(d.priv,bg);
  }else{f.fail=failures[i];CHECK(remesa_persistent_linked_background_prepare(&d,c[10],3,&bg)!=0);}
  bg_drop_children(&d,c);
  if(failures[i]==FAIL_MAP_UNCERTAIN||failures[i]==FAIL_UNMAP_UNCERTAIN||failures[i]==FAIL_UNPIN){
   CHECK(f.creates==13&&!f.destroys&&!f.unpins&&remesa_persistent_close(&d)==EBUSY);
  }else{f.fail=FAIL_NONE;CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!remesa_persistent_close(&d));}
  CHECK(!f.forbidden);
 }
 /* Failure while uploading the parent after program publication rolls back
  * both, whereas an uncertain parent lease retains the published program. */
 for(unsigned uncertain=0;uncertain<2;uncertain++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};void *c[11]={0},*bg=NULL;
  bg_setup(&f,&d,2740+uncertain,640,640,c);
  f.fail=uncertain?FAIL_MAP_UNCERTAIN:FAIL_MAP;f.fail_map_handle=12;
  CHECK(remesa_persistent_linked_background_prepare(&d,c[10],3,&bg)!=0);
  bg_drop_children(&d,c);
  if(uncertain){CHECK(bg&&!f.destroys&&!f.unpins&&remesa_persistent_close(&d)==EBUSY);}
  else{f.fail=FAIL_NONE;CHECK(!bg&&f.creates==f.destroys&&!remesa_persistent_close(&d));}
 }
 /* Parent disposal succeeds but program unpin is uncertain. That surviving
  * program must independently retain the complete targets/TA/COLOR/Z chain. */
 for(unsigned rollback=0;rollback<2;rollback++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};void *c[11]={0},*bg=NULL;
  bg_setup(&f,&d,2750+rollback,640,640,c);
  target_ops.unpin=background_unpin;background_fail_unpin_handle=13;
  if(rollback){
   f.fail=FAIL_READ_BODY;
   CHECK(remesa_persistent_linked_background_prepare(&d,c[10],3,&bg)==EOWNERDEAD&&!bg);
  }else{
   CHECK(!remesa_persistent_linked_background_prepare(&d,c[10],3,&bg));
   d.ops->destroy_bo(d.priv,bg);
  }
  bg_drop_children(&d,c);
  CHECK(f.creates==13&&f.destroys==1&&f.unpins==1&&!f.forbidden);
  CHECK(get(&f,13)->live&&get(&f,13)->pinned&&get(&f,11)->pinned&&get(&f,9)->pinned&&get(&f,10)->pinned);
  CHECK(remesa_persistent_close(&d)==EBUSY);
  background_fail_unpin_handle=0;
 }
 puts("PASS owned background: generated USE8/PDS64/secondary4/object68, retained physical USE/PDS/geometry oracle comparison, exact logical COLOR extents and depth1, RASTER/PROGRAM domains, matching pixel bank, shared/transitive lifetime and ordinary/uncertain upload/program-disposal retention; GPU0/API0/event0");return 0;
}
#endif
