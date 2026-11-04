/* SPDX-License-Identifier: MIT. Independent CPU interval proof; no renderer. */
#include "remesa/matrix_draw_domain.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static uint32_t state=0x535121;
static uint32_t next(void)
{state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static float unit(void)
{return (int)(next()%131073u)/65536.f-1;}
static uint32_t bits(float f)
{uint32_t b;memcpy(&b,&f,4);return b;}
static long double component(const float c[36],const float *v,unsigned row)
{
 long double sum=c[28+row];
 for(unsigned col=0;col<3;col++)sum+=(long double)c[16+4*col+row]*v[col];
 return sum;
}
int main(void)
{
 float v[3][8]={{-.5f,-.5f,-.375f,1,0,0,1,0},
                {.5f,-.5f,-.5f,1,0,0,1,0},{-.5f,.5f,-.625f,1,0,0,1,0}};
 float c[36]={0};unsigned accepted=0,rejected=0;
 c[16]=c[21]=.125f;c[26]=-9.f/7;c[27]=-1;c[30]=-2.f/7;c[34]=1;
 assert(!remesa_matrix_draw_values(v,sizeof v,3,32,c,sizeof c)); /* glFrustum. */
 c[27]=1;assert(remesa_matrix_draw_values(v,sizeof v,3,32,c,sizeof c)==EOPNOTSUPP);
 c[27]=-1;v[0][2]=0;assert(remesa_matrix_draw_values(v,sizeof v,3,32,c,sizeof c)==EOPNOTSUPP);
 v[0][2]=-.125f;assert(remesa_matrix_draw_values(v,sizeof v,3,32,c,sizeof c)==EOPNOTSUPP); /* Near clip. */
 v[0][2]=-.375f;
 /* Every signed IEEE magnitude endpoint, including tiny normal/subnormal inputs. */
 for(unsigned i=0;i<200000;i++){
  uint32_t b=next()%0x40800001u;float f;memcpy(&f,&b,4);
  struct remesa_matrix_q20 q=remesa_matrix_magnitude_q20(b);
  long double x=f;
  assert(q.lower<=q.upper&&q.upper<=4194304);
  assert((long double)q.lower/1048576<=x&&(long double)q.upper/1048576>=x);
  assert(q.upper-q.lower<=1);
  assert(!memcmp(&q,&(struct remesa_matrix_q20){remesa_matrix_magnitude_q20(b|0x80000000U).lower,remesa_matrix_magnitude_q20(b|0x80000000U).upper},sizeof q));
 }
 for(unsigned i=0;i<100000;i++){
  for(unsigned j=16;j<32;j++)c[j]=4*unit();
  c[31]=2*unit();
  /* Dense projective rows; separate cancellation-rich and modest fixtures. */
  if(i&1){
   for(unsigned col=0;col<3;col++)for(unsigned row=0;row<3;row++)c[16+4*col+row]=unit()*.25f;
   for(unsigned row=0;row<3;row++)c[28+row]=unit()*.125f;
   c[19]=unit()*.125f;c[23]=unit()*.125f;c[27]=-.5f;c[31]=1;
  }
  for(unsigned n=0;n<3;n++)for(unsigned k=0;k<3;k++)v[n][k]=unit();
  struct remesa_matrix_q20 q[16];uint32_t cb[36];memcpy(cb,c,sizeof cb);
  for(unsigned j=0;j<16;j++)q[j]=remesa_matrix_magnitude_q20(bits(c[16+j]));
  int safe=1,interior=1;
  for(unsigned n=0;n<3;n++){
   struct remesa_matrix_q20 p[3];for(unsigned j=0;j<3;j++)p[j]=remesa_matrix_magnitude_q20(bits(v[n][j]));
   long double w=component(c,v[n],3);
   if(w<.125L||w>4)safe=0;
   if(w<.14L||w>3.9L)interior=0;
   for(unsigned row=0;row<4;row++){
    long double x=component(c,v[n],row);
    struct remesa_matrix_interval b=remesa_matrix_projective_row(cb,q,p,(const unsigned char *)v[n],row);
    assert((long double)b.lower/1099511627776.L<=x&&(long double)b.upper/1099511627776.L>=x);
    if(row<3){if(fabsl(x)>.875L*w)safe=0;if(fabsl(x)>.8L*w)interior=0;}
   }
  }
  int error=remesa_matrix_draw_values(v,sizeof v,3,32,c,sizeof c);
  if(!error){assert(safe);accepted++;}else {assert(error==EOPNOTSUPP);rejected++;}
  if(!safe)assert(error==EOPNOTSUPP);
  if(interior)assert(!error);
  unsigned char packed[144]={0};for(unsigned n=0;n<3;n++)memcpy(packed+48*n,v[n],32);
  assert(remesa_matrix_draw_values(packed,sizeof packed,3,48,c,sizeof c)==error);
 }
 assert(accepted>1000&&rejected>1000);
 /* Large cancellation is enclosed, never allowed to wrap signed arithmetic. */
 for(unsigned i=16;i<32;i++)c[i]=4;
 for(unsigned n=0;n<3;n++)for(unsigned k=0;k<3;k++)v[n][k]=1;
 assert(remesa_matrix_draw_values(v,sizeof v,3,32,c,sizeof c)==EOPNOTSUPP);
 for(unsigned i=0;i<36;i++){
  float old=c[i];uint32_t nan=0x7fc00000;memcpy(c+i,&nan,4);
  assert(remesa_matrix_draw_values(v,sizeof v,3,32,c,sizeof c)==EOPNOTSUPP);c[i]=old;
 }
 printf("PASS projective:200000IEEEintervals,100000dense/cancellationcases,2strides,%uaccepted/%urefused,positiveW/clipping/overflow/NaN;GPU0\n",accepted,rejected);
 return 0;
}
