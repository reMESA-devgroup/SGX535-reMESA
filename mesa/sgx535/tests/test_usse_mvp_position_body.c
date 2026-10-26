/* SPDX-License-Identifier: MIT
 * Parameterized original writer oracles, not a GPU execution simulator. */
#include "remesa_usse_arithmetic.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static void reject(const struct remesa_usse_cpu_mvp_position_body *r,
 size_t capacity,int error)
{
 unsigned char out[112],old[112];memset(old,0xa5,sizeof(old));memcpy(out,old,sizeof(out));
 size_t n=99;
 assert(remesa_usse_cpu_mvp_position_body_encode(r,out,capacity,&n)==error);
 assert(n==99&&!memcmp(out,old,sizeof(out)));
}
int main(int argc,char **argv)
{
 assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
 unsigned char oracle[104],out[112];
 assert(fread(oracle,1,sizeof(oracle),f)==sizeof(oracle)&&fgetc(f)==EOF);fclose(f);
 struct remesa_usse_cpu_mvp_position_body r={.uniform_vectors=10,.source_dwords=41};
 size_t n=0;
 const unsigned spans[][2]={{9,37},{9,40},{10,41},{10,44},{10,128}};
 for(unsigned i=0;i<sizeof(spans)/sizeof(spans[0]);i++) {
  r.uniform_vectors=spans[i][0];r.source_dwords=spans[i][1];memset(out,0xa5,sizeof(out));
  assert(!remesa_usse_cpu_mvp_position_body_encode(&r,out,sizeof(out),&n));
  assert(n==sizeof(oracle)&&!memcmp(out,oracle,sizeof(oracle)));
  for(unsigned j=104;j<sizeof(out);j++)assert(out[j]==0xa5);
 }
 r.uniform_vectors=10;r.source_dwords=41;
 for(unsigned mode=0;mode<8;mode++) {
  struct remesa_usse_cpu_mvp_position_body bad=r;int error=ENOTSUP;
  if(mode==0)bad.position_base=4;
  if(mode==1)bad.uniform_vectors=8;
  if(mode==2)bad.uniform_vectors=11;
  if(mode==3){bad.source_dwords=33;error=ENOSPC;} /* MVP alone fits; full state does not. */
  if(mode==4){bad.source_dwords=40;error=ENOSPC;}
  if(mode==5)bad.source_dwords=129;
  if(mode==6){bad.reserved=1;error=EINVAL;}
  if(mode==7)error=ENOSPC;
  reject(&bad,mode==7?103:sizeof(out),error);
 }
 assert(remesa_usse_cpu_mvp_position_body_encode(NULL,out,sizeof(out),&n)==EINVAL);
 assert(remesa_usse_cpu_mvp_position_body_encode(&r,NULL,sizeof(out),&n)==EINVAL);
 assert(remesa_usse_cpu_mvp_position_body_encode(&r,out,SIZE_MAX,&n)==EINVAL);
 assert(remesa_usse_cpu_mvp_position_body_encode(&r,&r,sizeof(r),&n)==EINVAL);
 assert(remesa_usse_cpu_mvp_position_body_encode(&r,out,sizeof(out),(void *)out)==EINVAL);
 const unsigned offsets[]={8,16,48,80};
 for(unsigned c=0;c<4;c++) {
  struct remesa_usse_cpu_arithmetic a={.kind=0,.flags=0x41,
   .destination={0,c?0u:4u},.source={{2,c},{3,17+4*c},{6,48}}};
  assert(!remesa_usse_cpu_arithmetic_encode(&a,out,sizeof(out),&n));
  assert(n==8&&!memcmp(out,oracle+offsets[c],8));
  for(unsigned mode=0;mode<7;mode++) {
   struct remesa_usse_cpu_arithmetic bad=a;
   if(mode==0)bad.source[1].base++;
   if(mode==1)bad.source[0].base=4;
   if(mode==2)bad.destination.base=8;
   if(mode==3)bad.destination.cpu_class=1;
   if(mode==4)bad.source[2].base=52;
   if(mode==5)bad.flags=0x31;
   if(mode==6)bad.modifier[1]=1;
   memset(out,0xa5,sizeof(out));n=99;
   assert(remesa_usse_cpu_arithmetic_encode(&bad,out,sizeof(out),&n)==ENOTSUP);
   assert(n==99);for(unsigned j=0;j<sizeof(out);j++)assert(out[j]==0xa5);
  }
 }
 assert(REMESA_USSE_CPU_MVP_POSITION_INPUT_DWORDS==4);
 assert(REMESA_USSE_CPU_MVP_POSITION_TEMPORARY_DWORDS==8);
 assert(REMESA_USSE_CPU_MVP_POSITION_OUTPUT_DWORDS==4);
 puts("PASS complete captured MVP POSITION body104 against13 original writer records;4MUL/3ADD, full9/10uniform bounds, exact tuple/rollback; CPU ONLY, task8temp and full gears NOT admitted");
 return 0;
}
