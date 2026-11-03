/* SPDX-License-Identifier: MIT. Interposed syscalls, no device or GPU. */
#define main old_general_transport_main
#define __wrap_ioctl previous_general_ioctl
#include "test_general_session_transport.c"
#undef __wrap_ioctl
#undef main
#include <stddef.h>
static unsigned parameter_calls;
static size_t parameter_byte_fault=SIZE_MAX;
static int parameter_errno,parameter_refusal,parameter_positive;
static uint64_t native_receipt;
int __wrap_ioctl(int fd,unsigned long command,...)
{
 va_list args;va_start(args,command);void *data=va_arg(args,void *);va_end(args);
 if(command!=DRM_IOCTL_REMESA_GENERAL_PARAMETER){
  int e=previous_general_ioctl(fd,command,data);
  if(command==DRM_IOCTL_REMESA_GENERAL_SESSION&&((struct remesa_general_session_request *)data)->action==2)
   native_receipt=((struct remesa_general_session_request *)data)->operation;
  return e;
 }
 struct remesa_general_parameter_request *r=data;
 assert(fd==99&&remesa_general_parameter_request_valid(r)&&r->retired_operation==native_receipt);
 parameter_calls++;r->state=1;r->accepted_sequence=r->sequence;
 if(parameter_refusal){r->error=-parameter_refusal;r->accepted_sequence=0;}
 if(parameter_byte_fault<sizeof(*r))((unsigned char *)r)[parameter_byte_fault]^=1;
 if(parameter_errno){errno=parameter_errno;return -1;}
 return parameter_positive;
}
static struct remesa_linux_memory *typed(bool record)
{
 action_fault=byte_fault=io_fault=0;parameter_calls=0;parameter_byte_fault=SIZE_MAX;
 parameter_errno=parameter_refusal=parameter_positive=0;native_receipt=0;
 struct remesa_linux_memory *d=setup();struct remesa_general_session_request r;
 if(record)assert(!remesa_linux_memory_general_record_enable(d));
 assert(!remesa_linux_memory_general_typed_open(d,ids,3,3,&r)&&r.version==2);
 return d;
}
static void first_submit(struct remesa_linux_memory *d)
{struct remesa_general_session_request r;assert(!remesa_linux_memory_general_submit(d,1000,&r)&&r.version==2&&r.operation==101);}
static void cleanup(struct remesa_linux_memory *d)
{
 struct remesa_general_session_request r;assert(!remesa_linux_memory_general_close(d,&r)&&r.version==2);
 for(unsigned i=0;i<18;i++){
  assert(!ops->gpu_unmap(d,ids[i],0x40000000u+i*4096,4096));
  assert(!ops->unpin(d,ids[i])&&!ops->destroy(d,ids[i]));
 }
 assert(!remesa_linux_memory_detach(d));
}
#ifndef REMESA_PARAMETER_TRANSPORT_TEST_MAIN
#define REMESA_PARAMETER_TRANSPORT_TEST_MAIN main
#endif
int REMESA_PARAMETER_TRANSPORT_TEST_MAIN(void)
{
 assert(!old_general_transport_main());
 uint32_t bits[36];for(unsigned i=0;i<36;i++)bits[i]=0x3f800000;
 struct remesa_general_parameter_request r,request;
 struct remesa_linux_memory *d=typed(true);
 assert(sizeof(r)==592&&offsetof(struct remesa_general_parameter_request,data)==56&&
  offsetof(struct remesa_general_parameter_request,accepted_sequence)==568);
 assert(remesa_linux_memory_general_parameter_response(d,&r)==ENODATA);
 assert(remesa_linux_memory_general_parameters(d,1,4,bits,36,&r)==EAGAIN&&!parameter_calls);
 first_submit(d);unsigned m=memcalls,c=calls;
 assert(remesa_linux_memory_general_parameters(d,1,4,(void *)d,36,&r)==EINVAL);
 assert(remesa_linux_memory_general_parameters(d,1,4,bits,36,(void *)d)==EINVAL);
 assert(remesa_linux_memory_general_parameters(d,1,4,(void *)&r,36,&r)==EINVAL);
 assert(remesa_linux_memory_general_parameters(d,0,4,bits,36,&r)==EINVAL);
 assert(remesa_linux_memory_general_parameters(d,1,5,bits,36,&r)==EINVAL);
 assert(remesa_linux_memory_general_parameters(d,1,4,bits,128,&r)==EINVAL&&!parameter_calls);
 static const int atomic_errors[]={EINVAL,EACCES,ERANGE,EBUSY,ENODATA,ENOMEM,EOPNOTSUPP};
 for(unsigned i=0;i<sizeof atomic_errors/sizeof atomic_errors[0];i++){
  parameter_refusal=atomic_errors[i];assert(remesa_linux_memory_general_parameters(d,1,4,bits,36,&r)==atomic_errors[i]);
  assert(r.sequence==1&&!r.accepted_sequence&&r.state==1);
 }
 parameter_refusal=0;
 assert(!remesa_linux_memory_general_parameters(d,1,4,bits,36,&r)&&r.sequence==1&&r.accepted_sequence==1);
 assert(calls==c&&memcalls==m); /* Data ack does not submit a GPU job. */
 assert(!remesa_linux_memory_general_parameter_request(d,&request)&&request.retired_operation==101&&
  request.sequence==1&&!request.accepted_sequence&&remesa_general_parameter_request_valid(&request));
 assert(remesa_linux_memory_general_parameter_response(d,(void *)d)==EINVAL);
 struct remesa_general_session_request g;
 assert(!remesa_linux_memory_general_submit(d,1000,&g)&&g.operation==102);
 assert(!remesa_linux_memory_general_parameters(d,1,8,bits,1,&r)&&r.sequence==2&&r.retired_operation==102);
 cleanup(d);
 d=setup();assert(!remesa_linux_memory_general_open(d,ids,3,3,&g));
 assert(remesa_linux_memory_general_parameters(d,1,4,bits,36,&r)==EOPNOTSUPP);
 assert(!remesa_linux_memory_general_close(d,&g));
 for(unsigned i=0;i<18;i++){
  assert(!ops->gpu_unmap(d,ids[i],0x40000000u+i*4096,4096));
  assert(!ops->unpin(d,ids[i])&&!ops->destroy(d,ids[i]));
 }
 assert(!remesa_linux_memory_detach(d));
 unsigned faults=0;
 for(size_t byte=0;byte<sizeof(r);byte++){
  d=typed(true);first_submit(d);parameter_byte_fault=byte;
  assert(remesa_linux_memory_general_parameters(d,1,4,bits,36,&r)==EOWNERDEAD&&parameter_calls==1);
  assert(!remesa_linux_memory_general_parameter_response(d,&request));
  assert(remesa_linux_memory_general_parameters(d,1,4,bits,36,&r)==EOWNERDEAD&&parameter_calls==1);
  poisoned(d);faults++;
 }
 for(unsigned i=0;i<6;i++){
  d=typed(false);first_submit(d);
  if(i<4){static const int errors[]={EINTR,EIO,EINVAL,ETIMEDOUT};parameter_errno=errors[i];}
  else if(i==4)parameter_positive=1;
  else parameter_refusal=ESTALE;
  assert(remesa_linux_memory_general_parameters(d,1,4,bits,36,&r)==EOWNERDEAD&&parameter_calls==1);
  assert(remesa_linux_memory_general_parameter_response(d,&request)==ENODATA);poisoned(d);faults++;
 }
 printf("PASS %u parameter byte/errno faults; typed epoch/alias/refusal/ack/version separation, no extra GPU or memory calls; GPU0\n",faults);
 return 0;
}
