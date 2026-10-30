/* SPDX-License-Identifier: GPL-2.0-only */
/* Full actual memory service + private lease/seal helpers. GEM/MMU/PM/vmap
 * are injected CPU APIs; synthetic input authentication is a caller assumption.
 * No source or executor callback is permitted; no GPU program is qualified. */
#define REMESA_MEMORY_TEST
#define REMESA_PRIVATE_SESSION_SOURCE_TEST
#include "test_gem_shim.h"
#include "remesa_gem_memory.h"
struct remesa_gem_private {struct remesa_gem_memory *memory;struct remesa_memory_ticket ticket;};
struct sgx535_gma500_owner;struct sgx535_gma500_fixed_backend;struct remesa_stream_observation;
static int remesa_stream_source_claim(struct drm_device *d,const void *p){(void)d;(void)p;assert(0);return -EIO;}
static int remesa_stream_source_settled(const void *p){(void)p;assert(0);return -EIO;}
static int remesa_stream_source_release(const void *p){(void)p;assert(0);return -EIO;}
static void remesa_stream_source_poison(const void *p){(void)p;assert(0);}
static int remesa_gem_private_execute(struct remesa_gem_private *l,struct sgx535_gma500_owner *o,
 struct sgx535_gma500_fixed_backend *b,struct remesa_stream_observation *a,const void *p,
 const struct remesa_memory_view *v,unsigned n,unsigned t,unsigned k)
{(void)l;(void)o;(void)b;(void)a;(void)p;(void)v;(void)n;(void)t;(void)k;assert(0);return -EIO;}
#include "private_memory_actual.c"
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
/* Only the policy's consumed metadata is modeled; the guard implementation
 * is copied byte-exact from the physically retained profile, not rewritten. */
struct remesa_stream_batch {unsigned source_offset,normal_variant;};
#include "remesa_pds_write_guard.h"
static unsigned char canonical[131072];
static const struct remesa_stream_batch batch={0,0};
static int guarded(u64 offset,const void *data,unsigned n)
{return remesa_canonical_pds_page(&batch,1,canonical,offset,data,n);}
static struct drm_file file={1};static unsigned token,other;
static struct test_mmu mmu={true,{0}};
static struct drm_psb_private priv={&mmu,(void*)1,{0x28000000,32,0xe0000000}};
static struct drm_device dev={&priv};static unsigned checks;
#define CHECK(x) do{checks++;assert(x);}while(0)
static u32 ids[12];static struct remesa_memory_binding uses[11];
static struct remesa_gem_memory *memory;
static int request(unsigned action,u32 id,unsigned flags,u64 offset,const void *data,unsigned n)
{
 struct remesa_memory_request r={.version=1,.action=action,.id=id,.context=77,
  .flags=flags,.offset=offset,.length=n};
 if(data)memcpy(r.data,data,n);
 return remesa_gem_memory_request(memory,&file,&r);
}
static void setup(void)
{
 CHECK(!remesa_gem_memory_open(&dev,&file,77,&memory));endpoint=memory;
 for(unsigned i=0;i<12;i++){
  u64 size=i?4096:131072,va=0x40001000ULL+i*0x100000;
  struct remesa_memory_request r={.version=1,.action=REMESA_MEM_CREATE,.context=77,.bytes=size};
  CHECK(!remesa_gem_memory_request(memory,&file,&r));ids[i]=r.id;
  CHECK(!request(REMESA_MEM_PIN,r.id,0,0,NULL,0));
  r=(struct remesa_memory_request){.version=1,.action=REMESA_MEM_MAP,.context=77,.id=ids[i],.va=va};
  CHECK(!remesa_gem_memory_request(memory,&file,&r));
  if(i<11)uses[i]=(struct remesa_memory_binding){ids[i],va,size};
 }
 struct remesa_gem_memory *out=NULL;CHECK(!remesa_private_session_acquire(&dev,&file,77,&token,&out)&&out==memory);
}
static struct remesa_memory_ticket job(void)
{
 struct remesa_memory_ticket t={0};u32 command=0;
 CHECK(!remesa_gem_job_prepare(memory,uses,11,&command,sizeof command,&t));return t;
}
static int check_inputs(struct remesa_memory_ticket t)
{mutex_lock(&memory->lock);int e=remesa_private_session_inputs_locked(memory,t);mutex_unlock(&memory->lock);return e;}
static void finish(void)
{
 CHECK(!remesa_private_session_release(memory,&token));
 CHECK(!memory->private_session&&!memory->pds_write_guard);
 for(unsigned i=0;i<5;i++)CHECK(!memory->private_input_ids[i]&&!memory->bo[ids[i]&31].input_seal);
 CHECK(!remesa_gem_memory_close(memory));endpoint=memory=NULL;
 CHECK(!gems&&!pins&&!ptes&&!device_refs&&!module_refs&&!power&&!claimed);
}
int main(void)
{
 setup();struct remesa_memory_ticket t=job(),bad=t;bad.generation++;
 CHECK(remesa_private_session_seal_inputs(memory,&other,t,guarded)==-EACCES);
 CHECK(remesa_private_session_seal_inputs(memory,&token,bad,guarded)==-EBUSY);
 CHECK(remesa_private_session_seal_inputs(memory,&token,t,NULL)==-EINVAL);
 memory->bo[ids[2]&31].input_seal=2;
 CHECK(remesa_private_session_seal_inputs(memory,&token,t,guarded)==-EBUSY);
 CHECK(!memory->private_input_ids[0]&&!memory->pds_write_guard&&!memory->bo[ids[0]&31].input_seal);
 memory->bo[ids[2]&31].input_seal=0;
 CHECK(!remesa_private_session_seal_inputs(memory,&token,t,guarded));
 CHECK(remesa_private_session_seal_inputs(memory,&token,t,guarded)==-EBUSY);
 CHECK(!check_inputs(t));
 CHECK(request(REMESA_MEM_LEASE,ids[0],1,0,NULL,0)==-EBUSY);
 CHECK(!remesa_gem_job_cancel(memory,t));
 for(unsigned i=1;i<5;i++)CHECK(request(REMESA_MEM_LEASE,ids[i],1,0,NULL,0)==-EPERM);
 CHECK(!request(REMESA_MEM_LEASE,ids[1],0,0,NULL,0));
 CHECK(!request(REMESA_MEM_READ,ids[1],0,0,NULL,4096));
 CHECK(!request(REMESA_MEM_UNLEASE,ids[1],0,0,NULL,0));
 CHECK(request(REMESA_MEM_UNMAP,ids[0],0,0,NULL,0)==-EBUSY);
 CHECK(!request(REMESA_MEM_LEASE,ids[0],1,0,NULL,0));
 unsigned char data[4096]={0},saved[4096];
 data[68+2]=0x80;data[68+3]=0x3f; /* bounded dynamic float1 */
 CHECK(!request(REMESA_MEM_WRITE,ids[0],0,0,data,4096));memcpy(saved,data,4096);
 CHECK(!memcmp(memory->bo[ids[0]&31].cpu,saved,4096));
 data[0]=1;CHECK(request(REMESA_MEM_WRITE,ids[0],0,0,data,4096)==-EKEYREJECTED);
 CHECK(!memcmp(memory->bo[ids[0]&31].cpu,saved,4096));data[0]=0;
 data[70]=0xc0;data[71]=0x7f;CHECK(request(REMESA_MEM_WRITE,ids[0],0,0,data,4096)==-ERANGE);
 CHECK(!memcmp(memory->bo[ids[0]&31].cpu,saved,4096));
 CHECK(request(REMESA_MEM_WRITE,ids[0],0,4,saved,4096)==-EINVAL);
 CHECK(request(REMESA_MEM_WRITE,ids[0],0,0,saved,4)==-EINVAL);
 CHECK(!request(REMESA_MEM_UNLEASE,ids[0],0,0,NULL,0));
 t=job();CHECK(!check_inputs(t));CHECK(remesa_private_session_release(memory,&token)==-EBUSY);
 CHECK(!remesa_gem_job_cancel(memory,t));
 struct remesa_memory_binding original=uses[1];uses[1]=(struct remesa_memory_binding){ids[11],0x40b01000,4096};
 t=job();CHECK(check_inputs(t)==-EACCES);
 /* Integration check: simulate an already held source token, but forbid all
  * source/executor callbacks. The real wrapper must reject the swapped input
  * before ever consulting the modeled cold executor. No retirement is faked. */
 memory->private_source_held=true;
 struct remesa_gem_private loan={memory,t};
 CHECK(remesa_private_session_execute(memory,&token,&loan,(void*)1,(void*)1,(void*)1,(void*)1,11,10,20)==-EACCES);
 CHECK(!memory->private_source_poisoned);
 memory->private_source_held=false;
 CHECK(!remesa_gem_job_cancel(memory,t));uses[1]=original;
 t=job();CHECK(!check_inputs(t));CHECK(!remesa_gem_job_cancel(memory,t));
 /* Corrupted final seal refuses the entire unseal transaction. */
 memory->bo[ids[4]&31].input_seal=0;
 CHECK(remesa_private_session_release(memory,&token)==-EIO);
 CHECK(memory->bo[ids[0]&31].input_seal==1&&memory->pds_write_guard&&memory->private_session);
 memory->bo[ids[4]&31].input_seal=2;finish();
 pid_t pid=fork();CHECK(pid>=0);
 if(!pid){
  setup();t=job();CHECK(!remesa_private_session_seal_inputs(memory,&token,t,guarded));
  CHECK(!remesa_gem_job_cancel(memory,t));remesa_private_memory_file_lost(&dev,&file);
  CHECK(remesa_private_session_release(memory,&token)==-ENODEV);
  CHECK(memory->private_input_ids[0]==ids[0]&&memory->bo[ids[0]&31].input_seal==1);
  CHECK(remesa_gem_memory_close(memory)==-ENODEV);_exit(0); /* retained CPU model, not freed as retired */
 }
 int status;CHECK(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
 printf("PASS %u full memory-service/private input-seal checks; GEM/MMU/PM and authentication modeled; GPU0\n",checks);
 return 0;
}
