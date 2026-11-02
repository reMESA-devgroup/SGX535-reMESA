/* SPDX-License-Identifier: MIT. Interposed provider syscalls only; GPU0. */
#include "remesa_linux_memory.h"
#include <assert.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned calls,memcalls,next_id,action_fault,byte_fault,io_fault;
static uint32_t ids[18];
static const struct remesa_persistent_ops *ops;
int __wrap_ioctl(int,unsigned long,...);
int __wrap_ioctl(int fd,unsigned long command,...)
{
 va_list args;va_start(args,command);void *data=va_arg(args,void *);va_end(args);assert(fd==99);
 if(command==DRM_IOCTL_REMESA_GENERAL_SESSION){
  struct remesa_general_session_request *r=data;assert(remesa_general_session_request_valid(r));calls++;
  r->state=1;
  if(r->action==1){r->session=7;r->stages=16;}
  else if(r->action==2){r->operation=r->cookie=100+r->sequence;r->phase=9;r->ledger=7;r->stages=15;}
  else{r->state=3;r->stages=32;}
  if(r->action!=3){r->core=0x01130000;r->revision=0x10201;r->pds_base=0x20000000;}
  if(r->action==action_fault){
   if(io_fault){errno=io_fault;return -1;}
   ((unsigned char *)r)[byte_fault]^=1;
  }
  return 0;
 }
 assert(command==DRM_IOCTL_REMESA_MEMORY);struct remesa_memory_request *r=data;memcalls++;
 assert(r->context==77&&r->version==1);
 if(r->action==REMESA_MEM_CREATE){assert(next_id<18);r->id=ids[next_id++];}
 if(r->action==REMESA_MEM_INFO){
  unsigned i;for(i=0;i<18&&ids[i]!=r->id;i++);assert(i<18);
  struct remesa_memory_info v={.allocated_bytes=18*4096,.object_bytes=4096,
   .gpu_va=0x40000000u+i*4096,.objects=18,.pinned_objects=18,.mapped_pages=18,
   .object_pinned=1,.object_mapped_pages=1,.verified_pages=1};memcpy(r->data,&v,sizeof(v));
 }
 return 0;
}
static struct remesa_linux_memory *setup(void)
{
 struct remesa_linux_memory *d=NULL;calls=memcalls=next_id=0;
 assert(!remesa_linux_memory_attach(99,77,&d));
 for(unsigned i=0;i<18;i++){
  uint32_t id;assert(!ops->create(d,4096,&id)&&id==ids[i]);assert(!ops->pin(d,id));
  assert(!ops->gpu_map(d,id,0x40000000u+i*4096,4096));
 }
 return d;
}
static void poisoned(struct remesa_linux_memory *d)
{
 unsigned c=calls,m=memcalls;struct remesa_general_session_request r;uint32_t id=0;
 assert(remesa_linux_memory_general_open(d,ids,5,3,&r)==EOWNERDEAD);
 assert(remesa_linux_memory_general_submit(d,1000,&r)==EOWNERDEAD);
 assert(remesa_linux_memory_general_close(d,&r)==EOWNERDEAD);
 assert(remesa_linux_memory_detach(d)==EOWNERDEAD);
 assert(ops->create(d,4096,&id)==ENOSPC&&!id);
 for(unsigned i=0;i<18;i++)assert(ops->unpin(d,ids[i])==EOWNERDEAD&&ops->destroy(d,ids[i])==EOWNERDEAD);
 assert(calls==c&&memcalls==m);remesa_linux_memory_forget(d);
}
static void invoke_fault(void)
{
 struct remesa_linux_memory *d=setup();struct remesa_general_session_request r;
 int e=remesa_linux_memory_general_open(d,ids,5,3,&r);
 if(action_fault!=1){assert(!e);e=remesa_linux_memory_general_submit(d,1000,&r);}
 if(action_fault==3){assert(!e);e=remesa_linux_memory_general_close(d,&r);}
 assert(e==EOWNERDEAD);poisoned(d);
}
int main(void)
{
 ops=remesa_linux_memory_ops();for(unsigned i=0;i<18;i++)ids[i]=32+i;
 unsigned cases=0;
 for(action_fault=1;action_fault<=3;action_fault++){
  for(byte_fault=0;byte_fault<sizeof(struct remesa_general_session_request);byte_fault++){
   /* OPEN session is server-issued: any nonzero value is a valid handle. */
   if(action_fault==1&&byte_fault>=16&&byte_fault<24)continue;
   invoke_fault();cases++;
  }
  static const int errors[]={EINTR,EIO,EINVAL,ETIMEDOUT};
  for(unsigned i=0;i<4;i++){io_fault=errors[i];invoke_fault();cases++;}io_fault=0;
 }
 action_fault=0;struct remesa_linux_memory *d=setup();struct remesa_general_session_request r;
 assert(remesa_linux_memory_general_open(d,ids,2,3,&r)==EINVAL&&!calls);
 assert(remesa_linux_memory_general_open(d,ids,5,65,&r)==EINVAL&&!calls);
 uint32_t saved=ids[17];ids[17]=ids[0];assert(remesa_linux_memory_general_open(d,ids,5,3,&r)==EINVAL&&!calls);ids[17]=saved;
 assert(!remesa_linux_memory_general_open(d,ids,6,3,&r));unsigned m=memcalls;
 unsigned c=calls;
 assert(remesa_linux_memory_general_submit(d,0,&r)==EINVAL);
 assert(remesa_linux_memory_general_submit(d,10001,&r)==EINVAL&&calls==c&&memcalls==m);
 assert(remesa_linux_memory_general_open(d,ids,6,3,&r)==EALREADY);
 struct remesa_private_session_lab_request legacy;
 assert(remesa_linux_memory_private_lab_submit(d,1000,&legacy)==EOPNOTSUPP);
 assert(remesa_linux_memory_private_lab_close(d,&legacy)==EOPNOTSUPP);
 for(unsigned i=0;i<3;i++){
  assert(!remesa_linux_memory_general_submit(d,1000,&r));assert(r.operation==101+i&&r.sequence==i+1);
 }
 assert(remesa_linux_memory_general_submit(d,1000,&r)==ENOSPC);
 assert(!remesa_linux_memory_general_close(d,&r)&&r.state==3&&calls==5&&memcalls==m);
 assert(remesa_linux_memory_general_close(d,&r)==ENOENT);
 for(unsigned i=0;i<18;i++){
  assert(!ops->gpu_unmap(d,ids[i],0x40000000u+i*4096,4096));
  assert(!ops->unpin(d,ids[i])&&!ops->destroy(d,ids[i]));
 }
 assert(!remesa_linux_memory_detach(d));
 printf("PASS %u byte/errno fault cases plus bounded general transport sequence, legacy separation, zero per-SUBMIT memory queries; GPU0\n",cases);
 return 0;
}
