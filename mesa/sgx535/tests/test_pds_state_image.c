/* SPDX-License-Identifier: MIT */
/* stdin: three independent preserved carrier PDS objects,60bytes each.
 * Unknown cells are compared only under the encoder's explicit known mask.
 * This test opens no device and does not qualify a physical depth job. */
#include "remesa_pds_structure.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static void reject(const struct remesa_pds_state_image_request *r,int expected)
{
 unsigned char b[64],m[64],old[64];memset(old,0xa5,sizeof(old));
 memcpy(b,old,sizeof(b));memcpy(m,old,sizeof(m));size_t n=97;
 assert(remesa_pds_structure_encode_state_image(r,b,m,sizeof(b),&n)==expected);
 assert(n==97&&!memcmp(b,old,sizeof(b))&&!memcmp(m,old,sizeof(m)));
}
int main(void)
{
 unsigned char oracle[3][REMESA_PDS_STRUCTURE_STATE_BYTES];
 assert(fread(oracle,1,sizeof(oracle),stdin)==sizeof(oracle)&&fgetc(stdin)==EOF);
 const unsigned offsets[]={0x350,0x3bc,0x43c},counts[]={11,14,2},fixups[]={0x804,0xa04,0xc04};
 unsigned char b[64],m[64];size_t n=0;
 for(unsigned k=0;k<3;k++) {
  const struct remesa_pds_state_image_request r={
   .source_allocation_address=0x20004000,.source_allocation_bytes=0x1000,
   .source_offset=offsets[k],.state_dwords=counts[k],.use_lower19=fixups[k]};
  memset(b,0xa5,sizeof(b));memset(m,0xa5,sizeof(m));
  assert(!remesa_pds_structure_encode_state_image(&r,b,m,sizeof(b),&n));
  assert(n==60);
  for(unsigned i=0;i<60;i++) {
   unsigned known=i<12||i>=48?255:0;
   assert(m[i]==known);
   assert((b[i]&m[i])==(oracle[k][i]&m[i]));
   if(!known)assert(b[i]==0);
  }
  for(unsigned i=60;i<64;i++)assert(b[i]==0xa5&&m[i]==0xa5);
 }
 struct remesa_pds_state_image_request r={.source_allocation_address=0x20000000,
  .source_allocation_bytes=64,.state_dwords=16,.use_lower19=0x804};
 assert(!remesa_pds_structure_encode_state_image(&r,b,m,sizeof(b),&n));
 /* Last DWORD of the32bit address space is representable without wrap. */
 r.source_allocation_address=0xffffffc0;
 assert(!remesa_pds_structure_encode_state_image(&r,b,m,sizeof(b),&n));
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
 r.state_dwords=0;reject(&r,EINVAL);
 r.state_dwords=17;reject(&r,ENOTSUP);
 r.state_dwords=UINT32_MAX;reject(&r,ENOTSUP);r.state_dwords=2;
 memset(b,0xa5,sizeof(b));memset(m,0xa5,sizeof(m));n=97;
 assert(remesa_pds_structure_encode_state_image(&r,b,m,59,&n)==ENOSPC);
 assert(n==97);for(unsigned i=0;i<64;i++)assert(b[i]==0xa5&&m[i]==0xa5);
 assert(remesa_pds_structure_encode_state_image(&r,b,b,64,&n)==EINVAL);
 assert(remesa_pds_structure_encode_state_image(&r,b,m,64,NULL)==EINVAL);
 assert(remesa_pds_structure_encode_state_image(NULL,b,m,64,&n)==EINVAL);
 assert(remesa_pds_structure_encode_state_image(&r,(unsigned char *)&r,m,64,&n)==EINVAL);
 puts("PASS:3 independent state-upload carrier objects under explicit known masks; extent/overflow/alias and transactional rejection; CPU ONLY");
 return 0;
}
