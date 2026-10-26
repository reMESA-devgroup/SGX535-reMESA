/* SPDX-License-Identifier: MIT */
#include "pipe/p_state.h"
#include "remesa_fixed_state.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
 struct pipe_viewport_state legacy={.scale={16,16,.5},.translate={16,16,.5}};
 struct pipe_viewport_state canonical=legacy;
 canonical.swizzle_x=PIPE_VIEWPORT_SWIZZLE_POSITIVE_X;
 canonical.swizzle_y=PIPE_VIEWPORT_SWIZZLE_POSITIVE_Y;
 canonical.swizzle_z=PIPE_VIEWPORT_SWIZZLE_POSITIVE_Z;
 canonical.swizzle_w=PIPE_VIEWPORT_SWIZZLE_POSITIVE_W;
 assert(!remesa_viewport_swizzle_is_identity(NULL));
 assert(remesa_viewport_swizzle_is_identity(&legacy));
 assert(remesa_viewport_swizzle_is_identity(&canonical));
 const float triangle[3][4]={{-.5,-.5,0,1},{.5,-.5,0,1},{-.5,.5,0,1}};
 struct remesa_clipped_polygon a,b,saved;
 assert(!remesa_prepare_constant_triangle(triangle,&legacy,32,32,&a));
 assert(!remesa_prepare_constant_triangle(triangle,&canonical,32,32,&b));
 assert(!memcmp(&a,&b,sizeof(a)));saved=b;
 for(unsigned field=0;field<4;field++)for(unsigned value=0;value<8;value++) {
  struct pipe_viewport_state v=canonical;
  switch(field){case 0:v.swizzle_x=value;break;case 1:v.swizzle_y=value;break;case 2:v.swizzle_z=value;break;case 3:v.swizzle_w=value;break;}
  bool identity=value==2*field;
  assert(remesa_viewport_swizzle_is_identity(&v)==identity);
  int error=remesa_prepare_constant_triangle(triangle,&v,32,32,&b);
  if(identity)assert(!error&&!memcmp(&a,&b,sizeof(a)));
  else assert(error==ENOTSUP&&!memcmp(&saved,&b,sizeof(b)));
 }
 puts("PASS canonical/legacy viewport identity +32 component variations");return 0;
}
