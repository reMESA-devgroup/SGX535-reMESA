/* SPDX-License-Identifier: MIT */
#ifndef REMESA_SCENE_INITIAL_H
#define REMESA_SCENE_INITIAL_H
#include "scene_layout.h"
/* Exact preserved initial DATA11, distinct from the scene Z-clear constructor.
 * Extracted from frozen_kernel_initial.inc role0+4350; absent sparse fields
 * retain the original zero initialization. This is DATA, not shader CODE.
 * Source SHA256: bea8ffb63a0635cbb44363ad8d500ead22fba6fd11d21eeaf3a217439c0f6d2c
 * Independently compared with physically qualified ROLE0+4350..437b.
 * No generic ISP meaning, execution or depth-clear approval is inferred. */
static const unsigned char remesa_scene_initial_data[44]={
 0xc5,0x54,0x00,0x00,0x00,0x01,0xf0,0x07,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x04,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00
};
static inline int remesa_scene_initial_matches(const void *data,size_t bytes)
{return data&&bytes==sizeof(remesa_scene_initial_data)&&!memcmp(data,remesa_scene_initial_data,bytes);}
#endif
