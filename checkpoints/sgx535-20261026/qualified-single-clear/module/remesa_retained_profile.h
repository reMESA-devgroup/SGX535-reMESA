/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_RETAINED_PROFILE_H
#define REMESA_RETAINED_PROFILE_H
#include "remesa_retained_session.h"
int remesa_retained_triangle_images(void *,const struct remesa_memory_view *,unsigned,const void *,unsigned);
int remesa_retained_green_fragment_images(void *,const struct remesa_memory_view *,unsigned,const void *,unsigned);
#endif
