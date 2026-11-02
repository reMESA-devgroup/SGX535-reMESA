/* SPDX-License-Identifier: MIT
 * Two distinct captured Gallium frontend/provider operations on one bounded
 * general native session. Same homogeneous profiles3/4, no GL version or FPS
 * claim. All dumps/readbacks are in this LAB collector, never production FAST.
 */
#define REMESA_GEARS_HOMOGENEOUS_NATIVE
#define REMESA_CAPTURED_SCENE_NATIVE
#define REMESA_CAPTURED_SESSION_NATIVE
#include "gears_scene_native_client.c"
