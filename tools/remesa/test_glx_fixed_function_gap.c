/* SPDX-License-Identifier: MIT
 * CPU-only generated fixed-function NIR probe. This is NOT original glxgears,
 * and is NOT a rendering result. Device submit always refuses. */
int remesa_clear_fixture_unused_main(int,char **);
#define main remesa_clear_fixture_unused_main
#include "test_glx_lab_window.c"
#undef main
int main(int argc,char **argv)
{
 if(argc!=2)return 2;
 if(strcmp(argv[1],"0")&&strcmp(argv[1],"1"))return 2;
 int lit=argv[1][0]=='1';
 assert(XInitThreads());Display *d=XOpenDisplay(NULL);assert(d);
 assert(!glXRemesaLabInstall(d,&device,100000000));
 int required[]={GLX_RGBA,GLX_DOUBLEBUFFER,GLX_DEPTH_SIZE,1,None};
 XVisualInfo *dv=glXChooseVisual(d,DefaultScreen(d),required);
 printf("original_glxgears_depth_visual=%s\n",dv?"ACCEPTED":"REJECTED");assert(!dv);
 int attrs[]={GLX_RGBA,GLX_DOUBLEBUFFER,None};XVisualInfo *v=glXChooseVisual(d,DefaultScreen(d),attrs);assert(v);
 GLXContext c=glXCreateContext(d,v,NULL,True);assert(c);
 Colormap cm=XCreateColormap(d,RootWindow(d,v->screen),v->visual,AllocNone);
 XSetWindowAttributes wa={.colormap=cm};
 Window w=XCreateWindow(d,RootWindow(d,v->screen),0,0,32,32,0,v->depth,InputOutput,v->visual,CWColormap,&wa);
 assert(glXMakeCurrent(d,w,c));printf("renderer=%s CPU_ONLY=YES\n",glGetString(GL_RENDERER));
 glViewport(0,0,32,32);glDisable(GL_DITHER);
 glMatrixMode(GL_PROJECTION);glLoadIdentity();glFrustum(-1,1,-1,1,5,60);
 glMatrixMode(GL_MODELVIEW);glLoadIdentity();glTranslatef(0,0,-40);glRotatef(20,1,0,0);glRotatef(30,0,1,0);
 if(lit){GLfloat light[]={5,5,10,0},material[]={.8f,.1f,0,1};glLightfv(GL_LIGHT0,GL_POSITION,light);glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,material);glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glEnable(GL_NORMALIZE);}
 else glColor3f(0,1,0);
 glShadeModel(GL_FLAT);glBegin(GL_QUADS);glNormal3f(0,0,1);
 glVertex3f(-1,-1,1);glVertex3f(1,-1,1);glVertex3f(1,1,1);glVertex3f(-1,1,1);glEnd();glFlush();
 glXSwapBuffers(d,w);struct remesa_xcb_receipt receipt={0};int e=glXRemesaLabStatus(&receipt);
 printf("lit=%d rejection=%d submit_calls=%u published=%u\n",lit,e,submissions,receipt.image_acknowledged);
 assert(e==ENOTSUP&&submissions==0&&!receipt.image_acknowledged);
 glXMakeCurrent(d,None,NULL);glXDestroyContext(d,c);XDestroyWindow(d,w);XFreeColormap(d,cm);XFree(v);XCloseDisplay(d);
 return 0;
}
