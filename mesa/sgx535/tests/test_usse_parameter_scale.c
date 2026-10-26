/* SPDX-License-Identifier: MIT; whole linked original USE oracle required. */
#include "remesa_usse_arithmetic.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static void reject(struct remesa_usse_cpu_parameter_vertex*r,int e){unsigned char b[24],old[24];memset(old,0xa5,24);memcpy(b,old,24);size_t n=99;assert(remesa_usse_cpu_parameter_vertex_encode(r,b,24,&n)==e&&n==99&&!memcmp(b,old,24));}
int main(int argc,char**argv){
 assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f);unsigned char oracle[32],out[24];assert(fread(oracle,1,32,f)==32&&fgetc(f)==EOF);fclose(f);
 struct remesa_usse_cpu_parameter_vertex r={.parameter_base=1,.op=REMESA_USSE_ARITHMETIC_MULTIPLY};size_t n=0;
 for(unsigned i=0;i<2;i++){
  r.op=i?REMESA_USSE_ARITHMETIC_ADD:REMESA_USSE_ARITHMETIC_MULTIPLY;memset(out,0xa5,24);
  assert(!remesa_usse_cpu_parameter_vertex_encode(&r,out,24,&n)&&n==16&&!memcmp(out,oracle+16*i,16));for(unsigned j=16;j<24;j++)assert(out[j]==0xa5);
 }
 r.op=REMESA_USSE_ARITHMETIC_MAXIMUM;reject(&r,ENOTSUP);r.op=REMESA_USSE_ARITHMETIC_MULTIPLY;
 r.position_base=4;reject(&r,ENOTSUP);r.position_base=0;r.parameter_base=0;reject(&r,ENOTSUP);r.parameter_base=2;reject(&r,ENOTSUP);r.parameter_base=1;r.reserved=1;reject(&r,EINVAL);r.reserved=0;
 memset(out,0xa5,24);n=99;assert(remesa_usse_cpu_parameter_vertex_encode(&r,out,15,&n)==ENOSPC&&n==99);for(unsigned i=0;i<24;i++)assert(out[i]==0xa5);
 assert(remesa_usse_cpu_parameter_vertex_encode(&r,&r,sizeof(r),&n)==EINVAL);
 assert(remesa_usse_cpu_parameter_vertex_encode(NULL,out,24,&n)==EINVAL);
 puts("PASS complete parameter VS16 from typed ADD/MUL+tail;2 whole original linked oracles; unknown bindings/bounds/rollback rejected; no GPU");return 0;
}
