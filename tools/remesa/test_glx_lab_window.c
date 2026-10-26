/* SPDX-License-Identifier: MIT
 * Real GLX/default framebuffer/X11 transport test, explicitly CPU clear only.
 * Its device refuses every submission. NEVER an acceleration qualification.
 */
#include "glx_lab_frontend.h"
#include "remesa_device.h"
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xutil.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned submissions,allocations;
static int allocate(void *priv,size_t size,void **out)
{ (void)priv;*out=calloc(1,size);if(*out)allocations++;return *out?0:ENOMEM; }
static void destroy(void *priv,void *bo)
{ (void)priv;free(bo);allocations--; }
static int map(void *priv,void *bo,bool write,void **out)
{ (void)priv;(void)write;*out=bo;return 0; }
static void unmap(void *priv,void *bo,bool write)
{ (void)priv;(void)bo;(void)write; }
static int submit(void *priv,void *bo,const struct remesa_draw_packet *packet,void **fence)
{ (void)priv;(void)bo;(void)packet;*fence=NULL;submissions++;return ENOTSUP; }
static int wait(void *priv,void *fence,uint64_t timeout,bool visible)
{ (void)priv;(void)fence;(void)timeout;(void)visible;return ENOTSUP; }
static void destroy_fence(void *priv,void *fence)
{ (void)priv;(void)fence;abort(); }
static const struct remesa_device_ops ops={.allocate=allocate,.destroy_bo=destroy,
 .map=map,.unmap=unmap,.submit=submit,.wait=wait,.destroy_fence=destroy_fence};
static struct remesa_device device={.ops=&ops,.offline_test_backend=true};

int main(int argc,char **argv)
{
 if(argc!=2||(strcmp(argv[1],"--cpu-transport")&&strcmp(argv[1],"--negative-draw")&&strcmp(argv[1],"--deferred-destroy")))return 2;
 bool negative=!strcmp(argv[1],"--negative-draw");
 bool deferred=!strcmp(argv[1],"--deferred-destroy");
 setvbuf(stdout,NULL,_IONBF,0);
 assert(XInitThreads());
 Display *d=XOpenDisplay(NULL);if(!d){fprintf(stderr,"XOpenDisplay failed\n");return 2;}
 assert(glXRemesaLabInstall(d,&device,UINT64_C(100000000))==0);
 assert(!strcmp(glXQueryExtensionsString(d,DefaultScreen(d)),"GLX_ARB_get_proc_address "));
 int depth_attributes[]={GLX_RGBA,GLX_DOUBLEBUFFER,GLX_DEPTH_SIZE,1,None};
 assert(!glXChooseVisual(d,DefaultScreen(d),depth_attributes));
 puts("glxgears_depth_visual=UNSUPPORTED");
 int attributes[]={GLX_RGBA,GLX_DOUBLEBUFFER,GLX_RED_SIZE,8,GLX_GREEN_SIZE,8,GLX_BLUE_SIZE,8,None};
 XVisualInfo *v=glXChooseVisual(d,DefaultScreen(d),attributes);
 if(!v){fprintf(stderr,"glXChooseVisual failed; LAB=%d\n",glXRemesaLabStatus(NULL));return 3;}
 printf("visual_depth=%d visual_id=%lx\n",v->depth,v->visualid);
 GLXContext context=glXCreateContext(d,v,NULL,True);
 if(!context){fprintf(stderr,"glXCreateContext failed; LAB=%d\n",glXRemesaLabStatus(NULL));return 4;}
 Colormap cmap=XCreateColormap(d,RootWindow(d,v->screen),v->visual,AllocNone);
 XSetWindowAttributes wa={.colormap=cmap,.border_pixel=0,.event_mask=StructureNotifyMask};
 Window w=XCreateWindow(d,RootWindow(d,v->screen),0,0,32,32,0,v->depth,InputOutput,
  v->visual,CWColormap|CWEventMask|CWBorderPixel,&wa);
 XStoreName(d,w,"reMESA GLX CPU-clear transport test (not GPU)");
 XMapWindow(d,w);XSync(d,False);
 assert(glXMakeCurrent(d,w,context));
 printf("renderer=%s\nversion=%s\n",glGetString(GL_RENDERER),glGetString(GL_VERSION));
 for(unsigned frame=0;frame<2;frame++) {
  glClearColor(frame?1:0,frame?0:1,frame?1:0,1);
  glClear(GL_COLOR_BUFFER_BIT);assert(glGetError()==GL_NO_ERROR);
  if(!frame) {
   glEnable(GL_SCISSOR_TEST);glScissor(3,5,7,9);
   glClearColor(1,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
   glDisable(GL_SCISSOR_TEST);assert(glGetError()==GL_NO_ERROR);
  }
  uint32_t back[1024];glReadBuffer(GL_BACK);
  glReadPixels(0,0,32,32,GL_BGRA,GL_UNSIGNED_BYTE,back);assert(glGetError()==GL_NO_ERROR);
  for(unsigned y=0;y<32;y++)for(unsigned x=0;x<32;x++)
   assert(back[y*32+x]==(frame?UINT32_C(0xffff00ff):x>=3&&x<10&&y>=5&&y<14?
     UINT32_C(0xffff0000):UINT32_C(0xff00ff00)));
  glXSwapBuffers(d,w);
  struct remesa_xcb_receipt receipt={0};int error=glXRemesaLabStatus(&receipt);
  printf("frame=%u error=%d ack=%d cleanup=%d bytes=%u submissions=%u\n",
   frame,error,receipt.image_acknowledged,receipt.gc_released,receipt.snapshot_bytes,submissions);
  assert(!error&&receipt.image_acknowledged&&receipt.gc_released&&receipt.snapshot_bytes==4096);
  XImage *image=XGetImage(d,w,0,0,32,32,AllPlanes,ZPixmap);assert(image);
  unsigned mismatches=0;char path[32];snprintf(path,sizeof(path),"frame-%u.rgb",frame);
  FILE *pixels=fopen(path,"wb");assert(pixels);
  for(unsigned y=0;y<32;y++)for(unsigned x=0;x<32;x++) {
   unsigned long pixel=XGetPixel(image,x,y)&0xffffff;
   unsigned long expected=frame?0xff00ff:((x>=3&&x<10&&y>=18&&y<27)?0xff0000:0x00ff00);
   unsigned char rgb[3]={pixel>>16,pixel>>8,pixel};assert(fwrite(rgb,1,3,pixels)==3);
   mismatches+=pixel!=expected;
  }
  assert(!fclose(pixels));printf("frame=%u pixel_mismatches=%u\n",frame,mismatches);
  assert(!mismatches);
  XDestroyImage(image);
  uint32_t front[1024];glReadBuffer(GL_FRONT);
  glReadPixels(0,0,32,32,GL_BGRA,GL_UNSIGNED_BYTE,front);assert(glGetError()==GL_NO_ERROR);
  assert(!memcmp(front,back,sizeof(front)));
 }
 assert(submissions==0);
 if(negative) {
 /* A fixed-function draw outside the supported shader profile must not turn
  * the previously published magenta frame into an apparent draw success. */
 puts("negative: begin");glBegin(GL_TRIANGLES);
 glVertex3f(-.5f,-.5f,0);glVertex3f(.5f,-.5f,0);
 glVertex3f(0,.5f,0);puts("negative: end");glEnd();
 puts("negative: flush");glFlush();puts("negative: flushed");
 glDisable(GL_DITHER); /* successful state change must not erase rejection */
 glXSwapBuffers(d,w);
 struct remesa_xcb_receipt rejected={0};
 int draw_error=glXRemesaLabStatus(&rejected);
 printf("unsupported_draw error=%d ack=%d submissions=%u\n",draw_error,rejected.image_acknowledged,submissions);
 assert(draw_error==ENOTSUP&&!rejected.image_acknowledged&&submissions==0);
 XImage *retained=XGetImage(d,w,0,0,32,32,AllPlanes,ZPixmap);assert(retained);
 for(unsigned y=0;y<32;y++)for(unsigned x=0;x<32;x++)assert((XGetPixel(retained,x,y)&0xffffff)==0xff00ff);
 XDestroyImage(retained);
 }
 if(deferred)glXDestroyContext(d,context);
 assert(glXMakeCurrent(d,None,NULL)&&!glXGetCurrentContext());
 if(!deferred)glXDestroyContext(d,context);
 printf("allocations_after_context=%u\n",allocations);
 XDestroyWindow(d,w);XFreeColormap(d,cmap);XFree(v);XCloseDisplay(d);
 printf("allocations_after_display=%u\n",allocations);
 /* XMesa caches the Display screen until process exit, but attachments must
  * have released every backend allocation when the window is destroyed. */
 assert(allocations==0);
 printf("GLX_LAB_CPU_CLEAR_WINDOW_PASS frames=2 negative=%d deferred=%d GPU=NO submissions=0\n",negative,deferred);return 0;
}
