/* SPDX-License-Identifier: MIT
 * Whole original compiler/upload CPU oracles for authored live PARAM chains.
 * Deferred DATA2 is explicitly excluded, not treated as a concrete USE address. */
#include "remesa_pds_structure.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned get32(const unsigned char*p){return p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24);}
static void reject(const struct remesa_pds_state_image_request*r,int e){
 unsigned char b[64],m[64],old[64];memset(old,0xa5,64);memcpy(b,old,64);memcpy(m,old,64);size_t n=99;
 assert(remesa_pds_structure_encode_state_image(r,b,m,64,&n)==e&&n==99&&!memcmp(b,old,64)&&!memcmp(m,old,64));
}
int main(int argc,char**argv){
 assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f);unsigned char head[8],oracle[64];unsigned accepted=0,rejected=0;
 while(fread(head,1,8,f)==8){
  unsigned count=get32(head),len=get32(head+4);assert(len==60||len==64);assert(fread(oracle,1,len,f)==len);
  struct remesa_pds_state_image_request r={.source_allocation_address=0x20000000,.source_allocation_bytes=4096,.state_dwords=count,.use_lower19=0x804,.task_profile=REMESA_PDS_STATE_TASK_PARAMETERS};
  if(len==64){reject(&r,ENOTSUP);++rejected;continue;}
  unsigned char b[64],m[64];memset(b,0xa5,64);memset(m,0xa5,64);size_t n=0;
  assert(!remesa_pds_structure_encode_state_image(&r,b,m,64,&n)&&n==60);
  for(unsigned i=0;i<60;i++){
   unsigned known=i<16||(i>=40&&i<44)||i>=48?255:0;assert(m[i]==known);
   if(i<8||i>=12)assert((b[i]&m[i])==(oracle[i]&m[i]));
   if(!known)assert(!b[i]);
  }
  for(unsigned i=60;i<64;i++)assert(b[i]==0xa5&&m[i]==0xa5);
  r.source_allocation_bytes=count*4-1;reject(&r,ENOSPC);r.source_allocation_bytes=4096;
  r.task_profile=REMESA_PDS_STATE_TASK_ISP;if(count>16)reject(&r,ENOTSUP);
  ++accepted;
 }
 assert(feof(f)&&!ferror(f)&&accepted);fclose(f);
 struct remesa_pds_state_image_request r={.source_allocation_address=0x20000000,.source_allocation_bytes=4096,.state_dwords=17,.use_lower19=0x804,.task_profile=REMESA_PDS_STATE_TASK_PARAMETERS};
 reject(&r,ENOTSUP);r.state_dwords=37;reject(&r,ENOTSUP);r.state_dwords=41;reject(&r,ENOTSUP);
 r.state_dwords=129;reject(&r,ENOTSUP);r.state_dwords=0;reject(&r,EINVAL);
 printf("PASS %u whole-original large PARAM single-descriptor images; %u original two-descriptor cases rejected; mask/extent/ISP/rollback; CPU ONLY\n",accepted,rejected);return 0;
}
