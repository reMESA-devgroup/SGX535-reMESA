/* SPDX-License-Identifier: MIT */
/* Compare the current constructor with the retained physical FAST producer.
 * The runner builds retained_dpm_actions from the archived module source.
 * This checks CPU construction, not hardware reuse or arbitrary DPM contents. */
#include "../../../tools/remesa/lab_scene_actions.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int retained_dpm_actions(uint64_t, uint64_t, void *, size_t);

int main(void)
{
 static const uint64_t tables[] = {
  0x407ef000, 0x50000000, 0x60001000, 0xdc000000
 };
 static const uint64_t parameters[] = {
  0x30100000, 0x34000000, 0x38000000, 0x3e000000
 };
 unsigned cases = 0;
 for (unsigned t = 0; t < sizeof(tables)/sizeof(tables[0]); t++) {
  for (unsigned p = 0; p < sizeof(parameters)/sizeof(parameters[0]); p++) {
   struct sgx535_frozen_reg_action old[29], now[29];
   const struct remesa_lab_dpm_load_request r = {
    tables[t], 0x2000000, parameters[p], 0x2000000
   };
   size_t n = 0;
   memset(old, 0xa5, sizeof(old));
   memset(now, 0x5a, sizeof(now));
   assert(!retained_dpm_actions(tables[t], parameters[p], old, sizeof(old)));
   assert(!remesa_lab_dpm_actions_encode(&r, now, 29, &n));
   assert(n == 29 && !memcmp(old, now, sizeof(old)));
   /* Explicitly keep the four-load completion and subsequent INIT sequence. */
   assert(now[22].offset == 0x118 && now[22].value == 15 && now[22].mask == 15);
   assert(now[25].offset == 0x6a8 && now[25].value == 1);
   assert(now[26].offset == 0x12c && now[26].mask == 0x400000);
   cases++;
  }
 }
 printf("PASS %u relocated retained/current DPM plans; all29 actions byte-identical; GPU0\n", cases);
 return 0;
}
