/* SPDX-License-Identifier: MIT */
#include "remesa_render_target.h"
#include "remesa/scene_control_words.h"
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <float.h>
_Static_assert(sizeof(float)==4&&FLT_RADIX==2&&FLT_MANT_DIG==24&&FLT_MAX_EXP==128,
 "PDS vertex payload requires IEEE binary32");
static void put32(uint8_t *p,uint32_t v)
{for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
int remesa_render_target_encode_linear_bgra8(const struct remesa_render_target_request *request,
 struct remesa_render_target_state *out)
{
 if(!request||!out)return EINVAL;
 const struct remesa_render_target_request r=*request;
 if(!r.width||!r.height||!r.pitch_pixels||r.width>r.pitch_pixels)return EINVAL;
 /* Caller0x27060 asserts pitch&31==0. Restricting pitch to2048 keeps its
  * half-pitch field below the independently produced layout bits at26+. */
 if((r.pitch_pixels&31)||r.pitch_pixels>2048||r.height>2048)return ENOTSUP;
 if(r.gpu_va<UINT64_C(0x40000000)||r.gpu_va>UINT32_MAX||(r.gpu_va&3))return EINVAL;
 if(r.allocation_bytes>UINT64_C(0x100000000)-r.gpu_va)return EOVERFLOW;
 uint32_t stride=r.pitch_pixels*4,bytes=stride*r.height;
 if(r.allocation_bytes<bytes)return ENOSPC;
 struct remesa_render_target_state result={0};
 /* psb_dri ELF0x393c7..393e8, selected format/layout contributions both0.
  * Bit15 is a producer invariant; no architectural meaning is invented. */
 put32(result.target_data,(((r.pitch_pixels>>1)-1)<<16)|UINT32_C(0x8000));
 /* Observed relocation: maskfffffffc, right2/left2. Aligned VA is unchanged. */
 put32(result.target_data+4,(uint32_t)r.gpu_va);
 put32(result.target_data+8,0); /* selected scene origin */
 /* Scene caller passes PITCH as event extent width, not logical width. */
 put32(result.target_data+12,((r.height-1)<<12)|(r.pitch_pixels-1));
 /* Background producer0x39d85..39da4 instead uses logical width high12 and
  * height low12. cpp4 helper contributes0x0c000000, producer ORs0x60000000. */
 result.background_data_extent=UINT32_C(0x6c000000)|((r.width-1)<<12)|(r.height-1);
 /* Distinct consumers: raster0x2ac71 floors; termination0x2a5ca rounds up.
  * They must not share a rounding rule for non-multiple16 dimensions. */
 result.raster_410=((r.width>>4)<<16)|(r.height>>4);
 result.termination_extent=(((r.width+15)>>4)<<16)|((r.height+15)>>4);
 result.stride_bytes=stride;result.image_bytes=bytes;
 memcpy(out,&result,sizeof(result));return 0;
}
int remesa_depth_control_encode(bool depth_bits_present,bool test,bool write,
 enum remesa_depth_compare compare,uint32_t *out)
{
 if(!out||(unsigned)compare>REMESA_DEPTH_ALWAYS)return EINVAL;
 /* Original prefix0x2c0f3..2c1b4. Zero visual depth bits or disabled test forces
  * CPU compare ALWAYS plus write-disable. No other ISP bits are constructed. */
 uint32_t value=REMESA_DEPTH_CONTROL_MASK;
 if(depth_bits_present&&test)value=((uint32_t)compare<<22)|(write?0:UINT32_C(0x00100000));
 *out=value;return 0;
}
int remesa_isp_opaque_triangle_data_encode(
 const struct remesa_isp_opaque_triangle_request *request,uint8_t out[8])
{
 if(!request||!out)return EINVAL;
 const struct remesa_isp_opaque_triangle_request r=*request;
 if(r.rgba_mask>15||(unsigned)r.compare>REMESA_DEPTH_ALWAYS)return EINVAL;
 if((r.rgba_mask!=0&&r.rgba_mask!=15)||
    (r.compare!=REMESA_DEPTH_LESS&&r.compare!=REMESA_DEPTH_ALWAYS))return ENOTSUP;
 uint32_t depth,color;uint8_t bytes[8];
 int e=remesa_depth_control_encode(r.depth_bits_present,r.depth_test,
                                  r.depth_write,r.compare,&depth);if(e)return e;
 e=remesa_isp_opaque_color_mask_encode(r.rgba_mask,&color);if(e)return e;
 /* Original2c28d selects presence1; zero ISPword1 keeps bit1 clear, and
  * disabled stencil keeps bit2 clear. Setter2873a..28760 retains that
  * presence, and serializer2889a..288af writes header then only word0.
  * It does NOT serialize the unused word2 default0x0e000000. */
 put32(bytes,1);put32(bytes+4,depth|color);
 memcpy(out,bytes,sizeof(bytes));return 0;
}
int remesa_ta_state_reference_encode(uint64_t gpu_va,uint32_t state_data_dwords,
 bool state_header_bit13,uint8_t out[8])
{
 if(!out||!gpu_va||(gpu_va&15)||gpu_va>UINT32_MAX)return EINVAL;
 if(state_data_dwords<2)return EINVAL;
 if(state_data_dwords>24)return ENOTSUP;
 uint8_t bytes[8];
 /*3b755..3b761 selects background bit29 from state-header bit13.
  *3b7ac..3b80a supplies the base, mask and right4 relocation. */
 uint32_t background=UINT32_C(0x40000000)|
  (state_header_bit13?UINT32_C(0x20000000):0);
 put32(bytes,((uint32_t)(gpu_va>>4)&UINT32_C(0x0fffffff))|background);
 /*3b812..3b838: the state-upload producer fixes field0c=12 at28e60.
  *Field10 is separately committed ISP/MTE DATA bytes/4 (28e67..28e6d).
  *The opaque0x2a200 producer invariant is not named as hardware ISA. */
 put32(bytes+4,(12u<<24)|UINT32_C(0x0002a200)|
                ((state_data_dwords*4u+15u)>>4));
 memcpy(out,bytes,sizeof(bytes));return 0;
}
int remesa_ta_parameter_reference_encode(uint64_t gpu_va,
 uint32_t coefficient_dwords,uint8_t out[8])
{
 if(!out||!gpu_va||(gpu_va&15u)||gpu_va>UINT32_MAX)return EINVAL;
 if(coefficient_dwords<2u)return EINVAL;
 if(coefficient_dwords>128u)return ENOTSUP;
 uint8_t bytes[8];
 /*3b660..3b6ba: mask0fffffff, source right4, background40000000.
  *3b6bf..3b6e5: descriptor[3]=12 from29b7d, descriptor[4] is
  * committed coefficient byte count/4, rounded to a16-byte unit. */
 put32(bytes,((uint32_t)(gpu_va>>4)&UINT32_C(0x0fffffff))|
  UINT32_C(0x40000000));
 put32(bytes+4,(12u<<24)|UINT32_C(0x0001e100)|
  ((coefficient_dwords*4u+15u)>>4));
 memcpy(out,bytes,sizeof(bytes));return 0;
}
int remesa_isp_opaque_color_mask_encode(uint32_t rgba_mask,uint32_t *out)
{
 const struct remesa_isp_color_control_request r={.rgba_mask=rgba_mask};
 return remesa_isp_color_control_encode(&r,out);
}
int remesa_isp_color_control_encode(const struct remesa_isp_color_control_request *request,
 uint32_t *out)
{
 if(!request||!out)return EINVAL;
 const struct remesa_isp_color_control_request r=*request;
 if(r.rgba_mask>15)return EINVAL;
 bool partial=r.color_logic_op_enabled||r.blend_enabled||r.rgba_mask!=15;
 uint32_t result;
 /* Preserve original branch priority. In particular, a depth-writing shader
  * bypasses the no-color contribution even when all color channels are off. */
 if(r.fragment_depth_written)result=UINT32_C(0x0a000000);
 else if(r.fragment_uses_kill||r.alpha_test_enabled)
  result=partial?UINT32_C(0x04000000):UINT32_C(0x08000000);
 else {
  result=partial?UINT32_C(0x02000000):0;
  if(!r.rgba_mask&&!r.context_13c84)result|=UINT32_C(0x00200000);
 }
 *out=result;return 0;
}
int remesa_depth_surface_encode(const struct remesa_depth_surface_request *request,
 struct remesa_depth_surface_state *out)
{
 if(!out)return EINVAL;
 struct remesa_depth_surface_state result={.control_480=UINT32_C(0x000f0000)};
 if(!request){memcpy(out,&result,sizeof(result));return 0;}
 const struct remesa_depth_surface_request r=*request;
 if(!r.width||!r.height||r.width>r.pitch_pixels)return EINVAL;
 if((r.cpp!=2&&r.cpp!=4)||(r.pitch_pixels&31)||r.pitch_pixels>2048||r.height>2048)
  return ENOTSUP;
 if((unsigned)r.status>REMESA_DEPTH_SURFACE_DEFINED)return ENOTSUP;
 if(r.gpu_va<UINT64_C(0x40000000)||r.gpu_va>UINT32_MAX||(r.gpu_va&15))return EINVAL;
 if(r.allocation_bytes>UINT64_C(0x100000000)-r.gpu_va)return EOVERFLOW;
 result.minimum_backing_bytes=r.pitch_pixels*((r.height+31)&~31u)*r.cpp;
 if(r.allocation_bytes<result.minimum_backing_bytes)return ENOSPC;
 /* CPU prefix0x2ac04/0x2b12c. DEFINED is established by setter4b7b1,
  * independently named by4b76f. Do not call this a pending-clear flag. */
 result.control_480=(((r.pitch_pixels>>4)-1)<<8)|
  (r.cpp==4?UINT32_C(0x00050000):UINT32_C(0x000a0000));
 if(r.status==REMESA_DEPTH_SURFACE_DEFINED)result.control_480|=REMESA_SCENE_DEPTH_DEFINED_BITS;
 result.address_count=5;
 for(unsigned i=0;i<5;i++) {
  result.addresses[i].reg=i?UINT32_C(0x480)+i*4:UINT32_C(0xcb0);
  result.addresses[i].address_mask=i?UINT32_C(0x0ffffff0):UINT32_C(0xf0000000);
  result.addresses[i].word=(uint32_t)r.gpu_va&result.addresses[i].address_mask;
 }
 memcpy(out,&result,sizeof(result));return 0;
}
static int depth_surface_state_valid(const struct remesa_depth_surface_state *state)
{
 if(!state->address_count) {
  if(state->control_480!=UINT32_C(0x000f0000)||state->minimum_backing_bytes)return 0;
  for(unsigned i=0;i<5;i++)
   if(state->addresses[i].reg||state->addresses[i].address_mask||state->addresses[i].word)return 0;
  return 1;
 }
 if(state->address_count!=5)return 0;
 uint32_t control=state->control_480,format=control&UINT32_C(0x000f0000);
 if(format!=UINT32_C(0x00050000)&&format!=UINT32_C(0x000a0000))return 0;
 uint32_t pitch_field=(control>>8)&127;
 if(!(pitch_field&1))return 0; /* encoder requires pitch multiple32 */
 uint32_t canonical=format|(pitch_field<<8);
 if(control&UINT32_C(0x00200000))canonical|=UINT32_C(0x00200026);
 if(control&UINT32_C(0x00400000))canonical|=UINT32_C(0x004000d0);
 if(control!=canonical)return 0;
 uint32_t row_group=(pitch_field+1)*16*32*(format==UINT32_C(0x00050000)?4:2);
 if(!state->minimum_backing_bytes||state->minimum_backing_bytes%row_group||
    state->minimum_backing_bytes/row_group>64)return 0;
 uint32_t low=state->addresses[1].word;
 for(unsigned i=0;i<5;i++) {
  uint32_t mask=i?UINT32_C(0x0ffffff0):UINT32_C(0xf0000000);
  if(state->addresses[i].reg!=(i?UINT32_C(0x480)+i*4:UINT32_C(0xcb0))||
     state->addresses[i].address_mask!=mask||(state->addresses[i].word&~mask)||
     (i&&state->addresses[i].word!=low))return 0;
 }
 uint32_t address=state->addresses[0].word|low;
 if(address<UINT32_C(0x40000000)||
    (uint64_t)state->minimum_backing_bytes>UINT64_C(0x100000000)-address)return 0;
 return 1;
}
int remesa_depth_surface_finalize(const struct remesa_depth_surface_state *state,
 bool caller_flag,bool context_depth_present,struct remesa_depth_surface_state *out)
{
 if(!state||!out)return EINVAL;
 /* The prefix has either no backing, or all five split-mask contributions.
  * Never mutate a partially built record. The underlying register semantics
  * remain unknown; the two observed predicates are kept independent. */
 if(!depth_surface_state_valid(state))return EINVAL;
 if(context_depth_present&&!state->address_count)return ENODATA;
 struct remesa_depth_surface_state result=*state;
 result.control_480=remesa_scene_depth_finalize_word(result.control_480,caller_flag,context_depth_present);
 memcpy(out,&result,sizeof(result));return 0;
}
int remesa_depth_clear_geometry_encode(const struct remesa_depth_clear_request *request,
 struct remesa_depth_clear_geometry *out)
{
 if(!request||!out)return EINVAL;
 const struct remesa_depth_clear_request r=*request;
 if(!r.width||!r.height||r.x0>=r.x1||r.y0>=r.y1||r.x1>r.width||r.y1>r.height)
  return EINVAL;
 /* Same deliberately bounded surface profile as the existing depth prefix;
  * integer window coordinates are exactly representable in float32. */
 if(r.width>2048||r.height>2048)return ENOTSUP;
 if(!isfinite(r.depth)||r.depth<0||r.depth>1)return EINVAL;
 for(unsigned i=0;i<4;i++)if(!isfinite(r.color[i]))return EINVAL;
 struct remesa_depth_clear_geometry result={0};
 const uint32_t x[4]={r.x0,r.x1,r.x1,r.x0},y[4]={r.y0,r.y0,r.y1,r.y1};
 for(unsigned v=0;v<4;v++) {
  const float values[8]={(float)x[v],(float)y[v],r.depth,1,
                       r.color[0],r.color[1],r.color[2],r.color[3]};
  for(unsigned i=0;i<8;i++) {
   uint32_t bits;memcpy(&bits,&values[i],sizeof(bits));
   put32(result.vertices+v*32+i*4,bits);
  }
 }
 /* Expand an ordered four-corner fan without importing an index template.
  * The payload is ordinary independent vertex/index data, never shader CODE. */
 for(unsigned triangle=0;triangle<2;triangle++) {
  const unsigned vertex[3]={0,triangle+1,triangle+2};
  for(unsigned j=0;j<3;j++) {
   unsigned offset=(triangle*3+j)*2;
   result.indices[offset]=(uint8_t)vertex[j];
   result.indices[offset+1]=(uint8_t)(vertex[j]>>8);
  }
 }
 int error=remesa_depth_control_encode(true,true,true,REMESA_DEPTH_ALWAYS,&result.depth_control);
 if(error)return error;
 memcpy(out,&result,sizeof(result));return 0;
}
int remesa_scene_layout_encode(uint32_t width,uint32_t height,uint32_t option64,
 struct remesa_scene_layout *out)
{return remesa_scene_layout_build(width,height,option64,out);}

static int depth_prefix_span(const void *p,size_t bytes)
{return p&&bytes&&bytes-1<=UINTPTR_MAX-(uintptr_t)p;}
int remesa_depth_prefix_encode(const struct remesa_depth_prefix_request *request,
 uint8_t out[REMESA_DEPTH_PREFIX_BYTES])
{
 if(!depth_prefix_span(request,sizeof(*request))||
    !depth_prefix_span(out,REMESA_DEPTH_PREFIX_BYTES)||
    (uintptr_t)request%_Alignof(struct remesa_depth_prefix_request))return EINVAL;
 uintptr_t a=(uintptr_t)request,b=(uintptr_t)out;
 if(a<=b?b-a<sizeof(*request):a-b<REMESA_DEPTH_PREFIX_BYTES)return EINVAL;
 const struct remesa_depth_prefix_request r=*request;
 if(!r.color_width||!r.color_height||r.color_width!=r.depth.width||
    r.color_height!=r.depth.height)return EINVAL;
 if(!isfinite(r.clear_depth)||r.clear_depth<0.0f||r.clear_depth>1.0f)return EINVAL;
 if(r.clear_stencil>255u)return ERANGE;
 struct remesa_depth_surface_state state,finalized;
 int e=remesa_depth_surface_encode(&r.depth,&state);if(e)return e;
 e=remesa_depth_surface_finalize(&state,r.caller_finalize_flag,true,&finalized);
 if(e)return e;
 uint8_t bytes[REMESA_DEPTH_PREFIX_BYTES];
 put32(bytes,0x480);put32(bytes+4,finalized.control_480);
 for(unsigned i=0;i<5;i++) {
  put32(bytes+8+i*8,finalized.addresses[i].reg);
  put32(bytes+12+i*8,finalized.addresses[i].word);
 }
 /* Original2ab70's param5 is one byte.2b01b..2b028 combine it with0x200
  * for4bc;2b02b..2b035 copy param4float bits to4b8. */
 put32(bytes+48,0x4bc);put32(bytes+52,UINT32_C(0x200)|r.clear_stencil);
 uint32_t clear_bits;memcpy(&clear_bits,&r.clear_depth,sizeof(clear_bits));
 put32(bytes+56,0x4b8);put32(bytes+60,clear_bits);
 memcpy(out,bytes,sizeof(bytes));return 0;
}

int remesa_background_geometry_encode(const struct remesa_background_geometry_request *request,
 uint8_t out[24])
{
 if(!depth_prefix_span(request,sizeof(*request))||!depth_prefix_span(out,24)||
    (uintptr_t)request%_Alignof(struct remesa_background_geometry_request))return EINVAL;
 uintptr_t a=(uintptr_t)request,b=(uintptr_t)out;
 if(a<=b?b-a<sizeof(*request):a-b<24)return EINVAL;
 const struct remesa_background_geometry_request r=*request;
 if(!r.width||!r.height)return EINVAL;
 if(r.width>2048||r.height>2048)return ENOTSUP;
 if(!isfinite(r.depth)||r.depth<0||r.depth>1)return EINVAL;
 uint8_t result[24];uint32_t depth;memcpy(&depth,&r.depth,4);
 /* Original ELF2ef9b..2f020 adds1024, then shifts4/20 and combines.
  * The caller at2f4a0..2f4ad selects the dimensions in this exact order. */
 const uint32_t bias=1024u;
 const uint32_t packed[3]={ (bias<<20)|(bias<<4),
  ((bias+r.width)<<20)|(bias<<4),(bias<<20)|((bias+r.height)<<4) };
 for(unsigned i=0;i<3;i++){put32(result+8*i,packed[i]);put32(result+8*i+4,depth);}
 memcpy(out,result,24);return 0;
}

static int raster_reference(const struct remesa_raster_object_reference *r,
 uint64_t low,uint64_t high,uint64_t minimum,uint32_t *address)
{
 if(r->bo_gpu_va<low||r->bo_gpu_va>=high||!r->bo_bytes||
    r->bo_bytes>high-r->bo_gpu_va||r->byte_offset>r->bo_bytes||
    r->object_bytes<minimum||r->object_bytes>r->bo_bytes-r->byte_offset)return EINVAL;
 uint64_t va=r->bo_gpu_va+r->byte_offset;
 if(va&15)return EINVAL;
 *address=(uint32_t)va;return 0;
}
int remesa_raster_control_encode(const struct remesa_raster_control_request *request,
 uint8_t out[REMESA_RASTER_CONTROL_BYTES])
{
 if(!request||!out)return EINVAL;
 uintptr_t a=(uintptr_t)request,b=(uintptr_t)out;
 if(a>UINTPTR_MAX-sizeof(*request)||b>UINTPTR_MAX-REMESA_RASTER_CONTROL_BYTES||
    (a<b+REMESA_RASTER_CONTROL_BYTES&&b<a+sizeof(*request)))return EINVAL;
 const struct remesa_raster_control_request r=*request;
 /* Original2a39a finalizes only a present depth attachment. This opt-in
  * changes command DATA, not the resource's initialized/DEFINED status. */
 if(r.event_field0c!=16||(r.depth.caller_finalize_flag&&!r.has_depth))return ENOTSUP;
 struct remesa_render_target_state color;
 int e=remesa_render_target_encode_linear_bgra8(&r.color,&color);if(e)return e;
 if(r.depth.color_width!=r.color.width||r.depth.color_height!=r.color.height||
    r.depth.clear_stencil>255||!isfinite(r.depth.clear_depth)||
    r.depth.clear_depth<0||r.depth.clear_depth>1)return EINVAL;
 uint32_t background,event;
 e=raster_reference(&r.background,UINT64_C(0x30000000),UINT64_C(0x40000000),68,&background);if(e)return e;
 e=raster_reference(&r.event,UINT64_C(0x20000000),UINT64_C(0x30000000),4,&event);if(e)return e;
 uint8_t depth[REMESA_DEPTH_PREFIX_BYTES]={0};
 if(r.has_depth){e=remesa_depth_prefix_encode(&r.depth,depth);if(e)return e;}
 uint32_t clear_bits;memcpy(&clear_bits,&r.depth.clear_depth,sizeof(clear_bits));
 /* Constants are the observed CPU register-control writes of2ac41..2b106,
  * not shader instructions or an inferred architectural naming of bits. */
 uint32_t depth_values[6]={0xf0000,0,0,0,0,0},words[52];
 if(r.has_depth)for(unsigned i=0;i<6;i++) {
  const uint8_t *v=depth+i*8+4;
  depth_values[i]=(uint32_t)v[0]|((uint32_t)v[1]<<8)|((uint32_t)v[2]<<16)|((uint32_t)v[3]<<24);
 }
 remesa_scene_raster_words(color.raster_410,background,event,r.event_field0c,
  depth_values,clear_bits,r.depth.clear_stencil,words);
 uint8_t bytes[REMESA_RASTER_CONTROL_BYTES];
 for(unsigned i=0;i<52;i++)put32(bytes+i*4,words[i]);
 memcpy(out,bytes,sizeof(bytes));return 0;
}

int remesa_ta_control_encode(const struct remesa_raster_object_reference *request,
 uint8_t out[REMESA_TA_CONTROL_BYTES])
{
 if(!request||!out)return EINVAL;
 uintptr_t a=(uintptr_t)request,b=(uintptr_t)out;
 if(a>UINTPTR_MAX-sizeof(*request)||b>UINTPTR_MAX-REMESA_TA_CONTROL_BYTES||
    (a<b+REMESA_TA_CONTROL_BYTES&&b<a+sizeof(*request)))return EINVAL;
 const struct remesa_raster_object_reference r=*request;
 if(!r.bo_gpu_va||r.bo_gpu_va>UINT32_MAX||!r.bo_bytes||
    r.bo_bytes>UINT64_C(0x100000000)-r.bo_gpu_va||r.byte_offset>r.bo_bytes||
    r.object_bytes<4||r.object_bytes>r.bo_bytes-r.byte_offset)return EINVAL;
 uint64_t va=r.bo_gpu_va+r.byte_offset;if(va&3)return EINVAL;
 /* Original CPU writes2b2b5..2b30f and full32-bit relocation2b312..2b36b.
  * These are interface controls, not GPU shader instruction templates. */
 uint32_t words[14];remesa_scene_ta_words((uint32_t)va,words);
 uint8_t bytes[REMESA_TA_CONTROL_BYTES];
 for(unsigned i=0;i<14;i++)put32(bytes+i*4,words[i]);
 memcpy(out,bytes,sizeof(bytes));return 0;
}
