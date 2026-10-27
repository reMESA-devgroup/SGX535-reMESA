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
static int target_create(void *data,size_t n,uint32_t *id)
{
 if(n<=REMESA_SHADER_DRAW_SNAPSHOT_MAX_BYTES)return create(data,n,id);
 struct fixture *f=data;CHECK(n&&!(n&4095)&&n<=8u*1024*1024&&f->creates<64&&f->fail==FAIL_NONE);
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
  .va_start=0x20000000,.va_end=0x45000000,.byte_budget=32u*1024u*1024u,.resource_only_backend=true,
  .resource_limits={2048,2048,128,16u*1024u*1024u}};
 cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};
 cfg.placement[REMESA_VA_SURFACE]=(struct remesa_persistent_va_range){0x40001000,0x45000000,4096};
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
int main(void)
{
 CHECK(!ta_persistent_fixture());CHECK(!ta_linkage_fixture());
 const unsigned sizes[3][2]={{1280,800},{640,640},{641,479}};
 for(unsigned i=0;i<3;i++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};void *children[10]={0};
  unsigned w=sizes[i][0],h=sizes[i][1],pitch=(w+31u)&~31u;
  targets_setup(&f,&d,2600+i,w,h,children);void *binding=NULL,*second=NULL;
  struct remesa_persistent_render_target_backing color;
  struct remesa_persistent_depth_backing z;
  struct remesa_persistent_linked_ta_backing commands;
  CHECK(!remesa_persistent_render_target_snapshot(&d,children[8],w,h,pitch,&color));
  CHECK(!remesa_persistent_depth_snapshot(&d,children[9],&z));
  CHECK(!remesa_persistent_linked_ta_snapshot(&d,children[7],&commands));
  unsigned char color_head[16],depth_head[16];
  memcpy(color_head,get(&f,color.handle)->bytes,16);memcpy(depth_head,get(&f,z.handle)->bytes,16);
  unsigned creates=f.creates,maps=f.cpu_maps;
  CHECK(!remesa_persistent_linked_targets_prepare(&d,children[7],children[8],children[9],pitch,&binding));
  CHECK(f.creates==creates+1&&f.cpu_maps==maps+2); /* Only new immutable control object touched. */
  struct remesa_persistent_linked_targets_backing b;
  CHECK(!remesa_persistent_linked_targets_snapshot(&d,binding,&b));
  CHECK(b.used_bytes==72&&b.color_data_offset==0&&b.ta_control_offset==16&&b.allocation_bytes==4096);
  CHECK(b.color_access==3&&b.depth_access==3&&b.commands.read_allocation_count==7);
  CHECK(!memcmp(&b.commands,&commands,sizeof(commands))&&!memcmp(&b.color,&color,sizeof(color))&&
   !memcmp(&b.depth,&z,sizeof(z)));
  const uint8_t *bytes=get(&f,b.handle)->bytes;
  CHECK(!memcmp(bytes,color.state.target_data,16));
  uint8_t control[56];const struct remesa_raster_object_reference r={commands.gpu_va,commands.allocation_bytes,0,68};
  CHECK(!remesa_ta_control_encode(&r,control)&&!memcmp(bytes+16,control,56));
  for(unsigned n=72;n<4096;n++)CHECK(!bytes[n]);
  CHECK(!memcmp(color_head,get(&f,color.handle)->bytes,16)&&!memcmp(depth_head,get(&f,z.handle)->bytes,16));
  void *cpu=NULL;CHECK(d.ops->map(d.priv,children[8],false,&cpu)==EAGAIN&&!cpu);
  CHECK(d.ops->map(d.priv,children[9],false,&cpu)==EAGAIN&&!cpu); /* Undefined stays undefined. */
  maps=f.cpu_maps;for(unsigned n=0;n<100;n++)CHECK(!remesa_persistent_linked_targets_snapshot(&d,binding,&b));
  CHECK(f.cpu_maps==maps);
  CHECK(!remesa_persistent_linked_targets_prepare(&d,children[7],children[8],children[9],pitch,&second));
  targets_drop_children(&d,children);
  CHECK(!f.destroys&&!remesa_persistent_linked_targets_snapshot(&d,binding,&b));
  CHECK(d.ops->map(d.priv,binding,true,&cpu)==ENOTSUP&&!cpu);
  CHECK(!d.ops->map(d.priv,binding,false,&cpu));d.ops->destroy_bo(d.priv,binding);
  CHECK(!f.destroys&&remesa_persistent_close(&d)==EBUSY);d.ops->unmap(d.priv,binding,false);
  CHECK(f.destroys==1&&!remesa_persistent_linked_targets_snapshot(&d,second,&b));
  d.ops->destroy_bo(d.priv,second);
  CHECK(f.creates==12&&f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden&&!remesa_persistent_close(&d));
 }
 /* Input admission is based on logical COLOR extent and actual typed Z owner. */
 {
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};void *c[10]={0},*b=NULL,*wrong=NULL;
  targets_setup(&f,&d,2610,1280,800,c);unsigned before=f.creates;
  CHECK(remesa_persistent_linked_targets_prepare(&d,c[7],c[8],c[9],1279,&b)==EINVAL&&!b&&f.creates==before);
  CHECK(remesa_persistent_linked_targets_prepare(&d,c[7],c[8],c[9],1281,&b)==ENOTSUP&&!b&&f.creates==before);
  CHECK(remesa_persistent_linked_targets_prepare(&d,c[7],c[8],c[8],1280,&b)==EINVAL&&!b&&f.creates==before);
  CHECK(!remesa_persistent_allocate_placed(&d,4096,REMESA_VA_SURFACE,&wrong));before=f.creates;
  CHECK(remesa_persistent_linked_targets_prepare(&d,c[7],wrong,c[9],1280,&b)==ENOSPC&&!b&&f.creates==before);
  d.ops->destroy_bo(d.priv,wrong);wrong=NULL;
  CHECK(!remesa_persistent_depth_allocate(&d,640,640,4,&wrong));before=f.creates;
  CHECK(remesa_persistent_linked_targets_prepare(&d,c[7],c[8],wrong,1280,&b)==EINVAL&&!b&&f.creates==before);
  d.ops->destroy_bo(d.priv,wrong);
  void *cpu=NULL;CHECK(!d.ops->map(d.priv,c[8],true,&cpu));
  CHECK(remesa_persistent_linked_targets_prepare(&d,c[7],c[8],c[9],1280,&b)==EBUSY&&!b&&f.creates==before);
  d.ops->unmap(d.priv,c[8],true);
  targets_drop_children(&d,c);CHECK(f.creates==f.destroys&&!f.forbidden&&!remesa_persistent_close(&d));
 }
 const enum failure failures[]={FAIL_CREATE,FAIL_PIN,FAIL_MAP,FAIL_READ_BODY,FAIL_READ_TAIL,
  FAIL_MAP_UNCERTAIN,FAIL_UNMAP_UNCERTAIN,FAIL_UNPIN};
 for(unsigned i=0;i<sizeof(failures)/sizeof(failures[0]);i++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};void *c[10]={0},*binding=NULL;
  targets_setup(&f,&d,2620+i,640,640,c);
  if(failures[i]==FAIL_UNPIN){CHECK(!remesa_persistent_linked_targets_prepare(&d,c[7],c[8],c[9],640,&binding));
   f.fail=FAIL_UNPIN;d.ops->destroy_bo(d.priv,binding);
  }else{f.fail=failures[i];CHECK(remesa_persistent_linked_targets_prepare(&d,c[7],c[8],c[9],640,&binding)!=0);}
  targets_drop_children(&d,c);
  if(failures[i]==FAIL_MAP_UNCERTAIN||failures[i]==FAIL_UNMAP_UNCERTAIN||failures[i]==FAIL_UNPIN){
   CHECK(f.creates==11&&!f.destroys&&!f.unpins&&remesa_persistent_close(&d)==EBUSY);
  }else{f.fail=FAIL_NONE;CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!remesa_persistent_close(&d));}
  CHECK(!f.forbidden);
 }
 puts("PASS owned COLOR/Z binding: actual allocation limits/dimensions/pitch, targetDATA16+TAcontrol56, explicit READ|WRITE permissions, undefined contents preserved, TA/read-owner closure unchanged, shared reference/lease lifetime and rollback/quarantine; GPU0/API0/raster0");return 0;
}
