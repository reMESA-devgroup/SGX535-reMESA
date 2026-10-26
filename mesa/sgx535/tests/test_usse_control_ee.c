/* SPDX-License-Identifier: MIT; oracle is from independent original producers. */
#include "remesa_usse.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static void reject(struct remesa_usse_cpu_control_ee*r,int expected){
 unsigned char out[16],old[16];memset(old,0xa5,16);memcpy(out,old,16);size_t n=99;
 assert(remesa_usse_cpu_control_ee_encode(r,out,16,&n)==expected&&n==99&&!memcmp(out,old,16));
}
int main(int argc,char**argv){
 assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f);unsigned char oracle[24];assert(fread(oracle,1,24,f)==24&&fgetc(f)==EOF);fclose(f);
 const struct remesa_usse_cpu_control_ee cases[]={{0,0,0},{1,0,0},{0,1,0}};
 for(unsigned i=0;i<3;i++){
  unsigned char out[16];memset(out,0xa5,16);size_t n=0;
  assert(!remesa_usse_cpu_control_ee_encode(&cases[i],out,16,&n)&&n==8&&!memcmp(out,oracle+8*i,8));
  for(unsigned j=8;j<16;j++)assert(out[j]==0xa5);
  n=99;memset(out,0xa5,16);assert(remesa_usse_cpu_control_ee_encode(&cases[i],out,7,&n)==ENOSPC&&n==99);for(unsigned j=0;j<16;j++)assert(out[j]==0xa5);
 }
 struct remesa_usse_cpu_control_ee r={.mode8=2};reject(&r,ENOTSUP);r.mode8=0;r.record_last=2;reject(&r,ENOTSUP);
 r.mode8=1;r.record_last=1;reject(&r,ENOTSUP);r=(struct remesa_usse_cpu_control_ee){.reserved=1};reject(&r,EINVAL);
 r=(struct remesa_usse_cpu_control_ee){0};unsigned char out[16];size_t n=99;
 assert(remesa_usse_cpu_control_ee_encode(NULL,out,16,&n)==EINVAL&&n==99);
 assert(remesa_usse_cpu_control_ee_encode(&r,NULL,16,&n)==EINVAL&&n==99);
 assert(remesa_usse_cpu_control_ee_encode(&r,out,16,NULL)==EINVAL);
 assert(remesa_usse_cpu_control_ee_encode(&r,&r,sizeof(r),&n)==EINVAL);
 _Alignas(struct remesa_usse_cpu_control_ee) unsigned char bad[sizeof(r)+8];memset(bad,0,sizeof(bad));assert(remesa_usse_cpu_control_ee_encode((void*)(bad+1),out,16,&n)==EINVAL);
 puts("PASS EE CPU controls against2 original MVP modes + independent PARAM auxiliary; unsupported combos/bounds/alias/transactional; no GPU");return 0;
}
