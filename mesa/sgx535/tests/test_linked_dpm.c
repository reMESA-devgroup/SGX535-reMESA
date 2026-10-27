/* SPDX-License-Identifier: MIT. DPM owners and CPU plan, no MMIO/GPU execution. */
int persistent_fixture(void);
#define main persistent_fixture
#include "test_persistent_shader.c"
#undef main
static int dpm_create(void *data,size_t n,uint32_t *id)
{
 if(n<=REMESA_SHADER_DRAW_SNAPSHOT_MAX_BYTES)return create(data,n,id);
 struct fixture *f=data;CHECK(n==0x2000000&&f->creates<64&&f->fail==FAIL_NONE);
 struct backing *b=&f->backing[f->creates];b->bytes=malloc(n);CHECK(b->bytes);
 memset(b->bytes,0xa5,n);b->length=n;b->live=true;*id=++f->creates;return 0;
}
static struct remesa_persistent_ops dpm_ops;
static void setup(struct fixture *f,struct remesa_device *d,unsigned context,uint32_t tv,uint32_t pv,void **table,void **parameter)
{
 dpm_ops=ops;dpm_ops.create=dpm_create;
 struct remesa_persistent_config cfg={.ops=&dpm_ops,.priv=f,.context=context,
  .va_start=0x20000000,.va_end=0x48000000,.byte_budget=96u*1024u*1024u,.resource_only_backend=true};
 cfg.placement[REMESA_VA_RASTER]=(struct remesa_persistent_va_range){pv,0x40000000,1048576};
 cfg.placement[REMESA_VA_SURFACE]=(struct remesa_persistent_va_range){tv,0x48000000,4096};
 CHECK(!remesa_persistent_open(d,&cfg));
 CHECK(!remesa_persistent_allocate_placed(d,0x2000000,REMESA_VA_SURFACE,table));
 CHECK(!remesa_persistent_allocate_placed(d,0x2000000,REMESA_VA_RASTER,parameter));
}
static uint32_t word(void){uint32_t v;CHECK(scanf("%u",&v)==1);return v;}
static uint32_t le(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void unchanged_storage(struct fixture *f)
{
 for(unsigned id=1;id<=2;id++){
  const struct backing *b=get(f,id);const unsigned at[]={0,15,4095,4096,8192,0x1ffffff};
  for(unsigned j=0;j<sizeof(at)/sizeof(at[0]);j++)CHECK(b->bytes[at[j]]==0xa5);
 }
}
int main(void)
{
 CHECK(!persistent_fixture());
 for(unsigned variant=0;variant<2;variant++){
  uint32_t tv=word(),pv=word(),cookie[13],pairs[44];for(unsigned i=0;i<13;i++)cookie[i]=word();for(unsigned i=0;i<44;i++)pairs[i]=word();
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};void *t=NULL,*p=NULL,*control=NULL,*second=NULL;
  setup(&f,&d,2900+variant,tv,pv,&t,&p);unsigned maps=f.cpu_maps;
  CHECK(!remesa_persistent_linked_dpm_prepare(&d,t,p,&control)&&control&&f.creates==3&&f.cpu_maps==maps+2);
  struct remesa_persistent_linked_dpm_backing b;
  CHECK(!remesa_persistent_linked_dpm_snapshot(&d,control,&b));
  CHECK(b.data_bytes==240&&b.allocation_bytes==4096&&b.table.gpu_va==tv&&b.parameter.gpu_va==pv);
  CHECK(b.table.logical_bytes==0x2000000&&b.parameter.allocation_bytes==0x2000000);
  CHECK(b.table_access==(REMESA_ACCESS_READ|REMESA_ACCESS_WRITE)&&b.parameter_access==b.table_access);
  CHECK(!memcmp(b.plan.cookie,cookie,sizeof(cookie))&&!memcmp(b.plan.load_pairs,pairs,sizeof(pairs)));
  CHECK(b.plan.info_bytes==0x620000&&b.plan.original_wait_mask==7&&b.plan.retained_completion_mask==15);
  const uint8_t *bytes=get(&f,b.handle)->bytes;
  for(unsigned i=0;i<13;i++)CHECK(le(bytes+i*4)==cookie[i]);
  CHECK(le(bytes+52)==0x620000);for(unsigned i=0;i<44;i++)CHECK(le(bytes+56+i*4)==pairs[i]);
  CHECK(le(bytes+232)==7&&le(bytes+236)==15);for(unsigned i=240;i<4096;i++)CHECK(!bytes[i]);
  unchanged_storage(&f);maps=f.cpu_maps;
  for(unsigned i=0;i<100;i++)CHECK(!remesa_persistent_linked_dpm_snapshot(&d,control,&b));
  CHECK(f.cpu_maps==maps);void *cpu=NULL;CHECK(d.ops->map(d.priv,control,true,&cpu)==ENOTSUP&&!cpu);
  CHECK(d.ops->map(d.priv,t,false,&cpu)==EAGAIN&&!cpu);
  CHECK(remesa_persistent_linked_dpm_snapshot(&d,control,control)==EINVAL);
  struct remesa_persistent_linked_dpm_backing saved=b;
  CHECK(remesa_persistent_linked_dpm_snapshot(&d,control,(void *)((uint8_t *)&b+1))==EINVAL&&!memcmp(&b,&saved,sizeof(b)));
  CHECK(!d.ops->map(d.priv,t,true,&cpu));
  CHECK(remesa_persistent_linked_dpm_snapshot(&d,control,&b)==EBUSY&&!memcmp(&b,&saved,sizeof(b)));
  d.ops->unmap(d.priv,t,true);
  CHECK(!remesa_persistent_linked_dpm_snapshot(&d,control,&b));
  CHECK(remesa_persistent_linked_dpm_prepare(&d,t,p,&control)==EBUSY);
  CHECK(remesa_persistent_linked_dpm_prepare(&d,t,t,&second)==EINVAL&&!second);
  CHECK(remesa_persistent_linked_dpm_prepare(&d,p,t,&second)==ENOTSUP&&!second);
  CHECK(!remesa_persistent_linked_dpm_prepare(&d,t,p,&second));
  d.ops->destroy_bo(d.priv,t);d.ops->destroy_bo(d.priv,p);CHECK(!f.destroys);
  CHECK(!remesa_persistent_linked_dpm_snapshot(&d,control,&b));
  CHECK(!d.ops->map(d.priv,control,false,&cpu));d.ops->destroy_bo(d.priv,control);
  CHECK(!f.destroys&&remesa_persistent_close(&d)==EBUSY);d.ops->unmap(d.priv,control,false);
  CHECK(f.destroys==1&&!remesa_persistent_linked_dpm_snapshot(&d,second,&b));
  unchanged_storage(&f);d.ops->destroy_bo(d.priv,second);CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden&&!remesa_persistent_close(&d));
 }
 char trailing;CHECK(scanf(" %c",&trailing)==EOF);
 const enum failure failures[]={FAIL_CREATE,FAIL_PIN,FAIL_MAP,FAIL_READ_BODY,FAIL_READ_TAIL,FAIL_MAP_UNCERTAIN,FAIL_UNMAP_UNCERTAIN,FAIL_UNPIN};
 for(unsigned i=0;i<sizeof(failures)/sizeof(failures[0]);i++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};void *t=NULL,*p=NULL,*control=NULL;
  setup(&f,&d,2920+i,0x40000000,0x30100000,&t,&p);unsigned maps=f.cpu_maps;
  if(failures[i]==FAIL_UNPIN){CHECK(!remesa_persistent_linked_dpm_prepare(&d,t,p,&control));f.fail=FAIL_UNPIN;d.ops->destroy_bo(d.priv,control);}
  else{f.fail=failures[i];CHECK(remesa_persistent_linked_dpm_prepare(&d,t,p,&control)!=0);}
  unchanged_storage(&f);CHECK(f.cpu_maps<=maps+2);d.ops->destroy_bo(d.priv,t);d.ops->destroy_bo(d.priv,p);
  if(failures[i]==FAIL_MAP_UNCERTAIN||failures[i]==FAIL_UNMAP_UNCERTAIN||failures[i]==FAIL_UNPIN){
   CHECK(control&&f.creates==3&&!f.destroys&&!f.unpins&&get(&f,1)->pinned&&get(&f,2)->pinned&&remesa_persistent_close(&d)==EBUSY);
  }else{f.fail=FAIL_NONE;CHECK(!control&&f.creates==f.destroys&&f.pins==f.unpins&&!remesa_persistent_close(&d));}
  CHECK(!f.forbidden);
 }
 printf("PASS owned DPM:2 original CPU info/load oracles, table/parameter bounds,240-byte plan, shared refs/leases, zero-IO metadata, no64MiB map/clear, padding, ordinary unwind/uncertain quarantine retain storage; original wait7 distinct from completion15; GPU0/MMIO0 (%uchecks)\n",checks);return 0;
}
