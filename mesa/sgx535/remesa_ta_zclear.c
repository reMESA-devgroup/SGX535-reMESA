/* SPDX-License-Identifier: MIT */
#include "remesa_ta_zclear.h"
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
static int span(const void *p,size_t n)
{return p&&n&&n-1<=UINTPTR_MAX-(uintptr_t)p;}
static void float_le(uint8_t *p,float f)
{uint32_t bits;memcpy(&bits,&f,4);for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(bits>>(i*8));}
int remesa_ta_zclear_geometry_encode(const struct remesa_ta_zclear_request *request,
 struct remesa_ta_zclear_geometry *out)
{
 _Static_assert(sizeof(float)==4,"original scene clear requires float32");
 if(!span(request,sizeof(*request))||!span(out,sizeof(*out))||
    (uintptr_t)request%_Alignof(struct remesa_ta_zclear_request))return EINVAL;
 uintptr_t a=(uintptr_t)request,b=(uintptr_t)out;
 if(a<=b?b-a<sizeof(*request):a-b<sizeof(*out))return EINVAL;
 const struct remesa_ta_zclear_request r=*request;
 if(!r.color_width||!r.color_height||!r.depth_width||!r.depth_height)return EINVAL;
 if(r.color_width!=r.depth_width||r.color_height!=r.depth_height)return EINVAL;
 if(r.depth_width>2048||r.depth_height>2048)return ENOTSUP;
 if(!isfinite(r.clear_depth)||r.clear_depth<0.0f||r.clear_depth>1.0f)return EINVAL;
 struct remesa_ta_zclear_geometry value;
 /* Original2a89c..2a904 emits positions only. Integer dimensions are exactly
  * representable in this restricted float32 domain; no vertex transform runs. */
 for(unsigned i=0;i<4;i++) {
  const float position[4]={i&1?(float)r.depth_width:0.0f,
   i&2?(float)r.depth_height:0.0f,r.clear_depth,1.0f};
  for(unsigned c=0;c<4;c++)float_le(value.positions+i*16+c*4,position[c]);
 }
 /* Original2a878..2a896 triangle assembly, not shader instructions. */
 const uint16_t indices[6]={0,1,2,3,2,1};
 for(unsigned i=0;i<6;i++) {
  value.indices[i*2]=(uint8_t)indices[i];value.indices[i*2+1]=(uint8_t)(indices[i]>>8);
 }
 memcpy(out,&value,sizeof(value));return 0;
}
