/* SPDX-License-Identifier: MIT */
/* Integration of storage and existing descriptor encoders, CPU only. Literal
 * Vaio words below are arithmetic contract checks, NOT new original-library
 * captures or evidence of physical rendering at 1600x768. */
#include "remesa_resource_layout.h"
#include "remesa_render_target.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t word(const uint8_t *p)
{return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int main(void)
{
 const unsigned sizes[][2]={{1280,800},{640,640},{641,479},{1600,768}};
 const struct remesa_resource_limits limits={.max_width=2047,.max_height=2047,
  .row_alignment=128,.max_resource_bytes=16u*1024*1024};
 for(unsigned i=0;i<4;i++){
  unsigned w=sizes[i][0],h=sizes[i][1];struct remesa_drawable_layout layout;
  assert(!remesa_drawable_layout(&limits,w,h,true,4,32u*1024*1024,&layout));
  unsigned pitch=layout.planes[0].stride/4;
  assert(layout.count==3&&!(pitch&31)&&layout.planes[0].bytes==(size_t)pitch*4*h);
  assert(layout.total_bytes==3*layout.allocation_bytes[0]);
  struct remesa_drawable_layout unchanged=layout;
  assert(remesa_drawable_layout(&limits,w,h,true,4,layout.total_bytes-1,&layout)==ENOSPC);
  assert(!memcmp(&layout,&unchanged,sizeof(layout)));
  struct remesa_render_target_request cr={w,h,pitch,UINT64_C(0x40001000),layout.allocation_bytes[0]};
  struct remesa_render_target_state color;assert(!remesa_render_target_encode_linear_bgra8(&cr,&color));
  struct remesa_depth_surface_request zr={w,h,pitch,4,REMESA_DEPTH_SURFACE_DEFINED,
   UINT64_C(0x41001000),layout.allocation_bytes[2]};
  struct remesa_depth_surface_state depth;assert(!remesa_depth_surface_encode(&zr,&depth));
  assert(depth.minimum_backing_bytes<=zr.allocation_bytes);
  struct remesa_scene_layout scene;assert(!remesa_scene_layout_encode(w,h,0,&scene));
  assert(scene.required_bytes<=scene.allocation_pages*4096);
  struct remesa_background_geometry_request bg={w,h,1};uint8_t bytes[24];
  assert(!remesa_background_geometry_encode(&bg,bytes));
  if(w==1600){
   assert(pitch==1600&&color.stride_bytes==6400&&color.image_bytes==4915200);
   assert(layout.planes[0].pages==1200&&layout.total_bytes==14745600);
   assert(word(color.target_data)==0x031f8000&&word(color.target_data+12)==0x002ff63f);
   assert(color.background_data_extent==0x6c63f2ff);
   assert(color.raster_410==0x00640030&&color.termination_extent==0x00640030);
   assert(depth.control_480==0x00256326&&depth.minimum_backing_bytes==4915200);
   assert(scene.required_bytes==125728&&scene.allocation_pages==31&&scene.clear_page_count==16);
   struct remesa_render_target_state saved=color;cr.allocation_bytes=4915199;
   assert(remesa_render_target_encode_linear_bgra8(&cr,&color)==ENOSPC&&!memcmp(&saved,&color,sizeof color));
   struct remesa_depth_surface_state ds=depth;zr.allocation_bytes=4915199;
   assert(remesa_depth_surface_encode(&zr,&depth)==ENOSPC&&!memcmp(&ds,&depth,sizeof depth));
  }
  printf("PASS %ux%u storage/color/Z/scene/background; pitch=%u, drawable=%zu bytes; GPU0\n",w,h,pitch,layout.total_bytes);
 }
 return 0;
}
