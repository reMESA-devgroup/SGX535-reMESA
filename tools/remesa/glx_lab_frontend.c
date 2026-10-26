/* SPDX-License-Identifier: MIT */
#include "glx_lab_frontend.h"
#include "remesa_device.h"
#include "remesa_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_screen.h"
#include <X11/Xlib-xcb.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>

/* One explicit Display/device per process. XMesa caches its screen for the
 * Display lifetime; replacing an installed device would invalidate live
 * resources. There is deliberately no reset/uninstall operation. */
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static struct {
 Display *display;
 struct remesa_device *device;
 struct pipe_screen *screen;
 uint64_t timeout;
 int error;
 bool poisoned,closed,cpu_depth24,gears_runtime;
 struct remesa_xcb_receipt receipt;
} lab;
static int draw_error(struct pipe_context *pipe)
{
 struct remesa_draw_rejection_status status;
 int error=remesa_context_draw_rejections(pipe,&status);
 return error?error:status.failures?(status.error?status.error:EPROTO):remesa_context_error(pipe);
}

__attribute__((visibility("default")))
int glXRemesaLabInstall(Display *display,struct remesa_device *device,uint64_t timeout)
{
 int error=0;
 pthread_mutex_lock(&lock);
 if(lab.display)error=EBUSY;
 else if(!display||!device||!device->ops||!timeout||timeout>UINT64_C(1000000000))error=EINVAL;
 else if(device->offline_test_backend==device->qualified_hardware_backend||
         device->resource_only_backend)error=ENOTSUP;
 else {lab.display=display;lab.device=device;lab.timeout=timeout;}
 lab.error=error;
 pthread_mutex_unlock(&lock);return error;
}

__attribute__((visibility("default")))
int glXRemesaLabInstallDepth24Recorder(Display *display,struct remesa_device *device,uint64_t timeout)
{
 if(!device||!device->offline_test_backend||device->qualified_hardware_backend||device->resource_only_backend)return ENOTSUP;
 int e=glXRemesaLabInstall(display,device,timeout);if(e)return e;
 pthread_mutex_lock(&lock);lab.cpu_depth24=true;pthread_mutex_unlock(&lock);return 0;
}
__attribute__((visibility("default")))
int glXRemesaLabInstallGearsStream(Display *display,struct remesa_device *device,uint64_t timeout)
{
 if(!device||!device->gears_runtime||device->resource_only_backend||device->retained_fragment_carrier)return ENOTSUP;
 int e=glXRemesaLabInstall(display,device,timeout);if(e)return e;
 pthread_mutex_lock(&lock);lab.gears_runtime=true;pthread_mutex_unlock(&lock);return 0;
}
bool remesa_glx_lab_depth24_enabled(Display *display)
{
 pthread_mutex_lock(&lock);bool yes=display==lab.display&&(lab.cpu_depth24||lab.gears_runtime)&&!lab.closed;pthread_mutex_unlock(&lock);return yes;
}

struct pipe_screen *xlib_create_screen(Display *display)
{
 struct pipe_screen *screen=NULL;
 pthread_mutex_lock(&lock);
 if(!lab.display||display!=lab.display)lab.error=ENODEV;
 else if(lab.closed)lab.error=EPIPE;
 else if(lab.screen)lab.error=EALREADY;
 else {
  screen=remesa_screen_create(lab.device);
  if(!screen)lab.error=ENOTSUP;
  else if(lab.gears_runtime&&(lab.error=remesa_screen_enable_gears_runtime(screen))){screen->destroy(screen);screen=NULL;}
  else if(lab.cpu_depth24&&(lab.error=remesa_screen_enable_cpu_depth24_lab(screen))){screen->destroy(screen);screen=NULL;}
  else {lab.screen=screen;lab.error=0;}
 }
 pthread_mutex_unlock(&lock);return screen;
}

int remesa_glx_lab_present(Display *display,Drawable window,struct pipe_context *pipe,
 struct pipe_resource *resource,unsigned nboxes)
{
 int error=0;
 pthread_mutex_lock(&lock);
 memset(&lab.receipt,0,sizeof(lab.receipt));
 if(!lab.screen||display!=lab.display||!pipe||pipe->screen!=lab.screen||!resource||
    !window||window>UINT32_MAX)error=EINVAL;
 else if(lab.poisoned)error=EIO;
 else if(nboxes)error=ENOTSUP; /* No silently ignored partial-copy semantics. */
 else if((error=draw_error(pipe))==0) {
  /* No infinite fence wait, publication or exchange on timeout/error. The
   * backend retains/quarantines unretired resources as in the existing ABI. */
  error=remesa_resource_wait_writer(pipe,resource,lab.timeout);
  bool bottom_left=true;
  if(!error&&lab.gears_runtime)error=remesa_context_gears_bottom_left(pipe,&bottom_left);
  if(!error) {
   /* The window and XCB connection have the SAME XID namespace. Keep Xlib
    * ownership of events; flush buffered Xlib requests before checked XCB
    * requests, under the Display lock. Never disconnect this borrowed link.
    * This serialized interop is specific to this private frontend. */
   XLockDisplay(display);
   XFlush(display);
   struct xcb_connection_t *connection=XGetXCBConnection(display);
   error=connection?remesa_xcb_present_oriented(pipe,resource,
     bottom_left?REMESA_XCB_BOTTOM_LEFT:REMESA_XCB_TOP_LEFT,connection,(uint32_t)window,&lab.receipt):ENOTCONN;
   /* A shared Xlib connection cannot be disconnected here. Uncertain GC
    * cleanup poisons publication until the caller closes the Display/exits. */
   if(error&&!lab.receipt.gc_released)lab.poisoned=true;
   XUnlockDisplay(display);
  }
 }
 lab.error=error;pthread_mutex_unlock(&lock);return error;
}

bool remesa_glx_lab_finish(struct pipe_context *pipe,struct pipe_fence_handle *fence)
{
 int error=0;
 pthread_mutex_lock(&lock);
 memset(&lab.receipt,0,sizeof(lab.receipt));
 if(!lab.screen||!pipe||pipe->screen!=lab.screen)error=EINVAL;
 else if(lab.poisoned)error=EIO;
 else if((error=draw_error(pipe))==0&&fence&&
  !lab.screen->fence_finish(lab.screen,pipe,fence,lab.timeout))error=ETIMEDOUT;
 lab.error=error;pthread_mutex_unlock(&lock);return !error;
}

void remesa_glx_lab_fail(int error)
{
 pthread_mutex_lock(&lock);lab.error=error;
 memset(&lab.receipt,0,sizeof(lab.receipt));pthread_mutex_unlock(&lock);
}
void remesa_glx_lab_close_display(Display *display)
{
 pthread_mutex_lock(&lock);
 if(display==lab.display) {
  lab.closed=true;lab.screen=NULL;lab.error=EPIPE;
  memset(&lab.receipt,0,sizeof(lab.receipt));
 }
 pthread_mutex_unlock(&lock);
}

__attribute__((visibility("default")))
int glXRemesaLabStatus(struct remesa_xcb_receipt *receipt)
{
 pthread_mutex_lock(&lock);
 if(receipt)*receipt=lab.receipt;
 int error=lab.error;
 pthread_mutex_unlock(&lock);return error;
}
