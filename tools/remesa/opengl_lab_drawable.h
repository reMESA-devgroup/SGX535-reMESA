/* SPDX-License-Identifier: MIT */
#ifndef REMESA_OPENGL_LAB_DRAWABLE_H
#define REMESA_OPENGL_LAB_DRAWABLE_H
#include <stdbool.h>
#include <stdint.h>
#include "frontend/api.h"
struct pipe_context;
struct remesa_lab_drawable;
/* LAB only: the callback publishes this exact resource and returns zero only
 * after acknowledgement. It must not rotate attachments, retain borrowed
 * arguments, or reenter the drawable. Use remesa_xcb_present_oriented() with
 * the demonstrated row origin (classic remesa_xcb_present only for proven
 * top-left input), NOT the separately swap-owning
 * remesa_xcb_present_drawable(). Publication is not
 * rendering; waiting for its writer fence remains the publisher's job. */
typedef int (*remesa_lab_publish)(void *, struct pipe_context *,
                                 struct pipe_resource *);
/* Caller supplies a nonzero ID unique among its live frontend drawables. */
int remesa_lab_drawable_create(struct pipe_frontend_screen *, uint32_t id,
                              uint32_t width, uint32_t height, bool double_buffer,
                              remesa_lab_publish, void *,
                              struct remesa_lab_drawable **);
struct pipe_frontend_drawable *remesa_lab_drawable_base(struct remesa_lab_drawable *);
int remesa_lab_drawable_resize(struct remesa_lab_drawable *, uint32_t, uint32_t);
int remesa_lab_drawable_error(struct remesa_lab_drawable *);
/* Call after unbinding/destroying all state-tracker contexts using the base.
 * Screen/frontend lifetime is borrowed; there is no GLX registration. */
void remesa_lab_drawable_destroy(struct remesa_lab_drawable *);
#endif
