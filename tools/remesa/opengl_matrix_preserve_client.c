/* SPDX-License-Identifier: MIT. Normal Mesa API, three bounded draws.
 * B with clear establishes physical depth reference; clear+A then B without
 * clear tests real preservation. Collector alone records diagnostics.
 */
#define REMESA_OPENGL_NORMAL_NATIVE
#define REMESA_OPENGL_CONTINUOUS_NATIVE
#define REMESA_OPENGL_PRESERVE_NATIVE
#include "opengl_homogeneous_native_client.c"
