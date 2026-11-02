/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual backing lease/close/admission/loss fragments; metadata and mutexes
 * modeled. No source claim, hardware work or completion injection. */
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
typedef uint64_t u64;
typedef uint32_t u32;
#define OBJECTS 32
#define PAGE_SIZE 4096
#define THIS_MODULE NULL
#define REMESA_MEMORY_TEST
#define REMESA_RETAINED_IOCTL_TEST
enum {REMESA_JOB_PREPARED=1,REMESA_JOB_QUARANTINED=9,
 REMESA_MEM_LEASE=10,REMESA_MEM_READ,REMESA_MEM_WRITE,REMESA_MEM_UNLEASE,
 REMESA_MEM_SYNC,REMESA_MEM_CREATE,REMESA_MEM_UNMAP};
struct drm_device {unsigned id;};struct drm_file {unsigned id;};
struct mutex {unsigned rank;bool held;};
static unsigned rank,checks,cleanup;
#define CHECK(x) do {checks++;assert(x);} while(0)
static void mutex_lock(struct mutex *m)
{CHECK(!m->held&&((m->rank==1&&!rank)||(m->rank==2&&(rank==0||rank==1))));m->held=true;rank=m->rank;}
static void mutex_unlock(struct mutex *m)
{CHECK(m->held&&rank==m->rank);m->held=false;rank=0;}
/* Helpers always lock endpoint then memory; close locks memory alone. */
static bool endpoint_held;
static void lock(struct mutex *m)
{mutex_lock(m);if(m->rank==1)endpoint_held=true;}
static void unlock(struct mutex *m)
{mutex_unlock(m);if(m->rank==1)endpoint_held=false;else if(endpoint_held)rank=1;}
#define mutex_lock lock
#define mutex_unlock unlock
struct backing {bool live,lease,quarantine,pinned,reserved;unsigned holds,color_vmas,generation,input_seal,inserted;
 void *pages;struct {unsigned size;} gem;};
struct retained_job {bool live,attached,quarantined;unsigned state,count,ids[16];void *commands;};
struct resource {unsigned unused;};
struct remesa_gem_memory {
 struct drm_device *dev;struct drm_file *file;u64 context;struct mutex lock;
 const void *private_session;bool stream_bound,private_device_lost,excluded;
 bool private_source_held,private_source_poisoned;
 unsigned private_input_ids[5];int (*pds_write_guard)(u64,const void *,unsigned);
 unsigned private_work_ids[6];bool private_work_owned;
 unsigned private_dpm_ids[2];
 u64 private_dpm_retired_operation;bool private_dpm_ready;
 void *executor;struct backing bo[OBJECTS];struct retained_job jobs[8];struct resource exclude;
};
struct remesa_memory_request {unsigned action;};
static struct mutex endpoint_lock={1,false};
static struct remesa_gem_memory *endpoint;
static struct {bool initialized,attempted;} retained_entry;
static unsigned atomic_read(unsigned *n){return *n;}
static void drop_job_views(struct retained_job *p){(void)p;cleanup++;}
static void kfree(void *p){(void)p;cleanup++;}
static int unmap(struct remesa_gem_memory *s,struct backing *b){(void)s;(void)b;cleanup++;return 0;}
static void discard(struct remesa_gem_memory *s,struct backing *b){(void)s;(void)b;cleanup++;}
static void release_resource(struct resource *r){(void)r;cleanup++;}
static void sgx535_gma500_exclusive_release(struct drm_device *d){(void)d;cleanup++;}
static void gma_power_end(struct drm_device *d){(void)d;cleanup++;}
static void drm_dev_put(struct drm_device *d){(void)d;cleanup++;}
static void module_put(void *m){(void)m;cleanup++;}
#include "memory_close_actual.inc"
#include "session_gate_actual.inc"
#include "remesa_private_session.inc"
#include "remesa_private_memory_file_lost.inc"
static struct drm_device dev={1},otherdev={2};
static struct drm_file file={1},otherfile={2};
static struct remesa_gem_memory s;
static unsigned owner,otherowner;
static void fixture(void)
{
 CHECK(!rank&&!endpoint_held);
 s=(struct remesa_gem_memory){.dev=&dev,.file=&file,.context=77,.lock={2,false}};
 s.bo[0].live=true;endpoint=&s;cleanup=0;
 retained_entry.initialized=false;retained_entry.attempted=false;
}
static int acquire(struct remesa_gem_memory **out)
{return remesa_private_session_acquire(&dev,&file,77,&owner,out);}
int main(void)
{
 struct remesa_gem_memory *out=(void*)1;
 fixture();CHECK(acquire(NULL)==-EINVAL);
 CHECK(remesa_private_session_acquire(NULL,&file,77,&owner,&out)==-EINVAL&&!out);
 CHECK(remesa_private_session_acquire(&dev,NULL,77,&owner,&out)==-EINVAL&&!out);
 CHECK(remesa_private_session_acquire(&dev,&file,0,&owner,&out)==-EINVAL&&!out);
 CHECK(remesa_private_session_acquire(&dev,&file,77,NULL,&out)==-EINVAL&&!out);
 CHECK(remesa_private_session_acquire(&otherdev,&file,77,&owner,&out)==-EACCES&&!out);
 CHECK(remesa_private_session_acquire(&dev,&otherfile,77,&owner,&out)==-EACCES&&!out);
 CHECK(remesa_private_session_acquire(&dev,&file,78,&owner,&out)==-EACCES&&!out);
 endpoint=NULL;CHECK(acquire(&out)==-EACCES&&!out);
 for(unsigned fault=0;fault<10;fault++){
  fixture();
  switch(fault){case 0:s.stream_bound=true;break;case 1:s.executor=&owner;break;
   case 2:s.jobs[7].live=true;break;case 3:s.bo[31].lease=true;break;
   case 4:s.bo[31].holds=1;break;case 5:s.bo[31].quarantine=true;break;
   case 6:s.bo[31].color_vmas=1;break;case 7:s.private_device_lost=true;break;
   case 8:retained_entry.initialized=true;break;case 9:retained_entry.attempted=true;break;}
  CHECK(acquire(&out)==(fault==7?-ENODEV:-EBUSY)&&!out&&!s.private_session&&!cleanup);
 }
 for(unsigned i=0;i<2;i++){
  fixture();s.private_dpm_ids[i]=32;CHECK(acquire(&out)==-EBUSY&&!out);
 }
 fixture();CHECK(!acquire(&out)&&out==&s&&s.private_session==&owner);
 CHECK(remesa_private_memory_device_retained(&dev));
 CHECK(remesa_private_memory_rebind_blocked());
 CHECK(acquire(&out)==-EBUSY&&!out);
 CHECK(remesa_gem_memory_close(&s)==-EBUSY&&!cleanup);
 CHECK(remesa_private_session_release((void*)1,&owner)==-EACCES);
 CHECK(remesa_private_session_release(&s,&otherowner)==-EACCES);
 for(unsigned frame=0;frame<3;frame++){
  /* Job/lease metadata is modeled; actual admission body must reject it. */
  struct remesa_memory_request r={REMESA_MEM_CREATE};
  CHECK(session_gate(&s,&r)==-EBUSY);r.action=REMESA_MEM_UNMAP;
  CHECK(session_gate(&s,&r)==-EBUSY);
  for(unsigned action=REMESA_MEM_LEASE;action<=REMESA_MEM_SYNC;action++){
   r.action=action;CHECK(!session_gate(&s,&r));
   s.jobs[0].live=true;CHECK(session_gate(&s,&r)==-EBUSY);
   CHECK(remesa_private_session_release(&s,&owner)==-EBUSY);
   s.jobs[0].live=false;
  }
  s.bo[0].lease=true;CHECK(remesa_private_session_release(&s,&owner)==-EBUSY);
  s.bo[0].lease=false;CHECK(remesa_gem_memory_close(&s)==-EBUSY&&!cleanup);
 }
 for(unsigned fault=0;fault<4;fault++){
  if(fault==0)s.executor=&owner;
  if(fault==1)s.bo[0].holds=1;
  if(fault==2)s.bo[0].quarantine=true;
  if(fault==3)s.bo[0].color_vmas=1;
  CHECK(remesa_private_session_release(&s,&owner)==-EBUSY);
  s.executor=NULL;s.bo[0].holds=0;s.bo[0].quarantine=false;s.bo[0].color_vmas=0;
 }
 CHECK(!remesa_private_session_release(&s,&owner)&&!s.private_session&&!cleanup);
 CHECK(!remesa_private_memory_device_retained(&dev)&&!remesa_private_memory_rebind_blocked());
 CHECK(!remesa_gem_memory_close(&s)&&cleanup);
 /* Native postclose may already have cleared file after close refused. A
  * backing lease must still retain the device dependency graph and deny rebind. */
 fixture();CHECK(!acquire(&out));s.file=NULL;
 CHECK(remesa_private_memory_device_retained(&dev)&&remesa_private_memory_rebind_blocked());
 CHECK(remesa_private_session_release(&s,&owner)==-ENODEV);
 CHECK(remesa_gem_memory_close(&s)==-EBUSY&&!cleanup);
 for(unsigned loss=0;loss<2;loss++){
  fixture();CHECK(!acquire(&out));
  if(loss)remesa_private_memory_device_lost(&dev);
  else remesa_private_memory_file_lost(&dev,&file);
  CHECK(s.private_device_lost&&!s.file&&s.private_session==&owner&&s.bo[0].quarantine);
  CHECK(remesa_private_session_release(&s,&owner)==-ENODEV);
  CHECK(remesa_gem_memory_close(&s)==-ENODEV&&!cleanup);
 }
 CHECK(!rank&&!endpoint_held);printf("PASS %u backing-session metadata checks; GPU0\n",checks);
 return 0;
}
