/* SPDX-License-Identifier: MIT */
#ifndef REMESA_XCB_H
#define REMESA_XCB_H
#include <stdbool.h>
#include <stdint.h>
#include "remesa_present_scale.h"
struct pipe_context;struct pipe_resource;struct xcb_connection_t;
struct remesa_drawable_buffers;
struct remesa_xcb_receipt {
 uint32_t snapshot_bytes;bool image_request_sent,image_acknowledged,gc_released;
 uint8_t x_error;
 bool source_y_inverted;
 uint32_t image_requests,image_bytes;
 struct remesa_present_scale_receipt scale;
 bool fullscreen_request_sent,fullscreen_request_acknowledged;
};
/* CPU-assisted publication into a caller-owned window, never the root window.
 * Caller owns an externally serialized XCB connection and its window. A
 * dedicated connection is the default. The private GLX LAB frontend may use
 * XGetXCBConnection on the SAME Display/XID namespace under XLockDisplay,
 * after XFlush, without taking event-queue ownership or disconnecting it.
 * The resource is retained until checked acknowledgement/GC cleanup completes.
 * The context and connection must outlive this synchronous call.
 * Does not own scanout, change desktop state, create a compositor, or retry.
 * An acknowledged PutImage proves server acceptance, not physical visibility.
 * Direct and scaled images are split into checked row tiles bounded by the
 * server's base request limit; height must fit signed X11 destination Y.
 * A failed/uncertain GC cleanup requires the caller to close its connection
 * (Display for shared Xlib) to release server resources. The GLX LAB frontend
 * poisons further publication until that close. No internal resend/disconnect.
 * Call only with separate authorization when a real display is involved. */
int remesa_xcb_present(struct pipe_context *,struct pipe_resource *,struct xcb_connection_t *,uint32_t window,struct remesa_xcb_receipt *);
/* Publish an already acquired immutable BGRA8 CPU readback into an exactly
 * matching caller-owned window using the same checked XCB path. Explicit
 * origin permits GL bottom-left rows to be inverted for X11. No map, wait,
 * GL context, renderer selection or GPU operation occurs here. This function
 * cannot attest where pixels originated; callers must retain separate shader,
 * completion/retirement and readback evidence. CPU publication is not GLX swap.
 * Source and receipt must be disjoint. Padding is never transmitted. */
enum remesa_xcb_origin { REMESA_XCB_TOP_LEFT, REMESA_XCB_BOTTOM_LEFT };
/* Exact-size resource publication with explicit origin and no attachment
 * exchange. Suitable for a state-tracker frontend that owns its own swap. */
int remesa_xcb_present_oriented(struct pipe_context *,struct pipe_resource *,
 enum remesa_xcb_origin,struct xcb_connection_t *,uint32_t,struct remesa_xcb_receipt *);
/* Publish the completed back (or single front) attachment, then exchange the
 * owned front/back references ONLY after checked publication and GC cleanup.
 * Explicit orientation; no scaling, rendering, resubmission, GLX admission or
 * framebuffer-preservation claim. Caller serializes resize/binds/publication.
 * Receipt must not overlap attachments or the drawable state. */
int remesa_xcb_present_drawable(struct pipe_context *,struct remesa_drawable_buffers *,
 enum remesa_xcb_origin,struct xcb_connection_t *,uint32_t,struct remesa_xcb_receipt *);
int remesa_xcb_present_bgra(const void *,size_t stride,size_t capacity,
 uint32_t width,uint32_t height,enum remesa_xcb_origin,
 struct xcb_connection_t *,uint32_t window,struct remesa_xcb_receipt *);
/* Opt-in CPU scaling into an exactly matching resized drawable. Sends bounded
 * row tiles using the base X request limit (height <= INT16_MAX for signed
 * tile coordinates), no retries or synchronization with
 * scanout. A resize racing geometry validation may clip server-side output. */
int remesa_xcb_present_scaled(struct pipe_context *,struct pipe_resource *,struct xcb_connection_t *,
 uint32_t window,uint16_t width,uint16_t height,enum remesa_present_scale_mode,struct remesa_xcb_receipt *);
/* EWMH fullscreen request for a caller-owned window. Acknowledgement means
 * server acceptance only; the window manager may ignore or reject the request.
 * The caller observes ConfigureNotify before choosing publication dimensions. */
int remesa_xcb_request_fullscreen(struct xcb_connection_t *,uint32_t window,bool enabled,struct remesa_xcb_receipt *);
#endif
