/* SPDX-License-Identifier: MIT */
#include "remesa_pds_structure.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;assert(x);}while(0)
static unsigned le(const unsigned char *p)
{return (unsigned)p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
int main(void)
{
 unsigned char oracle[128],fields[8];CHECK(fread(oracle,1,128,stdin)==128);CHECK(fread(fields,1,8,stdin)==8&&fgetc(stdin)==EOF);
 for(unsigned k=0;k<2;k++) {
  const unsigned offsets[]={0,0x2c4},counts[]={8,4},fixups[]={0x404,0x604};
  struct remesa_pds_vertex_image_request r={.source_allocation_address=k?0x20004000:0x40000000,.source_allocation_bytes=0x1000,.source_offset=offsets[k],.vertex_stride_dwords=counts[k],.use_lower19=fixups[k]};
  unsigned char bytes[68],mask[68],copy[64],copy_mask[64];size_t n=991;
  memset(bytes,0xa7,68);memset(mask,0xa7,68);
  CHECK(!remesa_pds_structure_encode_vertex_image(&r,copy,copy_mask,64,&n));
  CHECK(!remesa_pds_structure_encode_compiled_vertex_image(&r,bytes,mask,68,&n)&&n==64);
  CHECK(le(bytes+20)==le(fields)&&le(bytes+36)==le(fields+4));
  for(unsigned i=0;i<64;i++) {
   unsigned cell=i/4;unsigned known=cell==0||cell==1||cell==4||cell==5||cell==8||cell==9||i>=48?255:0;
   CHECK(mask[i]==known);
   if(cell!=5&&cell!=9)CHECK(bytes[i]==copy[i]&&mask[i]==copy_mask[i]);
   CHECK((copy[i]&copy_mask[i])==(oracle[64*k+i]&copy_mask[i]));
  }
  for(unsigned i=64;i<68;i++)CHECK(bytes[i]==0xa7&&mask[i]==0xa7);
  unsigned char before[68],before_mask[68];memcpy(before,bytes,68);memcpy(before_mask,mask,68);n=991;
  CHECK(remesa_pds_structure_encode_compiled_vertex_image(&r,bytes,mask,63,&n)==ENOSPC&&n==991&&!memcmp(bytes,before,68)&&!memcmp(mask,before_mask,68));
  CHECK(remesa_pds_structure_encode_compiled_vertex_image(&r,bytes,bytes,68,&n)==EINVAL);
  CHECK(remesa_pds_structure_encode_compiled_vertex_image(&r,(void *)&r,mask,68,&n)==EINVAL);
  r.use_lower19=0x80000;CHECK(remesa_pds_structure_encode_compiled_vertex_image(&r,bytes,mask,68,&n)==ERANGE&&n==991&&!memcmp(bytes,before,68)&&!memcmp(mask,before_mask,68));
 }
 printf("PASS compiled-VS PDS control fields from original x86 stores; legacy2 masked carrier oracles unchanged; bytes/masks/bounds/alias/rollback; NO GPU (%u checks)\n",checks);
 return 0;
}
