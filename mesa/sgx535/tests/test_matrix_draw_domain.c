/* SPDX-License-Identifier: MIT. Compare integer bounds with the previous
 * ordinary frontend's float predicate; CPU test only, never a renderer. */
#include "remesa/matrix_draw_domain.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
struct remesa_gears_geometry_view {
 const void *vertices;size_t bytes;uint32_t vertex_count,stride_dwords,normal_from_attribute;
};
/* Historical W2 float predicate plus an independent long-double W1 bound.
 * The W2 behavior remains exact; outward W1 rounding is exercised separately. */
static int prior_float_domain(const struct remesa_gears_geometry_view *view,
 const unsigned char source[160])
{
 if(view->stride_dwords!=8||!view->vertices||!view->vertex_count||view->vertex_count>65535||
    view->bytes!=(uint64_t)view->vertex_count*32)return ENOTSUP;
 float constants[36];
 for(unsigned i=0;i<36;i++){
  uint32_t bits=(uint32_t)source[4+4*i]|(uint32_t)source[5+4*i]<<8|
   (uint32_t)source[6+4*i]<<16|(uint32_t)source[7+4*i]<<24;
  memcpy(&constants[i],&bits,4);
  if(!isfinite(constants[i])||fabsf(constants[i])>4)return ENOTSUP;
 }
 for(unsigned col=0;col<3;col++)for(unsigned row=0;row<4;row++){
  float v=constants[16+4*col+row];
  if(row==col){if(fabsf(v)>1)return ENOTSUP;}
  else if(v!=0)return ENOTSUP;
 }
 for(unsigned i=28;i<31;i++)if(fabsf(constants[i])>.5f)return ENOTSUP;
 if(constants[31]!=2&&constants[31]!=1)return ENOTSUP;
 bool light=false;
 for(unsigned i=32;i<35;i++){
  if(fabsf(constants[i])>1)return ENOTSUP;
  if(fabsf(constants[i])>=.0625f)light=true;
 }
 if(!light||constants[35]!=0)return ENOTSUP;
 for(uint32_t i=0;i<view->vertex_count;i++){
  float value[8];memcpy(value,(const unsigned char *)view->vertices+(size_t)i*32,sizeof value);
  bool normal=false;
  for(unsigned k=0;k<8;k++)if(!isfinite(value[k])||fabsf(value[k])>1)return ENOTSUP;
  for(unsigned k=4;k<7;k++)if(fabsf(value[k])>=.0625f)normal=true;
  /* Gallium pads a float3 NORMAL with W=1; explicit float4 inputs can
   * retain W=0. The selected attribute-normal shader consumes XYZ only.
   * Keep both canonical forms without changing the existing fetch bits. */
  if(value[3]!=1||(value[7]!=0&&value[7]!=1)||!normal)return ENOTSUP;
  if(constants[31]==1)for(unsigned k=0;k<3;k++)
   if(fabsl((long double)constants[16+5*k])*fabsl((long double)value[k])+
      fabsl((long double)constants[28+k])>.875L)return ENOTSUP;
 }
 return 0;
}

static unsigned cases;
static void put(unsigned char *p,uint32_t x)
{for(unsigned i=0;i<4;i++)p[i]=x>>(8*i);}
static void compare(const unsigned char *v,const unsigned char *source)
{
 unsigned char packed[144];memset(packed,0,sizeof packed);
 for(unsigned i=0;i<3;i++)memcpy(packed+48*i,v+32*i,32);
 const struct remesa_gears_geometry_view view={v,96,3,8,1};
 int expected=prior_float_domain(&view,source);
 assert(remesa_matrix_draw_values(v,96,3,32,source+4,144)==expected);
 assert(remesa_matrix_draw_values(packed,144,3,48,source+4,144)==expected);cases++;
}
int main(void)
{
 unsigned char v[96]={0},source[160]={0},saved_v[96],saved_source[160];
 for(unsigned i=0;i<3;i++){
  put(v+32*i,0xbf000000);put(v+32*i+4,0xbf000000);put(v+32*i+12,0x3f800000);
  put(v+32*i+24,0x3f800000);put(v+32*i+28,0x3f800000);
 }
 for(unsigned i=0;i<4;i++)put(source+4+4*i,0x3f800000);
 for(unsigned i=0;i<3;i++)put(source+4+4*(16+5*i),0x3f800000);
 put(source+4+4*28,0x3e800000);put(source+4+4*31,0x40000000);put(source+4+4*34,0x3f800000);
 assert(!remesa_matrix_draw_values(v,96,3,32,source+4,144));compare(v,source);
 memcpy(saved_v,v,sizeof v);memcpy(saved_source,source,sizeof source);
 const uint32_t boundaries[]={0,0x80000000,1,0x80000001,0x007fffff,0x00800000,
  0x3d7fffff,0x3d800000,0x3d800001,0x3effffff,0x3f000000,0x3f000001,
  0x3f7fffff,0x3f800000,0x3f800001,0x40000000,0x407fffff,0x40800000,
  0x40800001,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fc00000,0x7f800001};
 uint32_t random=0x535121;
 for(unsigned word=0;word<60;word++){
  unsigned char *at=word<36?source+4+4*word:v+4*(word-36);
  uint32_t initial=remesa_ta_ref_word(at);
  for(unsigned bit=0;bit<32;bit++){put(at,initial^(1u<<bit));compare(v,source);}
  for(unsigned i=0;i<sizeof boundaries/sizeof boundaries[0];i++){
   put(at,boundaries[i]);compare(v,source);put(at,boundaries[i]^0x80000000u);compare(v,source);
  }
  for(unsigned i=0;i<128;i++){random^=random<<13;random^=random>>17;random^=random<<5;put(at,random);compare(v,source);}
  put(at,initial);
 }
 assert(!memcmp(v,saved_v,sizeof v)&&!memcmp(source,saved_source,sizeof source));
 unsigned char packed[144]={0};for(unsigned i=0;i<3;i++)memcpy(packed+48*i,v+32*i,32);
 for(unsigned vertex=0;vertex<3;vertex++)for(unsigned byte=32;byte<48;byte++)for(unsigned bit=0;bit<8;bit++){
  packed[48*vertex+byte]^=1u<<bit;
  assert(remesa_matrix_draw_values(packed,144,3,48,source+4,144)==EOPNOTSUPP);packed[48*vertex+byte]^=1u<<bit;
 }
 assert(remesa_matrix_draw_values(NULL,96,3,32,source+4,144)==EINVAL);
 assert(remesa_matrix_draw_values(v,95,3,32,source+4,144)==EINVAL);
 assert(remesa_matrix_draw_values(v,96,3,32,source+4,143)==EINVAL);
 assert(remesa_matrix_draw_values(v,96,3,32,NULL,144)==EINVAL);
 assert(remesa_matrix_draw_values((void *)(UINTPTR_MAX-1),96,3,32,source+4,144)==EINVAL);
 assert(remesa_matrix_draw_values(v,96,3,32,(void *)(UINTPTR_MAX-1),144)==EINVAL);
 assert(remesa_matrix_draw_values(v,96,0,32,source+4,144)==EOPNOTSUPP);
 assert(remesa_matrix_draw_values(v,96,65536,32,source+4,144)==EOPNOTSUPP);
 assert(remesa_matrix_draw_values(v,96,3,44,source+4,144)==EOPNOTSUPP);
 unsigned char *maximum=calloc(65535,48);assert(maximum);
 for(unsigned i=0;i<65535;i++)memcpy(maximum+48*i,packed,48);
 assert(!remesa_matrix_draw_values(maximum,65535u*48,65535,48,source+4,144));
 put(maximum+65534u*48+12,0);assert(remesa_matrix_draw_values(maximum,65535u*48,65535,48,source+4,144)==EOPNOTSUPP);
 free(maximum);
 printf("PASS %u historical-float/IEEE integer equivalence cases x2strides,384 padding bit mutations, boundaries/special values/count/span; GPU0\n",cases);
 return 0;
}
