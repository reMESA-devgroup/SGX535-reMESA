/* SPDX-License-Identifier: MIT. Resource ownership only; no GPU/API execution. */
#define REMESA_LINKED_BACKGROUND_FIXTURE
#include "test_linked_background.c"
int main(void)
{
 uint8_t original[24];CHECK(fread(original,1,24,stdin)==24&&fgetc(stdin)==EOF);
 CHECK(!ta_persistent_fixture());CHECK(!ta_linkage_fixture());
 const unsigned sizes[3][2]={{1280,800},{640,640},{641,479}};
 for(unsigned i=0;i<3;i++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};void *c[11]={0},*aux=NULL,*second=NULL;
  bg_setup(&f,&d,2800+i,sizes[i][0],sizes[i][1],c);unsigned maps=f.cpu_maps;
  CHECK(!remesa_persistent_linked_event_auxiliary_prepare(&d,c[10],3,&aux)&&aux&&f.creates==12&&f.cpu_maps==maps+2);
  struct remesa_persistent_linked_event_auxiliary_backing b;
  CHECK(!remesa_persistent_linked_event_auxiliary_snapshot(&d,aux,&b));
  CHECK(b.allocation_bytes==4096&&b.used_bytes==40&&b.program_offset[0]==0&&b.program_offset[1]==32&&b.program_bytes[0]==16&&b.program_bytes[1]==8);
  CHECK(b.gpu_va>=0x20080000&&b.gpu_va<0x20090000&&b.targets.handle==11);
  CHECK(b.targets.commands.width==sizes[i][0]&&b.targets.commands.height==sizes[i][1]);
  const uint8_t *bytes=get(&f,b.handle)->bytes;
  CHECK(!memcmp(bytes,original,16)&&!memcmp(bytes+32,original+16,8));
  for(unsigned j=16;j<32;j++)CHECK(!bytes[j]);
  for(unsigned j=40;j<4096;j++)CHECK(!bytes[j]);
  for(unsigned j=0;j<2;j++){
   uint32_t fixup=0;CHECK(b.entries[j].bank==3);
   CHECK(!remesa_use_entry_preview_fixup(&b.entries[j],&fixup)&&fixup==b.use_lower19[j]);
  }
  CHECK(b.use_lower19[0]!=b.use_lower19[1]);
  uint8_t data[64],known[64];size_t n=0;
  const struct remesa_pds_psb_event_data_request r={b.targets.gpu_va,b.targets.allocation_bytes,0,b.use_lower19[1],b.use_lower19[0]};
  CHECK(!remesa_pds_structure_encode_psb_event_data(&r,data,known,64,&n)&&n==64);
  CHECK(b.event_data_bytes==64&&!memcmp(data,b.event_data,64)&&!memcmp(known,b.event_data_known_mask,64));
  /* The emitted target pointer names the live committed COLOR-state source,
   * not the COLOR pixel allocation, Z, or the auxiliary's own USE bytes. */
  CHECK(shader_pds_le32(b.event_data+8)==b.targets.gpu_va+b.targets.color_data_offset);
  CHECK(shader_pds_le32(b.event_data+8)!=b.targets.color.gpu_va&&shader_pds_le32(b.event_data+8)!=b.targets.depth.gpu_va);
  for(unsigned word=0;word<16;word++)for(unsigned byte=0;byte<4;byte++){
   bool unwritten=word==6||word==7||word==15;
   CHECK(b.event_data_known_mask[word*4+byte]==(unwritten?0:255));
   if(unwritten)CHECK(!b.event_data[word*4+byte]);
  }
  CHECK(shader_pds_le32(data)==b.use_lower19[1]&&shader_pds_le32(data+8)==b.targets.gpu_va&&shader_pds_le32(data+16)==(0x200000|b.use_lower19[0]));
  struct remesa_persistent_linked_event_auxiliary_backing saved=b;
  unsigned creates=f.creates,gpu_maps=f.gpu_maps;
  maps=f.cpu_maps;for(unsigned j=0;j<100;j++){
   CHECK(!remesa_persistent_linked_event_auxiliary_snapshot(&d,aux,&b)&&!memcmp(&b,&saved,sizeof(b)));
  }
  CHECK(f.cpu_maps==maps&&f.creates==creates&&f.gpu_maps==gpu_maps);void *cpu=NULL;
  CHECK(remesa_persistent_linked_event_auxiliary_snapshot(&d,aux,(void *)((uint8_t *)&b+1))==EINVAL&&!memcmp(&b,&saved,sizeof(b)));
  CHECK(!d.ops->map(d.priv,aux,false,&cpu));
  CHECK(remesa_persistent_linked_event_auxiliary_snapshot(&d,aux,&b)==EBUSY&&!memcmp(&b,&saved,sizeof(b)));
  d.ops->unmap(d.priv,aux,false);
  CHECK(d.ops->map(d.priv,aux,true,&cpu)==ENOTSUP&&!cpu);
  CHECK(d.ops->map(d.priv,c[8],false,&cpu)==EAGAIN&&!cpu&&get(&f,9)->bytes[0]==0xa5&&get(&f,10)->bytes[0]==0xa5);
  CHECK(remesa_persistent_linked_event_auxiliary_prepare(&d,c[10],3,&aux)==EBUSY);
  CHECK(remesa_persistent_linked_event_auxiliary_snapshot(&d,c[10],&b)==EINVAL);
  CHECK(remesa_persistent_linked_event_auxiliary_snapshot(&d,aux,aux)==EINVAL);
  CHECK(!remesa_persistent_linked_event_auxiliary_prepare(&d,c[10],3,&second));
  bg_drop_children(&d,c);CHECK(!f.destroys&&!remesa_persistent_linked_event_auxiliary_snapshot(&d,aux,&b));
  CHECK(!memcmp(&b,&saved,sizeof(b))); /* retained DATA pointers after public owners drop */
  CHECK(!d.ops->map(d.priv,aux,false,&cpu));d.ops->destroy_bo(d.priv,aux);
  CHECK(!f.destroys&&remesa_persistent_close(&d)==EBUSY);d.ops->unmap(d.priv,aux,false);
  CHECK(f.destroys==1&&!remesa_persistent_linked_event_auxiliary_snapshot(&d,second,&b));
  d.ops->destroy_bo(d.priv,second);CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden&&!remesa_persistent_close(&d));
 }
 {
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};void *c[11]={0},*aux=NULL;
  bg_setup(&f,&d,2810,640,640,c);unsigned before=f.creates;
  CHECK(remesa_persistent_linked_event_auxiliary_prepare(&d,c[10],4,&aux)==EBUSY&&!aux&&f.creates==before+1&&f.destroys==1);
  before=f.creates;CHECK(remesa_persistent_linked_event_auxiliary_prepare(&d,c[10],16,&aux)!=0&&!aux&&f.creates==before);
  CHECK(remesa_persistent_linked_event_auxiliary_prepare(&d,c[8],3,&aux)==EBUSY&&!aux&&f.creates==before);
  bg_drop_children(&d,c);CHECK(f.creates==f.destroys&&!f.forbidden&&!remesa_persistent_close(&d));
 }
 const enum failure failures[]={FAIL_CREATE,FAIL_PIN,FAIL_MAP,FAIL_READ_BODY,FAIL_READ_TAIL,FAIL_MAP_UNCERTAIN,FAIL_UNMAP_UNCERTAIN,FAIL_UNPIN};
 for(unsigned i=0;i<sizeof(failures)/sizeof(failures[0]);i++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};void *c[11]={0},*aux=NULL;
  bg_setup(&f,&d,2820+i,640,640,c);
  if(failures[i]==FAIL_UNPIN){CHECK(!remesa_persistent_linked_event_auxiliary_prepare(&d,c[10],3,&aux));f.fail=FAIL_UNPIN;d.ops->destroy_bo(d.priv,aux);}
  else{f.fail=failures[i];CHECK(remesa_persistent_linked_event_auxiliary_prepare(&d,c[10],3,&aux)!=0);}
  bg_drop_children(&d,c);
  if(failures[i]==FAIL_MAP_UNCERTAIN||failures[i]==FAIL_UNMAP_UNCERTAIN||failures[i]==FAIL_UNPIN){
   CHECK(aux&&f.creates==12&&!f.destroys&&!f.unpins&&get(&f,11)->pinned&&get(&f,9)->pinned&&get(&f,10)->pinned&&remesa_persistent_close(&d)==EBUSY);
  }else{f.fail=FAIL_NONE;CHECK(!aux&&f.creates==f.destroys&&f.pins==f.unpins&&!remesa_persistent_close(&d));}
  CHECK(!f.forbidden);
 }
 printf("PASS owned event auxiliaries: generated16+8 effective bytes,32-byte starts/40 used/page capacity, historical oracles, live target DATA references, bank agreement, zero-IO snapshots, shared/transitive/lease lifetime, ordinary rollback and uncertain disposal retain COLOR/Z/TA; GPU0/API0/eventCODE0 (%uchecks)\n",checks);return 0;
}
