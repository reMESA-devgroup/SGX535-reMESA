/* SPDX-License-Identifier: MIT */
/* CPU publication of an already retired GPU readback. No GPU mapping, command,
 * fence, image generation, rendering or receipt verification in this helper.
 * One bounded buffer: checked request AND matching SHM completion before reuse.
 * Caller keeps Xlib event ownership and holds its live Display lock. */
#include "remesa_fast_shm.h"
#include <xcb/shm.h>
#include <xcb/xcbext.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <X11/extensions/XShm.h>
#include <poll.h>
#include <time.h>
#include <stdint.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#define SHM_BYTES 4096000u
static pthread_mutex_t shm_lock=PTHREAD_MUTEX_INITIALIZER;
static struct {
 Display *display;
 xcb_connection_t *connection;
 uint64_t timeout_ns;
 int completion_type,major_opcode;
 uint32_t sequence;
 bool in_flight;
 xcb_shm_seg_t segment;
 void *pixels;
 int fd;
 bool attached,poisoned;
} image={.fd=-1};
static int check(xcb_connection_t *c,xcb_void_cookie_t cookie,unsigned char *code)
{
 if(!cookie.sequence)return EPROTO;
 xcb_generic_error_t *e=xcb_request_check(c,cookie);
 if(e){if(code)*code=e->error_code;free(e);return EIO;}
 return xcb_connection_has_error(c)?EPIPE:0;
}
static int release(void)
{
 int error=0;
 /* An uncertain read must retain its only backing until process teardown.
  * No detach/unmap/FDclose, retry, or borrowed Display dereference here. */
 if(image.poisoned||image.in_flight)return EBUSY;
 if(image.attached){
  if(xcb_connection_has_error(image.connection))error=EPIPE;
  else error=check(image.connection,xcb_shm_detach_checked(image.connection,image.segment),NULL);
 }
 /* These pages belong only to CPU presentation. The X server's FD/mapping
  * retains them independently if a broken connection delays its cleanup. */
 if(image.pixels&&munmap(image.pixels,SHM_BYTES)&&!error)error=errno;
 if(image.fd>=0&&close(image.fd)&&!error)error=errno;
 memset(&image,0,sizeof(image));image.fd=-1;return error;
}
static int initialize(xcb_connection_t *c,unsigned char *code)
{
 int event_base=XShmGetEventBase(image.display);
 const xcb_query_extension_reply_t *extension=xcb_get_extension_data(c,&xcb_shm_id);
 if(event_base<0||!extension||!extension->present||
    event_base!=extension->first_event||!extension->major_opcode)return ENOTSUP;
 image.completion_type=event_base+ShmCompletion;
 image.major_opcode=extension->major_opcode;
 XFlush(image.display);
 xcb_generic_error_t *xe=NULL;
 xcb_shm_query_version_reply_t *v=xcb_shm_query_version_reply(c,xcb_shm_query_version(c),&xe);
 if(!v||xe){if(xe&&code)*code=xe->error_code;free(xe);free(v);return ENOTSUP;}
 bool supported=v->major_version==1&&v->minor_version>=2;free(v);
 if(!supported)return ENOTSUP;
 char path[]="/tmp/remesa-fast-x11-XXXXXX";
 int rw=mkostemp(path,O_CLOEXEC),ro=-1,error=0;
 if(rw<0)return errno;
 if(ftruncate(rw,SHM_BYTES)){error=errno;goto done;}
 ro=open(path,O_RDONLY|O_CLOEXEC);if(ro<0){error=errno;goto done;}
 if(unlink(path)){error=errno;goto done;}path[0]=0;
 void *map=mmap(NULL,SHM_BYTES,PROT_READ|PROT_WRITE,MAP_SHARED,rw,0);
 if(map==MAP_FAILED){error=errno;goto done;}
 image.connection=c;image.pixels=map;image.fd=rw;rw=-1;
 image.segment=xcb_generate_id(c);
 if(!image.segment||image.segment==UINT32_MAX){error=ENOSPC;goto done;}
 /* XCB owns this descriptor after attach_fd, including on transport failure.
  * A read-only open description prevents server-side writable mmap. */
 int sent=ro;ro=-1;
 error=check(c,xcb_shm_attach_fd_checked(c,image.segment,sent,1),code);
 if(!error)image.attached=true;
 done:
 if(path[0]){int saved=errno;unlink(path);errno=saved;}
 if(ro>=0)close(ro);
 if(rw>=0)close(rw);
 if(error&&image.connection)release();
 return error;
}
/* Exactly one outstanding request can use this private segment+offset. */
struct completion_match {
 Display *display;int type,major_opcode;
 xcb_drawable_t drawable;xcb_shm_seg_t segment;uint32_t sequence;
};
static Bool completion_matches(Display *display,XEvent *event,XPointer argument)
{
 const struct completion_match *m=(const void *)argument;
 const XShmCompletionEvent *e=(const void *)event;
 if(display!=m->display||e->display!=m->display||e->type!=m->type||e->send_event||
    e->drawable!=m->drawable||e->shmseg!=m->segment||e->offset||
    e->major_code!=m->major_opcode||e->minor_code!=XCB_SHM_PUT_IMAGE)return False;
 /* Event serial is last request processed, not necessarily the PutImage
  * cookie. Reject older serials, including across32-bit wrap; a later sync
  * sequence is valid. One in-flight request prevents ambiguous reuse. */
 return (uint32_t)((uint32_t)e->serial-m->sequence)<UINT32_C(0x80000000);
}
static int monotonic_ns(uint64_t *result)
{
 struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return errno?errno:EIO;
 *result=(uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;return 0;
}
static int wait_completion(const struct completion_match *match)
{
 uint64_t now,deadline;int error=monotonic_ns(&now);if(error)return error;
 if(UINT64_MAX-now<image.timeout_ns)return EOVERFLOW;
 deadline=now+image.timeout_ns;
 for(;;){
  XEvent event;
  /* request_check may already have read the completion off the socket.
   * Search the Xlib-owned queue BEFORE waiting for fd readability. */
  if(XCheckIfEvent(image.display,&event,completion_matches,(XPointer)match))
   return xcb_connection_has_error(image.connection)?EPIPE:0;
  if(xcb_connection_has_error(image.connection))return EPIPE;
  error=monotonic_ns(&now);if(error)return error;
  if(now>=deadline)return ETIMEDOUT;
  uint64_t remaining=deadline-now;
  int milliseconds=(int)((remaining+UINT64_C(999999))/UINT64_C(1000000));
  int fd=ConnectionNumber(image.display);if(fd<0)return EPIPE;
  struct pollfd descriptor={.fd=fd,.events=POLLIN};
  int ready=poll(&descriptor,1,milliseconds);
  if(ready<0){if(errno==EINTR)continue;return errno?errno:EIO;}
  if(ready>0&&(descriptor.revents&(POLLERR|POLLHUP|POLLNVAL)))return EPIPE;
 }
}
int remesa_fast_shm_bind_display(Display *display,xcb_connection_t *connection,uint64_t timeout)
{
 if(!display||!connection||!timeout||timeout>UINT64_C(1000000000))return EINVAL;
 pthread_mutex_lock(&shm_lock);int error=0;
 if(image.poisoned)error=EIO;
 else if(image.in_flight)error=EBUSY;
 else if((image.display&&image.display!=display)||
         (image.connection&&image.connection!=connection))error=EBUSY;
 else if(XGetXCBConnection(display)!=connection)error=EACCES;
 else {image.display=display;image.connection=connection;image.timeout_ns=timeout;}
 pthread_mutex_unlock(&shm_lock);return error;
}
int remesa_fast_shm_present(xcb_connection_t *c,xcb_drawable_t drawable,xcb_gcontext_t gc,
 unsigned width,unsigned height,const void *pixels,size_t bytes,unsigned char *xerror,bool *sent,bool *ack)
{
 if(sent)*sent=false;if(ack)*ack=false;
 if(!c||!drawable||!gc||!pixels||!xerror||!sent||!ack||
    width!=1280||height!=800||bytes!=SHM_BYTES)return EINVAL;
 pthread_mutex_lock(&shm_lock);int error=0;
 if(image.poisoned){error=EIO;goto done;}
 if(!image.display||!image.timeout_ns){error=ENOTCONN;goto done;}
 if(image.in_flight){error=EBUSY;goto done;}
 if(image.connection&&image.connection!=c){error=EBUSY;goto done;}
 if(xcb_connection_has_error(c)){image.poisoned=true;error=EPIPE;goto done;}
 if(!image.attached){error=initialize(c,xerror);if(error)goto done;}
 memcpy(image.pixels,pixels,bytes);
 xcb_void_cookie_t put=xcb_shm_put_image_checked(c,drawable,gc,width,height,0,0,width,height,0,0,
  24,XCB_IMAGE_FORMAT_Z_PIXMAP,1,image.segment,0);
 *sent=put.sequence!=0;image.sequence=put.sequence;image.in_flight=*sent;
 error=check(c,put,xerror);
 if(!error){
  const struct completion_match match={image.display,image.completion_type,
   image.major_opcode,drawable,image.segment,put.sequence};
  error=wait_completion(&match);
  if(!error){image.in_flight=false;*ack=true;}
 }
 if(error)image.poisoned=true;
 done:pthread_mutex_unlock(&shm_lock);return error;
}
int remesa_fast_shm_close(xcb_connection_t *c)
{
 if(!c)return EINVAL;
 pthread_mutex_lock(&shm_lock);int error=0;
 if(image.connection&&image.connection!=c)error=EBUSY;
 else error=release();
 pthread_mutex_unlock(&shm_lock);return error;
}
