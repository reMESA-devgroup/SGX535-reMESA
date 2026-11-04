/* SPDX-License-Identifier: MIT. CPU bounds only; no rasterization or GPU. */
#include "remesa/matrix_draw_domain.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static uint32_t random_state=0x535121;
static uint32_t next(void)
{
 random_state^=random_state<<13;random_state^=random_state>>17;
 random_state^=random_state<<5;return random_state;
}
static float number(uint32_t bits)
{float value;memcpy(&value,&bits,4);return value;}
static void put(float *at,uint32_t bits)
{memcpy(at,&bits,4);}

int main(void)
{
 float v[3][8]={{-.5f,-.5f,0,1,0,0,1,1},
                {.5f,-.5f,0,1,0,0,1,1},{-.5f,.5f,0,1,0,0,1,1}};
 float c[36]={0};unsigned accepted=0,rejected=0;
 c[16]=c[21]=c[26]=c[31]=c[34]=1;c[28]=.25f;c[29]=1.f/256;
 assert(!remesa_matrix_draw_values(v,sizeof v,3,32,c,sizeof c));
 v[0][0]=.625f;assert(!remesa_matrix_draw_values(v,sizeof v,3,32,c,sizeof c));
 put(&v[0][0],0x3f200001);
 assert(remesa_matrix_draw_values(v,sizeof v,3,32,c,sizeof c)==EOPNOTSUPP);
 v[0][0]=1;c[31]=2;
 assert(!remesa_matrix_draw_values(v,sizeof v,3,32,c,sizeof c)); /* Original W2. */
 c[31]=1;
 assert(remesa_matrix_draw_values(v,sizeof v,3,32,c,sizeof c)==EOPNOTSUPP);
 for(unsigned i=0;i<100000;i++){
  uint32_t bits=next()%0x3f800001u;
  long double actual=fabsl((long double)number(bits));
  uint32_t upper=remesa_matrix_unit_upper_q15(bits);
  assert(upper<=32768 && (long double)upper/32768>=actual);
  /* Subtracting a tiny subnormal can round back to one Q15 unit even in
   * long double. The independent upper-bound assertion above stays exact. */
  assert((long double)upper/32768-actual<=1.L/32768);
  c[16]=(next()&65535)/65535.f;
  v[0][0]=(next()&65535)/65535.f;
  c[28]=(next()&65535)/131070.f;
  /* Other vertices/axes are harmless. Test signs independently: the
   * magnitude bound deliberately refuses cancellation-dependent clipping. */
  v[1][0]=v[2][0]=0;
  if(next()&1)c[16]=-c[16];
  if(next()&1)v[0][0]=-v[0][0];
  if(next()&1)c[28]=-c[28];
  long double bound=fabsl((long double)c[16])*fabsl((long double)v[0][0])+
                    fabsl((long double)c[28]);
  int error=remesa_matrix_draw_values(v,sizeof v,3,32,c,sizeof c);
  if(!error){assert(bound<=.875L);accepted++;}
  else {assert(error==EOPNOTSUPP);rejected++;}
  if(bound>.875L)assert(error==EOPNOTSUPP);
  if(bound<=.8L)assert(!error);
  unsigned char packed[144]={0};
  for(unsigned j=0;j<3;j++)memcpy(packed+48*j,v[j],32);
  assert(remesa_matrix_draw_values(packed,sizeof packed,3,48,c,sizeof c)==error);
 }
 assert(accepted>1000&&rejected>1000);
 c[16]=c[21]=c[26]=1;c[28]=c[29]=c[30]=0;
 for(unsigned j=0;j<3;j++)v[j][0]=0;
 for(unsigned i=0;i<36;i++){
  uint32_t old;memcpy(&old,&c[i],4);put(&c[i],0x7fc00000);
  assert(remesa_matrix_draw_values(v,sizeof v,3,32,c,sizeof c)==EOPNOTSUPP);
  put(&c[i],old);
 }
 printf("PASS W1 outward bounds:100000 independent long-double cases,2strides,%u accepted/%u refused,edge/W2/NaN; GPU0\n",accepted,rejected);
 return 0;
}
