/* SPDX-License-Identifier: MIT */
#include "remesa_ta_zclear.h"
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void reject(const struct remesa_ta_zclear_request *r,int expected)
{
 struct remesa_ta_zclear_geometry b,old;memset(&b,0xa5,sizeof(b));old=b;
 assert(remesa_ta_zclear_geometry_encode(r,&b)==expected);
 assert(!memcmp(&b,&old,sizeof(b)));
}
int main(void)
{
 unsigned char oracle[6][76];assert(fread(oracle,1,sizeof(oracle),stdin)==sizeof(oracle)&&fgetc(stdin)==EOF);
 const unsigned widths[]={32,32,32,64,32,1},heights[]={32,32,32,32,64,1};
 const float depths[]={1,.75,.25,1,0,.5};
 for(unsigned i=0;i<6;i++) {
  const struct remesa_ta_zclear_request r={widths[i],heights[i],widths[i],heights[i],depths[i]};
  struct remesa_ta_zclear_geometry b;
  assert(!remesa_ta_zclear_geometry_encode(&r,&b));assert(!memcmp(&b,oracle[i],sizeof(b)));
 }
 struct remesa_ta_zclear_request r={32,32,32,32,1};
 r.color_width=0;reject(&r,EINVAL);r.color_width=31;reject(&r,EINVAL);r.color_width=32;
 r.depth_height=31;reject(&r,EINVAL);r.depth_height=32;
 r.color_width=r.depth_width=2049;reject(&r,ENOTSUP);r.color_width=r.depth_width=32;
 r.clear_depth=NAN;reject(&r,EINVAL);r.clear_depth=INFINITY;reject(&r,EINVAL);
 r.clear_depth=-.01f;reject(&r,EINVAL);r.clear_depth=1.01f;reject(&r,EINVAL);r.clear_depth=1;
 reject(NULL,EINVAL);struct remesa_ta_zclear_geometry b;
 assert(remesa_ta_zclear_geometry_encode(&r,NULL)==EINVAL);
 assert(remesa_ta_zclear_geometry_encode(&r,(void *)&r)==EINVAL);
 assert(remesa_ta_zclear_geometry_encode((void *)((char *)&r+1),&b)==EINVAL);
 assert(remesa_ta_zclear_geometry_encode(&r,(void *)UINTPTR_MAX)==EINVAL);
 puts("PASS:6 original scene Z-clear POSITION4+index oracles; dimensions/depth/alias/transaction tests; CPU ONLY");return 0;
}
