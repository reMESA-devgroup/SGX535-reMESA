/* SPDX-License-Identifier: MIT. Interposed syscall lifecycle tests, GPU0. */
#include "remesa_linux_memory.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
int __wrap_ioctl(int,unsigned long,...);
static unsigned mode,copies,memcalls,next_id,records;
static int copy_ioctl(int fd,struct remesa_twod_copy_request *r)
{
 assert(fd==99&&r->version==1&&r->context==77);copies++;
 if(mode==1||mode==2||mode==3||mode==16){errno=mode==1?EOPNOTSUPP:mode==2?EINTR:mode==3?EFAULT:ENOMEM;return -1;}
 r->operation=mode==23?8+copies:8;r->cookie=9;r->stages=31;r->core=0x01130000;r->revision=0x00010201;
 if(mode==4)return 1;
 if(mode==5)r->stages=15;
 if(mode==6)r->source.id++;
 if(mode==7)r->core++;
 if(mode==8)r->revision++;
 if(mode==9)r->response_reserved=1;
 if(mode==10)r->cookie=0;
 if(mode==11)r->operation=0;
 if(mode==12){r->error=-EIO;r->retained=1;r->stages=3;}
 if(mode==13||mode==14){r->stages=1;r->cookie=r->core=r->revision=0;r->error=mode==13?-ENOMEM:INT_MIN;}
 if(mode==15){errno=EBUSY;return -1;}
 if(mode==17)r->error=EIO;
 if(mode==18)r->version++;
 if(mode==19)r->width++;
 if(mode==20)r->retained=1;
 return 0;
}
int __wrap_ioctl(int fd,unsigned long command,...)
{
 va_list args;va_start(args,command);void *p=va_arg(args,void *);va_end(args);
 if(command==DRM_IOCTL_REMESA_TWOD_COPY)return copy_ioctl(fd,p);
 assert(fd==99&&command==DRM_IOCTL_REMESA_MEMORY);struct remesa_memory_request *r=p;memcalls++;
 if(r->action==REMESA_MEM_OPEN){assert(!r->context);r->context=77;return 0;}
 assert(r->version==1&&r->context==77);
 if(r->action==REMESA_MEM_CREATE)r->id=next_id++;
 return 0;
}
static int record(void *p,const struct remesa_memory_request *a,const struct remesa_memory_request *b,int result,int error,bool before)
{(void)p;(void)a;(void)b;(void)result;(void)error;(void)before;records++;return 0;}
static struct remesa_twod_copy_request request(unsigned a,unsigned b)
{return (struct remesa_twod_copy_request){.source={a,18,18,72},.destination={b,18,18,72},.source_x=1,.source_y=1,.destination_x=1,.destination_y=1,.width=16,.height=16,.budget_ms=1000};}
int main(void)
{
 const struct remesa_persistent_ops *ops=remesa_linux_memory_ops();
 for(mode=0;mode<=23;mode++){
  struct remesa_linux_memory *d=NULL;next_id=32;copies=memcalls=records=0;
  if(mode==21){const struct remesa_memory_recorder r={record,NULL};assert(!remesa_linux_memory_begin_recorded(99,&d,&r)&&records==2);}
  else assert(!remesa_linux_memory_attach(99,77,&d));
  uint32_t a,b;assert(!ops->create(d,4096,&a)&&!ops->create(d,4096,&b));assert(!ops->pin(d,a)&&!ops->pin(d,b));
  assert(!ops->gpu_map(d,a,0x40001000,4096)&&!ops->gpu_map(d,b,0x41000000,4096));
  struct remesa_twod_copy_request r=request(a,b),saved=r;
  unsigned m=memcalls;
  assert(remesa_linux_memory_copy_2d(d,(void *)((unsigned char *)&r+1))==EINVAL);
  r.context=77;assert(remesa_linux_memory_copy_2d(d,&r)==EINVAL);r=saved;
  r.source.id=999;assert(remesa_linux_memory_copy_2d(d,&r)==EINVAL);r=saved;
  assert(!copies&&memcalls==m);
  if(!mode){
   void *cpu=NULL;assert(!ops->cpu_map(d,a,false,&cpu)&&cpu);m=memcalls;
   assert(remesa_linux_memory_copy_2d(d,&r)==EBUSY&&!copies&&memcalls==m);
   assert(!ops->cpu_unmap(d,a,false));
  }
  m=memcalls;int error=remesa_linux_memory_copy_2d(d,&r);assert(memcalls==m);
  assert(copies==(mode==21?0u:1u));
  if(mode==22){
   assert(!error);r=saved;assert(remesa_linux_memory_copy_2d(d,&r)==EOWNERDEAD&&copies==2&&memcalls==m);
   assert(ops->unpin(d,a)==EOWNERDEAD&&ops->unpin(d,b)==EOWNERDEAD);remesa_linux_memory_forget(d);continue;
  }
  if(mode==23)for(unsigned repeat=0;repeat<9;repeat++){
   r=saved;assert(!remesa_linux_memory_copy_2d(d,&r)&&r.operation==10+repeat&&memcalls==m);
  }
  int safe=!mode||mode==1||mode==13||mode==16||mode==21||mode==23;
  if(!mode||mode==23)assert(!error&&r.stages==31&&r.operation==(mode==23?18u:8u)&&r.cookie==9&&r.context==77);
  else if(mode==1||mode==21)assert(error==EOPNOTSUPP&&!memcmp(&r,&saved,sizeof r));
  else if(mode==13||mode==16)assert(error==ENOMEM);
  else{
   assert(error==EOWNERDEAD&&!memcmp(&r,&saved,sizeof r));
   assert(ops->unpin(d,a)==EOWNERDEAD&&ops->unpin(d,b)==EOWNERDEAD&&memcalls==m);
   assert(remesa_linux_memory_copy_2d(d,&r)==EOWNERDEAD&&copies==1);
   assert(remesa_linux_memory_detach(d)==EBUSY);
   remesa_linux_memory_forget(d);continue; /* Host metadata only, no forced cleanup. */
  }
  assert(safe);assert(!ops->gpu_unmap(d,a,0x40001000,4096)&&!ops->gpu_unmap(d,b,0x41000000,4096));
  assert(!ops->unpin(d,a)&&!ops->unpin(d,b)&&!ops->destroy(d,a)&&!ops->destroy(d,b));assert(!remesa_linux_memory_detach(d));
 }
 puts("PASS native-copy transport: one ioctl/no retry, no INFO/readback/hash, exact receipts, lease/ID/alignment refusal, malformed/error poisoning, known-unissued cancellation and recorder exclusion; GPU0");return 0;
}
