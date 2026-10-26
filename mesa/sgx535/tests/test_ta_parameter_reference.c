/* SPDX-License-Identifier: MIT; independently replayed original3b606 on argv. */
#include "remesa_render_target.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
static uint32_t get32(const unsigned char*p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int main(int argc,char**argv){
 assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f);unsigned char row[16],out[8],old[8];unsigned count=0;
 while(fread(row,1,16,f)==16){assert(!remesa_ta_parameter_reference_encode(get32(row),get32(row+4),out)&&!memcmp(out,row+8,8));++count;}
 assert(feof(f)&&!ferror(f)&&count==6);fclose(f);memset(old,0xa5,8);
 uint64_t invalid[]={0,1,0x20000001,UINT64_C(0x100000000)};
 for(unsigned i=0;i<4;i++){memcpy(out,old,8);assert(remesa_ta_parameter_reference_encode(invalid[i],5,out)==EINVAL&&!memcmp(out,old,8));}
 for(unsigned n=0;n<2;n++){memcpy(out,old,8);assert(remesa_ta_parameter_reference_encode(0x20006300,n,out)==EINVAL&&!memcmp(out,old,8));}
 memcpy(out,old,8);assert(remesa_ta_parameter_reference_encode(0x20006300,129,out)==ENOTSUP&&!memcmp(out,old,8));
 assert(remesa_ta_parameter_reference_encode(0x20006300,5,NULL)==EINVAL);
 unsigned char isp[8];assert(!remesa_ta_parameter_reference_encode(0x20006300,5,out));assert(!remesa_ta_state_reference_encode(0x20006300,5,false,isp));assert(memcmp(out,isp,8));
 puts("PASS6 original TA parameter references; VA/count bounds/rollback; ISP reference distinct; no GPU");return 0;
}
