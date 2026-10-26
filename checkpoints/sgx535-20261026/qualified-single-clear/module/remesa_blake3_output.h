#ifndef REMESA_BLAKE3_OUTPUT_H
#define REMESA_BLAKE3_OUTPUT_H
#include "remesa_blake3_namespace.h"
#include "blake3.h"
/* Exact fixed COLOR/Z size. Chunk calls <=1024 avoid recursive subtree stack
 * growth in the portable kernel backend. Caller owns serialized state. */
static inline void remesa_blake3_output(blake3_hasher *h,const void *data,uint8_t out[32])
{
 unsigned offset;
 blake3_hasher_init(h);
 for(offset=0;offset<4096000;offset+=1024)
  blake3_hasher_update(h,(const uint8_t*)data+offset,1024);
 blake3_hasher_finalize(h,out,32);
}
#endif
