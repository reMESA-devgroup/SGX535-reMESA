/* SPDX-License-Identifier: MIT. Full captured arithmetic body, CPU ONLY.
 * Original independent writer carriers are required; no GPU/model callback. */
#include "remesa_usse_arithmetic.h"
#include "remesa_usse.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned get32(const unsigned char*p){return p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24);}
static void reject(struct remesa_usse_cpu_gears_vertex_body r,int error,size_t cap){
 unsigned char out[328],old[328];memset(old,0xa5,sizeof(old));memcpy(out,old,sizeof(out));size_t n=777;
 assert(remesa_usse_cpu_gears_vertex_body_encode(&r,out,cap,&n)==error&&n==777&&!memcmp(out,old,sizeof(out)));
}
int main(int argc,char**argv){
 assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f);unsigned char header[12],oracle[312],out[328];unsigned count=0;
 while(fread(header,1,12,f)==12){
  unsigned attr=get32(header),uniforms=get32(header+4),size=get32(header+8);assert(attr<2&&uniforms==(attr?9u:10u)&&size==312&&fread(oracle,1,312,f)==312);
  struct remesa_usse_cpu_gears_vertex_body r={attr,uniforms,1+4*uniforms,REMESA_USSE_CPU_GEARS_TRIANGLES,0};size_t n=777;memset(out,0xa5,sizeof(out));
  assert(!remesa_usse_cpu_gears_vertex_body_encode(&r,out,sizeof(out),&n)&&n==312&&!memcmp(out,oracle,312));for(unsigned i=312;i<sizeof(out);i++)assert(out[i]==0xa5);
  r.source_dwords--;reject(r,ENOSPC,sizeof(out));r.source_dwords=128;assert(!remesa_usse_cpu_gears_vertex_body_encode(&r,out,sizeof(out),&n)&&!memcmp(out,oracle,312));
  r.source_dwords=129;reject(r,ENOTSUP,sizeof(out));r.source_dwords=1+4*uniforms;reject(r,ENOSPC,311);
  r.primitive=0;reject(r,ENOTSUP,sizeof(out));r.primitive=2;reject(r,ENOTSUP,sizeof(out));r.primitive=1;
  r.uniform_vectors=attr?10:9;reject(r,ENOTSUP,sizeof(out));r.uniform_vectors=uniforms;r.normal_from_attribute=2;reject(r,ENOTSUP,sizeof(out));r.normal_from_attribute=attr;r.reserved=1;reject(r,EINVAL,sizeof(out));++count;
 }
 assert(feof(f)&&!ferror(f)&&!fclose(f)&&count==2);
 struct remesa_usse_cpu_gears_vertex_body r={0,10,41,1,0};size_t n=777;
 assert(remesa_usse_cpu_gears_vertex_body_encode(NULL,out,sizeof(out),&n)==EINVAL);
 assert(remesa_usse_cpu_gears_vertex_body_encode(&r,NULL,sizeof(out),&n)==EINVAL);
 assert(remesa_usse_cpu_gears_vertex_body_encode(&r,out,SIZE_MAX,&n)==EINVAL);
 assert(remesa_usse_cpu_gears_vertex_body_encode(&r,&r,sizeof(r),&n)==EINVAL);
 assert(remesa_usse_cpu_gears_vertex_body_encode(&r,out,sizeof(out),(void*)out)==EINVAL);
 struct remesa_usse_cpu_move m={.source={3,33},.destination={0,4},.control12=2,.control21=1,.control23=1},decoded;
 assert(!remesa_usse_cpu_move_encode(&m,out,sizeof(out),&n)&&n==8&&!remesa_usse_cpu_move_decode(out,8,&decoded)&&!memcmp(&m,&decoded,sizeof(m)));
 const unsigned bad_sources[]={0,1,32,34,127,128};
 for(unsigned i=0;i<sizeof(bad_sources)/sizeof(bad_sources[0]);i++){m.source.number=bad_sources[i];memset(out,0xa5,sizeof(out));n=777;assert(remesa_usse_cpu_move_encode(&m,out,sizeof(out),&n)==ENOTSUP&&n==777);for(unsigned j=0;j<sizeof(out);j++)assert(out[j]==0xa5);}
 /* An arbitrary SA arithmetic tuple is still not a supported opcode family. */
 struct remesa_usse_cpu_arithmetic a={.kind=0,.flags=0x31,.destination={0,8},.source={{0,1},{3,10},{6,48}}};n=777;
 assert(remesa_usse_cpu_arithmetic_encode(&a,out,sizeof(out),&n)==ENOTSUP&&n==777);
 a=(struct remesa_usse_cpu_arithmetic){.kind=0,.flags=0x201,.destination={0,2},.source={{0,0},{6,52},{6,48}},.modifier={1,0,0}};
 assert(!remesa_usse_cpu_arithmetic_encode(&a,out,sizeof(out),&n)&&n==8);a.destination.base=3;n=777;assert(remesa_usse_cpu_arithmetic_encode(&a,out,sizeof(out),&n)==ENOTSUP&&n==777);
 puts("PASS both complete captured POSITION+lighting/material/COLOR0 bodies312 against13+26 original CPU writer records; source/topology/rollback/old gates; CPU ONLY, no launch or GPU admission");return 0;
}
