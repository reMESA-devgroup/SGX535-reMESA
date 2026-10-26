/* SPDX-License-Identifier: MIT */
#include "remesa_render_target.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned word(const unsigned char *p)
{return (unsigned)p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24);}
static void dump_case(unsigned n,const struct remesa_isp_opaque_triangle_request *r,
 const unsigned char out[8])
{
 printf("%u %u %u %u %u %u ",n,r->depth_bits_present,r->depth_test,
        r->depth_write,(unsigned)r->compare,r->rgba_mask);
 for(unsigned i=0;i<8;i++)printf("%02x",out[i]);
 puts("");
}
int main(int argc,char **argv)
{
 bool dump=argc==2&&!strcmp(argv[1],"--dump");assert(argc==1||dump);
 unsigned n=0;unsigned char out[8];
 for(unsigned c=0;c<2;c++)for(unsigned w=0;w<2;w++)for(unsigned m=0;m<2;m++) {
  struct remesa_isp_opaque_triangle_request r={true,true,w,c?REMESA_DEPTH_ALWAYS:REMESA_DEPTH_LESS,m?15:0};
  /* These values are checked separately against full original x86 producer
   * and state DATA serialization, not against a decoder sharing this code. */
  static const unsigned expected[2][2]={{0x00500000,0x00400000},{0x01d00000,0x01c00000}};
  assert(!remesa_isp_opaque_triangle_data_encode(&r,out));
  assert(word(out)==1&&word(out+4)==(expected[c][w]|(m?0:0x02200000)));
  if(dump)dump_case(n,&r,out);
  n++;
 }
 for(unsigned i=0;i<2;i++) {
  struct remesa_isp_opaque_triangle_request r={i,!i,true,REMESA_DEPTH_LESS,15};
  assert(!remesa_isp_opaque_triangle_data_encode(&r,out));assert(word(out)==1&&word(out+4)==0x01d00000);
  if(dump)dump_case(n,&r,out);
  n++;
 }
 unsigned char before[8];memset(out,0xa5,8);memcpy(before,out,8);
 struct remesa_isp_opaque_triangle_request r={true,true,true,REMESA_DEPTH_LESS,16};
 assert(remesa_isp_opaque_triangle_data_encode(&r,out)==EINVAL&&!memcmp(out,before,8));
 r.rgba_mask=1;assert(remesa_isp_opaque_triangle_data_encode(&r,out)==ENOTSUP&&!memcmp(out,before,8));
 r.rgba_mask=15;r.compare=REMESA_DEPTH_EQUAL;assert(remesa_isp_opaque_triangle_data_encode(&r,out)==ENOTSUP&&!memcmp(out,before,8));
 r.compare=(enum remesa_depth_compare)-1;assert(remesa_isp_opaque_triangle_data_encode(&r,out)==EINVAL&&!memcmp(out,before,8));
 assert(remesa_isp_opaque_triangle_data_encode(NULL,out)==EINVAL&&!memcmp(out,before,8));
 assert(remesa_isp_opaque_triangle_data_encode(&r,NULL)==EINVAL);
 r=(struct remesa_isp_opaque_triangle_request){true,true,true,REMESA_DEPTH_LESS,15};
 assert(!remesa_isp_opaque_triangle_data_encode(&r,(unsigned char *)&r));
 assert(word((unsigned char *)&r)==1&&word((unsigned char *)&r+4)==0x00400000);
 if(!dump)puts("PASS: 10 original-oracle ISP DATA updates, exact LE selection, unsupported-state and transactional/overlap checks; CPU ONLY, not complete TA job");
 return 0;
}
