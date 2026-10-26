/* SPDX-License-Identifier: MIT */
#define REMESA_LAB_STORAGE_FIXTURE
#include "test_lab_scene_storage.c"
static uint32_t uncertain_handle;
static int selective_sync(void *data,uint32_t id)
{if(id==uncertain_handle)return EOWNERDEAD;return sync_device(data,id);}
static int selective_unmap(void *data,uint32_t id,bool write)
{
 if(id!=uncertain_handle)return cpu_unmap(data,id,write);
 struct fixture *f=data;CHECK(get(f,id)->lease);++f->cpu_unmaps;
 return EOWNERDEAD; /* Retain the provider lease, never fake a release. */
}
int main(void)
{
 struct remesa_shader_image *images[]={image_for(0xff00ff00),image_for(0xffff00ff),image_for(0xffffff00)};
 struct remesa_depth_scene_image_request r=request(images);
 struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
 struct remesa_persistent_ops o=ops;o.create=native_create;
 struct remesa_persistent_config c=config(&f,&o);CHECK(!remesa_persistent_open(&d,&c));
 struct remesa_lab_scene_storage s={0};CHECK(!remesa_lab_scene_storage_prepare(&d,&r,&s));
 uint8_t expected[264];CHECK(!remesa_lab_scene_control_encode(&s,0,0x44c0,128,16,expected));
 struct remesa_lab_scene_storage saved=s;
 struct remesa_persistent_memory_backing receipt;
 CHECK(remesa_persistent_control_snapshot(&d,s.objects[REMESA_LAB_CONTROL],&receipt)==EINVAL);
 CHECK(remesa_lab_scene_control_stage(&s,0,0x44c0,128,32)==ENOTSUP&&!memcmp(&s,&saved,sizeof(s)));
 s.bindings[REMESA_LAB_CONTROL].generation++;
 CHECK(remesa_lab_scene_control_stage(&s,0,0x44c0,128,16)==ESTALE);s=saved;
 s.objects[REMESA_LAB_CONTROL]=s.objects[REMESA_LAB_COLOR];
 s.bindings[REMESA_LAB_CONTROL]=s.bindings[REMESA_LAB_COLOR];
 CHECK(remesa_lab_scene_control_stage(&s,0,0x44c0,128,16)==EINVAL);s=saved;
 fail_create_at=f.creates+1;
 CHECK(remesa_lab_scene_control_stage(&s,0,0x44c0,128,16)==ENOMEM&&!memcmp(&s,&saved,sizeof(s)));
 fail_create_at=0;
 const enum failure failures[]={FAIL_MAP,FAIL_READ_MAP,FAIL_READ_BODY,FAIL_READ_TAIL};
 for(unsigned i=0;i<sizeof(failures)/sizeof(failures[0]);i++) {
  f.fail_map_handle=f.creates+1;f.fail=failures[i];
  CHECK(remesa_lab_scene_control_stage(&s,0,0x44c0,128,16)==(i<2?ENOMEM:EPROTO));
  CHECK(!memcmp(&s,&saved,sizeof(s))&&f.creates-f.destroys==11);
  CHECK(get(&f,s.bindings[REMESA_LAB_CONTROL].handle)->bytes[0]==0xa5);
  f.fail=FAIL_NONE;
 }
 for(unsigned repeat=0;repeat<2;repeat++) {
  void *old=s.objects[REMESA_LAB_CONTROL];uint32_t old_handle=s.bindings[REMESA_LAB_CONTROL].handle;
  CHECK(!remesa_lab_scene_control_stage(&s,0,0x44c0,128,16));
  CHECK(s.objects[REMESA_LAB_CONTROL]!=old&&!f.backing[old_handle-1].live);
  CHECK(f.creates-f.destroys==11);
  CHECK(!remesa_persistent_control_snapshot(&d,s.objects[REMESA_LAB_CONTROL],&receipt));
  CHECK(receipt.logical_bytes==264&&receipt.allocation_bytes==4096);
  struct backing *b=get(&f,receipt.handle);
  CHECK(!memcmp(b->bytes,expected,264));
  for(size_t i=264;i<b->length;i++)if(b->bytes[i]){CHECK(false);}
  CHECK(!remesa_persistent_control_verify(&d,s.objects[REMESA_LAB_CONTROL]));
  void *mapped=NULL;
  CHECK(d.ops->map(d.priv,s.objects[REMESA_LAB_CONTROL],true,&mapped)==ENOTSUP&&!mapped);
  struct remesa_persistent_render_target_backing target;
  CHECK(remesa_persistent_render_target_snapshot(&d,s.objects[REMESA_LAB_CONTROL],32,32,32,&target)==ENOTSUP);
  for(unsigned i=0;i<REMESA_LAB_SCENE_ROLES;i++)if(i!=REMESA_LAB_CONTROL)
   CHECK(s.objects[i]==saved.objects[i]&&!memcmp(&s.bindings[i],&saved.bindings[i],sizeof(receipt)));
 }
 remesa_lab_scene_storage_destroy(&s);CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);
 CHECK(!remesa_persistent_close(&d));
 /* Uncertain candidate lease: preserve old bundle and retain quarantine. */
 for(unsigned uncertain=0;uncertain<3;uncertain++) {
  struct fixture *u=calloc(1,sizeof(*u));CHECK(u);u->allow_gpu_map=u->allow_sync=true;
  struct remesa_device *ud=calloc(1,sizeof(*ud));CHECK(ud);
  struct remesa_persistent_ops *uo=malloc(sizeof(*uo));CHECK(uo);*uo=ops;uo->create=native_create;
  if(uncertain==0)uo->cpu_unmap=selective_unmap;
  if(uncertain==1)uo->sync_for_device=selective_sync;
  c=config(u,uo);CHECK(!remesa_persistent_open(ud,&c));
  CHECK(!remesa_lab_scene_storage_prepare(ud,&r,&s));saved=s;
  uncertain_handle=u->creates+1;
  if(uncertain<2) {
   CHECK(remesa_lab_scene_control_stage(&s,0,0x44c0,128,16)==EOWNERDEAD&&!memcmp(&s,&saved,sizeof(s)));
   CHECK(u->creates==12&&!u->destroys&&!u->forbidden);
   CHECK(get(u,uncertain_handle)->lease==(uncertain==0));
  } else {
   CHECK(!remesa_lab_scene_control_stage(&s,0,0x44c0,128,16));
   get(u,s.bindings[REMESA_LAB_CONTROL].handle)->bytes[263]^=1;
   CHECK(remesa_persistent_control_verify(ud,s.objects[REMESA_LAB_CONTROL])==EPROTO);
   CHECK(remesa_persistent_control_snapshot(ud,s.objects[REMESA_LAB_CONTROL],&receipt)==EIO);
  }
  CHECK(remesa_persistent_close(ud)==EBUSY);
  /* Provider uncertainty/integrity loss intentionally retains ownership. */
  memset(&s,0,sizeof(s));uncertain_handle=0;
 }
 for(unsigned i=0;i<3;i++)remesa_shader_image_destroy(images[i]);
 printf("PASS control264 owned write/readback/padding/sync, immutable public ABI, replacement, rollback allocation/map/body/tail, stale metadata, retained uncertain lease; NO GPU (%u checks)\n",checks);
 return 0;
}
