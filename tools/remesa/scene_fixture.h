/* SPDX-License-Identifier: MIT */
#ifndef REMESA_SCENE_FIXTURE_H
#define REMESA_SCENE_FIXTURE_H
/* Offline producer/consumer fixture only. Addresses describe modeled backing
 * placement, never userspace authorization or an ioctl ABI. */
#include <stdint.h>
struct remesa_scene_fixture {
 uint32_t version,count;
 struct {uint64_t address,bytes;uint32_t id,access;} bindings[32];
};
_Static_assert(sizeof(struct remesa_scene_fixture)==776,"offline fixture layout");
#endif
