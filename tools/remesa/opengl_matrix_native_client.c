/* SPDX-License-Identifier: MIT
 * Real ordinary glClear/glDrawArrays -> Gallium -> native fill/matrix submit.
 * One-shot qualification tool, not the production loop: diagnostic captures
 * surround submission only here. No LAB frontend/catalogue enable or retry.
 * Borrows an already authorized fd; never enables module controls or boots.
 */
#define REMESA_OPENGL_NORMAL_NATIVE
#include "opengl_homogeneous_native_client.c"
