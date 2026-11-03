/* SPDX-License-Identifier: MIT. Compare integer predicates with the existing
 * ISP/viewport/scalar/Z-clear producers. CPU only, no GPU admission. */
#include "remesa/matrix_draw_domain.h"
#include "remesa_ta_scene.h"
#include "remesa_ta_zclear.h"
#include <assert.h>
#include <stdio.h>
static void make_state(unsigned width,unsigned height,unsigned char state[60],unsigned char prefix[44],struct remesa_ta_zclear_geometry *geometry)
{
 unsigned char isp[8],fragment[16],viewport[28],scalar[16];size_t n;
 const struct remesa_isp_opaque_triangle_request opaque={true,true,true,REMESA_DEPTH_LESS,15};
 struct remesa_ta_fragment_binding_request fs={.address_window_base=0x20000000,
  .primary={0x20010000,4096,160,60,12,1,0},.secondary={0x20010000,4096,256,4,0,0,0},
  .profile=REMESA_TA_FRAGMENT_INPUT_COLOR_CPU};
 const struct remesa_ta_window_viewport_request vp={.framebuffer_width=width,.framebuffer_height=height,
  .width=width,.height=height,.depth_bits=24,.far_z=1,.path=REMESA_TA_VIEWPORT_MATRIX_WINDOW};
 assert(!remesa_isp_opaque_triangle_data_encode(&opaque,isp));
 assert(!remesa_ta_fragment_binding_encode(&fs,fragment));
 assert(!remesa_ta_window_viewport_encode(&vp,viewport));
 assert(!remesa_ta_scalar_groups_encode(REMESA_TA_SCALAR_POSITION4_PRIMARY_COLOR4,scalar,sizeof scalar,&n)&&n==16);
 memset(state,0,60);for(unsigned i=0;i<4;i++)state[i]=isp[i]|fragment[i]|viewport[i]|scalar[i];state[1]|=0x10;
 memcpy(state+4,isp+4,4);memcpy(state+8,fragment+4,12);memcpy(state+20,viewport+4,24);memcpy(state+44,scalar+4,12);
 fs.primary.object_bytes=56;fs.primary.field10=0;fs.profile=REMESA_TA_FRAGMENT_SCENE_ZCLEAR;
 assert(!remesa_ta_scene_zclear_state_encode(&fs,prefix));
 const struct remesa_ta_zclear_request clear={width,height,width,height,1};
 assert(!remesa_ta_zclear_single_geometry_encode(&clear,geometry));
}
int main(void)
{
 unsigned char state[60],prefix[44];struct remesa_ta_zclear_geometry geometry;
 unsigned sizes=0,mutations=0;
 for(unsigned width=1;width<=2048;width++){
  unsigned height=2049-width;make_state(width,height,state,prefix,&geometry);
  assert(!remesa_matrix_draw_state_values(state,60,width,height));
  assert(!remesa_matrix_prefix_values(prefix,44,geometry.positions,48,geometry.indices,6,width,height));sizes++;
 }
 const unsigned dimensions[][2]={{1280,800},{640,640},{641,479},{1600,768}};
 for(unsigned d=0;d<4;d++){
  unsigned w=dimensions[d][0],h=dimensions[d][1];make_state(w,h,state,prefix,&geometry);
  assert(!remesa_matrix_draw_state_values(state,60,w,h));
  for(unsigned byte=0;byte<60;byte++)if(byte<8||byte>=20)for(unsigned bit=0;bit<8;bit++){
   state[byte]^=1u<<bit;assert(remesa_matrix_draw_state_values(state,60,w,h)==EOPNOTSUPP);state[byte]^=1u<<bit;mutations++;
  }
  for(unsigned byte=0;byte<44;byte++)if(byte<12||byte>=24)for(unsigned bit=0;bit<8;bit++){
   prefix[byte]^=1u<<bit;assert(remesa_matrix_prefix_values(prefix,44,geometry.positions,48,geometry.indices,6,w,h)==EOPNOTSUPP);prefix[byte]^=1u<<bit;mutations++;
  }
  for(unsigned byte=0;byte<48;byte++)for(unsigned bit=0;bit<8;bit++){
   geometry.positions[byte]^=1u<<bit;assert(remesa_matrix_prefix_values(prefix,44,geometry.positions,48,geometry.indices,6,w,h)==EOPNOTSUPP);geometry.positions[byte]^=1u<<bit;mutations++;
  }
  for(unsigned byte=0;byte<6;byte++)for(unsigned bit=0;bit<8;bit++){
   geometry.indices[byte]^=1u<<bit;assert(remesa_matrix_prefix_values(prefix,44,geometry.positions,48,geometry.indices,6,w,h)==EOPNOTSUPP);geometry.indices[byte]^=1u<<bit;mutations++;
  }
  /* Unconsumed fourth vertex and last3indices are not part of this check. */
  memset(geometry.positions+48,0xa5,16);memset(geometry.indices+6,0xa5,6);
  assert(!remesa_matrix_prefix_values(prefix,44,geometry.positions,48,geometry.indices,6,w,h));sizes++;
 }
 assert(remesa_matrix_draw_state_values(state,59,1280,800)==EINVAL);
 assert(remesa_matrix_draw_state_values(state,60,0,800)==EOPNOTSUPP);
 assert(remesa_matrix_draw_state_values(state,60,2049,800)==EOPNOTSUPP);
 assert(remesa_matrix_draw_state_values(NULL,60,1280,800)==EINVAL);
 assert(remesa_matrix_prefix_values(prefix,44,geometry.positions,64,geometry.indices,6,1280,800)==EINVAL);
 assert(remesa_matrix_prefix_values(prefix,44,geometry.positions,48,geometry.indices,12,1280,800)==EINVAL);
 printf("PASS %u actual ISP/viewport/scalar/clear producer sizes, %u payload/geometry/index bit mutations, exact consumed spans; GPU0\n",sizes,mutations);
 return 0;
}
