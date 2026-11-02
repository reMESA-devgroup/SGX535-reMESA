/* SPDX-License-Identifier: MIT. Actual transport with modeled syscalls/GPU0. */
#define main inherited_scene_transport_tests
#define __wrap_ioctl inherited_scene_ioctl
#include "test_scene_memory_transport.c"
#undef __wrap_ioctl
#undef main
static unsigned gears_calls,trace_calls,gears_profile=1,trace_fault;
static struct remesa_gears_scene_probe_request gears_raw;
static struct remesa_gears_scene_trace trace_raw;
int __wrap_ioctl(int fd,unsigned long cmd,...)
{
 va_list ap;va_start(ap,cmd);void *arg=va_arg(ap,void *);va_end(ap);assert(fd==99);
 if(cmd==DRM_IOCTL_REMESA_GEARS_SCENE_PROBE){
  gears_calls++;struct remesa_gears_scene_probe_request *r=arg;
  assert(r->version==2&&r->profile==gears_profile&&r->context==77&&r->budget_ms==1000&&!r->reserved_input);
  for(unsigned i=0;i<18;i++)assert(r->ids[i]==32+i);
  unsigned char zero[56]={0};assert(!memcmp((const unsigned char *)r+96,zero,56));
  r->operation=r->cookie=1;r->core=0x01130000;r->revision=0x10201;r->pds_base=0x20000000;r->phase=9;r->ledger=7;r->stages=15;
  if(fault>=1&&fault<=4){gears_raw=*r;errno=fault==1?EINTR:fault==2?EBUSY:fault==3?EOPNOTSUPP:EFAULT;return -1;}
  switch(fault){
   case 5:r->version++;break;case 6:r->profile++;break;case 7:r->context++;break;
   case 8:r->ids[17]++;break;case 9:r->budget_ms++;break;case 10:r->operation=0;break;
   case 11:r->cookie++;break;case 12:r->core++;break;case 13:r->revision++;break;
   case 14:r->pds_base++;break;case 15:r->phase++;break;case 16:r->ledger++;break;
   case 17:r->error=-EIO;break;case 18:r->retained=1;break;case 19:r->stages^=1;break;
   case 20:r->reserved[0]=1;break;case 21:r->reserved[1]=1;break;
   case 23:r->reserved_input=1;break;case 24:r->ids[0]++;break;
   case 25:gears_raw=*r;errno=ENOTTY;return -1;
  }
  gears_raw=*r;return fault==22?1:0;
 }
 if(cmd==DRM_IOCTL_REMESA_GEARS_SCENE_TRACE){
  trace_calls++;struct remesa_gears_scene_trace_request *q=arg;struct remesa_gears_scene_trace zero={0};
  assert(q->version==2&&!q->reserved&&q->context==77&&!memcmp(&q->trace,&zero,sizeof zero));
  struct remesa_scene_probe_trace *t=&q->trace.prefix;
  t->version=2;t->bytes=4280;t->context=77;t->operation=t->cookie=1;t->profile=gears_profile;t->stages=15;t->phase=9;t->ledger=7;
  t->binding_count=18;t->event_count=2;t->load_count=29;t->ta_count=31;t->raster_count=20;
  for(unsigned i=0;i<18;i++){struct remesa_scene_trace_binding *b=i<17?&t->bindings[i]:&q->trace.last_binding;*b=(struct remesa_scene_trace_binding){32+i,i<12||i==17?1:3,0x40000000u+i*4096u,4096};}
  switch(trace_fault){
   case 1:q->version++;break;case 2:q->reserved=1;break;case 3:q->context++;break;
   case 4:t->version++;break;case 5:t->bytes--;break;case 6:t->context++;break;
   case 7:t->operation++;break;case 8:t->cookie++;break;case 9:t->profile++;break;
   case 10:t->stages^=1;break;case 11:t->phase++;break;case 12:t->ledger++;break;
   case 13:t->error=-EIO;break;case 14:t->retained=1;break;case 15:t->reserved=1;break;
   case 16:t->binding_count=17;break;case 17:t->event_count=65;break;case 18:t->load_count--;break;
   case 19:t->ta_count--;break;case 20:t->raster_count--;break;
   case 21:q->trace.last_binding.id++;break;case 22:q->trace.last_binding.address++;break;
   case 23:q->trace.last_binding.bytes++;break;case 24:q->trace.last_binding.access^=2;break;
   case 25:t->bindings[0].id++;break;
   case 27:trace_raw=q->trace;errno=EINTR;return -1;
  }
  trace_raw=q->trace;return trace_fault==26?1:0;
 }
 return inherited_scene_ioctl(fd,cmd,arg);
}
static struct remesa_scene_packet gears_packet(void)
{
 struct remesa_scene_packet p=packet();p.binding_count=18;p.use_count=11;
 p.bindings[17].id=49;p.bindings[17].access=1;p.uses[10]=(struct remesa_scene_packet_use){49,0,8,4,0,0};p.controls.ta_bytes=104;
 if(!(gears_profile&1)){p.actions.width=p.actions.height=640;p.controls.width=p.controls.height=p.controls.color_pitch=p.controls.depth_pitch=640;}
 assert(remesa_scene_packet_valid(&p));return p;
}
static struct remesa_linux_memory *gears_setup(void)
{
 struct remesa_linux_memory *d=setup();gears_calls=trace_calls=0;uint32_t id;
 assert(!ops->create(d,4096,&id)&&id==49&&!ops->pin(d,id)&&!ops->gpu_map(d,id,0x40011000,4096));return d;
}
static void gears_cleanup(struct remesa_linux_memory *d)
{assert(!ops->gpu_unmap(d,49,0x40011000,4096)&&!ops->unpin(d,49)&&!ops->destroy(d,49));cleanup(d);}
int main(void)
{
 _Static_assert(sizeof(struct remesa_gears_scene_probe_request)==152,"request ABI");
 _Static_assert(sizeof(struct remesa_gears_scene_trace_request)==4296,"trace ABI");
 ops=remesa_linux_memory_ops();
 for(gears_profile=1;gears_profile<=4;gears_profile++)for(fault=0;fault<=25;fault++){
  struct remesa_linux_memory *d=gears_setup();struct remesa_scene_packet p=gears_packet();struct remesa_gears_scene_probe_request out;unsigned before=memcalls;
  assert(remesa_linux_memory_gears_scene_lab_response(d,&out)==ENODATA);
  int e=remesa_linux_memory_gears_scene_lab_submit(d,&p,gears_profile,1000,&out);
  assert(e==(fault?EOWNERDEAD:0)&&gears_calls==1&&memcalls==before&&!memcmp(&out,&gears_raw,sizeof out));
  assert(!remesa_linux_memory_gears_scene_lab_response(d,&out)&&!memcmp(&out,&gears_raw,sizeof out));
  struct remesa_scene_probe_request old;assert(remesa_linux_memory_scene_lab_response(d,&old)==ENODATA);
  if(!fault){assert(remesa_linux_memory_gears_scene_lab_submit(d,&p,gears_profile,1000,&out)==EALREADY);gears_cleanup(d);}
  else{
   assert(remesa_linux_memory_gears_scene_lab_submit(d,&p,gears_profile,1000,&out)==EOWNERDEAD&&gears_calls==1);
   struct remesa_gears_scene_trace t;assert(remesa_linux_memory_gears_scene_lab_trace(d,&t)==EOWNERDEAD&&!trace_calls);
   for(unsigned i=0;i<18;i++){void *cpu=NULL;assert(ops->gpu_unmap(d,32+i,0x40000000u+i*4096u,4096)==EOWNERDEAD&&ops->unpin(d,32+i)==EOWNERDEAD&&ops->destroy(d,32+i)==EOWNERDEAD&&ops->cpu_map(d,32+i,false,&cpu)==EOWNERDEAD&&!cpu);}
   assert(remesa_linux_memory_detach(d)==EOWNERDEAD&&memcalls==before);remesa_linux_memory_forget(d);
  }
 }
 fault=0;gears_profile=1;
 for(gears_profile=1;gears_profile<=4;gears_profile++)for(trace_fault=0;trace_fault<=27;trace_fault++){
  struct remesa_linux_memory *d=gears_setup();struct remesa_scene_packet p=gears_packet();struct remesa_gears_scene_probe_request out;
  struct remesa_gears_scene_trace t,saved;memset(&t,0xa5,sizeof t);saved=t;
  assert(remesa_linux_memory_gears_scene_lab_trace(d,&t)==ENODATA&&!memcmp(&t,&saved,sizeof t)&&!trace_calls);
  assert(!remesa_linux_memory_gears_scene_lab_submit(d,&p,gears_profile,1000,&out));unsigned before=memcalls;
  assert(remesa_linux_memory_gears_scene_lab_trace(d,(void *)d)==EINVAL&&!trace_calls);
  int e=remesa_linux_memory_gears_scene_lab_trace(d,&t);
  assert(e==(trace_fault==27?EINTR:trace_fault==26?EIO:trace_fault?EPROTO:0)&&trace_calls==1&&memcalls==before&&!memcmp(&t,&trace_raw,sizeof t));
  gears_cleanup(d); /* Read-only diagnostics do not undo proven retirement. */
 }
 trace_fault=0;
 for(gears_profile=1;gears_profile<=4;gears_profile++)for(fault=0;fault<=25;fault++){
  struct remesa_linux_memory *d=gears_setup();struct remesa_scene_packet p=gears_packet();struct remesa_gpu_scene_submit job={77,19,&p};struct remesa_gpu_scene_receipt receipt={0},saved=receipt;
  enum remesa_dispatch e=gears_profile<=2?remesa_linux_memory_gears_scene_lab_dispatch(d,&job,&receipt):remesa_linux_memory_gears_homogeneous_lab_dispatch(d,&job,&receipt);
  assert(e==(fault?REMESA_ISSUANCE_AMBIGUOUS:REMESA_ISSUED)&&gears_calls==1);
  if(fault){assert(!memcmp(&receipt,&saved,sizeof receipt));remesa_linux_memory_forget(d);}
  else{assert(receipt.context==77&&receipt.operation==19&&receipt.retired&&receipt.published&&receipt.acknowledged);gears_cleanup(d);}
 }
 fault=0;gears_profile=1;struct remesa_linux_memory *d=gears_setup();struct remesa_scene_packet p=gears_packet();struct remesa_gears_scene_probe_request out,saved;memset(&out,0xa5,sizeof out);saved=out;
 for(unsigned k=0;k<16;k++){
  struct remesa_scene_packet bad=p;unsigned profile=1,budget=1000;
  switch(k){case 0:profile=0;break;case 1:profile=5;break;case 2:budget=0;break;case 3:budget=10001;break;case 4:bad.owner++;break;case 5:bad.serial=0;break;case 6:bad.bindings[17].id=900;break;case 7:bad.bindings[17].id=32;break;case 8:bad.controls.depth_finalize=0;break;case 9:bad.controls.ta_bytes=96;break;case 10:bad.controls.height=640;break;case 11:bad.use_count=10;break;case 12:bad.window_count=3;break;case 13:bad.binding_count=17;break;case 14:bad.controls.schema=1;break;case 15:bad.controls.width=800;break;}
  assert(remesa_linux_memory_gears_scene_lab_submit(d,&bad,profile,budget,&out)==EINVAL&&!memcmp(&out,&saved,sizeof out)&&!gears_calls);
 }
 assert(remesa_linux_memory_gears_scene_lab_submit(d,&p,1,1000,(void *)&p)==EINVAL);
 assert(remesa_linux_memory_gears_scene_lab_submit(d,&p,1,1000,(void *)d)==EINVAL);
 assert(remesa_linux_memory_gears_scene_lab_submit(d,&p,1,1000,(void *)(UINTPTR_MAX-7))==EINVAL);
 void *cpu=NULL;assert(!ops->cpu_map(d,49,false,&cpu)&&cpu);assert(remesa_linux_memory_gears_scene_lab_submit(d,&p,1,1000,&out)==EBUSY&&!gears_calls);assert(!ops->cpu_unmap(d,49,false));
 struct remesa_scene_probe_request old;struct remesa_scene_packet legacy=packet();assert(!remesa_linux_memory_scene_lab_submit(d,&legacy,5,1000,&old));
 assert(remesa_linux_memory_gears_scene_lab_submit(d,&p,1,1000,&out)==EALREADY&&!gears_calls&&remesa_linux_memory_gears_scene_lab_response(d,&out)==ENODATA);gears_cleanup(d);
 puts("PASS separate gears request152/trace4296,18-owner single ioctl+all-resource poison,26 response/28 trace cases, explicit protocol separation/immutable metadata/last owner/local refusal; GPU0");return 0;
}
