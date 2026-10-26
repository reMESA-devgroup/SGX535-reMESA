/* SPDX-License-Identifier: MIT
 * Independent original compiler+linker oracle; no GPU qualification. */
#include "remesa_usse_arithmetic.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static void reject(const struct remesa_usse_cpu_parameter_mad_vertex *r,
 size_t cap,int error)
{
 unsigned char b[24],old[24];memset(old,0xa5,sizeof(old));memcpy(b,old,sizeof(b));
 size_t n=99;
 assert(remesa_usse_cpu_parameter_mad_vertex_encode(r,b,cap,&n)==error);
 assert(n==99&&!memcmp(b,old,sizeof(b)));
}
int main(int argc,char **argv)
{
 assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
 unsigned char oracle[16],out[24];
 assert(fread(oracle,1,sizeof(oracle),f)==sizeof(oracle)&&fgetc(f)==EOF);fclose(f);
 struct remesa_usse_cpu_parameter_mad_vertex r={.multiply_parameter_base=1,
  .add_parameter_base=5,.source_dwords=9};
 const unsigned spans[]={9,20,128};size_t n=0;
 for(unsigned i=0;i<sizeof(spans)/sizeof(spans[0]);i++) {
  r.source_dwords=spans[i];memset(out,0xa5,sizeof(out));
  assert(!remesa_usse_cpu_parameter_mad_vertex_encode(&r,out,sizeof(out),&n));
  assert(n==sizeof(oracle)&&!memcmp(out,oracle,sizeof(oracle)));
  for(unsigned j=16;j<sizeof(out);j++)assert(out[j]==0xa5);
 }
 r.source_dwords=9;
 for(unsigned mode=0;mode<8;mode++) {
  struct remesa_usse_cpu_parameter_mad_vertex bad=r;int e=ENOTSUP;
  if(mode==0)bad.position_base=4;
  if(mode==1)bad.multiply_parameter_base=5;
  if(mode==2)bad.add_parameter_base=9;
  if(mode==3){bad.source_dwords=0;e=ENOSPC;}
  if(mode==4){bad.source_dwords=8;e=ENOSPC;}
  if(mode==5)bad.source_dwords=129;
  if(mode==6){bad.reserved=1;e=EINVAL;}
  if(mode==7)e=ENOSPC;
  reject(&bad,mode==7?15:sizeof(out),e);
 }
 assert(remesa_usse_cpu_parameter_mad_vertex_encode(NULL,out,sizeof(out),&n)==EINVAL);
 assert(remesa_usse_cpu_parameter_mad_vertex_encode(&r,NULL,sizeof(out),&n)==EINVAL);
 assert(remesa_usse_cpu_parameter_mad_vertex_encode(&r,out,SIZE_MAX,&n)==EINVAL);
 assert(remesa_usse_cpu_parameter_mad_vertex_encode(&r,&r,sizeof(r),&n)==EINVAL);
 assert(remesa_usse_cpu_parameter_mad_vertex_encode(&r,out,sizeof(out),(void *)out)==EINVAL);
 const struct remesa_usse_cpu_arithmetic body={.kind=0,.flags=0x41,
  .destination={1,0},.source={{2,0},{3,1},{3,5}}};
 assert(!remesa_usse_cpu_arithmetic_encode(&body,out,sizeof(out),&n));
 assert(n==8&&!memcmp(out,oracle,8));
 for(unsigned mode=0;mode<9;mode++) {
  struct remesa_usse_cpu_arithmetic bad=body;
  if(mode==0)bad.kind=2;
  if(mode==1)bad.flags=0x31;
  if(mode==2)bad.destination.base=1;
  if(mode==3)bad.destination.cpu_class=2;
  if(mode==4)bad.source[0].base=4;
  if(mode==5)bad.source[1].base=5;
  if(mode==6)bad.source[2].base=9;
  if(mode==7)bad.source[0].cpu_class=3;
  if(mode==8)bad.modifier[2]=1;
  memset(out,0xa5,sizeof(out));n=99;
  assert(remesa_usse_cpu_arithmetic_encode(&bad,out,sizeof(out),&n)==ENOTSUP);
  assert(n==99);for(unsigned j=0;j<sizeof(out);j++)assert(out[j]==0xa5);
 }
 /* Existing one-parameter APIs retain their old coefficient-index gate. */
 struct remesa_usse_arithmetic_sources old={.op=REMESA_USSE_ARITHMETIC_ADD,
  .left={2,0},.right={3,5}};
 assert(remesa_usse_arithmetic_sources_encode(&old,out,sizeof(out),&n)==ENOTSUP);
 struct remesa_usse_cpu_parameter_vertex one={.op=REMESA_USSE_ARITHMETIC_ADD,
  .parameter_base=5};
 assert(remesa_usse_cpu_parameter_vertex_encode(&one,out,sizeof(out),&n)==ENOTSUP);
 puts("PASS whole original linked two-parameter MUL+ADD VS16; exact common kind0 tuple, source bounds, transactional refusal, old parameter gates; CPU ONLY");
 return 0;
}
