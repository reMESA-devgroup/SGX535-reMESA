/* SPDX-License-Identifier: MIT. Bounds only, no instruction/GPU admission. */
#include "remesa/parameter_window.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
 unsigned checks=0;
#define CHECK(x) do{checks++;assert(x);}while(0)
 for(unsigned count=1;count<=127;count++){
  unsigned transfer=(count+4u)&~3u,use=transfer*4u,pds=(use+16u+31u)&~31u;
  struct remesa_parameter_window w={.owner=77,.generation=12,.gpu_va=0x20001000,.id=42,
   .allocation_bytes=4096,.coefficient_dwords=count,.effective_source_dwords=count+1,
   .transfer_dwords=transfer,.use_offset=use,.use_bytes=8,.pds_offset=pds,.pds_bytes=60,
   .ta_offset=pds+64,.ta_bytes=8};
  CHECK(remesa_parameter_window_valid(&w));
  for(unsigned off=0;off<=w.ta_offset+8;off++){
   CHECK(remesa_parameter_window_patch_valid(&w,42,off,4)==(off>=4&&off<=count*4&&!(off&3)));
  }
  CHECK(remesa_parameter_window_patch_valid(&w,42,4,count*4));
  CHECK(!remesa_parameter_window_patch_valid(&w,43,4,4));
  CHECK(!remesa_parameter_window_patch_valid(&w,42,4,0));
  CHECK(!remesa_parameter_window_patch_valid(&w,42,4,3));
  CHECK(!remesa_parameter_window_patch_valid(&w,42,UINT64_MAX-3,4));
  struct remesa_parameter_window saved=w;
  for(unsigned fault=0;fault<14;fault++){
   switch(fault){
    case 0:w.owner=0;break;case 1:w.generation=0;break;case 2:w.id=0;break;
    case 3:w.gpu_va=0xfffffffcu;break;case 4:w.gpu_va++;break;
    case 5:w.effective_source_dwords++;break;case 6:w.transfer_dwords=0;break;
    case 7:w.source_offset=0xfffffffcu;break;case 8:w.use_offset=0;break;
    case 9:w.use_bytes=16;break;case 10:w.pds_offset=w.use_offset;break;
    case 11:w.ta_offset=w.pds_offset;break;case 12:w.allocation_bytes=w.ta_offset;break;
    case 13:w.coefficient_dwords=128;break;
   }
   CHECK(!remesa_parameter_window_valid(&w));w=saved;
  }
  struct remesa_parameter_patch_policy p={.window=w,.maximum_abs_bits=0x44800000};
  const unsigned words[]={0,0x80000000,0x3f800000,0xc4800000,0x44800000,0x44800001,0x7f800000,0xff800000,0x7fc00000,0xffffffff};
  for(unsigned i=0;i<sizeof words/sizeof words[0];i++){
   unsigned char data[4];for(unsigned k=0;k<4;k++)data[k]=(unsigned char)(words[i]>>(8*k));
   CHECK(remesa_parameter_patch_bounded_valid(&p,42,4,data,4)==((words[i]&0x7fffffffu)<=0x44800000));
  }
  CHECK(!remesa_parameter_patch_bounded_valid(&p,42,4,NULL,4));
 }
 CHECK(!remesa_parameter_window_valid(NULL));
 printf("PASS %u parameter metadata/window/bounded-bit-pattern checks; GPU0\n",checks);return 0;
}
