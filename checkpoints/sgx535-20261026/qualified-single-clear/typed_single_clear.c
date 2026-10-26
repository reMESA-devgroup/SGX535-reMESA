#include "remesa_ta_zclear.h"
#include "remesa_ta_index.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv){
 if(argc!=2)return 2;
 struct remesa_ta_zclear_request r={1280,800,1280,800,1.0f};struct remesa_ta_zclear_geometry q;
 if(remesa_ta_zclear_geometry_encode(&r,&q))return 3;
 const float baseline[16]={0,0,1,1,1280,0,1,1,0,800,1,1,1280,800,1,1};
 if(memcmp(q.positions,baseline,64))return 4;
 /* Exact covering triangle. Fourth vertex remains reserved and is not indexed. */
 float x=2560.0f,y=1600.0f;memcpy(q.positions+16,&x,4);memcpy(q.positions+36,&y,4);
 uint8_t old[20],next[20];struct remesa_ta_index_request t={
 .index_bo_gpu_va=0x20000000,.index_bo_bytes=131072,.index_byte_offset=0x4344,
 .pds_bo_gpu_va=0x20000000,.pds_bo_bytes=131072,.pds_byte_offset=0x4280,.pds_object_bytes=64,
 .first_index=0,.index_count=6,.pds_data_dwords=12,.descriptor_a_field10=4,.descriptor_b_field38=4};
 if(remesa_ta_index_triangles_encode(&t,old))return 5;
 const uint32_t expected[5]={0x81400006,0x20004344,0,0x02000428,0x02000103};
 if(memcmp(old,expected,20))return 6;
 t.index_count=3;if(remesa_ta_index_triangles_encode(&t,next))return 7;
 if(next[0]!=3||memcmp(old+1,next+1,19))return 8;
 /* Integer barycentric coverage proof for ALL1280x800 pixel centres. */
 for(uint64_t py=0;py<800;py++)for(uint64_t px=0;px<1280;px++)
  if((2*px+1)*800+(2*py+1)*1280>=4*1280ULL*800)return 9;
 FILE *f=fopen(argv[1],"wbx");return !f||fwrite(q.positions,1,64,f)!=64||fwrite(old,1,20,f)!=20||fwrite(next,1,20,f)!=20||fclose(f);
}
