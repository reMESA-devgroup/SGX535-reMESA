/* SPDX-License-Identifier: MIT. Interposed syscalls only; GPU0. */
#include "remesa_linux_memory.h"
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int __wrap_ioctl(int,unsigned long,...);
static unsigned memcalls,sessioncalls,next_id,mode,fault_action,records;
static uint64_t context=77;
static uint32_t ids[11];
static struct remesa_private_session_lab_request wire_req[4],wire_res[4];
static bool replay;
static const struct remesa_persistent_ops *ops;
static void reply(struct remesa_private_session_lab_request *r)
{
 r->state=1;
 if(r->action==1){r->session=1;r->stages=16;}
 else if(r->action==2){r->operation=r->sequence+1;r->cookie=r->operation;r->phase=9;r->ledger=7;r->stages=15;}
 else {r->state=3;r->stages=32;return;}
 r->core=0x01130000;r->revision=0x00010201;
}
int __wrap_ioctl(int fd,unsigned long command,...)
{
 va_list args;va_start(args,command);void *p=va_arg(args,void *);va_end(args);assert(fd==99);
 if(command==DRM_IOCTL_REMESA_PRIVATE_SESSION_LAB){
  struct remesa_private_session_lab_request *r=p;unsigned action=r->action;
  assert(action>=1&&action<=3&&r->version==1&&r->context==context);sessioncalls++;
  if(replay){assert(sessioncalls<=4&&!memcmp(r,&wire_req[sessioncalls-1],sizeof(*r)));*r=wire_res[sessioncalls-1];return 0;}
  struct remesa_private_session_lab_request check=*r;check.version=check.action=0;check.context=check.session=check.sequence=0;
  memset(check.ids,0,sizeof(check.ids));check.budget_ms=check.max_jobs=0;
  unsigned char zero[sizeof(check)]={0};assert(!memcmp(&check,zero,sizeof(check)));
  if(action==1){assert(!r->session&&!r->sequence&&r->max_jobs==2&&!r->budget_ms);assert(!memcmp(r->ids,ids,sizeof(ids)));}
  else{assert(r->session==1&&!r->max_jobs);for(unsigned i=0;i<11;i++)assert(!r->ids[i]);}
  reply(r);
  if(action!=fault_action || !mode)return 0;
  if(mode<=4){errno=mode==1?EINTR:mode==2?EBUSY:mode==3?EOPNOTSUPP:EFAULT;return -1;}
  switch(mode){
  case 5:return 1;
  case 6:r->version++;break;case 7:r->action++;break;case 8:r->context++;break;
  case 9:r->session=action==1?0:r->session+1;break;
  case 10:r->sequence++;break;case 11:r->ids[4]++;break;case 12:r->budget_ms++;break;
  case 13:r->max_jobs++;break;case 14:r->flags=1;break;case 15:r->operation=0;break;
  case 16:r->cookie++;break;case 17:r->core++;break;case 18:r->revision++;break;
  case 19:r->phase++;break;case 20:r->ledger++;break;case 21:r->error=-EIO;break;
  case 22:r->state=2;break;case 23:r->stages^=1;break;case 24:r->reserved=1;break;
  case 25:r->operation=r->cookie=2;break;
  case 26:r->operation=r->cookie=3;break;
  default:abort();
  }
  return 0;
 }
 assert(command==DRM_IOCTL_REMESA_MEMORY);memcalls++;struct remesa_memory_request *r=p;
 if(r->action==REMESA_MEM_OPEN){assert(r->version==1&&!r->context);r->context=context;return 0;}
 assert(r->context==context&&r->version==1);
 if(r->action==REMESA_MEM_CREATE){assert(next_id<11);r->id=ids[next_id++];}
 if(r->action==REMESA_MEM_INFO){
  unsigned i;for(i=0;i<11;i++)if(ids[i]==r->id)break;assert(i<11);
  struct remesa_memory_info info={.allocated_bytes=11*4096,.object_bytes=4096,.gpu_va=0x40000000u+i*4096u,
   .objects=11,.pinned_objects=11,.mapped_pages=11,.object_pinned=1,.object_mapped_pages=1,.verified_pages=1};
  memcpy(r->data,&info,sizeof(info));
 }
 return 0;
}
static int record(void *p,const struct remesa_memory_request *a,const struct remesa_memory_request *b,int result,int error,bool before)
{(void)p;(void)a;(void)b;(void)result;(void)error;(void)before;records++;return 0;}
static struct remesa_linux_memory *setup(void)
{
 struct remesa_linux_memory *d=NULL;next_id=memcalls=sessioncalls=records=0;
 assert(!remesa_linux_memory_attach(99,context,&d));
 for(unsigned i=0;i<11;i++){uint32_t id;assert(!ops->create(d,4096,&id)&&id==ids[i]);assert(!ops->pin(d,id));assert(!ops->gpu_map(d,id,0x40000000u+i*4096u,4096));}
 return d;
}
static void cleanup(struct remesa_linux_memory *d)
{
 for(unsigned i=0;i<11;i++){assert(!ops->gpu_unmap(d,ids[i],0x40000000u+i*4096u,4096));assert(!ops->unpin(d,ids[i]));assert(!ops->destroy(d,ids[i]));}
 assert(!remesa_linux_memory_detach(d));
}
static void uncertain(struct remesa_linux_memory *d)
{
 unsigned s=sessioncalls,m=memcalls;struct remesa_private_session_lab_request r={0};uint32_t id=0;
 assert(remesa_linux_memory_private_lab_open(d,ids,2,&r)==EOWNERDEAD);
 assert(remesa_linux_memory_private_lab_submit(d,10000,&r)==EOWNERDEAD);
 assert(remesa_linux_memory_private_lab_close(d,&r)==EOWNERDEAD);
 struct remesa_twod_copy_request copy={0};struct remesa_twod_fill_request fill={0};
 assert(remesa_linux_memory_copy_2d(d,&copy)==EOWNERDEAD);
 assert(remesa_linux_memory_fill_2d(d,&fill)==EOWNERDEAD);
 assert(remesa_linux_memory_detach(d)==EOWNERDEAD);
 assert(ops->create(d,4096,&id)==ENOSPC&&!id);
 for(unsigned i=0;i<11;i++){
  assert(ops->unpin(d,ids[i])==EOWNERDEAD&&ops->destroy(d,ids[i])==EOWNERDEAD);
  assert(ops->gpu_unmap(d,ids[i],0x40000000u+i*4096u,4096)==EOWNERDEAD);
  void *cpu=NULL;assert(ops->cpu_map(d,ids[i],false,&cpu)==EOWNERDEAD&&!cpu);
 }
 assert(s==sessioncalls&&m==memcalls);remesa_linux_memory_forget(d);
}
static void load_wire(const char *directory)
{
 const char *names[]={"OPEN","frame-000001","frame-000002","CLOSE"};char path[4096];
 for(unsigned i=0;i<4;i++)for(unsigned k=0;k<2;k++){
  int n=snprintf(path,sizeof(path),"%s/%s-%s.bin",directory,names[i],k?"RESPONSE":"REQUEST");assert(n>0&&(unsigned)n<sizeof(path));
  FILE *f=fopen(path,"rb");assert(f);assert(fread(k?&wire_res[i]:&wire_req[i],1,sizeof(wire_req[i]),f)==sizeof(wire_req[i]));assert(fgetc(f)==EOF);assert(!fclose(f));
 }
 context=wire_req[0].context;memcpy(ids,wire_req[0].ids,sizeof(ids));
}
int main(int argc,char **argv)
{
 ops=remesa_linux_memory_ops();for(unsigned i=0;i<11;i++)ids[i]=32+i;
 struct remesa_private_session_lab_request r={0},saved;unsigned cases=0;
 /* Every prefix field, receipt and transport failure must retain ownership. */
 for(fault_action=1;fault_action<=3;fault_action++)for(mode=1;mode<=24;mode++){
  if(mode==15&&fault_action!=2)continue; /* Zero operation is correct for OPEN/CLOSE. */
  struct remesa_linux_memory *d=setup();int e=remesa_linux_memory_private_lab_open(d,ids,2,&r);
  if(fault_action!=1){assert(!e);e=remesa_linux_memory_private_lab_submit(d,10000,&r);}
  if(fault_action==3){assert(!e);e=remesa_linux_memory_private_lab_close(d,&r);}
  assert(e==EOWNERDEAD);uncertain(d);cases++;
 }
 /* Duplicate/reversed per-job operation cannot be accepted as fresh retirement. */
 for(mode=25;mode<=26;mode++){
  fault_action=2;struct remesa_linux_memory *d=setup();assert(!remesa_linux_memory_private_lab_open(d,ids,2,&r));
  assert(!remesa_linux_memory_private_lab_submit(d,10000,&r));assert(remesa_linux_memory_private_lab_submit(d,10000,&r)==EOWNERDEAD);uncertain(d);cases++;
 }
 mode=fault_action=0;struct remesa_linux_memory *d=setup();unsigned m=memcalls;
 memset(&r,0xa5,sizeof(r));saved=r;
 uint32_t bad[11];memcpy(bad,ids,sizeof(bad));bad[10]=bad[0];
 assert(remesa_linux_memory_private_lab_open(d,bad,2,&r)==EINVAL);
 bad[10]=9999;assert(remesa_linux_memory_private_lab_open(d,bad,2,&r)==EINVAL);
 assert(remesa_linux_memory_private_lab_open(d,ids,0,&r)==EINVAL);
 assert(remesa_linux_memory_private_lab_open(d,ids,65,&r)==EINVAL);
 assert(remesa_linux_memory_private_lab_open(d,ids,2,(void*)((unsigned char*)&r+1))==EINVAL);
 assert(remesa_linux_memory_private_lab_submit(d,10000,&r)==ENOENT);
 assert(remesa_linux_memory_private_lab_close(d,&r)==ENOENT);
 assert(!memcmp(&r,&saved,sizeof(r))&&memcalls==m&&!sessioncalls);
 void *cpu=NULL;assert(!ops->cpu_map(d,ids[0],false,&cpu));m=memcalls;
 assert(remesa_linux_memory_private_lab_open(d,ids,2,&r)==EBUSY&&memcalls==m&&!sessioncalls);
 assert(!ops->cpu_unmap(d,ids[0],false));m=memcalls;
 assert(!remesa_linux_memory_private_lab_open(d,ids,2,&r)&&memcalls==m+11&&sessioncalls==1);m=memcalls;
 assert(remesa_linux_memory_private_lab_open(d,ids,2,&r)==EALREADY&&memcalls==m&&sessioncalls==1);
 assert(remesa_linux_memory_detach(d)==EBUSY);
 struct remesa_twod_copy_request copy={0};struct remesa_twod_fill_request fill={0};
 assert(remesa_linux_memory_copy_2d(d,&copy)==EBUSY&&memcalls==m&&sessioncalls==1);
 assert(remesa_linux_memory_fill_2d(d,&fill)==EBUSY&&memcalls==m&&sessioncalls==1);
 assert(remesa_linux_memory_private_lab_submit(d,0,&r)==EINVAL);
 assert(remesa_linux_memory_private_lab_submit(d,10001,&r)==EINVAL);
 assert(!ops->cpu_map(d,ids[0],false,&cpu));m=memcalls;
 assert(remesa_linux_memory_private_lab_submit(d,10000,&r)==EBUSY&&memcalls==m&&sessioncalls==1);
 assert(remesa_linux_memory_private_lab_close(d,&r)==EBUSY&&memcalls==m&&sessioncalls==1);
 assert(!ops->cpu_unmap(d,ids[0],false));m=memcalls;
 assert(!remesa_linux_memory_private_lab_submit(d,10000,&r)&&r.operation==2&&sessioncalls==2&&memcalls==m);
 assert(!remesa_linux_memory_private_lab_submit(d,10000,&r)&&r.operation==3&&sessioncalls==3&&memcalls==m);
 assert(remesa_linux_memory_private_lab_submit(d,10000,&r)==ENOSPC&&sessioncalls==3&&memcalls==m);
 assert(!remesa_linux_memory_private_lab_close(d,&r)&&r.state==3&&r.stages==32&&sessioncalls==4&&memcalls==m);
 assert(remesa_linux_memory_private_lab_open(d,ids,2,&r)==EALREADY&&sessioncalls==4&&memcalls==m);
 assert(remesa_linux_memory_private_lab_submit(d,10000,&r)==ENOENT);
 assert(remesa_linux_memory_private_lab_close(d,&r)==ENOENT);cleanup(d);
 d=setup();assert(!remesa_linux_memory_private_lab_open(d,ids,2,&r));m=memcalls;
 assert(!remesa_linux_memory_private_lab_close(d,&r)&&r.sequence==0&&sessioncalls==2&&memcalls==m);cleanup(d);
 /* Recorder-only endpoint cannot silently omit the distinct LAB ABI. */
 const struct remesa_memory_recorder rec={record,NULL};d=NULL;
 assert(!remesa_linux_memory_begin_recorded(99,&d,&rec));m=memcalls;
 assert(remesa_linux_memory_private_lab_open(d,ids,2,&r)==EOPNOTSUPP&&memcalls==m);
 assert(!remesa_linux_memory_detach(d));
 printf("PASS %u private-session fault scenarios, ownership retention, one-query OPEN and zero diagnostic I/O per SUBMIT; GPU0\n",cases);
 if(argc==2){
  load_wire(argv[1]);replay=true;d=setup();assert(!remesa_linux_memory_private_lab_open(d,ids,2,&r));m=memcalls;
  assert(!remesa_linux_memory_private_lab_submit(d,10000,&r));assert(!remesa_linux_memory_private_lab_submit(d,10000,&r));
  assert(!remesa_linux_memory_private_lab_close(d,&r)&&sessioncalls==4&&memcalls==m);cleanup(d);
  puts("PASS exact physical OPEN/SUBMIT/SUBMIT/CLOSE wire replay through C transport; memory metadata modeled, no physical execution");
 }
 return 0;
}
