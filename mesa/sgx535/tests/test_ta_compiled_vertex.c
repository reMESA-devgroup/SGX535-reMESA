/* SPDX-License-Identifier: MIT */
#include "remesa_ta_index.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;assert(x);}while(0)
int main(void)
{
 struct remesa_ta_index_request r={.index_bo_gpu_va=0x20000000,.index_bo_bytes=0x10000,.index_byte_offset=0x200,.pds_bo_gpu_va=0x20000000,.pds_bo_bytes=0x10000,.pds_byte_offset=0x4220,.pds_object_bytes=64,.pds_data_dwords=12};
 char hex[41];unsigned count=0;
 while(scanf("%u %u %u %u %40s",&r.index_count,&r.first_index,&r.descriptor_a_field10,&r.descriptor_b_field38,hex)==5) {
  CHECK(strlen(hex)==40);unsigned char expected[20],out[22];memset(out,0xa5,22);
  for(unsigned i=0;i<20;i++){unsigned v;CHECK(sscanf(hex+2*i,"%2x",&v)==1);expected[i]=v;}
  CHECK(!remesa_ta_index_compiled_position_triangles_encode(&r,out+1)&&!memcmp(out+1,expected,20)&&out[0]==0xa5&&out[21]==0xa5);
  CHECK(remesa_ta_index_triangles_encode(&r,out+1)==ENOTSUP);count++;
 }
 CHECK(count==2);
 for(unsigned mode=0;mode<8;mode++) {
  r.index_count=3;r.first_index=0;r.descriptor_a_field10=4;r.descriptor_b_field38=16;r.pds_byte_offset=0x4220;r.pds_data_dwords=12;r.pds_object_bytes=64;
  int error=ENOTSUP;
  if(mode==0)r.descriptor_b_field38=4;
  if(mode==1)r.descriptor_a_field10=12;
  if(mode==2)r.pds_data_dwords=13;
  if(mode==3)r.index_count=4;
  if(mode==4){r.pds_byte_offset=0x4221;error=EINVAL;}
  if(mode==5){r.pds_object_bytes=63;error=ERANGE;}
  if(mode==6){r.pds_byte_offset=0x200;error=EINVAL;}
  if(mode==7)r.descriptor_b_field38=32;
  unsigned char out[20],saved[20];memset(out,0x83,20);memcpy(saved,out,20);
  CHECK(remesa_ta_index_compiled_position_triangles_encode(&r,out)==error&&!memcmp(out,saved,20));
 }
 CHECK(remesa_ta_index_compiled_position_triangles_encode(NULL,(void *)&r)==EINVAL);
 printf("PASS compiled POSITION4 TA original whole-writer/relocation2 oracles; source stride separate from output byte count, legacy profile refusal and transactional negatives; NO GPU (%u checks)\n",checks);return 0;
}
