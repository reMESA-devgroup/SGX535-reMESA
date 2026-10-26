/* SPDX-License-Identifier: MIT */
/* Independent original29385 CPU output on stdin (three 60-byte objects).
 * DATA2 is a deferred relocation placeholder in that oracle and is explicitly
 * excluded from the byte comparison. No GPU, DS or USE semantics are claimed. */
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
 unsigned char oracle[3][60];
 assert(fread(oracle,1,sizeof(oracle),stdin)==sizeof(oracle)&&fgetc(stdin)==EOF);
 const unsigned counts[]={2,5,9};
 unsigned char b[64],m[64],isp[64],im[64];size_t n=0,in=0;
 for(unsigned k=0;k<3;k++) {
  struct remesa_pds_state_image_request r={.source_allocation_address=0x20000000,
   .source_allocation_bytes=4096,.state_dwords=counts[k],.use_lower19=0x804,
   .task_profile=REMESA_PDS_STATE_TASK_PARAMETERS};
  memset(b,0xa5,sizeof(b));memset(m,0xa5,sizeof(m));
  assert(!remesa_pds_structure_encode_state_image(&r,b,m,sizeof(b),&n)&&n==60);
  for(unsigned i=0;i<60;i++) {
   unsigned known=i<16||(i>=40&&i<44)||i>=48?255:0;
   assert(m[i]==known);
   if(i<8||i>=12)assert((b[i]&m[i])==(oracle[k][i]&m[i]));
   if(!known)assert(b[i]==0);
  }
  assert(b[8]==4&&b[9]==8&&b[10]==0x20&&b[11]==0);
  for(unsigned i=60;i<64;i++)assert(b[i]==0xa5&&m[i]==0xa5);
  r.task_profile=REMESA_PDS_STATE_TASK_ISP;
  assert(!remesa_pds_structure_encode_state_image(&r,isp,im,sizeof(isp),&in)&&in==n);
  for(unsigned i=0;i<60;i++) {
   if((i>=12&&i<16)||(i>=40&&i<44))assert(isp[i]==0&&im[i]==0);
   else assert(isp[i]==b[i]&&im[i]==m[i]);
  }
  r.task_profile=2;reject(&r,ENOTSUP);
  r.task_profile=UINT32_MAX;reject(&r,ENOTSUP);
  r.task_profile=REMESA_PDS_STATE_TASK_PARAMETERS;r.state_dwords=0;reject(&r,EINVAL);
  r.state_dwords=17;reject(&r,ENOTSUP);r.state_dwords=counts[k];
  r.source_allocation_bytes=counts[k]*4-1;reject(&r,ENOSPC);
  r.source_allocation_bytes=4096;r.use_lower19=0x80000;reject(&r,ERANGE);
 }
 puts("PASS:3 original PARAM secondary PDS objects; deferred DATA2 excluded; default ISP unchanged; profile/extent/relocation rejection transactional; CPU ONLY");
 return 0;
}
