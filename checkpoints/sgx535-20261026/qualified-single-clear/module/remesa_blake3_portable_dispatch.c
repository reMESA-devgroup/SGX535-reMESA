#include "remesa_blake3_namespace.h"
#include "blake3_impl.h"
void blake3_compress_in_place(uint32_t cv[8],const uint8_t b[64],uint8_t len,uint64_t count,uint8_t flags){blake3_compress_in_place_portable(cv,b,len,count,flags);}
void blake3_compress_xof(const uint32_t cv[8],const uint8_t b[64],uint8_t len,uint64_t count,uint8_t flags,uint8_t out[64]){blake3_compress_xof_portable(cv,b,len,count,flags,out);}
void blake3_hash_many(const uint8_t*const*inputs,size_t n,size_t blocks,const uint32_t key[8],uint64_t count,bool increment,uint8_t flags,uint8_t start,uint8_t end,uint8_t*out){blake3_hash_many_portable(inputs,n,blocks,key,count,increment,flags,start,end,out);}
size_t blake3_simd_degree(void){return 1;}
