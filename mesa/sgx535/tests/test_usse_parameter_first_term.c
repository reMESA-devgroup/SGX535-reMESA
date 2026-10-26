/* SPDX-License-Identifier: MIT; whole original compiler+linker oracle. */
#include "remesa_usse_arithmetic.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static void reject(const struct remesa_usse_cpu_parameter_first_term_vertex *r,
 size_t cap,int error)
{
 unsigned char out[32],old[32];memset(old,0xa5,sizeof(old));memcpy(out,old,sizeof(out));
 size_t n=99;
 assert(remesa_usse_cpu_parameter_first_term_vertex_encode(r,out,cap,&n)==error);
 assert(n==99&&!memcmp(out,old,sizeof(out)));
}
int main(int argc,char **argv)
{
 assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
 unsigned char oracle[24],out[32];
 assert(fread(oracle,1,sizeof(oracle),f)==sizeof(oracle)&&fgetc(f)==EOF);fclose(f);
 struct remesa_usse_cpu_parameter_first_term_vertex r={.parameter_base=1,
  .source_dwords=5};size_t n=0;
 const unsigned spans[]={5,20,128};
 for(unsigned i=0;i<sizeof(spans)/sizeof(spans[0]);i++) {
  r.source_dwords=spans[i];memset(out,0xa5,sizeof(out));
  assert(!remesa_usse_cpu_parameter_first_term_vertex_encode(&r,out,sizeof(out),&n));
  assert(n==sizeof(oracle)&&!memcmp(out,oracle,sizeof(oracle)));
  for(unsigned j=24;j<sizeof(out);j++)assert(out[j]==0xa5);
 }
 r.source_dwords=5;
 for(unsigned mode=0;mode<7;mode++) {
  struct remesa_usse_cpu_parameter_first_term_vertex bad=r;int e=ENOTSUP;
  if(mode==0)bad.position_base=1; /* Other broadcast channel not admitted. */
  if(mode==1)bad.parameter_base=5;
  if(mode==2){bad.source_dwords=0;e=ENOSPC;}
  if(mode==3){bad.source_dwords=4;e=ENOSPC;}
  if(mode==4)bad.source_dwords=129;
  if(mode==5){bad.reserved=1;e=EINVAL;}
  if(mode==6)e=ENOSPC;
  reject(&bad,mode==6?23:sizeof(out),e);
 }
 assert(remesa_usse_cpu_parameter_first_term_vertex_encode(NULL,out,sizeof(out),&n)==EINVAL);
 assert(remesa_usse_cpu_parameter_first_term_vertex_encode(&r,NULL,sizeof(out),&n)==EINVAL);
 assert(remesa_usse_cpu_parameter_first_term_vertex_encode(&r,out,SIZE_MAX,&n)==EINVAL);
 assert(remesa_usse_cpu_parameter_first_term_vertex_encode(&r,&r,sizeof(r),&n)==EINVAL);
 assert(remesa_usse_cpu_parameter_first_term_vertex_encode(&r,out,sizeof(out),(void *)out)==EINVAL);
 /* The old identity MUL remains exactly the original body+tail suffix. */
 const struct remesa_usse_cpu_parameter_vertex mul={.parameter_base=1,
  .op=REMESA_USSE_ARITHMETIC_MULTIPLY};
 assert(!remesa_usse_cpu_parameter_vertex_encode(&mul,out,sizeof(out),&n));
 assert(n==16&&!memcmp(out,oracle+8,16));
 puts("PASS whole original linked MVP first-term VS24; replicated input X, PARAM0 vector, B5+existing MUL/tail, source bounds and rollback; CPU ONLY, no complete matrix");
 return 0;
}
