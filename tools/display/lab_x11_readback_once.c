/* SPDX-License-Identifier: MIT */
/* A CPU X11 publication probe, not GLX, a renderer, or a GPU submission tool.
 * Input provenance must be authenticated separately before this is invoked. */
#define _POSIX_C_SOURCE 200809L
#include "remesa_xcb.h"
#include <xcb/xcb.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int check(xcb_connection_t *c,xcb_void_cookie_t cookie)
{
 if(!cookie.sequence)return EPROTO;
 xcb_generic_error_t *e=xcb_request_check(c,cookie);
 if(e){free(e);return EIO;}
 return xcb_connection_has_error(c)?EPIPE:0;
}
static int read_frame(const char *path,unsigned char frame[4096])
{
 int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return errno;
 struct stat st;int error=0;
 if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size!=4096)error=EINVAL;
 size_t done=0;
 while(!error&&done<4096){ssize_t n=read(fd,frame+done,4096-done);if(n<0&&errno==EINTR)continue;if(n<=0){error=EIO;break;}done+=(size_t)n;}
 close(fd);return error;
}
static int wait_mapped(xcb_connection_t *c,xcb_window_t window)
{
 /* Bounded wait for this window only; never repaint or retry publication. */
 for(unsigned i=0;i<40;i++){
  xcb_generic_event_t *e;
  while((e=xcb_poll_for_event(c))){
   bool mapped=(e->response_type&127)==XCB_MAP_NOTIFY&&((xcb_map_notify_event_t *)e)->window==window;
   bool xerror=e->response_type==0;free(e);
   if(mapped)return 0;
   if(xerror)return EIO;
  }
  if(xcb_connection_has_error(c))return EPIPE;
  struct pollfd p={.fd=xcb_get_file_descriptor(c),.events=POLLIN};
  int r=poll(&p,1,50);if(r<0&&errno!=EINTR)return errno;
 }
 return ETIMEDOUT;
}
int main(int argc,char **argv)
{
 const char *display=getenv("DISPLAY");
 if(argc!=5||strcmp(argv[1],"--publish-captured-32x32-once")||!display||strcmp(display,":0")){
  fprintf(stderr,"CPU publication only. DISPLAY=:0 %s --publish-captured-32x32-once INPUT.bgra SERVER.bgrx REPORT.json\nNo renderer or GPU operation. Authenticate input provenance separately.\n",argv[0]);return 64;
 }
 unsigned char input[4096];int error=read_frame(argv[2],input);if(error){fprintf(stderr,"input errno=%d\n",error);return 1;}
 FILE *report=fopen(argv[4],"wx");if(!report){perror("report");return 1;}
 xcb_connection_t *c=NULL;xcb_window_t window=0;bool created=false,mapped=false,capture=false,match=false,closed=false;
 struct remesa_xcb_receipt receipt={0};unsigned mismatch=0;int number=0;
 c=xcb_connect(display,&number);if(!c||xcb_connection_has_error(c)){error=EPIPE;goto done;}
 const xcb_setup_t *setup=xcb_get_setup(c);xcb_screen_iterator_t it=xcb_setup_roots_iterator(setup);
 while(number--&&it.rem)xcb_screen_next(&it);
 if(!it.rem||it.data->root_depth!=24){error=ENOTSUP;goto done;}
 xcb_screen_t *screen=it.data;window=xcb_generate_id(c);if(!window||window==UINT32_MAX){error=ENOSPC;goto done;}
 uint32_t values[]={screen->black_pixel,XCB_EVENT_MASK_STRUCTURE_NOTIFY|XCB_EVENT_MASK_EXPOSURE};
 error=check(c,xcb_create_window_checked(c,24,window,screen->root,64,64,32,32,0,
  XCB_WINDOW_CLASS_INPUT_OUTPUT,screen->root_visual,XCB_CW_BACK_PIXEL|XCB_CW_EVENT_MASK,values));
 if(error)goto done;
 created=true;
 const char title[]="reMESA readback publication probe - CPU X11 blit";
 error=check(c,xcb_change_property_checked(c,XCB_PROP_MODE_REPLACE,window,XCB_ATOM_WM_NAME,XCB_ATOM_STRING,8,sizeof(title)-1,title));if(error)goto done;
 error=check(c,xcb_map_window_checked(c,window));if(error)goto done;
 error=wait_mapped(c,window);if(error)goto done;
 mapped=true;
 error=remesa_xcb_present_bgra(input,128,sizeof(input),32,32,REMESA_XCB_BOTTOM_LEFT,c,window,&receipt);if(error)goto done;
 xcb_generic_error_t *xe=NULL;
 xcb_get_image_reply_t *image=xcb_get_image_reply(c,xcb_get_image(c,XCB_IMAGE_FORMAT_Z_PIXMAP,window,0,0,32,32,0xffffff),&xe);
 if(!image||xe||image->depth!=24||image->visual!=screen->root_visual||xcb_get_image_data_length(image)!=4096){free(xe);free(image);error=EIO;goto done;}
 unsigned char *actual=xcb_get_image_data(image);capture=true;
 for(unsigned y=0;y<32;y++)for(unsigned x=0;x<32;x++)
  for(unsigned b=0;b<3;b++)mismatch+=actual[(y*32+x)*4+b]!=input[((31-y)*32+x)*4+b];
 match=mismatch==0;
 FILE *f=fopen(argv[3],"wx");if(!f){error=errno;free(image);goto done;}
 if(fwrite(actual,1,4096,f)!=4096)error=EIO;
 if(fclose(f)&&!error)error=EIO;
 free(image);if(!error&&!match)error=EILSEQ;
 if(!error){struct timespec pause={.tv_sec=1};nanosleep(&pause,NULL);}
 done:
 if(c){if(created){int cleanup=check(c,xcb_destroy_window_checked(c,window));closed=!cleanup;if(!error)error=cleanup;}xcb_disconnect(c);}
 int written=fprintf(report,"{\"scope\":\"CPU_X11_PUBLICATION_OF_EXTERNALLY_AUTHENTICATED_READBACK\",\"new_GPU_submissions\":0,\"GLX_context\":false,\"renderer\":\"none - no GL library used\",\"width\":32,\"height\":32,\"mapped\":%s,\"image_acknowledged\":%s,\"image_requests\":%u,\"source_y_inverted\":%s,\"server_readback\":%s,\"RGB24_match\":%s,\"mismatched_channels\":%u,\"window_destroyed\":%s,\"error\":%d}\n",mapped?"true":"false",receipt.image_acknowledged?"true":"false",receipt.image_requests,receipt.source_y_inverted?"true":"false",capture?"true":"false",match?"true":"false",mismatch,closed?"true":"false",error);
 if(written<0&&!error)error=EIO;
 if(fclose(report)&&!error)error=EIO;
 return error?1:0;
}
