/* SPDX-License-Identifier: MIT */
#ifndef REMESA_XCB_STAGING_H
#define REMESA_XCB_STAGING_H
#include "remesa_xcb.h"
#include <stddef.h>
/* Private, caller-owned CPU publication storage. Initialize to zero; serialize
 * all use with the same Display/context lock and release after the last call.
 * No GPU backing or user mmap is retained. Checked publication is synchronous. */
struct remesa_xcb_staging { unsigned char *pixels; size_t capacity; };
int remesa_xcb_present_oriented_staged(struct pipe_context *,struct pipe_resource *,
 enum remesa_xcb_origin,struct xcb_connection_t *,uint32_t,
 struct remesa_xcb_staging *,struct remesa_xcb_receipt *);
void remesa_xcb_staging_release(struct remesa_xcb_staging *);
#endif
