/* SPDX-License-Identifier: MIT. Host packing policy; CPU ONLY, no GPU model. */
#include "remesa_pds_structure.h"
#include "remesa_render_target.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned get32(const unsigned char*p){return p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24);}
static void reject(struct remesa_pds_parameter_source_request r,int error){
 unsigned char coefficients[508],out[528],before[528];memset(coefficients,0xff,sizeof(coefficients));
 memset(out,0xa5,sizeof(out));memcpy(before,out,sizeof(out));size_t n=777;
 assert(remesa_pds_structure_encode_parameter_source(&r,coefficients,r.coefficient_dwords<=127?r.coefficient_dwords*4:508,out,sizeof(out),&n)==error);
 assert(n==777&&!memcmp(out,before,sizeof(out)));
}
int main(int argc,char**argv){
 assert(argc==3);unsigned char coefficients[508],out[528],image[60],mask[60],ta[8];
 /* Arbitrary float bit patterns including signed zero/NaN must stay exact. */
 for(unsigned i=0;i<sizeof(coefficients);i++)coefficients[i]=(unsigned char)(i*71+128);
 unsigned accepted=0;
 for(unsigned c=1;c<=127;c++)for(unsigned aligned=0;aligned<2;aligned++){
  unsigned effective=1+c,transfer=aligned?(effective+3)&~3u:effective;
  struct remesa_pds_parameter_source_request r={0x20000000,131072,0x6400,c,transfer};
  struct remesa_pds_dma_partition p;assert(!remesa_pds_structure_dma_partition(transfer,&p));
  if(p.count!=1){reject(r,ENOTSUP);continue;}
  memset(out,0xa5,sizeof(out));size_t n=999;
  assert(!remesa_pds_structure_encode_parameter_source(&r,coefficients,c*4,out,sizeof(out),&n)&&n==transfer*4);
  assert(get32(out)==0x20006400&&!memcmp(out+4,coefficients,c*4));
  for(unsigned i=effective*4;i<n;i++)assert(out[i]==0);
  for(unsigned i=n;i<sizeof(out);i++)assert(out[i]==0xa5);
  struct remesa_pds_state_image_request s={0x20000000,131072,0x6400,transfer,0x2044,REMESA_PDS_STATE_TASK_PARAMETERS};
  assert(!remesa_pds_structure_encode_state_image(&s,image,mask,60,&n)&&n==60&&get32(image+4)==p.control[0]);
  assert(!remesa_ta_parameter_reference_encode(0x20006300,transfer,ta));
  assert(get32(ta+4)==(0x0c01e100u|((transfer+3)/4)));
  r.source_allocation_bytes=r.source_offset+transfer*4-1;reject(r,ENOSPC);++accepted;
 }
 struct remesa_pds_parameter_source_request r={0x20000000,131072,0x6400,16,20};
 reject((struct remesa_pds_parameter_source_request){0x20000000,131072,0x6400,16,17},ENOTSUP);
 reject((struct remesa_pds_parameter_source_request){0x20000000,131072,0x6400,16,24},EINVAL);
 reject((struct remesa_pds_parameter_source_request){0x20000000,131072,0x6400,0,4},EINVAL);
 reject((struct remesa_pds_parameter_source_request){0x20000000,131072,0x6400,128,128},ENOTSUP);
 reject((struct remesa_pds_parameter_source_request){0x20000001,131072,0x6400,16,20},EINVAL);
 reject((struct remesa_pds_parameter_source_request){0xfffffff0,80,0,16,20},EOVERFLOW);
 reject((struct remesa_pds_parameter_source_request){0x20000000,131072,131072,16,20},ENOSPC);
 memset(out,0xa5,sizeof(out));size_t n=777;
 assert(remesa_pds_structure_encode_parameter_source(&r,coefficients,64,out,79,&n)==ENOSPC&&n==777);
 assert(remesa_pds_structure_encode_parameter_source(&r,coefficients,63,out,80,&n)==EINVAL&&n==777);
 assert(remesa_pds_structure_encode_parameter_source(&r,out,64,out,80,&n)==EINVAL&&n==777);
 assert(remesa_pds_structure_encode_parameter_source(&r,NULL,64,out,80,&n)==EINVAL&&n==777);
 assert(remesa_pds_structure_encode_parameter_source(&r,coefficients,64,out,80,NULL)==EINVAL);
 for(unsigned i=0;i<sizeof(out);i++)assert(out[i]==0xa5);
 /* Compare committed counts against actual original CPU consumers, not a
  * software rendering oracle. Carriers are created by retained x86 replay. */
 FILE*f=fopen(argv[1],"rb");assert(f);unsigned char row[24];unsigned count=0;
 while(fread(row,1,24,f)==24){struct remesa_pds_dma_partition p;assert(!remesa_pds_structure_dma_partition(get32(row),&p));assert(p.count==get32(row+4));for(unsigned i=0;i<2;i++){assert(p.source_word_offset[i]==get32(row+8+4*i));assert(p.control[i]==get32(row+16+4*i));}++count;}
 assert(feof(f)&&!ferror(f)&&!fclose(f)&&count==10);
 f=fopen(argv[2],"rb");assert(f);unsigned char ref[16];count=0;
 while(fread(ref,1,16,f)==16){assert(!remesa_ta_parameter_reference_encode(get32(ref),get32(ref+4),ta)&&!memcmp(ta,ref+8,8));++count;}
 assert(feof(f)&&!ferror(f)&&!fclose(f)&&count==10);
 printf("PASS %u exact/aligned source policies; prefix/bit-exact coefficients/zero-fill/guard/extent/rollback; 10 original DMA+TA count oracles; CPU ONLY\n",accepted);return 0;
}
