/* SPDX-License-Identifier: MIT */
#ifndef REMESA_GLX_LAB_TRIANGLE_H
#define REMESA_GLX_LAB_TRIANGLE_H
struct remesa_device;
/* One original GLSL/NIR draw into a fresh32x32 LAB window. Caller supplies
 * the already authenticated one-attempt backend, protected directory and
 * external timeout/recovery controller. Never opens DRM or enables a gate.
 * Auxiliary retained programs remain explicitly part of the LAB backend.
 * CPU return alone cannot attest submission/completion/retirement. */
int remesa_glx_lab_triangle(struct remesa_device *,const char *directory);
#endif
