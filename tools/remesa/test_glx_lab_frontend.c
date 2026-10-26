/* SPDX-License-Identifier: MIT */
#include "glx_lab_frontend.h"
#include "remesa_device.h"
#include "remesa_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_screen.h"
#include <X11/Xlib-xcb.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned checks,creates,waits,presents,locks,unlocks,flushes,finishes;
#define CHECK(x) do { checks++; assert(x); } while(0)
static int context_error,writer_error,present_error;
static struct remesa_draw_rejection_status rejections;
static bool finish_ready=true,cleanup=true;
static uint64_t last_timeout;
static struct pipe_screen screen;
static struct pipe_context context={.screen=&screen},foreign;
static Display *const display=(Display *)(uintptr_t)16;
static struct pipe_resource *const resource=(struct pipe_resource *)(uintptr_t)32;
struct pipe_screen *remesa_screen_create(struct remesa_device *device)
{ CHECK(device!=NULL);creates++;return &screen; }
int remesa_context_error(const struct pipe_context *pipe)
{ CHECK(pipe==&context);return context_error; }
int remesa_context_draw_rejections(struct pipe_context *pipe,struct remesa_draw_rejection_status *out)
{ CHECK(pipe==&context);*out=rejections;return 0; }
int remesa_resource_wait_writer(struct pipe_context *pipe,struct pipe_resource *res,uint64_t timeout)
{ CHECK(pipe==&context&&res==resource);last_timeout=timeout;waits++;return writer_error; }
int remesa_xcb_present_oriented(struct pipe_context *pipe,struct pipe_resource *res,
 enum remesa_xcb_origin origin,struct xcb_connection_t *connection,uint32_t window,
 struct remesa_xcb_receipt *receipt)
{
 CHECK(pipe==&context&&res==resource&&origin==REMESA_XCB_BOTTOM_LEFT);
 CHECK(connection==(void *)(uintptr_t)48&&window==64);
 CHECK(locks==unlocks+1&&flushes==locks);
 presents++;receipt->gc_released=cleanup;receipt->image_acknowledged=!present_error;
 return present_error;
}
void XLockDisplay(Display *d){CHECK(d==display);locks++;}
void XUnlockDisplay(Display *d){CHECK(d==display);unlocks++;}
int XFlush(Display *d){CHECK(d==display);flushes++;return 1;}
xcb_connection_t *XGetXCBConnection(Display *d)
{ CHECK(d==display);return (void *)(uintptr_t)48; }
static bool finish(struct pipe_screen *s,struct pipe_context *p,
 struct pipe_fence_handle *f,uint64_t timeout)
{ CHECK(s==&screen&&p==&context&&f!=NULL);last_timeout=timeout;finishes++;return finish_ready; }
int main(void)
{
 const struct remesa_device_ops ops={0};
 struct remesa_device device={.ops=&ops,.offline_test_backend=true};
 CHECK(xlib_create_screen(display)==NULL&&glXRemesaLabStatus(NULL)==ENODEV&&creates==0);
 CHECK(glXRemesaLabInstall(NULL,&device,1)==EINVAL);
 CHECK(glXRemesaLabInstall(display,&device,0)==EINVAL);
 CHECK(glXRemesaLabInstall(display,&device,UINT64_C(1000000001))==EINVAL);
 device.qualified_hardware_backend=true;
 CHECK(glXRemesaLabInstall(display,&device,1)==ENOTSUP);
 device.offline_test_backend=false;device.qualified_hardware_backend=false;
 CHECK(glXRemesaLabInstall(display,&device,1)==ENOTSUP);
 device.offline_test_backend=true;device.resource_only_backend=true;
 CHECK(glXRemesaLabInstall(display,&device,1)==ENOTSUP);
 device.resource_only_backend=false;
 CHECK(glXRemesaLabInstall(display,&device,123456)==0);
 CHECK(glXRemesaLabInstall(display,&device,1)==EBUSY);
 CHECK(xlib_create_screen((Display *)(uintptr_t)17)==NULL&&creates==0);
 CHECK(xlib_create_screen(display)==&screen&&creates==1);
 CHECK(xlib_create_screen(display)==NULL&&creates==1);
 CHECK(remesa_glx_lab_present(display,64,&foreign,resource,0)==EINVAL&&waits==0);
 CHECK(remesa_glx_lab_present(display,64,&context,resource,1)==ENOTSUP&&waits==0);
 context_error=ENOTSUP;
 CHECK(remesa_glx_lab_present(display,64,&context,resource,0)==ENOTSUP&&waits==0);
 context_error=0;writer_error=ETIMEDOUT;
 rejections=(struct remesa_draw_rejection_status){.failures=1,.error=ENOTSUP};
 CHECK(remesa_glx_lab_present(display,64,&context,resource,0)==ENOTSUP&&waits==0);
 CHECK(!remesa_glx_lab_finish(&context,NULL)&&presents==0);
 rejections=(struct remesa_draw_rejection_status){0}; /* fresh fixture context */
 CHECK(remesa_glx_lab_present(display,64,&context,resource,0)==ETIMEDOUT);
 CHECK(waits==1&&presents==0&&last_timeout==123456&&locks==0);
 writer_error=0;
 CHECK(remesa_glx_lab_present(display,64,&context,resource,0)==0);
 CHECK(presents==1&&locks==1&&unlocks==1);
 struct remesa_xcb_receipt receipt={0};
 CHECK(glXRemesaLabStatus(&receipt)==0&&receipt.image_acknowledged&&receipt.gc_released);
 screen.fence_finish=finish;finish_ready=false;
 CHECK(!remesa_glx_lab_finish(&context,(void *)(uintptr_t)1));
 CHECK(glXRemesaLabStatus(NULL)==ETIMEDOUT&&finishes==1&&last_timeout==123456);
 CHECK(glXRemesaLabStatus(&receipt)==ETIMEDOUT&&!receipt.image_acknowledged);
 CHECK(remesa_glx_lab_finish(&context,NULL));
 finish_ready=true;CHECK(remesa_glx_lab_finish(&context,(void *)(uintptr_t)1));
 present_error=EIO;cleanup=false;
 CHECK(remesa_glx_lab_present(display,64,&context,resource,0)==EIO);
 unsigned prior_waits=waits;
 CHECK(remesa_glx_lab_present(display,64,&context,resource,0)==EIO&&waits==prior_waits&&presents==2);
 CHECK(!remesa_glx_lab_finish(&context,NULL));
 CHECK(locks==unlocks);
 remesa_glx_lab_close_display(display);
 CHECK(xlib_create_screen(display)==NULL&&glXRemesaLabStatus(NULL)==EPIPE&&creates==1);
 printf("GLX_LAB_FRONTEND_PASS checks=%u GPU=NO\n",checks);return 0;
}
