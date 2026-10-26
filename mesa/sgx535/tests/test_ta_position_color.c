/* SPDX-License-Identifier: MIT; independent original CPU command oracle stdin. */
#include "remesa_ta_index.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define C(x) do{checks++;assert(x);}while(0)
static struct remesa_ta_index_request base(void)
{return (struct remesa_ta_index_request){.index_bo_gpu_va=0x20000000,.index_bo_bytes=0x10000,.index_byte_offset=0x200,
 .pds_bo_gpu_va=0x20000000,.pds_bo_bytes=0x10000,.pds_byte_offset=0x2000,.pds_object_bytes=64,
 .index_count=3,.pds_data_dwords=12,.descriptor_a_field10=8,.descriptor_b_field38=32};}
int main(void)
{
 unsigned rows=0,seen4=0,seen8=0;char hex[41];struct remesa_ta_index_request r=base();
 while(scanf("%u %u %u %u %40s",&r.index_count,&r.first_index,&r.descriptor_a_field10,&r.descriptor_b_field38,hex)==5){
  C(strlen(hex)==40);unsigned char expected[20],out[22];memset(out,0xa5,22);
  for(unsigned i=0;i<20;i++){unsigned v;C(sscanf(hex+2*i,"%2x",&v)==1);expected[i]=v;}
  C(!remesa_ta_index_compiled_position_color_triangles_encode(&r,out+1));
  C(!memcmp(out+1,expected,20)&&out[0]==0xa5&&out[21]==0xa5);
  C(remesa_ta_index_compiled_position_triangles_encode(&r,out+1)==ENOTSUP);
  C(remesa_ta_index_triangles_encode(&r,out+1)==ENOTSUP);
  seen4|=r.descriptor_a_field10==4;seen8|=r.descriptor_a_field10==8;rows++;
 }
 C(rows==3&&seen4&&seen8);
 for(unsigned mode=0;mode<14;mode++){
  r=base();int e=ENOTSUP;
  if(mode==0)r.descriptor_b_field38=24;
  if(mode==1)r.descriptor_b_field38=16;
  if(mode==2)r.descriptor_b_field38=48; /* unknown additional outputs */
  if(mode==3)r.descriptor_b_field38=0;
  if(mode==4)r.descriptor_a_field10=12; /* source layout not demonstrated */
  if(mode==5)r.descriptor_a_field10=0;
  if(mode==6)r.pds_data_dwords=13;
  if(mode==7)r.index_count=4;
  if(mode==8){r.pds_object_bytes=63;e=ERANGE;}
  if(mode==9){r.pds_byte_offset=0x2001;e=EINVAL;}
  if(mode==10){r.pds_byte_offset=0x200;e=EINVAL;}
  if(mode==11){r.index_byte_offset=0x201;e=EINVAL;}
  if(mode==12){r.index_bo_bytes=0x205;e=ERANGE;}
  if(mode==13){r.pds_bo_bytes=0x203f;e=ERANGE;}
  unsigned char out[20],old[20];memset(out,0x83,20);memcpy(old,out,20);
  C(remesa_ta_index_compiled_position_color_triangles_encode(&r,out)==e&&!memcmp(out,old,20));
 }
 r=base();C(remesa_ta_index_compiled_position_color_triangles_encode(NULL,(void *)&r)==EINVAL);
 C(remesa_ta_index_compiled_position_color_triangles_encode(&r,NULL)==EINVAL);
 unsigned char expected[20];C(!remesa_ta_index_compiled_position_color_triangles_encode(&r,expected));
 union{struct remesa_ta_index_request r;unsigned char bytes[sizeof(r)];}alias;alias.r=r;
 C(!remesa_ta_index_compiled_position_color_triangles_encode(&alias.r,alias.bytes)&&!memcmp(alias.bytes,expected,20));
 printf("PASS POSITION4+COLOR0 TA3 whole original flow/command oracles, distinct/shared input layouts, prior profile rejection and transactional negatives; CPU ONLY (%u checks)\n",checks);
 return 0;
}
