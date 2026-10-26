/* SPDX-License-Identifier: MIT */
/* Interposed syscalls exercise transport only. These fixtures are NOT GPU pixels. */
#include "remesa_linux.h"
#include "remesa/native_uapi.h"
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
static unsigned calls,opens,closes;
static int mode;
int open(const char *path,int flags,...)
{ (void)path;(void)flags;++opens;return 123; }
int close(int fd) { assert(fd==123);++closes;return 0; }
int ioctl(int fd,unsigned long op,...)
{
 va_list args;void *arg;assert(fd==123);va_start(args,op);arg=va_arg(args,void *);va_end(args);
 if(op==DRM_IOCTL_VERSION) { struct drm_version *v=arg;memcpy(v->name,"gma500",6);v->name_len=6;return 0; }
 assert(op==DRM_IOCTL_REMESA_NATIVE_DRAW);++calls;
 struct remesa_native_draw *r=arg;
 if(mode==1) { errno=EINTR;return -1; }
 r->operation=1;r->phase=9;r->events=7;r->issued=1;r->published=1;r->status=1;
 memset(r->color,0x5a,sizeof(r->color));
 if(mode==2) r->phase=8;
 if(mode==3) r->positions[0][0]^=1;
 if(mode==4) r->operation_errno=-EIO;
 return mode==5 ? 1 : 0;
}
int main(void)
{
 struct remesa_draw_packet p;struct remesa_native_draw request,before;
 const float vertices[3][4]={{8,8,.5,1},{24,8,.5,1},{8,24,.5,1}};
 const uint16_t indices[3]={0,1,2};
 assert(!remesa_draw_packet_build(&p,vertices,3,indices,3,0xffff00ff));
 assert(!remesa_linux_request_build(&p,&request));before=request;
 p.vertex_count=5;assert(remesa_linux_request_build(&p,&request)==ENOTSUP);
 assert(!memcmp(&before,&request,sizeof(request)));p.vertex_count=3;
 for(mode=0;mode<6;++mode) {
  struct remesa_device d={0};struct remesa_linux_receipt receipt;void *bo,*ptr,*fence=NULL;
  assert(!remesa_linux_open(&d,"INTERPOSED-NO-DEVICE"));
  assert(remesa_linux_response(&d,&request)==EAGAIN);
  assert(!d.ops->allocate(d.priv,4096,&bo));
  assert(d.ops->map(d.priv,bo,false,&ptr)==EAGAIN);
  unsigned prior=calls;int error=d.ops->submit(d.priv,bo,&p,&fence);
  assert(calls==prior+1 && fence);
  assert(!remesa_linux_last_receipt(&d,&receipt) && receipt.attempted);
  assert(!remesa_linux_response(&d,&request));
  if(!mode) {
   assert(!error && !d.ops->wait(d.priv,fence,0,true));
   assert(!d.ops->map(d.priv,bo,false,&ptr));
   assert(!memcmp(ptr,request.color,4096));d.ops->unmap(d.priv,bo,false);
  } else {
   assert(error && d.ops->wait(d.priv,fence,0,true));
   assert(d.ops->map(d.priv,bo,false,&ptr)==EIO && !ptr);
   assert(d.ops->map(d.priv,bo,true,&ptr)==EIO && !ptr);
  }
  /* A fence authenticates only its submitting endpoint. Wrong-owner cleanup
   * must not consume it, and stale/unknown pointers must not be dereferenced. */
  struct remesa_device foreign={0};
  assert(!remesa_linux_open(&foreign,"INTERPOSED-NO-DEVICE"));
  assert(foreign.ops->wait(foreign.priv,fence,0,true)==EINVAL);
  foreign.ops->destroy_fence(foreign.priv,fence);
  assert(d.ops->wait(d.priv,fence,0,true)==error);
  assert(!remesa_linux_close_checked(&foreign));
  assert(d.ops->wait(d.priv,(void *)1,0,true)==EINVAL);
  d.ops->destroy_fence(d.priv,(void *)1);
  assert(remesa_linux_close_checked(&d)==EBUSY);
  void *second=(void *)1;
  assert(d.ops->submit(d.priv,bo,&p,&second)==EBUSY && !second && calls==prior+1);
  d.ops->destroy_fence(d.priv,fence);
  assert(d.ops->wait(d.priv,fence,0,true)==EINVAL);
  d.ops->destroy_fence(d.priv,fence);
  d.ops->destroy_bo(d.priv,bo);remesa_linux_close(&d);
 }
 assert(opens==12 && closes==12 && calls==6);
 puts("PASS: 6 interposed transport outcomes; no device access; failed/altered responses unreadable; no retry; fence owner/stale rejection");
}
