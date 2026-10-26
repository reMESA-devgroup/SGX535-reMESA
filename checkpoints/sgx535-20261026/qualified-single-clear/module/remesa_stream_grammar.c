/* SPDX-License-Identifier: MIT */
#include "remesa_stream_grammar.h"
#ifdef __KERNEL__
#include <linux/string.h>
#include <linux/errno.h>
#else
#include <string.h>
#include <errno.h>
#endif
static __u32 word(const unsigned char *p)
{return (__u32)p[0]|((__u32)p[1]<<8)|((__u32)p[2]<<16)|((__u32)p[3]<<24);}
static int in_range(__u64 offset,__u64 bytes,__u64 capacity)
{return offset<=capacity && bytes<=capacity-offset;}
static int bounded(const unsigned char *p)
{return (word(p)&0x7fffffffU)<=0x44800000U;} /* finite |x| <=1024 */
int remesa_stream_mask_sources(const struct remesa_stream_batch *b,unsigned n,
 unsigned char *p,__u64 bytes,__u64 va)
{
 unsigned i,j,k;__u64 mvp,light;
 if(!b || !p || !n || n>64 || bytes!=131072 || va>0xffffffffULL)return -EINVAL;
 /* Validate every span before touching any byte. */
 for(i=0;i<n;i++) {
  __u64 offset=b[i].source_offset,capacity=b[i].normal_variant?160:176;
  __u32 effective=b[i].normal_variant?37:41;
  if(b[i].normal_variant>1 || (offset&15) || b[i].source_capacity!=capacity ||
    b[i].source_effective_dwords!=effective || b[i].source_dma_dwords!=effective+3 ||
    !in_range(offset,capacity,bytes) || va+offset>0xffffffffULL ||
    word(p+offset)!=(__u32)(va+offset))return -EINVAL;
  for(j=0;j<i;j++)if(offset<b[j].source_offset+b[j].source_capacity &&
   b[j].source_offset<offset+capacity)return -EINVAL;
  for(k=effective*4;k<capacity;k++)if(p[offset+k])return -EINVAL;
  mvp=offset+4+4*16;light=offset+4+(b[i].normal_variant?8:9)*16;
  for(k=0;k<16;k++)if(!bounded(p+mvp+k*4))return -ERANGE;
  for(k=0;k<3;k++)if(!bounded(p+light+k*4))return -ERANGE;
 }
 for(i=0;i<n;i++) {
  mvp=b[i].source_offset+68;light=b[i].source_offset+4+(b[i].normal_variant?8:9)*16;
  memset(p+mvp,0,64);memset(p+light,0,12);
 }
 return 0;
}
int remesa_stream_validate_geometry(const struct remesa_stream_batch *b,unsigned n,
 const unsigned char *p,__u64 bytes)
{
 unsigned i,j,k;
 if(!b || !p || !n || n>64 || bytes!=131072)return -EINVAL;
 /* Geometry occupies the independent128KiB GEN backing (role2). */
 for(i=0;i<n;i++) {
  if(!b[i].vertex_count || b[i].vertex_count>65535 || !b[i].index_count ||
   b[i].index_count>65535 || b[i].index_count%3 || b[i].normal_variant>1 ||
   b[i].vertex_stride!=(b[i].normal_variant?32u:16u) ||
   b[i].vertex_bytes!=(__u64)b[i].vertex_count*b[i].vertex_stride ||
   b[i].index_bytes!=(__u64)b[i].index_count*2 ||
   !in_range(b[i].vertex_offset,b[i].vertex_bytes,bytes) ||
   !in_range(b[i].index_offset,b[i].index_bytes,bytes))return -EINVAL;
  for(j=0;j<b[i].index_count;j++) {
   __u64 off=b[i].index_offset+j*2;
   unsigned index=p[off]|((unsigned)p[off+1]<<8);
   if(index>=b[i].vertex_count)return -ERANGE;
  }
  for(k=0;k<b[i].vertex_bytes;k+=4)if(!bounded(p+b[i].vertex_offset+k))return -ERANGE;
 }
 return 0;
}
