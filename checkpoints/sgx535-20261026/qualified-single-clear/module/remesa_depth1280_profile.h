/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_DEPTH1280_PROFILE_H
#define REMESA_DEPTH1280_PROFILE_H
#include "remesa_gem_memory.h"
int remesa_depth1280_sha256(const void *,size_t,u8 [32]);
int remesa_retained_depth1280_images(void *,const struct remesa_memory_view *,unsigned,const void *,unsigned);
#endif
