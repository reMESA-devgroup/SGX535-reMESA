/* SPDX-License-Identifier: MIT. Descriptor and sealed-input admission; GPU0. */
#include "remesa_fixed_state.h"
#include "pipe/p_state.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static void read_exact(const char *path,void *out,size_t bytes)
{
 FILE *f=fopen(path,"rb");assert(f&&fread(out,1,bytes,f)==bytes&&fgetc(f)==EOF&&!fclose(f));
}
int main(int argc,char **argv)
{
 assert(argc==3);unsigned char pn[96],source[160],pn_saved[96],source_saved[160];
 read_exact(argv[1],pn,sizeof pn);read_exact(argv[2],source,sizeof source);
 memcpy(pn_saved,pn,sizeof pn);memcpy(source_saved,source,sizeof source);
 for(unsigned i=0;i<2;i++)assert(!remesa_admit_homogeneous_w2_inputs(pn,96,source+4,144,i?640:1280,i?640:800));
 unsigned refused=0;
 for(unsigned i=0;i<96+144;i++)for(unsigned bit=0;bit<8;bit++){
  unsigned char *p=i<96?pn+i:source+4+i-96;*p^=1u<<bit;
  assert(remesa_admit_homogeneous_w2_inputs(pn,96,source+4,144,1280,800)==ENOTSUP);
  *p^=1u<<bit;refused++;
 }
 assert(!memcmp(pn,pn_saved,sizeof pn)&&!memcmp(source,source_saved,sizeof source));
 assert(remesa_admit_homogeneous_w2_inputs(NULL,96,source+4,144,1280,800)==EINVAL);
 assert(remesa_admit_homogeneous_w2_inputs(pn,96,NULL,144,1280,800)==EINVAL);
 assert(remesa_admit_homogeneous_w2_inputs((void *)(UINTPTR_MAX-8),96,source+4,144,1280,800)==EINVAL);
 assert(remesa_admit_homogeneous_w2_inputs(pn,96,(void *)(UINTPTR_MAX-8),144,1280,800)==EINVAL);
 assert(remesa_admit_homogeneous_w2_inputs(pn,95,source+4,144,1280,800)==ENOTSUP);
 assert(remesa_admit_homogeneous_w2_inputs(pn,96,source+4,143,1280,800)==ENOTSUP);
 assert(remesa_admit_homogeneous_w2_inputs(pn,96,source+4,144,641,479)==ENOTSUP);
 assert(remesa_admit_homogeneous_w2_inputs(pn,96,source+4,144,1600,768)==ENOTSUP);
 struct pipe_rasterizer_state good={.fill_front=PIPE_POLYGON_MODE_FILL,.fill_back=PIPE_POLYGON_MODE_FILL,
  .half_pixel_center=1,.bottom_edge_rule=1,.front_ccw=1,.depth_clip_near=1,.depth_clip_far=1,.clamp_vertex_color=1};
 assert(!remesa_validate_homogeneous_w2_rasterizer(&good));
 assert(remesa_validate_rasterizer(&good)==ENOTSUP); /* Old default guard intact. */
 assert(remesa_validate_homogeneous_w2_rasterizer(NULL)==EINVAL);
#define BAD(field,value) do{struct pipe_rasterizer_state bad=good;bad.field=value;assert(remesa_validate_homogeneous_w2_rasterizer(&bad)==ENOTSUP);refused++;}while(0)
 BAD(half_pixel_center,0);BAD(bottom_edge_rule,0);BAD(front_ccw,0);BAD(depth_clip_near,0);BAD(depth_clip_far,0);BAD(clamp_vertex_color,0);
 BAD(scissor,1);BAD(cull_face,PIPE_FACE_BACK);BAD(flatshade,1);BAD(light_twoside,1);BAD(clamp_fragment_color,1);
 BAD(fill_front,PIPE_POLYGON_MODE_LINE);BAD(fill_back,PIPE_POLYGON_MODE_LINE);BAD(offset_tri,1);BAD(poly_smooth,1);
 BAD(poly_stipple_enable,1);BAD(multisample,1);BAD(force_persample_interp,1);BAD(conservative_raster_mode,1);
 BAD(rasterizer_discard,1);BAD(tile_raster_order_fixed,1);BAD(subpixel_precision_x,1);BAD(subpixel_precision_y,1);
 BAD(depth_clamp,1);BAD(clip_halfz,1);BAD(unclamped_fragment_depth_values,1);BAD(clip_plane_enable,1);
#undef BAD
 printf("PASS finite W2 raw input/descriptor contracts; %u mutations refused, default guard and input bytes unchanged; GPU0\n",refused);
 return 0;
}
