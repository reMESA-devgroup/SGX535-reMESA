/* SPDX-License-Identifier: MIT. Fixed CPU profile/ownership; no GPU execution. */
#define REMESA_LINKED_BACKGROUND_FIXTURE
#include "test_linked_background.c"
static void codec(const uint8_t original[84])
{
 const uint32_t values[]={0,1,0x1003,0x7ffff};
 for(unsigned i=0;i<4;i++)for(unsigned j=0;j<4;j++)for(unsigned offset=0;offset<=64;offset+=16){
  struct remesa_pds_psb_event_data_request r={0x40001000,4096,offset,values[i],values[j]};
  uint8_t out[400],mask[400],data[64],known[64];size_t n=0,dn=0;
  memset(out,0xa5,sizeof out);memset(mask,0x5a,sizeof mask);
  CHECK(!remesa_pds_structure_encode_psb_event_profile(&r,out,mask,sizeof out,&n)&&n==148);
  CHECK(!memcmp(out+64,original,84));
  CHECK(!remesa_pds_structure_encode_psb_event_data(&r,data,known,64,&dn)&&dn==64);
  CHECK(!memcmp(out,data,64)&&!memcmp(mask,known,64));
  for(unsigned k=64;k<148;k++)CHECK(mask[k]==255);
  for(unsigned k=148;k<400;k++)CHECK(out[k]==0xa5&&mask[k]==0x5a);
  CHECK(shader_pds_le32(out+72)==0x90000005&&shader_pds_le32(out+92)==0x90000014);
 }
 struct remesa_pds_psb_event_data_request r={0x40001000,4096,0,0x1003,0xe03};
 uint8_t out[400],mask[400],saved[400],savedmask[400];size_t n=999;
 memset(out,0xa5,sizeof out);memset(mask,0x5a,sizeof mask);memcpy(saved,out,400);memcpy(savedmask,mask,400);
 CHECK(remesa_pds_structure_encode_psb_event_profile(&r,out,mask,147,&n)==ENOSPC);
 CHECK(n==999&&!memcmp(out,saved,400)&&!memcmp(mask,savedmask,400));
 r.end_use_lower19=0x80000;
 CHECK(remesa_pds_structure_encode_psb_event_profile(&r,out,mask,400,&n)==ERANGE);
 CHECK(n==999&&!memcmp(out,saved,400)&&!memcmp(mask,savedmask,400));r.end_use_lower19=0x1003;
 CHECK(remesa_pds_structure_encode_psb_event_profile(&r,out,out,400,&n)==EINVAL);
 CHECK(remesa_pds_structure_encode_psb_event_profile(&r,out,mask,400,(void *)out)==EINVAL);
 CHECK(remesa_pds_structure_encode_psb_event_profile(&r,(void *)&r,mask,400,&n)==EINVAL);
 CHECK(remesa_pds_structure_encode_psb_event_profile((void *)((uint8_t *)&r+1),out,mask,400,&n)==EINVAL);
 CHECK(n==999&&!memcmp(out,saved,400)&&!memcmp(mask,savedmask,400));
 CHECK(!remesa_pds_structure_encode_psb_event_profile(&r,out,mask,148,&n)&&n==148);
}
int main(void)
{
 uint8_t original[84];CHECK(fread(original,1,84,stdin)==84&&fgetc(stdin)==EOF);codec(original);
 CHECK(!ta_persistent_fixture());CHECK(!ta_linkage_fixture());
 const unsigned sizes[][2]={{1280,800},{640,640},{641,479}};
 for(unsigned k=0;k<3;k++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  void *c[11]={0},*aux=NULL,*event=NULL,*second=NULL;
  bg_setup(&f,&d,5100+k,sizes[k][0],sizes[k][1],c);
  CHECK(!remesa_persistent_linked_event_auxiliary_prepare(&d,c[10],3,&aux));
  unsigned before=f.creates;CHECK(!remesa_persistent_linked_event_profile_prepare(&d,aux,&event)&&event&&f.creates==before+1);
  struct remesa_persistent_linked_event_profile_backing b,saved;
  CHECK(!remesa_persistent_linked_event_profile_snapshot(&d,event,&b));saved=b;
  CHECK(b.allocation_bytes==4096&&b.reserved_bytes==400&&b.used_bytes==148&&b.data_bytes==64&&b.code_offset==64&&b.code_bytes==84&&b.descriptor_field0c==16);
  CHECK(b.gpu_va%32==0&&b.gpu_va>=0x20000000&&b.gpu_va+b.allocation_bytes<=0x30000000);
  const uint8_t *bytes=get(&f,b.handle)->bytes;
  CHECK(!memcmp(bytes,b.auxiliary.event_data,64)&&!memcmp(bytes+64,original,84));
  for(unsigned i=148;i<4096;i++)CHECK(!bytes[i]);
  unsigned maps=f.cpu_maps,creates=f.creates,gpu_maps=f.gpu_maps,reads=f.reads,writes=f.writes;
  for(unsigned i=0;i<100;i++)CHECK(!remesa_persistent_linked_event_profile_snapshot(&d,event,&b)&&!memcmp(&b,&saved,sizeof b));
  CHECK(f.cpu_maps==maps&&f.creates==creates&&f.gpu_maps==gpu_maps&&f.reads==reads&&f.writes==writes);
  CHECK(remesa_persistent_linked_event_profile_snapshot(&d,event,(void *)((uint8_t *)&b+1))==EINVAL&&!memcmp(&b,&saved,sizeof b));
  CHECK(remesa_persistent_linked_event_profile_snapshot(&d,aux,&b)==EINVAL&&!memcmp(&b,&saved,sizeof b));
  CHECK(remesa_persistent_linked_event_profile_prepare(&d,aux,&event)==EBUSY);
  CHECK(remesa_persistent_linked_event_profile_prepare(&d,event,&second)==EINVAL&&!second);
  void *cpu=NULL;CHECK(!d.ops->map(d.priv,aux,false,&cpu));
  CHECK(remesa_persistent_linked_event_profile_snapshot(&d,event,&b)==EBUSY&&!memcmp(&b,&saved,sizeof b));
  d.ops->unmap(d.priv,aux,false);
  CHECK(d.ops->map(d.priv,event,true,&cpu)==ENOTSUP&&!cpu);
  CHECK(remesa_persistent_linked_event_profile_snapshot(&d,event,event)==EINVAL);
  CHECK(!remesa_persistent_linked_event_profile_prepare(&d,aux,&second));
  bg_drop_children(&d,c);d.ops->destroy_bo(d.priv,aux);
  CHECK(!f.destroys&&!remesa_persistent_linked_event_profile_snapshot(&d,event,&b)&&!memcmp(&b,&saved,sizeof b));
  CHECK(!d.ops->map(d.priv,event,false,&cpu));d.ops->destroy_bo(d.priv,event);CHECK(!f.destroys);
  CHECK(remesa_persistent_close(&d)==EBUSY);d.ops->unmap(d.priv,event,false);CHECK(f.destroys==1);
  CHECK(!remesa_persistent_linked_event_profile_snapshot(&d,second,&b));d.ops->destroy_bo(d.priv,second);
  CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden&&!remesa_persistent_close(&d));
 }
 const enum failure failures[]={FAIL_CREATE,FAIL_PIN,FAIL_MAP,FAIL_READ_BODY,FAIL_READ_TAIL,FAIL_MAP_UNCERTAIN,FAIL_UNMAP_UNCERTAIN,FAIL_UNPIN};
 for(unsigned i=0;i<sizeof failures/sizeof failures[0];i++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};void *c[11]={0},*aux=NULL,*event=NULL;
  bg_setup(&f,&d,5200+i,640,640,c);CHECK(!remesa_persistent_linked_event_auxiliary_prepare(&d,c[10],3,&aux));
  if(failures[i]==FAIL_UNPIN){CHECK(!remesa_persistent_linked_event_profile_prepare(&d,aux,&event));f.fail=FAIL_UNPIN;d.ops->destroy_bo(d.priv,event);}
  else{f.fail=failures[i];CHECK(remesa_persistent_linked_event_profile_prepare(&d,aux,&event)!=0);}
  bg_drop_children(&d,c);d.ops->destroy_bo(d.priv,aux);
  if(failures[i]==FAIL_MAP_UNCERTAIN||failures[i]==FAIL_UNMAP_UNCERTAIN||failures[i]==FAIL_UNPIN){
   CHECK(event&&f.creates==13&&!f.destroys&&!f.unpins&&get(&f,12)->pinned&&get(&f,9)->pinned&&get(&f,10)->pinned&&remesa_persistent_close(&d)==EBUSY);
  }else{f.fail=FAIL_NONE;CHECK(!event&&f.creates==f.destroys&&f.pins==f.unpins&&!remesa_persistent_close(&d));}
  CHECK(!f.forbidden);
 }
 printf("PASS fixed psb event: CODE84 matches preserved physical profile, DATA64/commit148/reserve400/page distinct, owned transitive links/public release/leases, zero-IO snapshots, ordinary rollback/uncertain quarantine; GPU0/native-admission0 (%uchecks)\n",checks);return 0;
}
