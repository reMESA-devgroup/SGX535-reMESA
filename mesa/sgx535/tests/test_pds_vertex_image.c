/* SPDX-License-Identifier: MIT */
/* stdin: two independent preserved primary vertex carrier PDS objects,
 *64bytes each. Unknown metadata remains masked, even when observed zero.
 * No device, shader transform, fetch instruction or GPU admission is tested. */
#include "remesa_pds_structure.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static void reject(const struct remesa_pds_vertex_image_request *r,int expected)
{
 unsigned char b[64],m[64],old[64];memset(old,0xa5,sizeof(old));
 memcpy(b,old,sizeof(b));memcpy(m,old,sizeof(m));size_t n=97;
 assert(remesa_pds_structure_encode_vertex_image(r,b,m,sizeof(b),&n)==expected);
 assert(n==97&&!memcmp(b,old,sizeof(b))&&!memcmp(m,old,sizeof(m)));
}
int main(void)
{
 unsigned char oracle[2][REMESA_PDS_STRUCTURE_VERTEX_BYTES];
 assert(fread(oracle,1,sizeof(oracle),stdin)==sizeof(oracle)&&fgetc(stdin)==EOF);
 const unsigned offsets[]={0,0x2c4},counts[]={8,4},fixups[]={0x404,0x604};
 const uint64_t addresses[]={0x40000000,0x20004000};
 unsigned char b[68],m[68];size_t n=0;
 for(unsigned k=0;k<2;k++) {
  const struct remesa_pds_vertex_image_request r={
   .source_allocation_address=addresses[k],.source_allocation_bytes=0x1000,
   .source_offset=offsets[k],.vertex_stride_dwords=counts[k],.use_lower19=fixups[k]};
  memset(b,0xa5,sizeof(b));memset(m,0xa5,sizeof(m));
  assert(!remesa_pds_structure_encode_vertex_image(&r,b,m,sizeof(b),&n));
  assert(n==64);
  for(unsigned i=0;i<64;i++) {
   unsigned cell=i/4;unsigned known=cell==0||cell==1||cell==4||cell==8||i>=48?255:0;
   assert(m[i]==known);
   assert((b[i]&m[i])==(oracle[k][i]&m[i]));
   if(!known)assert(b[i]==0);
  }
  for(unsigned i=64;i<68;i++)assert(b[i]==0xa5&&m[i]==0xa5);
 }
 struct remesa_pds_vertex_image_request r={.source_allocation_address=0x20000000,
  .source_allocation_bytes=64,.vertex_stride_dwords=16,.use_lower19=0x804};
 assert(!remesa_pds_structure_encode_vertex_image(&r,b,m,sizeof(b),&n));
 /* Last DWORD of the32bit address space is representable without wrap. */
 r.source_allocation_address=0xffffffc0;
 assert(!remesa_pds_structure_encode_vertex_image(&r,b,m,sizeof(b),&n));
 r.source_allocation_bytes=65;reject(&r,EOVERFLOW);
 r.source_allocation_address=0x20000000;r.source_allocation_bytes=64;
 r.source_offset=4;reject(&r,ENOSPC);
 r.source_offset=UINT64_MAX-3;reject(&r,ENOSPC);
 r.source_offset=1;reject(&r,EINVAL);r.source_offset=0;
 r.source_allocation_address=0;reject(&r,EINVAL);
 r.source_allocation_address=UINT64_C(0x100000000);reject(&r,EINVAL);
 r.source_allocation_address=0x20000001;reject(&r,EINVAL);
 r.source_allocation_address=0x20000000;
 r.use_lower19=0x80000;reject(&r,ERANGE);r.use_lower19=0x804;
 r.vertex_stride_dwords=0;reject(&r,EINVAL);
 r.vertex_stride_dwords=17;reject(&r,ENOTSUP);
 r.vertex_stride_dwords=UINT32_MAX;reject(&r,ENOTSUP);r.vertex_stride_dwords=2;
 memset(b,0xa5,sizeof(b));memset(m,0xa5,sizeof(m));n=97;
 assert(remesa_pds_structure_encode_vertex_image(&r,b,m,63,&n)==ENOSPC);
 assert(n==97);for(unsigned i=0;i<64;i++)assert(b[i]==0xa5&&m[i]==0xa5);
 assert(remesa_pds_structure_encode_vertex_image(&r,b,b,64,&n)==EINVAL);
 assert(remesa_pds_structure_encode_vertex_image(&r,b,m,64,NULL)==EINVAL);
 assert(remesa_pds_structure_encode_vertex_image(NULL,b,m,64,&n)==EINVAL);
 assert(remesa_pds_structure_encode_vertex_image(&r,(unsigned char *)&r,m,64,&n)==EINVAL);
 puts("PASS:2 independent primary vertex carrier objects under explicit masks; record extent/stride/lower19/overflow/alias and transactional rejection; CPU ONLY");
 return 0;
}
