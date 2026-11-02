/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual memory/core/pool exclusions. No source or hardware callback allowed. */
#define main old_input_fixture_main
#include "test_private_inputs.c"
#undef main

static void dpm_setup(void)
{
 CHECK(!remesa_gem_memory_open(&dev,&file,77,&memory));endpoint=memory;
 /* Deliberately different indices/VA from retained roles7/8. */
 const u64 va[3]={0x40001000,0x30100000,0x50000000};
 for(unsigned i=0;i<3;i++){
  u64 size=i?33554432:4096;
  struct remesa_memory_request r={.version=1,.action=REMESA_MEM_CREATE,.context=77,.bytes=size};
  CHECK(!remesa_gem_memory_request(memory,&file,&r));ids[i]=r.id;
  CHECK(!request(REMESA_MEM_PIN,r.id,0,0,NULL,0));
  r=(struct remesa_memory_request){.version=1,.action=REMESA_MEM_MAP,.context=77,.id=ids[i],.va=va[i]};
  CHECK(!remesa_gem_memory_request(memory,&file,&r));uses[i]=(struct remesa_memory_binding){ids[i],va[i],size};
 }
 struct remesa_gem_memory *out=NULL;
 CHECK(!remesa_private_session_acquire(&dev,&file,77,&token,&out)&&out==memory);
}
static struct remesa_memory_ticket dpm_job(void)
{
 struct remesa_memory_ticket t={0};u32 command=0;
 CHECK(!remesa_gem_job_prepare(memory,uses,3,&command,sizeof command,&t));return t;
}
static int seal(struct remesa_memory_ticket t)
{return remesa_private_session_seal_dpm(memory,&token,t,ids[2],ids[1]);}
static int sealed_job(struct remesa_memory_ticket t)
{mutex_lock(&memory->lock);int e=remesa_private_session_dpm_locked(memory,t);mutex_unlock(&memory->lock);return e;}
int main(void)
{
 CHECK(!old_input_fixture_main());
 dpm_setup();struct remesa_memory_ticket t=dpm_job(),bad=t;bad.generation++;
 struct retained_job *j=ticket_job(memory,t);CHECK(j);
 CHECK(seal(t)==-EACCES); /* requires the real existing cold claim */
 CHECK(!remesa_gem_job_claim_zero_dpm(memory,t,ids[2],ids[1]));
 unsigned zeroed=test_zeroed_pages,maps=vmap_calls,flushed=flushes;
 CHECK(remesa_private_session_seal_dpm(NULL,&token,t,ids[2],ids[1])==-EINVAL);
 CHECK(remesa_private_session_seal_dpm(memory,NULL,t,ids[2],ids[1])==-EINVAL);
 CHECK(remesa_private_session_seal_dpm((void*)1,&token,t,ids[2],ids[1])==-EACCES);
 CHECK(remesa_private_session_seal_dpm(memory,&other,t,ids[2],ids[1])==-EACCES);
 CHECK(seal(bad)==-EBUSY);
 CHECK(remesa_private_session_seal_dpm(memory,&token,t,0,ids[1])==-EACCES);
 CHECK(remesa_private_session_seal_dpm(memory,&token,t,ids[2],ids[2])==-EACCES);
 CHECK(remesa_private_session_seal_dpm(memory,&token,t,ids[1],ids[2])==-ERANGE);
 j->dpm_mask=0;CHECK(seal(t)==-EACCES);j->dpm_mask=6;
 j->cookie=1;CHECK(seal(t)==-EBUSY);j->cookie=0;
 j->quarantined=true;CHECK(seal(t)==-EBUSY);j->quarantined=false;
 for(unsigned role=1;role<=2;role++)for(unsigned fault=0;fault<18;fault++){
  struct backing *b=&memory->bo[ids[role]&31],saved=*b;
  void *cpu=j->cpu_views[role];
  switch(fault){
   case 0:b->live=false;break;case 1:b->generation++;break;case 2:b->holds++;break;
   case 3:b->lease=true;break;case 4:b->quarantine=true;break;case 5:b->pinned=false;break;
   case 6:b->reserved=false;break;case 7:b->pages=NULL;break;case 8:b->inserted--;break;
   case 9:b->gem.size=4096;break;case 10:b->cached_job_view=(void*)1;break;
   case 11:j->cpu_views[role]=(void*)1;break;case 12:atomic_set(&b->color_vmas,1);break;
   case 13:b->input_seal=2;break;case 14:b->initialized=false;break;
   case 15:b->zero_pristine=false;break;case 16:b->va.start++;break;
   case 17:b->va.start=0x20000000;break;
  }
  int e=seal(t);CHECK(e==(fault==14||fault==15?-ENODATA:fault>=16?-ERANGE:-EACCES));
  CHECK(!memory->private_dpm_ids[0]&&!memory->private_dpm_ids[1]);
  *b=saved;j->cpu_views[role]=cpu;
 }
 for(unsigned i=0;i<5;i++){
  memory->private_input_ids[i]=ids[0];CHECK(seal(t)==-EBUSY);memory->private_input_ids[i]=0;
 }
 for(unsigned i=0;i<6;i++){
  memory->private_work_ids[i]=ids[0];CHECK(seal(t)==-EBUSY);memory->private_work_ids[i]=0;
 }
 memory->private_source_held=true;CHECK(seal(t)==-EBUSY);memory->private_source_held=false;
 memory->private_source_poisoned=true;CHECK(seal(t)==-EIO);memory->private_source_poisoned=false;
 memory->private_device_lost=true;CHECK(seal(t)==-ENODEV);memory->private_device_lost=false;
 memory->file=NULL;CHECK(seal(t)==-ENODEV);memory->file=&file;
 CHECK(!seal(t)&&!sealed_job(t));CHECK(seal(t)==-EBUSY);
 CHECK(memory->private_dpm_ids[0]==ids[2]&&memory->private_dpm_ids[1]==ids[1]);
 CHECK(test_zeroed_pages==zeroed&&vmap_calls==maps&&flushes==flushed);
 CHECK(remesa_private_session_seal_inputs(memory,&token,t,guarded)==-EBUSY);
 for(unsigned loss=0;loss<2;loss++){
  /* Each child retains the deliberately lost model until process exit; it
   * never fabricates a release/recovery path for the production driver. */
  pid_t child=fork();CHECK(child>=0);
  if(!child){
   if(loss)remesa_private_memory_device_lost(&dev);
   else remesa_private_memory_file_lost(&dev,&file);
   CHECK(memory->private_dpm_ids[0]==ids[2]&&memory->private_dpm_ids[1]==ids[1]);
   CHECK(memory->private_device_lost&&!memory->file);
   CHECK(sealed_job(t)==-ENODEV&&seal(t)==-ENODEV);
   CHECK(remesa_private_session_release(memory,&token)==-ENODEV);
   CHECK(remesa_gem_memory_close(memory)==-ENODEV);
   _exit(0);
  }
  int status;CHECK(waitpid(child,&status,0)==child);CHECK(WIFEXITED(status)&&WEXITSTATUS(status)==0);
 }
 CHECK(!remesa_gem_job_cancel(memory,t));
 for(unsigned role=1;role<=2;role++){
  unsigned char data[4]={1,2,3,4};
  CHECK(request(REMESA_MEM_WRITE,ids[role],0,0,data,sizeof data)==-EPERM);
  CHECK(request(REMESA_MEM_LEASE,ids[role],1,0,NULL,0)==-EPERM);
  CHECK(request(REMESA_MEM_SYNC,ids[role],0,0,NULL,0)==-EPERM);
  CHECK(!request(REMESA_MEM_LEASE,ids[role],0,0,NULL,0));
  CHECK(!request(REMESA_MEM_READ,ids[role],0,0,NULL,4));
  CHECK(remesa_private_session_release(memory,&token)==-EBUSY);
  CHECK(!request(REMESA_MEM_UNLEASE,ids[role],0,0,NULL,0));
 }
 /* Normal input upload still works; only the exact two generations are sealed. */
 unsigned char byte=0x5a;
 CHECK(!request(REMESA_MEM_LEASE,ids[0],1,0,NULL,0));
 CHECK(!request(REMESA_MEM_WRITE,ids[0],0,0,&byte,1));
 CHECK(!request(REMESA_MEM_UNLEASE,ids[0],0,0,NULL,0));
 t=dpm_job();j=ticket_job(memory,t);
 CHECK(!remesa_gem_job_claim_zero_dpm(memory,t,ids[2],ids[1])&&!sealed_job(t));
 j->dpm_mask=0;CHECK(sealed_job(t)==-EACCES);j->dpm_mask=6;
 unsigned id=memory->private_dpm_ids[0];memory->private_dpm_ids[0]^=32;
 CHECK(sealed_job(t)==-EACCES);memory->private_dpm_ids[0]=id;
 /* Model possible GPU publication only to distinguish ownership from cold
  * initialization: an ownership seal never restores the zero credential. */
 memory->bo[ids[1]&31].zero_pristine=false;memory->bo[ids[2]&31].zero_pristine=false;
 CHECK(!sealed_job(t));CHECK(!remesa_gem_job_cancel(memory,t));
 t=dpm_job();CHECK(remesa_gem_job_claim_zero_dpm(memory,t,ids[2],ids[1])==-ENODATA);
 CHECK(!remesa_gem_job_cancel(memory,t)&&test_zeroed_pages==zeroed);
 memory->private_source_held=true;CHECK(remesa_private_session_release(memory,&token)==-EBUSY);
 memory->private_source_held=false;
 memory->private_dpm_ids[1]=0;CHECK(remesa_private_session_release(memory,&token)==-EIO);
 CHECK(memory->private_dpm_ids[0]==ids[2]);memory->private_dpm_ids[1]=ids[1];
 memory->bo[ids[2]&31].generation++;CHECK(remesa_private_session_release(memory,&token)==-EIO);
 memory->bo[ids[2]&31].generation--;
 CHECK(!remesa_private_session_release(memory,&token));
 CHECK(!memory->private_dpm_ids[0]&&!memory->private_dpm_ids[1]);
 CHECK(!request(REMESA_MEM_LEASE,ids[2],1,0,NULL,0));
 CHECK(!request(REMESA_MEM_UNLEASE,ids[2],0,0,NULL,0));
 CHECK(!remesa_gem_memory_close(memory));endpoint=memory=NULL;
 CHECK(!gems&&!pins&&!ptes&&!device_refs&&!module_refs&&!power&&!claimed);
 printf("PASS %u actual-core general DPM seal/lease/identity/rollback checks; no warm admission or additional pool clear; GPU0\n",checks);
 return 0;
}
