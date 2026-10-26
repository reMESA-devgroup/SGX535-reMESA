/* SPDX-License-Identifier: MIT */
#ifndef REMESA_GLX_LAB_FRONTEND_H
#define REMESA_GLX_LAB_FRONTEND_H
#include <X11/Xlib.h>
#include <stdint.h>
#include <errno.h>
#include "remesa_xcb.h"
struct remesa_device;
struct pipe_screen;
struct pipe_context;
struct pipe_resource;
struct pipe_fence_handle;
/* Private Xlib GLX LAB ABI, never a general DRI constructor. Install before
 * the first GLX operation on this Display. The caller serializes GL/GLX,
 * initializes Xlib threading before opening displays if needed, and retains
 * Display/device/backend until process exit. No implicit device opening,
 * backend selection, module loading, renderer fallback or execution consent.
 * Only an explicitly recording OR qualified hardware device is accepted.
 * Physical admission remains enforced by that device's existing validators.
 */
int glXRemesaLabInstall(Display *, struct remesa_device *, uint64_t timeout_ns);
/* Separate CPU-only Z24X8 attachment selector; no GPU depth draw support. */
int glXRemesaLabInstallDepth24Recorder(Display *,struct remesa_device *,uint64_t);
bool remesa_glx_lab_depth24_enabled(Display *);
/* Diagnostic error of the last frontend operation, not a GL error or evidence
 * of hardware rendering. Receipt means X server acceptance only. */
int glXRemesaLabStatus(struct remesa_xcb_receipt *);
struct pipe_screen *xlib_create_screen(Display *);
/* Used only by the opt-in XMesa overlay. No attachment rotation here. */
int remesa_glx_lab_present(Display *, Drawable, struct pipe_context *,
                           struct pipe_resource *, unsigned nboxes);
bool remesa_glx_lab_finish(struct pipe_context *,struct pipe_fence_handle *);
void remesa_glx_lab_fail(int);
void remesa_glx_lab_close_display(Display *);
/* Explicit private full-frame route; the backend must own the compiled
 * profile and actual submission/readback lifetime. Offline fixtures identify
 * themselves through offline_test_backend and never advertise acceleration. */
int glXRemesaLabInstallGearsStream(Display *,struct remesa_device *,uint64_t);
#endif
