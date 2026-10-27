/* SPDX-License-Identifier: MIT. Region ownership and CPU plan only. */
#define REMESA_LINKED_BACKGROUND_FIXTURE
#include "test_linked_background.c"
static unsigned region_word(void){unsigned v;CHECK(scanf("%u",&v)==1);return v;}
int main(void)
{
 CHECK(!ta_persistent_fixture());CHECK(!ta_linkage_fixture());
 for(unsigned variant=0;variant<3;variant++){
  unsigned w=region_word(),h=region_word(),base=region_word(),required=region_word(),pages=region_word(),first=region_word(),clear=region_word();
  uint32_t cookie[15],ta[14],raster[2];for(unsigned i=0;i<15;i++)cookie[i]=region_word();
  for(unsigned i=0;i<14;i++)ta[i]=region_word();for(unsigned i=0;i<2;i++)raster[i]=region_word();
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};void *c[11]={0},*region=NULL,*second=NULL;
  bg_setup(&f,&d,3000+variant,w,h,c);unsigned maps=f.cpu_maps;
  CHECK(!remesa_persistent_linked_region_prepare(&d,c[10],&region)&&region&&f.creates==12&&f.cpu_maps==maps+2);
  struct remesa_persistent_linked_region_backing b;
  CHECK(!remesa_persistent_linked_region_snapshot(&d,region,&b));
  CHECK(b.region.gpu_va==base&&b.region.width==w&&b.region.height==h&&b.targets.commands.width==w&&b.targets.commands.height==h);
  CHECK(b.region.layout.required_bytes==required&&b.region.allocation_bytes==pages*4096&&b.region.layout.allocation_pages==pages);
  CHECK(b.region.layout.clear_first_page==first&&b.region.layout.clear_page_count==clear);
  CHECK(!memcmp(b.region.layout.cookie,cookie,sizeof(cookie))&&!memcmp(b.ta_scene,ta,sizeof(ta))&&!memcmp(b.raster_scene,raster,sizeof(raster)));
  CHECK(b.region.region_gpu_va==(uint64_t)base+cookie[7]&&b.raster_scene[1]==b.region.region_gpu_va&&b.ta_scene[13]==b.region.region_gpu_va);
  CHECK(b.access==(REMESA_ACCESS_READ|REMESA_ACCESS_WRITE)&&cookie[7]!=0x1000);
  bool zero=true;for(unsigned i=0;i<b.region.allocation_bytes;i++)zero&=!get(&f,b.region.handle)->bytes[i];CHECK(zero);
  CHECK(get(&f,9)->bytes[0]==0xa5&&get(&f,10)->bytes[0]==0xa5);
  maps=f.cpu_maps;for(unsigned i=0;i<100;i++)CHECK(!remesa_persistent_linked_region_snapshot(&d,region,&b));CHECK(f.cpu_maps==maps);
  void *cpu=NULL;CHECK(d.ops->map(d.priv,region,true,&cpu)==ENOTSUP&&!cpu);
  CHECK(d.ops->map(d.priv,c[8],false,&cpu)==EAGAIN&&!cpu);
  CHECK(remesa_persistent_linked_region_prepare(&d,c[10],&region)==EBUSY);
  CHECK(remesa_persistent_linked_region_snapshot(&d,c[10],&b)==EINVAL);
  CHECK(remesa_persistent_linked_region_snapshot(&d,region,region)==EINVAL);
  struct remesa_persistent_linked_region_backing saved=b;
  CHECK(remesa_persistent_linked_region_snapshot(&d,region,(void *)((uint8_t *)&b+1))==EINVAL&&!memcmp(&b,&saved,sizeof(b)));
  struct remesa_persistent_render_target_backing invalid;
  CHECK(remesa_persistent_render_target_snapshot(&d,region,w,h,(w+31u)&~31u,&invalid)==ENOTSUP);
  CHECK(!remesa_persistent_linked_region_prepare(&d,c[10],&second));
  bg_drop_children(&d,c);CHECK(!f.destroys&&!remesa_persistent_linked_region_snapshot(&d,region,&b));
  CHECK(!d.ops->map(d.priv,region,false,&cpu));d.ops->destroy_bo(d.priv,region);
  CHECK(!f.destroys&&remesa_persistent_close(&d)==EBUSY);d.ops->unmap(d.priv,region,false);
  CHECK(f.destroys==1&&!remesa_persistent_linked_region_snapshot(&d,second,&b));
  d.ops->destroy_bo(d.priv,second);CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden&&!remesa_persistent_close(&d));
 }
 char trailing;CHECK(scanf(" %c",&trailing)==EOF);
 {
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};void *c[11]={0},*region=NULL;
  bg_setup(&f,&d,3010,640,640,c);unsigned before=f.creates;
  CHECK(remesa_persistent_linked_region_prepare(&d,c[8],&region)==EBUSY&&!region&&f.creates==before);
  bg_drop_children(&d,c);CHECK(f.creates==f.destroys&&!f.forbidden&&!remesa_persistent_close(&d));
 }
 const enum failure failures[]={FAIL_CREATE,FAIL_PIN,FAIL_MAP,FAIL_READ_BODY,FAIL_READ_TAIL,FAIL_MAP_UNCERTAIN,FAIL_UNMAP_UNCERTAIN,FAIL_UNPIN};
 for(unsigned i=0;i<sizeof(failures)/sizeof(failures[0]);i++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};void *c[11]={0},*region=NULL;
  bg_setup(&f,&d,3020+i,640,640,c);
  if(failures[i]==FAIL_UNPIN){CHECK(!remesa_persistent_linked_region_prepare(&d,c[10],&region));f.fail=FAIL_UNPIN;d.ops->destroy_bo(d.priv,region);}
  else{f.fail=failures[i];CHECK(remesa_persistent_linked_region_prepare(&d,c[10],&region)!=0);}
  bg_drop_children(&d,c);
  if(failures[i]==FAIL_MAP_UNCERTAIN||failures[i]==FAIL_UNMAP_UNCERTAIN||failures[i]==FAIL_UNPIN){
   CHECK(region&&f.creates==12&&!f.destroys&&!f.unpins&&get(&f,11)->pinned&&get(&f,9)->pinned&&get(&f,10)->pinned&&remesa_persistent_close(&d)==EBUSY);
  }else{f.fail=FAIL_NONE;CHECK(!region&&f.creates==f.destroys&&f.pins==f.unpins&&!remesa_persistent_close(&d));}
  CHECK(!f.forbidden);
 }
 printf("PASS target-bound region:3 original CPU layout/TA/raster oracles, actual cookie7 offsets, capacity/zero policy, live COLOR/Z/TA dimensions, zero-IO snapshots, shared/transitive/lease lifetime and ordinary/uncertain retention; GPU0/MMIO0/API0 (%uchecks)\n",checks);return 0;
}
