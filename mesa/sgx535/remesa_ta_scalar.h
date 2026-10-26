/* SPDX-License-Identifier: MIT */
#ifndef REMESA_TA_SCALAR_H
#define REMESA_TA_SCALAR_H
#include <stddef.h>
#include <stdint.h>
/* Restricted original2daad CPU layout profiles. These names describe authored
 * fragment inputs and CPU packed records, not physical VS export semantics. */
enum remesa_ta_scalar_profile {
 REMESA_TA_SCALAR_POSITION4=0,
 REMESA_TA_SCALAR_POSITION4_PRIMARY_COLOR4=1
};
#define REMESA_TA_SCALAR_BYTES 16u
/* Original default group200 + nontextured group400 plus default group800, headerE00. Position4 is
 * the constant-FS profile; primary-color4 requires a matching FS input. Other
 * inputs, programmable-VS layouts, fog/point outputs and texture coordinates
 * remain unsupported. Original setter also writes group4000=0 for these two
 * profiles; that group is not included in this delta. Group800 comes from
 * the original default initializer, without an architectural interpretation. No GPU fetch/export or
 * complete initial-state admission is provided. Output/written disjoint;
 * failures leave both unchanged. Source extent/job ownership remain separate. */
int remesa_ta_scalar_groups_encode(enum remesa_ta_scalar_profile,
 uint8_t *bytes,size_t capacity,size_t *written);
#endif
