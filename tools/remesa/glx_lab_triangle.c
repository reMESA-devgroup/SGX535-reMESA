/* SPDX-License-Identifier: MIT */
#define GL_GLEXT_PROTOTYPES
#include "glx_lab_triangle.h"
#include "glx_lab_frontend.h"
#include "retained_fragment_capture.h"
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xutil.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

int remesa_glx_lab_triangle(struct remesa_device *device,const char *directory)
{
 int error=ENOTSUP;Display *display=NULL;XVisualInfo *visual=NULL;
 GLXContext context=NULL;Window window=0;Colormap colormap=0;
 GLuint shaders[2]={0},program=0,buffer=0;bool current=false,attempted=false;
 unsigned green=0,zero=0;struct remesa_xcb_receipt receipt={0};
 uint32_t readback[1024];unsigned char published[3072];
 /* The one-attempt native retained controller remains the authority. */
 if(!device||!directory||!device->qualified_hardware_backend||
    device->offline_test_backend)return EINVAL;
 if(!XInitThreads())return EIO;
 display=XOpenDisplay(NULL);if(!display)return ENOTCONN;
 error=glXRemesaLabInstall(display,device,UINT64_C(500000000));if(error)goto done;
 int attributes[]={GLX_RGBA,GLX_DOUBLEBUFFER,GLX_RED_SIZE,8,GLX_GREEN_SIZE,8,GLX_BLUE_SIZE,8,None};
 visual=glXChooseVisual(display,DefaultScreen(display),attributes);if(!visual){error=ENOTSUP;goto done;}
 context=glXCreateContext(display,visual,NULL,True);if(!context){error=ENOTSUP;goto done;}
 colormap=XCreateColormap(display,RootWindow(display,visual->screen),visual->visual,AllocNone);
 XSetWindowAttributes wa={.colormap=colormap,.border_pixel=0};
 window=XCreateWindow(display,RootWindow(display,visual->screen),0,0,32,32,0,
  visual->depth,InputOutput,visual->visual,CWColormap|CWBorderPixel,&wa);
 XStoreName(display,window,"reMESA one-attempt GLX LAB triangle");
 XMapWindow(display,window);XSync(display,False);
 if(!glXMakeCurrent(display,window,context)){error=EIO;goto done;}current=true;
 /* Record the actual context and drawable, never the CPU transport fixture's
  * renderer string. The private factory is mandatory; a foreign context is
  * rejected before compiling shaders or attempting a draw. These strings are
  * diagnostics, not evidence of GPU execution or an OpenGL feature claim. */
 const char *renderer=(const char *)glGetString(GL_RENDERER);
 const char *version=(const char *)glGetString(GL_VERSION);
 const char *vendor=(const char *)glGetString(GL_VENDOR);
 XWindowAttributes actual;
 if(!renderer||!version||!vendor||!strstr(renderer,"reMESA")||
    !XGetWindowAttributes(display,window,&actual)||actual.width!=32||actual.height!=32) {
  error=EPROTO;goto done;
 }
 char identity[1024];int identity_bytes=snprintf(identity,sizeof(identity),
  "scope=PRIVATE_GLX_LAB_NOT_GENERAL_OPENGL\nrenderer=%s\nvendor=%s\nversion=%s\n"
  "drawable_width=%d\ndrawable_height=%d\nGPU_execution_requires_external_receipt=true\n",
  renderer,vendor,version,actual.width,actual.height);
 if(identity_bytes<0||(size_t)identity_bytes>=sizeof(identity)){error=EOVERFLOW;goto done;}
 error=remesa_fragment_save(directory,"glx-context.txt",identity,(size_t)identity_bytes);
 if(error)goto done;
 const GLchar *texts[2]={
  "#version 100\nattribute vec4 position;void main(){gl_Position=position;}\n",
  "#version 100\nprecision highp float;void main(){gl_FragColor=vec4(0.0,1.0,0.0,1.0);}\n"};
 for(unsigned i=0;i<2;i++) {
  shaders[i]=glCreateShader(i?GL_FRAGMENT_SHADER:GL_VERTEX_SHADER);
  glShaderSource(shaders[i],1,&texts[i],NULL);glCompileShader(shaders[i]);
  GLint compiled=0;glGetShaderiv(shaders[i],GL_COMPILE_STATUS,&compiled);
  if(!compiled){error=ENOTSUP;goto done;}
 }
 program=glCreateProgram();for(unsigned i=0;i<2;i++)glAttachShader(program,shaders[i]);
 glLinkProgram(program);GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);
 if(!linked){error=ENOTSUP;goto done;}glUseProgram(program);
 GLint slot=glGetAttribLocation(program,"position");if(slot<0){error=EPROTO;goto done;}
 const GLfloat vertices[12]={-.484375f,-.484375f,0,1,.515625f,-.484375f,0,1,-.484375f,.515625f,0,1};
 glGenBuffers(1,&buffer);glBindBuffer(GL_ARRAY_BUFFER,buffer);
 glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
 glVertexAttribPointer(slot,4,GL_FLOAT,GL_FALSE,16,NULL);glEnableVertexAttribArray(slot);
 glDisable(GL_DITHER);glViewport(0,0,32,32);glDrawBuffer(GL_BACK);
 if(glGetError()!=GL_NO_ERROR){error=EIO;goto done;}
 /* No prior clear/load request: the qualified native profile is REPLACE. */
 attempted=true;glDrawArrays(GL_TRIANGLES,0,3);glFlush();
 if(glGetError()!=GL_NO_ERROR){error=EIO;goto done;}
 glXSwapBuffers(display,window);error=glXRemesaLabStatus(&receipt);if(error)goto done;
 if(!receipt.image_acknowledged||!receipt.gc_released||receipt.snapshot_bytes!=4096){error=EPROTO;goto done;}
 glReadBuffer(GL_FRONT);glReadPixels(0,0,32,32,GL_BGRA,GL_UNSIGNED_BYTE,readback);
 if(glGetError()!=GL_NO_ERROR){error=EIO;goto done;}
 for(unsigned y=0;y<32;y++)for(unsigned x=0;x<32;x++) {
  /* Independent sample-center oracle; no sample ties this quarter geometry. */
  bool inside=x>=8&&y>=8&&x+y<=31;
  uint32_t expected=inside?UINT32_C(0xff00ff00):0;
  if(readback[y*32+x]!=expected){error=EILSEQ;goto done;}
  green+=inside;zero+=!inside;
 }
 error=remesa_fragment_save(directory,"glx-readback.bin",readback,sizeof(readback));if(error)goto done;
 XImage *image=XGetImage(display,window,0,0,32,32,AllPlanes,ZPixmap);
 if(!image){error=EIO;goto done;}
 for(unsigned y=0;y<32;y++)for(unsigned x=0;x<32;x++) {
  uint32_t rgb=XGetPixel(image,x,y)&0xffffff;
  if(rgb!=(readback[(31-y)*32+x]&0xffffff))error=EILSEQ;
  published[(y*32+x)*3]=rgb>>16;published[(y*32+x)*3+1]=rgb>>8;published[(y*32+x)*3+2]=rgb;
 }
 XDestroyImage(image);if(error)goto done;
 error=remesa_fragment_save(directory,"glx-window-rgb.bin",published,sizeof(published));
done:
 {
  char report[256];int n=snprintf(report,sizeof(report),
   "{\"error\":%d,\"draw_attempted\":%s,\"put_image_ack\":%s,\"green\":%u,\"zero\":%u,\"retirement_requires_external_receipt\":true}\n",
   error,attempted?"true":"false",receipt.image_acknowledged?"true":"false",green,zero);
  if(n>0&&(size_t)n<sizeof(report)) {
   int saved=remesa_fragment_save(directory,"glx-client-result.json",report,(size_t)n);
   if(!error)error=saved;
  }
 }
 if(current) {
  glUseProgram(0);if(buffer)glDeleteBuffers(1,&buffer);
  if(program)glDeleteProgram(program);
  for(unsigned i=0;i<2;i++)if(shaders[i])glDeleteShader(shaders[i]);
  glXMakeCurrent(display,None,NULL);
 }
 if(context)glXDestroyContext(display,context);
 if(window)XDestroyWindow(display,window);
 if(colormap)XFreeColormap(display,colormap);
 if(visual)XFree(visual);
 if(display)XCloseDisplay(display);
 return error;
}
