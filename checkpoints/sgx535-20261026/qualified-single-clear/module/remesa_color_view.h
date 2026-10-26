/* SPDX-License-Identifier: MIT */
#ifndef REMESA_COLOR_VIEW_H
#define REMESA_COLOR_VIEW_H
/* Private fixed COLOR read view, NOT a GEM/PRIME handle. The byte offset is
 * below the 32-bit DRM GEM offset namespace (256 MiB) and below4GiB so Linux
 * 5.10/i386 unmap_mapping_range's pgoff cast cannot truncate it. */
#define REMESA_COLOR_VIEW_OFFSET 0x08000000ULL
#define REMESA_COLOR_VIEW_PGOFF 0x8000UL
#define REMESA_COLOR_VIEW_BYTES 4096000UL
#define REMESA_COLOR_VIEW_PAGES 1000UL
#define REMESA_COLOR_VIEW_GPU_VA 0x40001000ULL
#endif
