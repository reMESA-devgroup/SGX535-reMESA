/* SPDX-License-Identifier: MIT. Syscall model only; synthetic small BOs/GPU0. */
#include "remesa_linux_memory.h"
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
static unsigned created,memcalls,scenecalls,fault;
static struct remesa_scene_probe_request raw_response;
int __wrap_ioctl(int fd,unsigned long cmd,...)
{
 assert(fd==99);va_list ap;va_start(ap,cmd);void *arg=va_arg(ap,void *);va_end(ap);
 if(cmd==DRM_IOCTL_REMESA_MEMORY){
  memcalls++;struct remesa_memory_request *r=arg;
  if(r->action==REMESA_MEM_OPEN)r->context=77;
  else if(r->action==REMESA_MEM_CREATE)r->id=32+created++;
  return 0;
 }
 assert(cmd==DRM_IOCTL_REMESA_SCENE_PROBE);scenecalls++;
 struct remesa_scene_probe_request *r=arg;assert(r->version==1&&r->profile==5&&r->context==77&&r->budget_ms==1000);
 for(unsigned i=0;i<17;i++)assert(r->ids[i]==32+i);
 struct remesa_scene_probe_request check=*r;memset(&check,0,88);
 unsigned char zero[sizeof(check)]={0};assert(!memcmp(&check,zero,sizeof(check)));
 r->operation=r->cookie=1;r->core=0x01130000;r->revision=0x10201;r->pds_base=0x20000000;r->phase=9;r->ledger=7;r->stages=15;
 if(fault>=1&&fault<=4){raw_response=*r;errno=fault==1?EINTR:fault==2?EBUSY:fault==3?EOPNOTSUPP:EFAULT;return -1;}
 switch(fault){
 case 5:r->version++;break;case 6:r->profile++;break;case 7:r->context++;break;
 case 8:r->ids[16]++;break;case 9:r->budget_ms++;break;case 10:r->operation=0;break;
 case 11:r->cookie++;break;case 12:r->core++;break;case 13:r->revision++;break;
 case 14:r->pds_base++;break;case 15:r->phase++;break;case 16:r->ledger++;break;
 case 17:r->error=-EIO;break;case 18:r->retained=1;break;case 19:r->stages^=1;break;
 case 20:r->reserved[0]=1;break;case 21:r->reserved[1]=1;break;
 }
 raw_response=*r;return fault==22?1:0;
}
static const struct remesa_persistent_ops *ops;
static struct remesa_scene_packet packet(void)
{
 struct remesa_scene_packet p={.owner=77,.serial=1,.version=1,.binding_count=17,.use_count=10,.window_count=2};
 for(unsigned i=0;i<17;i++){p.bindings[i].id=32+i;p.bindings[i].access=i<12?1:3;}
 p.actions=(struct remesa_scene_packet_actions){1,32,1280,800,46,47,48,0};
 p.controls=(struct remesa_scene_packet_controls){2,80,1280,800,1280,1280,4,0x3f800000,44,45,32,0,96,42,0,68,43,0,148,1};
 for(unsigned i=0;i<10;i++)p.uses[i]=(struct remesa_scene_packet_use){32+i,0,8,4,0,0};
 for(unsigned i=0;i<2;i++)p.windows[i]=(struct remesa_scene_packet_window){33,0,6,34,0,144,3,0};
 assert(remesa_scene_packet_valid(&p));return p;
}
static struct remesa_linux_memory *setup(void)
{
 struct remesa_linux_memory *d=NULL;created=memcalls=scenecalls=0;assert(!remesa_linux_memory_attach(99,77,&d));
 for(unsigned i=0;i<17;i++){uint32_t id;assert(!ops->create(d,4096,&id)&&id==32+i);assert(!ops->pin(d,id));assert(!ops->gpu_map(d,id,0x40000000u+i*4096u,4096));}
 return d;
}
static void cleanup(struct remesa_linux_memory *d)
{
 for(unsigned i=0;i<17;i++){assert(!ops->gpu_unmap(d,32+i,0x40000000u+i*4096u,4096));assert(!ops->unpin(d,32+i));assert(!ops->destroy(d,32+i));}
 assert(!remesa_linux_memory_detach(d));
}
static int record_memory(void *p,const struct remesa_memory_request *a,const struct remesa_memory_request *b,int result,int error,bool before)
{(void)p;(void)a;(void)b;(void)result;(void)error;(void)before;return 0;}
int main(void)
{
 ops=remesa_linux_memory_ops();
 for(fault=0;fault<=22;fault++){
  struct remesa_linux_memory *d=setup();struct remesa_scene_packet p=packet();
  struct remesa_scene_probe_request out;memset(&out,0xa5,sizeof out);unsigned before=memcalls;
  int e=remesa_linux_memory_scene_lab_submit(d,&p,5,1000,&out);
  assert(e==(fault?EOWNERDEAD:0)&&scenecalls==1&&memcalls==before&&!memcmp(&out,&raw_response,sizeof out));
  if(!fault){assert(remesa_linux_memory_scene_lab_submit(d,&p,5,1000,&out)==EALREADY&&scenecalls==1);cleanup(d);}
  else{
   assert(remesa_linux_memory_scene_lab_submit(d,&p,5,1000,&out)==EOWNERDEAD&&scenecalls==1);
   assert(remesa_linux_memory_detach(d)==EOWNERDEAD);
   for(unsigned i=0;i<17;i++){void *cpu=NULL;assert(ops->gpu_unmap(d,32+i,0x40000000u+i*4096u,4096)==EOWNERDEAD);assert(ops->unpin(d,32+i)==EOWNERDEAD&&ops->destroy(d,32+i)==EOWNERDEAD);assert(ops->cpu_map(d,32+i,false,&cpu)==EOWNERDEAD&&!cpu);}
   uint32_t id=0;assert(ops->create(d,4096,&id)==ENOSPC&&!id);assert(memcalls==before);
   remesa_linux_memory_forget(d);
  }
 }
 fault=0;struct remesa_linux_memory *d=setup();struct remesa_scene_packet p=packet();
 struct remesa_scene_probe_request out,saved;memset(&out,0xa5,sizeof out);saved=out;
 for(unsigned f=0;f<14;f++){
  struct remesa_scene_packet bad=p;unsigned profile=5,budget=1000;
  switch(f){case 0:profile=0;break;case 1:profile=7;break;case 2:budget=0;break;case 3:budget=10001;break;
   case 4:bad.owner++;break;case 5:bad.serial=0;break;case 6:bad.bindings[0].id=900;break;
   case 7:bad.bindings[0].id=bad.bindings[1].id;break;case 8:bad.controls.depth_finalize=0;break;
   case 9:bad.controls.ta_bytes=68;break;case 10:bad.controls.height=640;break;
   case 11:bad.use_count=11;break;case 12:bad.window_count=3;break;case 13:bad.binding_count=18;break;}
  assert(remesa_linux_memory_scene_lab_submit(d,&bad,profile,budget,&out)==EINVAL&&!memcmp(&out,&saved,sizeof out));
 }
 assert(remesa_linux_memory_scene_lab_submit(d,&p,5,1000,(void *)&p)==EINVAL);
 assert(remesa_linux_memory_scene_lab_submit(d,&p,5,1000,(void *)d)==EINVAL);
 assert(remesa_linux_memory_scene_lab_submit(d,&p,5,1000,(void *)(UINTPTR_MAX-7))==EINVAL);
 assert(!scenecalls);void *cpu=NULL;assert(!ops->cpu_map(d,32,false,&cpu)&&cpu);
 assert(remesa_linux_memory_scene_lab_submit(d,&p,5,1000,&out)==EBUSY&&!scenecalls);
 assert(!ops->cpu_unmap(d,32,false));assert(!remesa_linux_memory_scene_lab_submit(d,&p,5,1000,&out));cleanup(d);
 const struct remesa_memory_recorder recorder={record_memory,NULL};d=NULL;
 assert(!remesa_linux_memory_begin_recorded(99,&d,&recorder));unsigned before_calls=scenecalls;
 assert(remesa_linux_memory_scene_lab_submit(d,&p,5,1000,&out)==EOPNOTSUPP&&scenecalls==before_calls);
 assert(!remesa_linux_memory_detach(d));
 puts("PASS scene capture resource transport: single ioctl, complete immutable/identity/retirement receipt, all-resource poison on22 uncertain responses, local metadata/lease/alias refusal; synthetic backing/GPU0");
}
