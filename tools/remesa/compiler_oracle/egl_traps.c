/* SPDX-License-Identifier: MIT */
/* Every unexpected original device/EGL call terminates the CPU replay. */
#include <stdio.h>
#include <stdlib.h>
void KEGLBindImage(void){fputs("BLOCKED KEGLBindImage\n",stderr);_Exit(91);}
void KEGLCreateRenderSurface(void){fputs("BLOCKED KEGLCreateRenderSurface\n",stderr);_Exit(91);}
void KEGLDestroyRenderSurface(void){fputs("BLOCKED KEGLDestroyRenderSurface\n",stderr);_Exit(91);}
void KEGLSurfaceUnbind(void){fputs("BLOCKED KEGLSurfaceUnbind\n",stderr);_Exit(91);}
void KEGLGetImageSource(void){fputs("BLOCKED KEGLGetImageSource\n",stderr);_Exit(91);}
void KEGLSurfaceBind(void){fputs("BLOCKED KEGLSurfaceBind\n",stderr);_Exit(91);}
void KEGLUnbindImage(void){fputs("BLOCKED KEGLUnbindImage\n",stderr);_Exit(91);}
void KEGLResizeRenderSurface(void){fputs("BLOCKED KEGLResizeRenderSurface\n",stderr);_Exit(91);}
void KEGLGetDrawableParameters(void){fputs("BLOCKED KEGLGetDrawableParameters\n",stderr);_Exit(91);}
