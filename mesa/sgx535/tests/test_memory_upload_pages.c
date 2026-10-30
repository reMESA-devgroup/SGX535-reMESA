/* SPDX-License-Identifier: MIT. Actual C provider, modeled kernel; GPU0. */
#include "remesa_linux_memory.h"
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

int __wrap_ioctl(int,unsigned long,...);
static unsigned next_id,memcalls,upload_calls,sessioncalls,fault_at,fault_kind;
static bool uploading;
static unsigned char source[8192],backing[16384];
static const struct remesa_persistent_ops *ops;
int __wrap_ioctl(int fd,unsigned long command,...)
{
 va_list args;va_start(args,command);void *p=va_arg(args,void *);va_end(args);assert(fd==99);
 if(command==DRM_IOCTL_REMESA_PRIVATE_SESSION_LAB){
  struct remesa_private_session_lab_request *r=p;assert(r->action==1&&r->context==77&&r->max_jobs==2);sessioncalls++;
  r->session=1;r->state=1;r->stages=16;r->core=0x01130000;r->revision=0x00010201;return 0;
 }
 assert(command==DRM_IOCTL_REMESA_MEMORY);memcalls++;
 struct remesa_memory_request *r=p;assert(r->version==1&&r->context==77);
 if(uploading){
  upload_calls++;assert(r->id==32);
  assert(r->action==REMESA_MEM_LEASE || r->action==REMESA_MEM_WRITE || r->action==REMESA_MEM_UNLEASE);
  if(upload_calls==1)assert(r->action==REMESA_MEM_LEASE&&r->flags==1);
  else if(upload_calls<=3){
   assert(r->action==REMESA_MEM_WRITE&&r->offset==4096*(upload_calls-1)&&r->length==4096);
   assert(!memcmp(r->data,source+4096*(upload_calls-2),4096));
  }else assert(upload_calls==4&&r->action==REMESA_MEM_UNLEASE);
  if(upload_calls==fault_at){
   if(fault_kind==1){errno=EBUSY;return -1;}
   if(fault_kind==2){errno=EINTR;return -1;}
   r->id++;return 0;
  }
  if(r->action==REMESA_MEM_WRITE)memcpy(backing+r->offset,r->data,4096);
  return 0;
 }
 if(r->action==REMESA_MEM_CREATE)r->id=32+next_id++;
 if(r->action==REMESA_MEM_INFO){
  assert(r->id>=32&&r->id<43);unsigned i=r->id-32;
  struct remesa_memory_info info={.allocated_bytes=11*16384,.object_bytes=16384,.gpu_va=0x40000000u+i*0x10000u,
   .objects=11,.pinned_objects=11,.mapped_pages=44,.object_pinned=1,.object_mapped_pages=4,.verified_pages=4};
  memcpy(r->data,&info,sizeof(info));
 }
 return 0;
}
static struct remesa_linux_memory *setup(void)
{
 next_id=memcalls=upload_calls=sessioncalls=0;uploading=false;
 memset(backing,0xa5,sizeof(backing));
 struct remesa_linux_memory *d=NULL;assert(!remesa_linux_memory_attach(99,77,&d));
 for(unsigned i=0;i<11;i++){uint32_t id;assert(!ops->create(d,16384,&id)&&id==32+i);assert(!ops->pin(d,id));assert(!ops->gpu_map(d,id,0x40000000u+i*0x10000u,16384));}
 return d;
}
static void check_untouched(void)
{for(unsigned i=0;i<4096;i++)assert(backing[i]==0xa5&&backing[12288+i]==0xa5);}
int main(void)
{
 ops=remesa_linux_memory_ops();for(unsigned i=0;i<sizeof(source);i++)source[i]=(unsigned char)(i^(i>>8));
 for(unsigned active=0;active<2;active++)for(fault_at=0;fault_at<=4;fault_at++)for(fault_kind=1;fault_kind<=3;fault_kind++){
  struct remesa_linux_memory *d=setup();struct remesa_private_session_lab_request r={0};
  uint32_t ids[11];for(unsigned i=0;i<11;i++)ids[i]=32+i;
  if(active)assert(!remesa_linux_memory_private_lab_open(d,ids,2,&r));
  unsigned before=memcalls;uploading=true;
  int e=remesa_linux_memory_upload_pages(d,32,4096,source,sizeof(source));
  if(!fault_at){
   assert(!e&&upload_calls==4&&memcalls==before+4&&!memcmp(backing+4096,source,sizeof(source)));check_untouched();
  }else{
   assert(upload_calls==fault_at&&memcalls==before+fault_at);check_untouched();
   if(fault_at==1&&fault_kind==1){
    assert(e==EBUSY); /* Explicit lease denial, before any writes. */
   }else{
    assert(e==EOWNERDEAD);before=memcalls;
    assert(remesa_linux_memory_upload_pages(d,32,4096,source,sizeof(source))==EOWNERDEAD);
    assert(remesa_linux_memory_private_lab_submit(d,10000,&r)==EOWNERDEAD);
    assert(remesa_linux_memory_private_lab_close(d,&r)==EOWNERDEAD);
    assert(remesa_linux_memory_detach(d)==EOWNERDEAD);
    for(unsigned i=0;i<11;i++)assert(ops->unpin(d,ids[i])==EOWNERDEAD&&ops->destroy(d,ids[i])==EOWNERDEAD);
    assert(memcalls==before&&sessioncalls==active);
   }
  }
  uploading=false;remesa_linux_memory_forget(d); /* Host model only, not retirement. */
 }
 fault_at=0;struct remesa_linux_memory *d=setup();unsigned before=memcalls;
 assert(remesa_linux_memory_upload_pages(NULL,32,4096,source,8192)==EINVAL);
 assert(remesa_linux_memory_upload_pages(d,32,4096,NULL,8192)==EINVAL);
 assert(remesa_linux_memory_upload_pages(d,32,4096,source,0)==EINVAL);
 assert(remesa_linux_memory_upload_pages(d,32,4097,source,4096)==EINVAL);
 assert(remesa_linux_memory_upload_pages(d,32,4096,source,4095)==EINVAL);
 assert(remesa_linux_memory_upload_pages(d,32,12288,source,8192)==EINVAL);
 assert(remesa_linux_memory_upload_pages(d,32,UINT64_MAX-4095,source,8192)==EINVAL);
 assert(remesa_linux_memory_upload_pages(d,999,4096,source,8192)==EINVAL);
 assert(remesa_linux_memory_upload_pages(d,32,4096,(void*)(UINTPTR_MAX-4095),8192)==EINVAL);
 assert(memcalls==before);
 void *cpu=NULL;assert(!ops->cpu_map(d,32,true,&cpu));before=memcalls;
 assert(remesa_linux_memory_upload_pages(d,32,4096,source,8192)==EBUSY&&memcalls==before);
 assert(!ops->cpu_unmap(d,32,true));
 uint32_t ids[11];for(unsigned i=0;i<11;i++)ids[i]=32+i;
 struct remesa_private_session_lab_request r={0};assert(!remesa_linux_memory_private_lab_open(d,ids,2,&r));before=memcalls;
 assert(remesa_linux_memory_upload_pages(d,33,4096,source,8192)==EPERM&&memcalls==before&&sessioncalls==1);
 uploading=true;assert(!remesa_linux_memory_upload_pages(d,32,4096,source,8192)&&upload_calls==4&&memcalls==before+4&&sessioncalls==1);
 uploading=false;remesa_linux_memory_forget(d);
 puts("PASS page upload: no READ/SYNC/INFO, exact two-page offsets and untouched guards, 24 lease/write/unlease fault cases, range/overflow/lease/PDS-role gates; kernel modeled; GPU0");return 0;
}
