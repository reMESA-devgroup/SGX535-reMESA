/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual retained ioctl and frozen 11-role envelope; downstream init/capture/
 * submit callbacks and locks are modeled. No DRM ioctl or GPU is invoked. */
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "remesa/retained_uapi.h"
typedef uint64_t u64;
#define REMESA_LAB_DEPTH1280_PROFILE
#define CAP_SYS_ADMIN 1
#define HZ 100u
#define SGX535_BO_COLOR 5u
#define SGX535_BO_Z 10u
#define REMESA_SEQUENCE_RETIRED 9u
#define remesa_retained_depth1280_images ((void*)1)
_Static_assert(REMESA_RETAINED_ROLES==11,"use the actual frozen depth profile envelope");
struct drm_device {unsigned id;};struct drm_file {unsigned id;};
struct mutex {bool held;};static struct mutex endpoint_lock;
static void mutex_lock(struct mutex *m){assert(!m->held);m->held=true;}
static void mutex_unlock(struct mutex *m){assert(m->held);m->held=false;}
struct remesa_gem_memory {
 struct drm_device *dev;struct drm_file *file;u64 context;
 struct {u64 end;} root;bool stream_bound;const void *private_session;
};
struct remesa_memory_binding {unsigned id;u64 va,bytes;};
struct remesa_retained_result {
 struct {struct {u64 operation;unsigned slot,generation;} ticket;u64 serial;} fence;
 unsigned state;int error;bool admitted,attached;
};
struct remesa_retained_submit {
 bool initialized,attempted,closing;
 struct {unsigned request_bytes,response_bytes;unsigned char request[640],response[4352];} snapshots;
};
static struct remesa_retained_submit retained_entry;
static struct {bool complete,ready;} depth_capture;
static u64 retained_mmu_end;
static struct remesa_gem_memory *endpoint;
static bool admin,admitted;static unsigned initializations,captures,submissions;
static bool capable(int cap){assert(cap==1);return admin;}
static bool remesa_retained_lab_admitted(void){return admitted;}
static int remesa_retained_submit_init(struct remesa_retained_submit *r,
 struct remesa_gem_memory *s,void *images,void *limit,unsigned rev,unsigned ticks,unsigned samples)
{
 assert(endpoint_lock.held&&r==&retained_entry&&s==endpoint&&images==(void*)1);
 assert(limit==&retained_mmu_end&&rev==0x00010201&&ticks==10*HZ&&samples==100000);
 initializations++;r->initialized=true;return 0;
}
static int depth1280_capture_prepare(struct drm_device *d,struct drm_file *f,u64 c,unsigned color,unsigned z)
{
 assert(endpoint_lock.held&&endpoint->dev==d&&endpoint->file==f&&endpoint->context==c);
 assert(color&&z);captures++;return 0;
}
static int remesa_retained_submit_once(struct remesa_retained_submit *r,
 const struct remesa_memory_binding *uses,unsigned n,const void *cmd,unsigned bytes,
 unsigned budget,struct remesa_retained_result *out)
{
 assert(endpoint_lock.held&&r==&retained_entry&&uses&&cmd&&n==11&&bytes==264&&budget==10000);
 submissions++;memset(out,0,sizeof(*out));out->admitted=true;return 0;
}
#include "retained_ioctl_actual.inc"
static struct drm_device dev={1};
static struct drm_file file={1};
#ifndef PRIVATE_ADMISSION_OLD_WITNESS
static struct drm_device otherdev={2};
static struct drm_file otherfile={2};
#endif
static struct remesa_gem_memory memory;
static struct remesa_retained_request request;
static void fixture(void)
{
 assert(!endpoint_lock.held);admin=admitted=true;initializations=captures=submissions=0;
 memset(&retained_entry,0,sizeof(retained_entry));memset(&depth_capture,0,sizeof(depth_capture));
 memory=(struct remesa_gem_memory){.dev=&dev,.file=&file,.context=77,.root={0xdfffffffULL}};endpoint=&memory;
 request=(struct remesa_retained_request){.version=REMESA_RETAINED_VERSION,.context=77};
 for(unsigned i=0;i<11;i++)request.bindings[i]=(struct remesa_retained_binding){.id=32+i,.va=0x40001000ULL+i*4096,.bytes=4096};
}
int main(void)
{
#ifdef PRIVATE_ADMISSION_OLD_WITNESS
 fixture();memory.private_session=&memory;
 assert(!remesa_retained_submit_ioctl(&dev,&request,&file));
 assert(initializations==1&&captures==1&&submissions==1&&!endpoint_lock.held);
 puts("WITNESS old retained ioctl enters init/capture/submit with an active private owner; modeled callbacks, GPU0");
#else
 for(unsigned f=0;f<9;f++){
  fixture();
  if(f==1)memory.private_session=&memory;
  if(f==2)memory.stream_bound=true;
  if(f==3)memory.file=&otherfile;
  if(f==4)memory.dev=&otherdev;
  if(f==5)request.context++;
  if(f==6)admitted=false;
  if(f==7)admin=false;
  if(f==8)request.reserved[0]=1;
  struct remesa_retained_request before=request;
  int code=remesa_retained_submit_ioctl(&dev,&request,&file);assert(!endpoint_lock.held);
  if(!f)assert(!code&&initializations==1&&captures==1&&submissions==1);
  else {
   assert(code<0&&!initializations&&!captures&&!submissions);
   assert(!memcmp(&before,&request,sizeof(request)));
   if(f==1||f==2)assert(code==-EBUSY);
  }
 }
 puts("PASS nine actual retained-admission cases; untouched envelope and zero downstream effects on denial; GPU0");
#endif
 return 0;
}
