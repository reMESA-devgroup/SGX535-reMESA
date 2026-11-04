/* SPDX-License-Identifier: MIT. Actual transport with interposed syscalls;
 * no device access, pixels or hardware receipts. */
#define main inherited_general_transport_tests
#define __wrap_ioctl inherited_general_ioctl
#include "test_general_session_transport.c"
#undef __wrap_ioctl
#undef main
static unsigned matrix_calls,mode,changed_byte;
static int reply_errno;
static struct remesa_matrix_submit_request raw;
int __wrap_ioctl(int fd,unsigned long command,...)
{
 va_list args;va_start(args,command);void *data=va_arg(args,void *);va_end(args);assert(fd==99);
 if(command!=DRM_IOCTL_REMESA_MATRIX_SUBMIT)return inherited_general_ioctl(fd,command,data);
 struct remesa_matrix_submit_request *r=data;assert(remesa_matrix_submit_request_valid(r));matrix_calls++;
 if(mode==2){raw=*r;errno=reply_errno;return -1;}
 if(r->flags==REMESA_MATRIX_CLOSE_SESSION){
  r->stages=8;if(mode==9)r->cookie=1;raw=*r;return 0;
 }
 if(mode==3||mode==4){r->error=-ENOMEM;r->stages=mode==3?8:9;r->operation=mode==3?0:100;raw=*r;return 0;}
 r->operation=r->cookie=100;r->core=0x01130000;r->revision=0x10201;r->pds_base=0x20000000;
 r->phase=9;r->ledger=7;r->stages=(r->flags&REMESA_MATRIX_KEEP_SESSION)?23:15;
 if(mode>=7)r->operation=r->cookie=mode==10?101:100+matrix_calls;
 if(mode==8)r->stages=15; /* Must not accept CLOSED for a KEEP request. */
 if(mode==1)((unsigned char *)r)[changed_byte]^=1;
 if(mode==5){r->error=-EOPNOTSUPP;r->retained=1;r->cookie=0;r->stages=1;r->phase=r->ledger=0;}
 raw=*r;return mode==6?1:0;
}
static struct remesa_scene_packet matrix_packet(void)
{
 struct remesa_scene_packet p={.owner=77,.serial=1,.version=1,.binding_count=18,.use_count=11,.window_count=2};
 for(unsigned i=0;i<18;i++){p.bindings[i].id=32+i;p.bindings[i].access=i<12||i==17?1:3;}
 p.actions=(struct remesa_scene_packet_actions){1,32,1280,800,46,47,48,0};
 p.controls=(struct remesa_scene_packet_controls){2,80,1280,800,1280,1280,4,0x3f800000,44,45,32,0,104,42,0,68,43,0,148,1};
 for(unsigned i=0;i<11;i++)p.uses[i]=(struct remesa_scene_packet_use){32+i,0,8,4,0,0};
 p.windows[0]=(struct remesa_scene_packet_window){33,0,6,34,0,48,3,0};
 p.windows[1]=(struct remesa_scene_packet_window){35,0,6,36,0,144,3,0};
 assert(remesa_scene_packet_valid(&p));return p;
}
static struct remesa_linux_memory *matrix_setup(void)
{matrix_calls=0;return setup();}
static void matrix_cleanup(struct remesa_linux_memory *d)
{
 for(unsigned i=0;i<18;i++){
  assert(!ops->gpu_unmap(d,ids[i],0x40000000u+i*4096,4096));
  assert(!ops->unpin(d,ids[i])&&!ops->destroy(d,ids[i]));
 }
 assert(!remesa_linux_memory_detach(d));
}
static void matrix_poisoned(struct remesa_linux_memory *d)
{
 struct remesa_matrix_submit_request r;struct remesa_scene_packet p=matrix_packet();unsigned c=matrix_calls,m=memcalls;uint32_t id=0;
 assert(remesa_linux_memory_matrix_submit(d,&p,1000,&r)==EOWNERDEAD);
 assert(!remesa_linux_memory_matrix_response(d,&r)&&!memcmp(&r,&raw,sizeof r));
 assert(ops->create(d,4096,&id)==ENOSPC&&!id);
 for(unsigned i=0;i<18;i++)assert(ops->unpin(d,ids[i])==EOWNERDEAD&&ops->destroy(d,ids[i])==EOWNERDEAD);
 assert(remesa_linux_memory_detach(d)==EOWNERDEAD&&matrix_calls==c&&memcalls==m);
 remesa_linux_memory_forget(d);
}
int main(void)
{
 ops=remesa_linux_memory_ops();for(unsigned i=0;i<18;i++)ids[i]=32+i;
 unsigned cases=0;
 mode=1;
 for(changed_byte=0;changed_byte<sizeof(struct remesa_matrix_submit_request);changed_byte++){
  struct remesa_linux_memory *d=matrix_setup();struct remesa_scene_packet p=matrix_packet();struct remesa_matrix_submit_request r;
  unsigned m=memcalls;assert(remesa_linux_memory_matrix_submit(d,&p,1000,&r)==EOWNERDEAD&&matrix_calls==1&&memcalls==m);
  assert(!memcmp(&r,&raw,sizeof r));matrix_poisoned(d);cases++;
 }
 const int errors[]={EINVAL,ENOTTY,EOPNOTSUPP,EPERM,EACCES,EBUSY,ENOMEM,ENOENT,ENOSPC,EINTR,EIO,ETIMEDOUT,EFAULT,ENODEV};
 mode=2;
 for(unsigned i=0;i<sizeof errors/sizeof errors[0];i++){
  reply_errno=errors[i];struct remesa_linux_memory *d=matrix_setup();struct remesa_scene_packet p=matrix_packet();struct remesa_matrix_submit_request r;
  int e=remesa_linux_memory_matrix_submit(d,&p,1000,&r);assert(matrix_calls==1);
  if(i<9){assert(e==errors[i]);assert(remesa_linux_memory_matrix_submit(d,&p,1000,&r)==EALREADY&&matrix_calls==1);matrix_cleanup(d);}
  else{assert(e==EOWNERDEAD);matrix_poisoned(d);}cases++;
 }
 for(mode=3;mode<=6;mode++){
  struct remesa_linux_memory *d=matrix_setup();struct remesa_scene_packet p=matrix_packet();struct remesa_matrix_submit_request r;
  int e=remesa_linux_memory_matrix_submit(d,&p,1000,&r);
  if(mode==3||mode==4){assert(e==ENOMEM);matrix_cleanup(d);}else{assert(e==EOWNERDEAD);matrix_poisoned(d);}cases++;
 }
 mode=0;struct remesa_linux_memory *d=matrix_setup();struct remesa_scene_packet p=matrix_packet(),saved=p;
 struct remesa_matrix_submit_request r;memset(&r,0xa5,sizeof r);struct remesa_matrix_submit_request before=r;
 assert(remesa_linux_memory_matrix_response(d,&r)==ENODATA&&!memcmp(&r,&before,sizeof r));
 assert(remesa_linux_memory_matrix_submit(d,&p,0,&r)==EINVAL&&!matrix_calls);
 assert(remesa_linux_memory_matrix_submit(d,&p,1000,(void *)&p)==EINVAL&&!memcmp(&p,&saved,sizeof p));
 p.windows[1].vertex_count=4098;p.windows[1].vertex_bytes=4098*48;p.windows[1].index_bytes=4098*2;
 assert(remesa_linux_memory_matrix_submit(d,&p,1000,&r)==EINVAL&&!matrix_calls);p=saved;
 p.uses[0].bank=5;assert(remesa_linux_memory_matrix_submit(d,&p,1000,&r)==EINVAL&&!matrix_calls);p=saved;
 void *cpu=NULL;assert(!ops->cpu_map(d,ids[0],false,&cpu)&&cpu);
 assert(remesa_linux_memory_matrix_submit(d,&p,1000,&r)==EBUSY&&!matrix_calls);
 assert(remesa_linux_memory_matrix_submit(d,&p,1000,cpu)==EINVAL);
 assert(!ops->cpu_unmap(d,ids[0],false));
 unsigned m=memcalls;struct remesa_gpu_scene_submit submit={77,9,&p};struct remesa_gpu_scene_receipt receipt={0};
 assert(remesa_linux_memory_matrix_dispatch(d,&submit,&receipt)==REMESA_ISSUED&&matrix_calls==1&&memcalls==m);
 assert(receipt.context==77&&receipt.operation==9&&receipt.retired&&receipt.published&&receipt.acknowledged&&!receipt.failed);
 assert(!remesa_linux_memory_matrix_response(d,&r)&&r.operation==100&&r.cookie==100);
 assert(remesa_linux_memory_matrix_dispatch(d,&submit,&receipt)==REMESA_NOT_ISSUED&&matrix_calls==1&&memcalls==m);
 struct remesa_general_session_request lab;assert(remesa_linux_memory_general_open(d,ids,5,2,&lab)==EALREADY&&memcalls==m);
 matrix_cleanup(d);
 for(unsigned length=2;length<=100;length=length==2?10:length==10?100:101){
  mode=7;d=matrix_setup();p=matrix_packet();assert(!remesa_linux_memory_matrix_continuous(d,true));
  for(unsigned i=0;i<length;i++){
   assert(!remesa_linux_memory_matrix_submit(d,&p,1000,&r));
   assert(r.flags==REMESA_MATRIX_KEEP_SESSION&&r.stages==23&&r.operation==101+i);
  }
  assert(remesa_linux_memory_matrix_continuous(d,false)==EBUSY);
  assert(!remesa_linux_memory_matrix_close(d));
  assert(!remesa_linux_memory_matrix_response(d,&r)&&r.flags==REMESA_MATRIX_CLOSE_SESSION&&r.stages==8&&!r.operation);
  assert(remesa_linux_memory_matrix_close(d)==ENOENT);
  assert(remesa_linux_memory_matrix_submit(d,&p,1000,&r)==EALREADY);
  matrix_cleanup(d);
 }
 mode=7;d=matrix_setup();p=matrix_packet();assert(!remesa_linux_memory_matrix_continuous(d,true));
 struct remesa_scene_packet preserve=p;preserve.controls.schema=3;preserve.controls.ta_bytes=76;
 preserve.use_count=10;memset(&preserve.uses[10],0,sizeof(preserve.uses[0]));
 preserve.window_count=2;
 assert(remesa_linux_memory_matrix_submit(d,&preserve,1000,&r)==EACCES&&!matrix_calls);
 assert(!remesa_linux_memory_matrix_submit(d,&p,1000,&r));
 preserve.controls.depth_id++;
 assert(remesa_linux_memory_matrix_submit(d,&preserve,1000,&r)==EACCES&&matrix_calls==1);
 preserve.controls.depth_id--;
 assert(!remesa_linux_memory_matrix_submit(d,&preserve,1000,&r)&&r.flags==5&&r.stages==23&&matrix_calls==2);
 assert(!remesa_linux_memory_matrix_close(d));matrix_cleanup(d);
 mode=7;d=matrix_setup();p=matrix_packet();assert(!remesa_linux_memory_matrix_continuous(d,true));
 assert(!remesa_linux_memory_matrix_submit(d,&p,1000,&r));
 matrix_cleanup(d);assert(matrix_calls==2); /* COLOR unmap closes before freeing. */
 mode=7;d=matrix_setup();p=matrix_packet();assert(!remesa_linux_memory_matrix_continuous(d,true));
 assert(!remesa_linux_memory_matrix_submit(d,&p,1000,&r));
 /* Destroying DEPTH first must close the held source before its GPU unmap. */
 assert(!ops->gpu_unmap(d,ids[13],0x40000000u+13*4096,4096)&&matrix_calls==2);
 assert(!ops->unpin(d,ids[13])&&!ops->destroy(d,ids[13]));
 for(unsigned i=0;i<18;i++)if(i!=13){
  assert(!ops->gpu_unmap(d,ids[i],0x40000000u+i*4096,4096));
  assert(!ops->unpin(d,ids[i])&&!ops->destroy(d,ids[i]));
 }
 assert(!remesa_linux_memory_detach(d));
 for(unsigned failure=8;failure<=10;failure++){
  mode=failure;d=matrix_setup();p=matrix_packet();assert(!remesa_linux_memory_matrix_continuous(d,true));
  int e=remesa_linux_memory_matrix_submit(d,&p,1000,&r);
  if(failure==8)assert(e==EOWNERDEAD);
  else{
   assert(!e);
   if(failure==9)assert(remesa_linux_memory_matrix_close(d)==EOWNERDEAD);
   else assert(remesa_linux_memory_matrix_submit(d,&p,1000,&r)==EOWNERDEAD);
  }
  matrix_poisoned(d);
 }
 printf("PASS %u matrix byte/errno/retention faults plus aliases/mapping/limits, checked callback receipt and exactly1 submit with0 extra memory calls; GPU0\n",cases);
 return 0;
}
