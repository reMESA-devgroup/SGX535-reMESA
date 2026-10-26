/* SPDX-License-Identifier: MIT */
#include "remesa_ta_index.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static struct remesa_ta_index_request base(void){return (struct remesa_ta_index_request){
 .index_bo_gpu_va=0x20000000,.index_bo_bytes=0x10000,.index_byte_offset=0x200,
 .pds_bo_gpu_va=0x20000000,.pds_bo_bytes=0x10000,.pds_byte_offset=0x4220,
 .pds_object_bytes=64,.first_index=0,.index_count=3,.pds_data_dwords=12,
 .descriptor_a_field10=8,.descriptor_b_field38=8};}
static unsigned checks;
#define CHECK(x) do {assert(x);checks++;}while(0)
static void reject(struct remesa_ta_index_request r,int rc){uint8_t o[20],old[20];memset(o,0xa5,20);memcpy(old,o,20);CHECK(remesa_ta_index_triangles_encode(&r,o)==rc);CHECK(!memcmp(o,old,20));}
int main(int argc,char **argv){
 const unsigned cases[][5]={{3,0,12,8,8},{6,3,12,4,4},{9,7,12,9,9},{3,0,12,12,12}};
 uint8_t o[22];struct remesa_ta_index_request r;
 for(unsigned i=0;i<4;i++){
  r=base();r.index_count=cases[i][0];r.first_index=cases[i][1];r.pds_data_dwords=cases[i][2];r.descriptor_a_field10=cases[i][3];r.descriptor_b_field38=cases[i][4];r.pds_object_bytes=64;
  memset(o,0xa5,22);CHECK(!remesa_ta_index_triangles_encode(&r,o+1));CHECK(o[0]==0xa5&&o[21]==0xa5);
  if(argc==2&&!strcmp(argv[1],"--dump")){printf("%u %u %u %u %u ",cases[i][0],cases[i][1],cases[i][2],cases[i][3],cases[i][4]);for(unsigned j=1;j<21;j++)printf("%02x",o[j]);puts("");}
 }
 for(unsigned i=0;i<2;i++){
  r=base();if(!i){r.index_count=6;r.index_byte_offset=0x4344;r.pds_byte_offset=0x4280;r.descriptor_a_field10=4;r.descriptor_b_field38=4;}
  CHECK(!remesa_ta_index_triangles_encode(&r,o));
  if(argc==2&&!strcmp(argv[1],"--dump")){printf("carrier %u ",i);for(unsigned j=0;j<20;j++)printf("%02x",o[j]);puts("");}
 }
 r=base();r.pds_data_dwords=13;reject(r,ENOTSUP);r=base();r.descriptor_b_field38=4;reject(r,ENOTSUP);r=base();r.descriptor_a_field10=17;r.descriptor_b_field38=17;reject(r,ENOTSUP);
 r=base();r.index_count=0;reject(r,ENOTSUP);r.index_count=4;reject(r,ENOTSUP);r.index_count=65538;reject(r,ENOTSUP);
 r=base();r.pds_data_dwords=253;reject(r,ENOTSUP);r=base();r.descriptor_a_field10=2045;reject(r,ENOTSUP);r=base();r.descriptor_b_field38=509;reject(r,ENOTSUP);
 r=base();r.index_byte_offset=UINT64_MAX;reject(r,ERANGE);r.first_index=1;reject(r,EOVERFLOW);
 r=base();r.index_bo_bytes=0x205;reject(r,ERANGE);r=base();r.index_bo_gpu_va=0xffffffff;reject(r,ERANGE);
 r=base();r.pds_byte_offset=0x4221;reject(r,EINVAL);r=base();r.index_byte_offset=0x201;reject(r,EINVAL);
 r=base();r.pds_object_bytes=63;reject(r,ERANGE);r=base();r.pds_bo_bytes=0x425f;reject(r,ERANGE);
 r=base();r.pds_byte_offset=0x200;reject(r,EINVAL);
 CHECK(remesa_ta_index_triangles_encode(NULL,o)==EINVAL);r=base();CHECK(remesa_ta_index_triangles_encode(&r,NULL)==EINVAL);
 CHECK(remesa_ta_index_triangles_encode((const void *)UINTPTR_MAX,o)==EINVAL);
 CHECK(remesa_ta_index_triangles_encode(&r,(void *)UINTPTR_MAX)==EINVAL);
 /* Exact endpoint and maximum restricted descriptor fields. */
 r=base();r.index_bo_bytes=0x206;r.pds_bo_bytes=0x4260;CHECK(!remesa_ta_index_triangles_encode(&r,o));
 r=base();r.descriptor_a_field10=16;r.descriptor_b_field38=16;CHECK(!remesa_ta_index_triangles_encode(&r,o));
 union{struct remesa_ta_index_request r;uint8_t b[sizeof(struct remesa_ta_index_request)];}alias;alias.r=base();uint8_t expected[20];CHECK(!remesa_ta_index_triangles_encode(&alias.r,expected));CHECK(!remesa_ta_index_triangles_encode(&alias.r,alias.b));CHECK(!memcmp(alias.b,expected,20));
 if(argc==1)printf("PASS %u focused checks\n",checks);
 return 0;
}
