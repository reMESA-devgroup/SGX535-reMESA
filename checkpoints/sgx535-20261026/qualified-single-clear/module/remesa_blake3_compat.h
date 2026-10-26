#ifndef REMESA_BLAKE3_COMPAT_H
#define REMESA_BLAKE3_COMPAT_H
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/bug.h>
#include <linux/bitops.h>
/* Library-internal bounded-size invariants warn without panicking the Mini.
 * Driver ownership/PTE/address/size/fence/retirement gates are unchanged. */
#define assert(condition) WARN_ON_ONCE(!(condition))
#endif
