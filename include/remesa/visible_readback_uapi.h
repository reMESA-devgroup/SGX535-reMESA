/* SPDX-License-Identifier: MIT */
#ifndef REMESA_VISIBLE_READBACK_UAPI_H
#define REMESA_VISIBLE_READBACK_UAPI_H
#include <linux/types.h>
/* LAB read-only envelope. No ioctl number/table entry is registered here.
 * A native adapter must qualify its isolated route before using this ABI.
 * Reads only already-visible held jobs; does not issue/poll/publish/ack/retire.
 * All input data bytes and reserved must be zero. On success length bytes of
 * data contain the requested chunk; unused data stays zero. No CPU VA/PFN or
 * caller-supplied GPU address is accepted. Context is bound to the DRM file.
 */
#define REMESA_VISIBLE_READBACK_VERSION 1u
#define REMESA_VISIBLE_READBACK_CHUNK 4096u
struct remesa_visible_readback_request {
 __u32 version,id;
 __u64 context,operation,offset;
 __u32 slot,generation,length,reserved;
 __u8 data[REMESA_VISIBLE_READBACK_CHUNK];
};
#endif
