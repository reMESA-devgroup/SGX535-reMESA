/* SPDX-License-Identifier: MIT. Actual factory/owner/Gallium, fake ioctls GPU0. */
#define _GNU_SOURCE
#include "remesa_loader.h"
#include "remesa_dri.h"
#include "remesa_screen.h"
#include "remesa_linux_memory.h"
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
int __wrap_ioctl(int,unsigned long,...);
int __wrap_remesa_dri_probe_fd(int,struct remesa_dri_identity *);
static int borrowed,owned=-1;static unsigned mode,opens,closes,memcalls,fills,copies,next_id=32,operation;
struct backing {unsigned id;size_t bytes;unsigned char *data;};static struct backing backing[8];
int __wrap_remesa_dri_probe_fd(int fd,struct remesa_dri_identity *id)
{assert(fd!=borrowed&&fcntl(fd,F_GETFD)&FD_CLOEXEC);owned=fd;if(mode==1)return ENODEV;*id=(struct remesa_dri_identity){0x8086,0x8108};return 0;}
static struct backing *find(unsigned id)
{for(unsigned i=0;i<8;i++)if(backing[i].id==id)return &backing[i];abort();}
int __wrap_ioctl(int fd,unsigned long cmd,...)
{
 va_list ap;va_start(ap,cmd);void *arg=va_arg(ap,void *);va_end(ap);assert(fd==owned&&fcntl(fd,F_GETFD)>=0);
 if(cmd==DRM_IOCTL_REMESA_MEMORY){
  struct remesa_memory_request *r=arg;memcalls++;assert(r->version==1);
  if(r->action==REMESA_MEM_OPEN){opens++;assert(!r->context);
   if(mode==2||mode==3){errno=mode==2?EOPNOTSUPP:EINTR;return -1;}r->context=77;return 0;}
  assert(r->context==77);
  if(r->action==REMESA_MEM_CLOSE){closes++;if(mode==4){errno=EINTR;return -1;}return 0;}
  if(r->action==REMESA_MEM_CREATE){
   if(mode==6){errno=ENOMEM;return -1;}
   struct backing *b=NULL;for(unsigned i=0;i<8;i++)if(!backing[i].id){b=&backing[i];break;}assert(b);
   b->id=r->id=next_id++;b->bytes=r->bytes;b->data=calloc(1,b->bytes);assert(b->data);return 0;
  }
  struct backing *b=find(r->id);
  if(r->action==REMESA_MEM_READ||r->action==REMESA_MEM_WRITE){assert(r->offset<=b->bytes&&r->length<=b->bytes-r->offset);
   if(r->action==REMESA_MEM_READ)memcpy(r->data,b->data+r->offset,r->length);else memcpy(b->data+r->offset,r->data,r->length);}
  if(r->action==REMESA_MEM_DESTROY){free(b->data);*b=(struct backing){0};}
  return 0;
 }
 if(cmd==DRM_IOCTL_REMESA_TWOD_FILL){
  struct remesa_twod_fill_request *r=arg;assert(r->version==1&&r->context==77);fills++;
  if(mode==5){errno=EINTR;return -1;}
  r->operation=++operation;r->cookie=1;r->stages=31;r->core=0x01130000;r->revision=0x00010201;return 0;
 }
 assert(cmd==DRM_IOCTL_REMESA_TWOD_COPY);struct remesa_twod_copy_request *r=arg;copies++;
 assert(r->version==1&&r->context==77);r->operation=++operation;r->cookie=1;r->stages=31;r->core=0x01130000;r->revision=0x00010201;return 0;
}
static struct pipe_resource *image(struct pipe_screen *s)
{struct pipe_resource t={.target=PIPE_TEXTURE_2D,.format=PIPE_FORMAT_B8G8R8A8_UNORM,.width0=32,.height0=32,.depth0=1,.array_size=1,.bind=PIPE_BIND_RENDER_TARGET};return s->resource_create(s,&t);}
static void drop(struct pipe_resource *r)
{if(r&&!--r->reference.count)r->screen->resource_destroy(r->screen,r);}
static void one(unsigned fault)
{
 mode=fault;borrowed=open("/dev/null",O_RDWR|O_CLOEXEC);assert(borrowed>=0);struct remesa_native_2d_report report;
 struct pipe_screen *s=remesa_loader_native_2d_screen(borrowed,&report);
 if(mode<=3&&mode){
  assert(!s&&report.error==(mode==1?ENODEV:mode==2?EOPNOTSUPP:EOWNERDEAD));
  if(mode==3)assert(report.retained&&report.cleanup_error==EOWNERDEAD&&fcntl(owned,F_GETFD)>=0);
  else assert(!report.retained&&!report.cleanup_error&&fcntl(owned,F_GETFD)<0&&errno==EBADF);
  assert(opens==(mode==1?0u:1u)&&!closes&&!fills&&!copies&&fcntl(borrowed,F_GETFD)>=0);close(borrowed);return;
 }
 assert(s&&!report.error&&!report.retained&&!s->caps.graphics&&!s->caps.accelerated&&opens==1&&!closes&&!fills&&!copies);
 assert(fcntl(owned,F_GETFD)&FD_CLOEXEC);
 struct pipe_context *c=s->context_create(s,NULL,0);assert(c);struct pipe_resource *a=image(s),*b=mode==6?NULL:image(s);
 if(mode==6){assert(!a);s->destroy(s);c->destroy(c);assert(closes==1&&fcntl(owned,F_GETFD)<0);close(borrowed);return;}
 assert(a&&b);struct pipe_box box={.width=16,.height=16,.depth=1};unsigned char pixel[4]={1,2,3,4};unsigned calls=memcalls;
 c->clear_texture(c,a,0,&box,pixel);
 if(mode==5){assert(remesa_context_error(c)==EOWNERDEAD&&fills==1&&memcalls==calls);s->destroy(s);c->destroy(c);drop(a);drop(b);assert(!closes&&fcntl(owned,F_GETFD)>=0);close(borrowed);return;}
 assert(!remesa_context_error(c)&&fills==1&&memcalls==calls);
 c->resource_copy_region(c,b,0,0,0,0,a,0,&box);assert(!remesa_context_error(c)&&copies==1&&operation==2&&memcalls==calls);
 assert(remesa_context_native_fill_count(c)==1&&remesa_context_native_copy_count(c)==1&&!remesa_context_cpu_clear_count(c)&&!remesa_context_cpu_copy_count(c));
 s->destroy(s);assert(!s->context_create(s,NULL,0)&&!image(s)&&!closes&&fcntl(owned,F_GETFD)>=0);
 c->clear_texture(c,a,0,&box,pixel);assert(remesa_context_error(c)==ESHUTDOWN&&fills==1);
 close(borrowed);c->destroy(c);drop(a);assert(!closes&&fcntl(owned,F_GETFD)>=0);drop(b);
 assert(closes==1);
 if(mode==4){assert(fcntl(owned,F_GETFD)>=0&&remesa_screen_collect(s)==EOWNERDEAD&&closes==1);return;}
 assert(fcntl(owned,F_GETFD)<0&&errno==EBADF);
}
int main(void)
{
 struct remesa_native_2d_report r;assert(!remesa_loader_native_2d_screen(-1,&r)&&r.error==EBADF&&!r.retained);
 int ro=open("/dev/null",O_RDONLY);assert(ro>=0);assert(!remesa_loader_native_2d_screen(ro,&r)&&r.error==EACCES);close(ro);
 for(unsigned i=0;i<=6;i++){pid_t p=fork();assert(p>=0);if(!p){one(i);_exit(0);}int status;assert(waitpid(p,&status,0)==p&&WIFEXITED(status)&&!WEXITSTATUS(status));}
 puts("PASS native2D factory: owned CLOEXEC session, combined copy/fill monotonic sequence, children retain FD, original FD independent, closed-screen refusal, constructor rollback, ambiguous OPEN/CLOSE/submission retention; injected GPU0/API0");return 0;
}
