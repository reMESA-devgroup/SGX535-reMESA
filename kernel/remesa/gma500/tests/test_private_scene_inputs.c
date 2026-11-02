/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual memory requests/holds/seals. Trusted READ classification and initial
 * image admission are modeled; no source claim, MMIO or GPU is permitted. */
#define main previous_input_main
#include "test_private_inputs.c"
#undef main
static u32 scene_ids[32];
static struct remesa_memory_binding scene_uses[32];
static const u32 read_mask=0x1fff;
static struct remesa_memory_ticket scene_job(void)
{
 struct remesa_memory_ticket t={0};u32 command=0;
 CHECK(!remesa_gem_job_prepare(memory,scene_uses,18,&command,4,&t));
 CHECK(!remesa_gem_job_claim_zero_dpm(memory,t,scene_ids[16],scene_ids[15]));
 return t;
}
static int scene_seal(struct remesa_memory_ticket t,u32 mask)
{return remesa_private_session_seal_scene_inputs(memory,&token,t,mask);}
static int scene_check(struct remesa_memory_ticket t,u32 mask)
{
 mutex_lock(&memory->lock);
 int e=remesa_private_session_scene_inputs_locked(memory,t,mask);
 mutex_unlock(&memory->lock);return e;
}
int main(void)
{
 CHECK(!previous_input_main());
 CHECK(!remesa_gem_memory_open(&dev,&file,77,&memory));endpoint=memory;
 for(unsigned i=0;i<19;i++){
  u64 bytes=i==15||i==16?33554432:4096;
  u64 va=i==15?0x30100000:i==16?0x50000000:0x20001000+i*4096;
  struct remesa_memory_request r={.version=1,.action=REMESA_MEM_CREATE,.context=77,.bytes=bytes};
  CHECK(!remesa_gem_memory_request(memory,&file,&r));scene_ids[i]=r.id;
  CHECK(!request(REMESA_MEM_PIN,r.id,0,0,NULL,0));
  r.action=REMESA_MEM_MAP;r.va=va;r.bytes=0;
  CHECK(!remesa_gem_memory_request(memory,&file,&r));
  if(i<18)scene_uses[i]=(struct remesa_memory_binding){r.id,va,bytes};
 }
 struct remesa_gem_memory *out=NULL;
 CHECK(!remesa_private_session_acquire(&dev,&file,77,&token,&out)&&out==memory);
 struct remesa_memory_ticket t=scene_job(),bad=t;bad.generation++;
 CHECK(scene_seal(t,read_mask)==-EACCES); /* no pool ownership yet */
 CHECK(!remesa_private_session_seal_dpm(memory,&token,t,scene_ids[16],scene_ids[15]));
 unsigned zeroed=test_zeroed_pages,maps=vmap_calls,f=flushes;
 CHECK(remesa_private_session_seal_scene_inputs(NULL,&token,t,read_mask)==-EINVAL);
 CHECK(remesa_private_session_seal_scene_inputs(memory,NULL,t,read_mask)==-EINVAL);
 CHECK(remesa_private_session_seal_scene_inputs((void*)1,&token,t,read_mask)==-EACCES);
 CHECK(remesa_private_session_seal_scene_inputs(memory,&other,t,read_mask)==-EACCES);
 CHECK(scene_seal(t,0)==-EINVAL&&scene_seal(t,1u<<18)==-EACCES);
 CHECK(scene_seal(t,1u<<15)==-EACCES&&scene_seal(bad,read_mask)==-EBUSY);
 struct retained_job *j=ticket_job(memory,t);CHECK(j);
 for(unsigned role=0;role<13;role++)for(unsigned fault=0;fault<16;fault++){
  struct backing *b=&memory->bo[scene_ids[role]&31],saved=*b;
  switch(fault){
   case 0:b->live=false;break;case 1:b->generation++;break;case 2:b->holds++;break;
   case 3:b->lease=true;break;case 4:b->quarantine=true;break;case 5:b->pinned=false;break;
   case 6:b->reserved=false;break;case 7:b->pages=NULL;break;case 8:b->inserted--;break;
   case 9:b->gem.size=0;break;case 10:b->input_seal=2;break;case 11:b->initialized=false;break;
   case 12:atomic_set(&b->color_vmas,1);break;case 13:b->holds=0;break;
   case 14:b->input_seal=1;break;case 15:b->inserted++;break;
  }
  CHECK(scene_seal(t,read_mask)==-EACCES);
  CHECK(!memory->private_scene_binding_count&&!memory->private_scene_input_mask);
  for(unsigned k=0;k<32;k++)CHECK(!memory->private_scene_input_ids[k]);
  for(unsigned k=0;k<13;k++)if(k!=role)CHECK(!memory->bo[scene_ids[k]&31].input_seal);
  *b=saved;
 }
 CHECK(!scene_seal(t,read_mask)&&!scene_check(t,read_mask));
 CHECK(scene_seal(t,read_mask)==-EBUSY);
 CHECK(scene_check(t,read_mask^1)==-EACCES);
 for(unsigned role=0;role<13;role++){
  struct backing *b=&memory->bo[scene_ids[role]&31];
  b->generation++;CHECK(scene_check(t,read_mask)==-EACCES);b->generation--;
  b->input_seal=0;CHECK(scene_check(t,read_mask)==-EACCES);b->input_seal=2;
  j->ids[role]=scene_ids[18];CHECK(scene_check(t,read_mask)==-EACCES);j->ids[role]=scene_ids[role];
 }
 CHECK(!remesa_gem_job_cancel(memory,t));
 for(unsigned role=0;role<13;role++){
  unsigned char byte=0x5a;
  CHECK(request(REMESA_MEM_WRITE,scene_ids[role],0,0,&byte,1)==-EPERM);
  CHECK(request(REMESA_MEM_LEASE,scene_ids[role],1,0,NULL,0)==-EPERM);
  CHECK(request(REMESA_MEM_SYNC,scene_ids[role],0,0,NULL,0)==-EPERM);
  CHECK(!request(REMESA_MEM_LEASE,scene_ids[role],0,0,NULL,0));
  CHECK(!request(REMESA_MEM_READ,scene_ids[role],0,0,NULL,4));
  CHECK(remesa_private_session_release(memory,&token)==-EBUSY);
  CHECK(!request(REMESA_MEM_UNLEASE,scene_ids[role],0,0,NULL,0));
 }
 CHECK(!request(REMESA_MEM_LEASE,scene_ids[18],1,0,NULL,0));
 CHECK(!request(REMESA_MEM_UNLEASE,scene_ids[18],0,0,NULL,0));
 t=scene_job();CHECK(!scene_check(t,read_mask));
 CHECK(remesa_private_session_release(memory,&token)==-EBUSY);
 CHECK(!remesa_gem_job_cancel(memory,t));
 for(unsigned loss=0;loss<2;loss++){
  pid_t child=fork();CHECK(child>=0);
  if(!child){
   if(loss)remesa_private_memory_device_lost(&dev);else remesa_private_memory_file_lost(&dev,&file);
   CHECK(memory->private_scene_input_mask==read_mask);
   CHECK(remesa_private_session_release(memory,&token)==-ENODEV);_exit(0);
  }
  int status;CHECK(waitpid(child,&status,0)==child&&WIFEXITED(status)&&!WEXITSTATUS(status));
 }
 struct backing *last=&memory->bo[scene_ids[12]&31];last->generation++;
 CHECK(remesa_private_session_release(memory,&token)==-EACCES);
 for(unsigned i=0;i<13;i++)CHECK(memory->bo[scene_ids[i]&31].input_seal==2);
 last->generation--;CHECK(!remesa_private_session_release(memory,&token));
 CHECK(!memory->private_scene_binding_count&&!memory->private_scene_input_mask);
 for(unsigned i=0;i<32;i++)CHECK(!memory->private_scene_input_ids[i]);
 for(unsigned i=0;i<13;i++)CHECK(!memory->bo[scene_ids[i]&31].input_seal);
 CHECK(test_zeroed_pages==zeroed&&vmap_calls>=maps&&flushes==f);
 CHECK(!request(REMESA_MEM_LEASE,scene_ids[12],1,0,NULL,0));
 CHECK(!request(REMESA_MEM_UNLEASE,scene_ids[12],0,0,NULL,0));
 /* The metadata API also supports the complete 32-binding capacity. Bit31
  * must remain an unsigned READ bit, without a shift by32 or truncated ID. */
 for(unsigned i=19;i<32;i++){
  struct remesa_memory_request r={.version=1,.action=REMESA_MEM_CREATE,.context=77,.bytes=4096};
  CHECK(!remesa_gem_memory_request(memory,&file,&r));scene_ids[i]=r.id;
  CHECK(!request(REMESA_MEM_PIN,r.id,0,0,NULL,0));
  r.action=REMESA_MEM_MAP;r.va=0x21000000+i*4096;r.bytes=0;
  CHECK(!remesa_gem_memory_request(memory,&file,&r));
 }
 for(unsigned i=0;i<32;i++){
  struct backing *b=&memory->bo[scene_ids[i]&31];
  scene_uses[i]=(struct remesa_memory_binding){scene_ids[i],b->va.start,b->gem.size};
 }
 CHECK(!remesa_private_session_acquire(&dev,&file,77,&token,&out));
 u32 command=0;
 CHECK(!remesa_gem_job_prepare(memory,scene_uses,32,&command,4,&t));
 CHECK(!remesa_gem_job_claim_zero_dpm(memory,t,scene_ids[16],scene_ids[15]));
 CHECK(!remesa_private_session_seal_dpm(memory,&token,t,scene_ids[16],scene_ids[15]));
 CHECK(!scene_seal(t,read_mask|0x80000000u)&&!scene_check(t,read_mask|0x80000000u));
 CHECK(memory->private_scene_input_ids[31]==scene_ids[31]);
 CHECK(!remesa_gem_job_cancel(memory,t)&&!remesa_private_session_release(memory,&token));
 for(unsigned i=0;i<32;i++)CHECK(!memory->bo[scene_ids[i]&31].input_seal);
 CHECK(!remesa_gem_memory_close(memory));endpoint=memory=NULL;
 CHECK(!gems&&!pins&&!ptes&&!device_refs&&!module_refs&&!power&&!claimed);
 printf("PASS %u actual-core general READ seal checks; no GPU, hashes, new clear or flush\n",checks);
 return 0;
}
