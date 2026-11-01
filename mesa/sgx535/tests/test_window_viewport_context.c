/* SPDX-License-Identifier: MIT. Gallium state/serializer integration; GPU0. */
int original_driver_fixture(void);
#define main original_driver_fixture
#include "test_driver.c"
#undef main
#include "remesa_ta_scene.h"
#include "window_viewport_fixture.h"
int main(void)
{
 check_window_viewports();
 puts("PASS window viewport Gallium/encoder:4 target sizes, full/subrect,16/24-bit depth, reversed range, unsupported orientation/fractional/nonfinite/bounds refusal, preserved outputs, zero providerIO and unchanged legacy admission; GPU0/API0");
 return 0;
}
