/* SPDX-License-Identifier: MIT */
#ifndef REMESA_LOADER_H
#define REMESA_LOADER_H
struct pipe_screen;
/* Loader fd intake for isolated Gallium clients. Borrows fd, owns a CLOEXEC
 * duplicate. This diagnostic screen is not OpenGL-capable and is not registered
 * in Mesa's DRI loader. No draw or memory ioctl is issued during construction. */
struct pipe_screen *remesa_loader_diagnostic_screen(int drm_fd, int *error);
struct remesa_native_2d_failure;
struct remesa_native_2d_report {
 int error,cleanup_error;
 /* NonNULL means retained, uncertain ownership. Keep this handle; no forced
  * fd/provider destruction or retry is offered. Kernel recovery is separate. */
 struct remesa_native_2d_failure *retained;
};
/* Explicit privileged native 2D factory, not the DRI/OpenGL constructor.
 * Borrows the supplied fd, owns its CLOEXEC duplicate and GEM session until
 * the last screen child dies. Fixed qualified private-memory profile; drawing
 * stays disabled. Does not change kernel enable gates or install anything. */
struct pipe_screen *remesa_loader_native_2d_screen(int,struct remesa_native_2d_report *);
#endif
