/* SPDX-License-Identifier: MIT. Independent CPU bounds, never a renderer. */
#include "remesa/matrix_draw_domain.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static unsigned random_state=0x535121;
static unsigned next(void)
{random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state;}
static float unit(void)
{float value=(next()&65535)/65535.f;return next()&1?-value:value;}
int main(void)
{
 float c[36]={0},v[3][8]={{-.5f,-.5f,0,1,0,0,1,1},
                         {.5f,-.5f,0,1,0,0,1,1},{-.5f,.5f,0,1,0,0,1,1}};
 unsigned accepted=0,rejected=0;
 c[17]=1;c[20]=-1;c[26]=c[31]=c[34]=1;c[28]=.25f;
 assert(!remesa_matrix_draw_values(v,sizeof v,3,32,c,sizeof c)); /* Exact90. */
 c[16]=c[21]=.70710677f;c[17]=.70710677f;c[20]=-.70710677f;c[28]=.125f;
 assert(!remesa_matrix_draw_values(v,sizeof v,3,32,c,sizeof c)); /*45degrees. */
 for(unsigned i=0;i<100000;i++){
  c[31]=(i&1)?1:2;
  for(unsigned col=0;col<3;col++)for(unsigned row=0;row<3;row++)c[16+4*col+row]=unit();
  for(unsigned row=0;row<3;row++)c[28+row]=unit()*.5f;
  for(unsigned n=0;n<3;n++)for(unsigned k=0;k<3;k++)v[n][k]=unit();
  long double maximum=0;
  for(unsigned n=0;n<3;n++)for(unsigned row=0;row<3;row++){
   long double bound=fabsl((long double)c[28+row]);
   for(unsigned col=0;col<3;col++)bound+=fabsl((long double)c[16+4*col+row])*fabsl((long double)v[n][col]);
   if(bound>maximum)maximum=bound;
  }
  int error=remesa_matrix_draw_values(v,sizeof v,3,32,c,sizeof c);
  if(!error){assert(maximum<=.875L*c[31]);accepted++;}
  else {assert(error==EOPNOTSUPP);rejected++;}
  if(maximum>.875L*c[31])assert(error==EOPNOTSUPP);
  if(maximum<=.8L*c[31])assert(!error);
  unsigned char packed[144]={0};for(unsigned n=0;n<3;n++)memcpy(packed+48*n,v[n],32);
  assert(remesa_matrix_draw_values(packed,sizeof packed,3,48,c,sizeof c)==error);
 }
 assert(accepted>1000&&rejected>1000);
 /* Maximum possible unsigned sum3.5*2^30 must refuse, never wrap/accept. */
 for(unsigned col=0;col<3;col++)for(unsigned row=0;row<3;row++)c[16+4*col+row]=1;
 for(unsigned row=0;row<3;row++)c[28+row]=.5f;
 for(unsigned n=0;n<3;n++)for(unsigned k=0;k<3;k++)v[n][k]=1;
 for(unsigned w=1;w<=2;w++){c[31]=w;assert(remesa_matrix_draw_values(v,sizeof v,3,32,c,sizeof c)==EOPNOTSUPP);}
 for(unsigned i=19;i<28;i+=4){c[i]=.125f;assert(remesa_matrix_draw_values(v,sizeof v,3,32,c,sizeof c)==EOPNOTSUPP);c[i]=0;}
 printf("PASS fullaffinerows:100000longdoublecases,2strides,90/45rotation,%uaccepted/%urefused,maxsum/variableWrefused;GPU0\n",accepted,rejected);
 return 0;
}
