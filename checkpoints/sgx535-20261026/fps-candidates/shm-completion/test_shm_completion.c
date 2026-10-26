/* Actual SHM helper with mocked Xlib/XCB, file/mmap/poll/clock APIs. GPU0. */
#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <X11/extensions/XShm.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>
#include <xcb/shm.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
static int mock_mkostemp(char *,int);static int mock_ftruncate(int,off_t);
static int mock_open(const char *,int,...);static int mock_unlink(const char *);
static void *mock_mmap(void *,size_t,int,int,int,off_t);static int mock_munmap(void *,size_t);static int mock_close(int);
static int mock_poll(struct pollfd *,nfds_t,int);static int mock_clock_gettime(clockid_t,struct timespec *);
static int mock_connection_number(Display *);static void *mock_memcpy(void *,const void *,size_t);
#define mkostemp mock_mkostemp
#define ftruncate mock_ftruncate
#define open mock_open
#define unlink mock_unlink
#define mmap mock_mmap
#define munmap mock_munmap
#define close mock_close
#define poll mock_poll
#define clock_gettime mock_clock_gettime
#define memcpy mock_memcpy
#undef ConnectionNumber
#define ConnectionNumber(d) mock_connection_number(d)
#include "remesa_fast_shm.c"
#undef mkostemp
#undef ftruncate
#undef open
#undef unlink
#undef mmap
#undef munmap
#undef close
#undef poll
#undef clock_gettime
#undef memcpy
static Display *const display=(Display *)(uintptr_t)0x1000,*const foreign_display=(Display *)(uintptr_t)0x2000;
static xcb_connection_t *const connection=(xcb_connection_t *)(uintptr_t)0x3000,*const foreign_connection=(xcb_connection_t *)(uintptr_t)0x4000;
static bool display_alive=true,extension_present=true,version_supported=true;
static unsigned char shared_pixels[SHM_BYTES],input_pixels[SHM_BYTES];
static XEvent events[64];static unsigned event_count;
static unsigned copies,put_calls,poll_calls,event_checks,attach_calls,detach_calls,map_calls,unmap_calls,fd_closes,bind_lookups,flush_calls;
static unsigned generated_ids=0x200,checked_put_error,attach_error,detach_error;
static int connection_error,descriptor_number=42,poll_error;
static uint64_t now_ns;static uint32_t next_put_sequence=100,last_put_sequence;
static unsigned complete_poll=1;static bool zero_cookie,interrupt_first,clock_error;
static bool complete_immediately=true,never_complete,disconnect_poll;
xcb_extension_t xcb_shm_id={"MIT-SHM",0};
static void alive(Display *d){assert(display_alive&&d==display);}
static int mock_connection_number(Display *d){alive(d);return descriptor_number;}
xcb_connection_t *XGetXCBConnection(Display *d){alive(d);++bind_lookups;return connection;}
int XShmGetEventBase(Display *d){alive(d);return extension_present?65:-1;}
int XFlush(Display *d){alive(d);++flush_calls;return 1;}
static const xcb_query_extension_reply_t extension={.present=1,.major_opcode=130,.first_event=65};
const xcb_query_extension_reply_t *xcb_get_extension_data(xcb_connection_t *c,xcb_extension_t *e){assert(display_alive&&c==connection&&e==&xcb_shm_id);return extension_present?&extension:NULL;}
xcb_shm_query_version_cookie_t xcb_shm_query_version(xcb_connection_t *c){assert(display_alive&&c==connection);return(xcb_shm_query_version_cookie_t){1};}
xcb_shm_query_version_reply_t *xcb_shm_query_version_reply(xcb_connection_t *c,xcb_shm_query_version_cookie_t cookie,xcb_generic_error_t **error){assert(display_alive&&c==connection&&cookie.sequence==1);*error=NULL;xcb_shm_query_version_reply_t *v=calloc(1,sizeof(*v));assert(v);v->major_version=1;v->minor_version=version_supported?2:1;return v;}
uint32_t xcb_generate_id(xcb_connection_t *c){assert(display_alive&&c==connection);return generated_ids++;}
xcb_void_cookie_t xcb_shm_attach_fd_checked(xcb_connection_t *c,xcb_shm_seg_t seg,int32_t fd,uint8_t readonly){assert(display_alive&&c==connection&&seg&&fd==43&&readonly==1);++attach_calls;return(xcb_void_cookie_t){10};}
xcb_void_cookie_t xcb_shm_detach_checked(xcb_connection_t *c,xcb_shm_seg_t seg){assert(display_alive&&c==connection&&seg==image.segment&&!image.in_flight&&!image.poisoned);++detach_calls;return(xcb_void_cookie_t){11};}
int xcb_connection_has_error(xcb_connection_t *c){assert(display_alive&&c==connection);return connection_error;}
static XEvent completion_event(void){
 XShmCompletionEvent e={.type=65+ShmCompletion,.serial=(unsigned long)(uint32_t)(last_put_sequence+1),.send_event=False,.display=display,.drawable=42,.major_code=130,.minor_code=XCB_SHM_PUT_IMAGE,.shmseg=image.segment,.offset=0};XEvent out;memset(&out,0,sizeof(out));_Static_assert(sizeof(e)<=sizeof(out),"extension event size");memcpy(&out,&e,sizeof(e));return out;
}
static void push(XEvent e){assert(event_count<64);events[event_count++]=e;}
static void push_key(void){XEvent e;memset(&e,0,sizeof(e));e.xkey.type=KeyPress;e.xkey.display=display;e.xkey.window=42;e.xkey.keycode=113;push(e);}
static void push_expose(void){XEvent e;memset(&e,0,sizeof(e));e.xexpose.type=Expose;e.xexpose.display=display;e.xexpose.window=42;e.xexpose.width=1280;push(e);}
Bool XCheckIfEvent(Display *d,XEvent *out,Bool(*predicate)(Display *,XEvent *,XPointer),XPointer arg){
 alive(d);++event_checks;assert(predicate&&arg);
 for(unsigned i=0;i<event_count;i++)if(predicate(d,&events[i],arg)){*out=events[i];memmove(events+i,events+i+1,(event_count-i-1)*sizeof(*events));--event_count;return True;}
 return False;
}
xcb_void_cookie_t xcb_shm_put_image_checked(xcb_connection_t *c,xcb_drawable_t drawable,xcb_gcontext_t gc,uint16_t tw,uint16_t th,uint16_t sx,uint16_t sy,uint16_t sw,uint16_t sh,int16_t dx,int16_t dy,uint8_t depth,uint8_t format,uint8_t send_event,xcb_shm_seg_t seg,uint32_t offset){
 assert(display_alive&&c==connection&&drawable==42&&gc==77&&tw==1280&&th==800&&!sx&&!sy&&sw==1280&&sh==800&&!dx&&!dy&&depth==24&&format==XCB_IMAGE_FORMAT_Z_PIXMAP&&send_event==1&&seg==image.segment&&!offset&&!image.in_flight&&!image.poisoned);++put_calls;last_put_sequence=zero_cookie?0:next_put_sequence++;return(xcb_void_cookie_t){last_put_sequence};
}
xcb_generic_error_t *xcb_request_check(xcb_connection_t *c,xcb_void_cookie_t cookie){
 assert(display_alive&&c==connection&&cookie.sequence);unsigned error=0;
 if(cookie.sequence==10)error=attach_error;
 else if(cookie.sequence==11)error=detach_error;
 else {assert(cookie.sequence==last_put_sequence&&image.in_flight);error=checked_put_error;if(!error&&complete_immediately&&!never_complete)push(completion_event());}
 if(!error)return NULL;xcb_generic_error_t *e=calloc(1,sizeof(*e));assert(e);e->error_code=error;return e;
}
static int mock_mkostemp(char *p,int flags){assert(p&&flags==O_CLOEXEC);strcpy(p,"/tmp/mock-remesa-shm");return 42;}
static int mock_ftruncate(int fd,off_t n){assert(fd==42&&n==SHM_BYTES);return 0;}
static int mock_open(const char *p,int flags,...){assert(!strcmp(p,"/tmp/mock-remesa-shm")&&flags==(O_RDONLY|O_CLOEXEC));return 43;}
static int mock_unlink(const char *p){assert(!strcmp(p,"/tmp/mock-remesa-shm"));return 0;}
static void *mock_mmap(void *hint,size_t n,int prot,int flags,int fd,off_t offset){assert(!hint&&n==SHM_BYTES&&prot==(PROT_READ|PROT_WRITE)&&flags==MAP_SHARED&&fd==42&&!offset);++map_calls;memset(shared_pixels,0xcd,sizeof(shared_pixels));return shared_pixels;}
static int mock_munmap(void *p,size_t n){assert(p==shared_pixels&&n==SHM_BYTES&&!image.poisoned&&!image.in_flight);++unmap_calls;return 0;}
static int mock_close(int fd){assert(fd==42);++fd_closes;return 0;}
static void *mock_memcpy(void *d,const void *s,size_t n){if(d==shared_pixels){assert(!image.in_flight&&!image.poisoned);++copies;}return memcpy(d,s,n);}
static int mock_clock_gettime(clockid_t id,struct timespec *t){assert(id==CLOCK_MONOTONIC);if(clock_error){errno=EIO;return -1;}t->tv_sec=now_ns/1000000000;t->tv_nsec=now_ns%1000000000;return 0;}
static int mock_poll(struct pollfd *fd,nfds_t n,int timeout){
 assert(display_alive&&n==1&&fd->fd==42&&fd->events==POLLIN&&timeout>0&&timeout<=1000);++poll_calls;
 if(interrupt_first&&poll_calls==1){now_ns+=1000000;errno=EINTR;return -1;}
 if(poll_error){errno=poll_error;return -1;}
 if(disconnect_poll){connection_error=1;fd->revents=POLLHUP;return 1;}
 if(never_complete){now_ns+=(uint64_t)timeout*1000000;fd->revents=0;return 0;}
 now_ns+=1000000;fd->revents=POLLIN;
 if(poll_calls>=complete_poll)push(completion_event());else if(poll_calls&1)push_key();else push_expose();
 return 1;
}
static void reset_process_fixture(void){
 /* Only the CPU test models process teardown by resetting private helper
  * state. Production has no reset/retry path for a poisoned segment. */
 memset(&image,0,sizeof(image));image.fd=-1;display_alive=extension_present=version_supported=true;event_count=0;copies=put_calls=poll_calls=event_checks=attach_calls=detach_calls=map_calls=unmap_calls=fd_closes=bind_lookups=flush_calls=0;checked_put_error=attach_error=detach_error=0;connection_error=poll_error=0;descriptor_number=42;now_ns=0;next_put_sequence=100;last_put_sequence=0;complete_poll=1;zero_cookie=interrupt_first=clock_error=never_complete=disconnect_poll=false;complete_immediately=true;memset(input_pixels,0xa5,sizeof(input_pixels));
}
static int present(unsigned w,unsigned h,bool *sent,bool *ack,unsigned char *xe){return remesa_fast_shm_present(connection,42,77,w,h,input_pixels,sizeof(input_pixels),xe,sent,ack);}
static void bind(void){assert(!remesa_fast_shm_bind_display(display,connection,1000000000));}
static void poisoned_retained(void){
 assert(image.poisoned&&image.pixels==shared_pixels&&image.fd==42);unsigned cp=copies,puts=put_calls,de=detach_calls,un=unmap_calls,fc=fd_closes;unsigned char xe=0;bool sent=true,ack=true;memset(input_pixels,0x5c,sizeof(input_pixels));assert(present(1280,800,&sent,&ack,&xe)==EIO&&!sent&&!ack);assert(remesa_fast_shm_bind_display(display,connection,1000000000)==EIO);assert(remesa_fast_shm_close(connection)==EBUSY);assert(cp==copies&&puts==put_calls&&de==detach_calls&&un==unmap_calls&&fc==fd_closes);for(unsigned i=0;i<SHM_BYTES;i++)assert(shared_pixels[i]==0xa5);
 display_alive=false;assert(remesa_fast_shm_bind_display(display,connection,1000000000)==EIO);assert(present(1280,800,&sent,&ack,&xe)==EIO);assert(remesa_fast_shm_close(connection)==EBUSY);display_alive=true;
}
int main(void){
 bool sent,ack;unsigned char xe=0;
 reset_process_fixture();assert(present(1280,800,&sent,&ack,&xe)==ENOTCONN&&!sent&&!ack&&!copies);assert(remesa_fast_shm_bind_display(NULL,connection,1)==EINVAL);assert(remesa_fast_shm_bind_display(display,connection,1000000001)==EINVAL);
 bind();assert(remesa_fast_shm_bind_display(foreign_display,connection,1)==EBUSY);assert(remesa_fast_shm_bind_display(display,foreign_connection,1)==EBUSY);
 for(unsigned frame=0;frame<120;frame++){
  memset(input_pixels,(unsigned char)frame,sizeof(input_pixels));push_key();push_expose();unsigned oldpoll=poll_calls;assert(!present(1280,800,&sent,&ack,&xe)&&sent&&ack&&!image.in_flight&&!image.poisoned);assert(!memcmp(input_pixels,shared_pixels,sizeof(input_pixels)));assert(poll_calls==oldpoll&&event_count==2&&events[0].type==KeyPress&&events[0].xkey.keycode==113&&events[1].type==Expose);event_count=0;
 }
 assert(map_calls==1&&attach_calls==1&&copies==120&&put_calls==120&&!unmap_calls&&!fd_closes);assert(!remesa_fast_shm_close(connection)&&detach_calls==1&&unmap_calls==1&&fd_closes==1);assert(!remesa_fast_shm_close(connection)&&detach_calls==1&&unmap_calls==1&&fd_closes==1);
 /* All matching fields, synthetic bit and older serial are filtered. Later
  * sequence progress and32-bit rollover remain valid completion semantics. */
 reset_process_fixture();bind();assert(!present(1280,800,&sent,&ack,&xe));complete_immediately=false;never_complete=true;last_put_sequence=next_put_sequence;XEvent good=completion_event();
 for(unsigned kind=0;kind<10;kind++){XEvent bad=good;XShmCompletionEvent e;memcpy(&e,&bad,sizeof(e));switch(kind){case 0:e.type++;break;case 1:e.display=foreign_display;break;case 2:e.drawable++;break;case 3:e.shmseg++;break;case 4:e.offset=4;break;case 5:e.major_code++;break;case 6:e.minor_code++;break;case 7:e.send_event=True;break;case 8:e.serial=next_put_sequence-1;break;case 9:e.serial=(uint32_t)(next_put_sequence-0x80000000u);break;}memcpy(&bad,&e,sizeof(e));push(bad);}
 push(good);assert(!present(1280,800,&sent,&ack,&xe)&&sent&&ack&&event_count==10&&!poll_calls);assert(!remesa_fast_shm_close(connection));
 reset_process_fixture();bind();next_put_sequence=UINT32_MAX;assert(!present(1280,800,&sent,&ack,&xe)&&sent&&ack&&!poll_calls);assert(!remesa_fast_shm_close(connection));
 reset_process_fixture();bind();complete_immediately=false;complete_poll=3;assert(!present(1280,800,&sent,&ack,&xe)&&sent&&ack&&poll_calls==3&&event_count==2&&events[0].type==KeyPress&&events[1].type==Expose);assert(!remesa_fast_shm_close(connection));
 reset_process_fixture();bind();complete_immediately=false;interrupt_first=true;assert(!present(1280,800,&sent,&ack,&xe)&&poll_calls==2&&put_calls==1);assert(!remesa_fast_shm_close(connection));
 /* A resize is rejected before any map/copy/Put; it cannot rewrite a buffer. */
 reset_process_fixture();bind();assert(present(800,600,&sent,&ack,&xe)==EINVAL&&!sent&&!ack&&!put_calls&&!copies&&!map_calls);assert(!present(1280,800,&sent,&ack,&xe));unsigned cp=copies;assert(present(1280,799,&sent,&ack,&xe)==EINVAL&&copies==cp&&put_calls==1);assert(remesa_fast_shm_close(foreign_connection)==EBUSY);assert(!remesa_fast_shm_close(connection));
 for(unsigned failure=0;failure<7;failure++){
  reset_process_fixture();bind();push_key();push_expose();
  if(failure==0){complete_immediately=false;never_complete=true;}
  if(failure==1)checked_put_error=XCB_WINDOW;
  if(failure==2)zero_cookie=true;
  if(failure==3){complete_immediately=false;disconnect_poll=true;}
  if(failure==4)clock_error=true;
  if(failure==5){complete_immediately=false;poll_error=EBADF;}
  if(failure==6){complete_immediately=false;descriptor_number=-1;}
  int error=present(1280,800,&sent,&ack,&xe);assert(error&&!ack&&sent==(failure!=2));if(failure==0)assert(error==ETIMEDOUT&&poll_calls==1);if(failure==1)assert(error==EIO&&xe==XCB_WINDOW);if(failure==2)assert(error==EPROTO);if(failure==3)assert(error==EPIPE);assert(copies==1&&put_calls==1);
  unsigned queued=event_count;if(failure==0){push(completion_event());queued++;}poisoned_retained();assert(event_count==queued);
 }
 /* Normalclose while a modeled request is still in-flight refuses to detach;
  * the real mutex serializes concurrentclose with the bounded completionwait. */
 reset_process_fixture();bind();assert(!present(1280,800,&sent,&ack,&xe));image.in_flight=true;assert(remesa_fast_shm_close(connection)==EBUSY&&!detach_calls&&!unmap_calls&&!fd_closes);assert(remesa_fast_shm_bind_display(display,connection,1)==EBUSY);image.in_flight=false;assert(!remesa_fast_shm_close(connection));
 reset_process_fixture();bind();extension_present=false;assert(present(1280,800,&sent,&ack,&xe)==ENOTSUP&&!put_calls&&!copies&&!map_calls);assert(!remesa_fast_shm_close(connection));
 reset_process_fixture();bind();version_supported=false;assert(present(1280,800,&sent,&ack,&xe)==ENOTSUP&&!put_calls&&!copies&&!map_calls);assert(!remesa_fast_shm_close(connection));
 reset_process_fixture();bind();attach_error=XCB_ACCESS;assert(present(1280,800,&sent,&ack,&xe)==EIO&&!put_calls&&!copies&&map_calls==1&&unmap_calls==1&&fd_closes==1&&!image.pixels);assert(!remesa_fast_shm_close(connection));
 puts("PASS actual SHM helper:120frames singlebuffer,checked+completion before reuse,arrow/Expose preserved,10nonmatching completion fields,queuedbeforepoll,multipoll/EINTR,timeout/disconnect/error/zeroCookie/wrap/latecompletion/resize,poison retained nooverwrite/detach/unmap/retry,borrowedDisplay notdereferenced afterteardown,closeinflight,normalcleanup; CPU mocks GPU0 Xserver0");return 0;
}
