/* SPDX-License-Identifier: MIT */
#ifndef REMESA_FAST_SHM_H
#define REMESA_FAST_SHM_H
#include <xcb/xcb.h>
#include <stdbool.h>
#include <stddef.h>
int remesa_fast_shm_present(xcb_connection_t *,xcb_drawable_t,xcb_gcontext_t,
 unsigned,unsigned,const void *,size_t,unsigned char *,bool *,bool *);
int remesa_fast_shm_close(xcb_connection_t *);
#endif
