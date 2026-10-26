/* SPDX-License-Identifier: MIT */
#include "remesa_ta_scene.h"
#include <errno.h>
#include <string.h>
#include <math.h>
static void put32(uint8_t *p,uint32_t x)
{ p[0]=(uint8_t)x;p[1]=(uint8_t)(x>>8);p[2]=(uint8_t)(x>>16);p[3]=(uint8_t)(x>>24); }
static int object_address(const struct remesa_ta_fragment_pds_reference *p,
 uint64_t window,uint64_t required,uint64_t *address)
{
 uint64_t effective,end;
 if (!p->bo_gpu_va || !p->bo_bytes || p->bo_gpu_va>UINT32_MAX ||
     p->bo_bytes>UINT64_C(0x100000000)-p->bo_gpu_va ||
     p->byte_offset>p->bo_bytes || p->object_bytes>p->bo_bytes-p->byte_offset)
  return EINVAL;
 if (p->object_bytes!=required) return ENOTSUP;
 effective=p->bo_gpu_va+p->byte_offset;end=effective+p->object_bytes;
 if ((effective&31) || effective<window || end>window+UINT64_C(0x10000000))
  return EINVAL;
 *address=effective;
 return 0;
}
int remesa_ta_fragment_binding_encode(
 const struct remesa_ta_fragment_binding_request *request,uint8_t out[16])
{
 struct remesa_ta_fragment_binding_request r;
 uint64_t p,s;uint32_t resource_blocks,primary_span,max_batch,max_chunks,control;
 uint8_t result[16];int e;
 if (!request || !out ||
     (uintptr_t)request>UINTPTR_MAX-sizeof(r) ||
     (uintptr_t)out>UINTPTR_MAX-sizeof(result)) return EINVAL;
 memcpy(&r,request,sizeof(r));
 if (r.address_window_base>UINT32_C(0xf0000000) ||
     (r.address_window_base&UINT32_C(0x0fffffff))) return EINVAL;
 if (r.profile!=REMESA_TA_FRAGMENT_AUTHORED_CONSTANT &&
     r.profile!=REMESA_TA_FRAGMENT_SCENE_ZCLEAR&&
     r.profile!=REMESA_TA_FRAGMENT_INPUT_COLOR_CPU)return ENOTSUP;
 if (r.primary.data_dwords!=12 ||
     r.primary.field10!=(r.profile==REMESA_TA_FRAGMENT_INPUT_COLOR_CPU?1u:0u) ||
     r.primary.field14!=(r.profile==REMESA_TA_FRAGMENT_AUTHORED_CONSTANT?1u:0u) ||
     r.secondary.data_dwords!=0 || r.secondary.field10!=0 || r.secondary.field14!=0)
  return ENOTSUP;
 e=object_address(&r.primary,r.address_window_base,
  r.profile==REMESA_TA_FRAGMENT_INPUT_COLOR_CPU?60:56,&p);if(e)return e;
 e=object_address(&r.secondary,r.address_window_base,4,&s);if(e)return e;
 if (p<s+r.secondary.object_bytes && s<p+r.primary.object_bytes) return EINVAL;
 /* Original2892d..28a19 and28a89..28ac9 CPU expressions. Preserve the
  * scheduler's count32 and subsequent field truncation:32 is not31.
  * primary.field10 is compiled-result+bc, NOT its12 DATA DWORDs.
  */
 primary_span=(r.primary.field10+3u)&~3u;
 resource_blocks=(UINT32_C(0x4df)-((r.secondary.field10+31u)&~31u))&UINT32_C(0xffffffa0);
 resource_blocks/=3u;
 max_batch=4u;
 if(r.primary.field14){
  uint32_t n=192u/((r.primary.field14*4u+15u)&~15u);
  if(n<max_batch)max_batch=n;
 }
 if(primary_span && resource_blocks/(primary_span*4u)<max_batch)
  max_batch=resource_blocks/(primary_span*4u);
 if(!max_batch)max_batch=1;
 max_chunks=32u;
 if(primary_span){max_chunks=resource_blocks/(primary_span*4u);if(max_chunks>32)max_chunks=32;if(!max_chunks)max_chunks=1;}
 {uint32_t n=max_chunks+3u;if(n>32)n=32;if(n>max_batch*8u)max_chunks=max_batch*8u-3u;}
 control=(((r.secondary.field10*4u+127u)<<11)&UINT32_C(0x01fc0000))|
  r.primary.field10|((max_batch-1u)<<16)|((max_chunks<<25)&UINT32_C(0x3e000000))|
  (r.secondary.field10?UINT32_C(0x8000):0u);
 put32(result,UINT32_C(0x40));
 put32(result+4,((uint32_t)(s>>4)&UINT32_C(0x00ffffff))|
  ((r.secondary.data_dwords<<24)&UINT32_C(0xfc000000)));
 put32(result+8,control);
 put32(result+12,((uint32_t)(p>>4)&UINT32_C(0x00ffffff))|
  ((r.primary.data_dwords<<24)&UINT32_C(0xfc000000)));
 memcpy(out,result,sizeof(result));return 0;
}

int remesa_background_object_encode(const struct remesa_background_object_request *request,
 uint8_t out[68])
{
 struct remesa_background_object_request r;uint8_t result[68];uint64_t p,s;int e;
 if(!request||!out||(uintptr_t)request>UINTPTR_MAX-sizeof(r)||
    (uintptr_t)out>UINTPTR_MAX-sizeof(result))return EINVAL;
 uintptr_t a=(uintptr_t)request,b=(uintptr_t)out;
 if(a<=b?b-a<sizeof(r):a-b<sizeof(result))return EINVAL;
 memcpy(&r,request,sizeof(r));
 if(r.address_window_base>UINT32_C(0xf0000000)||
    (r.address_window_base&UINT32_C(0x0fffffff)))return EINVAL;
 /* Original39e3a/39e41 set primary metadata12/1;3fb0f..1d set0/0
  * for the empty secondary. field14 is an unconsumed canonical API input. */
 if(r.primary.data_dwords!=12||r.primary.field10!=1||r.primary.field14||
    r.secondary.data_dwords||r.secondary.field10||r.secondary.field14)return ENOTSUP;
 e=object_address(&r.primary,r.address_window_base,64,&p);if(e)return e;
 e=object_address(&r.secondary,r.address_window_base,4,&s);if(e)return e;
 if(p<s+4&&s<p+64)return EINVAL;
 put32(result,((uint32_t)(s>>4)&UINT32_C(0x00ffffff))|
  ((r.secondary.data_dwords<<24)&UINT32_C(0xfc000000)));
 put32(result+4,r.primary.field10|UINT32_C(0x30000)|
  (((r.secondary.field10+127u)<<11)&UINT32_C(0x01fc0000)));
 put32(result+8,((uint32_t)(p>>4)&UINT32_C(0x00ffffff))|
  ((r.primary.data_dwords<<24)&UINT32_C(0xfc000000)));
 /* These are the audited caller's scalar arguments, not shader opcodes. */
 const float coefficients[6]={0,0,1,0,0,1};
 for(unsigned i=0;i<6;i++){uint32_t word;memcpy(&word,&coefficients[i],4);put32(result+12+4*i,word);}
 put32(result+36,2);put32(result+40,0); /* Direct constructor stores. */
 const struct remesa_background_geometry_request geometry={r.width,r.height,r.depth};
 e=remesa_background_geometry_encode(&geometry,result+44);if(e)return e;
 memcpy(out,result,sizeof(result));return 0;
}

static void put_float32(uint8_t *p,float value)
{
 uint32_t word;memcpy(&word,&value,sizeof(word));put32(p,word);
}
int remesa_ta_window_viewport_encode(
 const struct remesa_ta_window_viewport_request *request,uint8_t out[28])
{
 struct remesa_ta_window_viewport_request r;uint8_t result[28];
 double near_units,far_units,delta,zmax;float reciprocal,matrix_zoffset,matrix_zscale;
 if(!request || !out || (uintptr_t)request>UINTPTR_MAX-sizeof(r) ||
    (uintptr_t)out>UINTPTR_MAX-sizeof(result))return EINVAL;
 memcpy(&r,request,sizeof(r));
 if(!r.framebuffer_width || !r.framebuffer_height ||
    r.framebuffer_width>2048 || r.framebuffer_height>2048 ||
    !r.width || !r.height || r.x>r.framebuffer_width || r.y>r.framebuffer_height ||
    r.width>r.framebuffer_width-r.x || r.height>r.framebuffer_height-r.y ||
    !isfinite(r.near_z) || !isfinite(r.far_z) ||
    r.near_z<0.0f || r.near_z>1.0f || r.far_z<0.0f || r.far_z>1.0f)
  return EINVAL;
 if(r.depth_bits!=16 && r.depth_bits!=24)return ENOTSUP;
 if(r.path!=REMESA_TA_VIEWPORT_MATRIX_WINDOW &&
    r.path!=REMESA_TA_VIEWPORT_INITIAL_ZERO)return ENOTSUP;
 near_units=(double)r.near_z*65536.0;far_units=(double)r.far_z*65536.0;
 if(near_units!=(double)(uint32_t)near_units || far_units!=(double)(uint32_t)far_units)
  return ENOTSUP;
 if(r.path==REMESA_TA_VIEWPORT_INITIAL_ZERO){
  /* Original initializer zeroes these six fields; guard-disabled2be59 leaves
   * them unchanged. Host emission does not name the predicate's GPU meaning. */
  memset(result,0,sizeof(result));put32(result,UINT32_C(0x100));
  memcpy(out,result,sizeof(result));return 0;
 }
 /* Public c8adf stores matrix Z components asfloat32. Original2be59 then
  * stores the reciprocal DepthMaxF tofloat32 BEFORE multiplying them.
  * The dyadic input restriction bounds these products to <=49 significant
  * bits: double represents the original x87 intermediate exactly here.
  */
 zmax=(double)((UINT32_C(1)<<r.depth_bits)-1u);
 reciprocal=(float)(1.0/zmax);
 delta=(double)r.far_z-(double)r.near_z;
 matrix_zscale=(float)(delta*0.5*zmax);
 matrix_zoffset=(float)((delta*0.5+(double)r.near_z)*zmax);
 put32(result,UINT32_C(0x100));
 put_float32(result+4,(float)((double)r.x+(double)r.width*0.5));
 put_float32(result+8,(float)((double)r.width*0.5));
 put_float32(result+12,(float)((double)r.framebuffer_height-((double)r.y+(double)r.height*0.5)));
 put_float32(result+16,(float)(-(double)r.height*0.5));
 put_float32(result+20,(float)((double)reciprocal*(double)matrix_zoffset));
 put_float32(result+24,(float)((double)reciprocal*(double)matrix_zscale));
 memcpy(out,result,sizeof(result));return 0;
}

static uint32_t read32(const uint8_t *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int remesa_ta_constant_draw_state_encode(
 const struct remesa_ta_constant_draw_state_request *request,uint8_t out[56])
{
 struct remesa_ta_constant_draw_state_request r;
 uint8_t isp[8],fragment[16],viewport[28],scalar[REMESA_TA_SCALAR_BYTES],result[56];
 size_t scalar_bytes=0;int e;
 if(!request || !out || (uintptr_t)request>UINTPTR_MAX-sizeof(r) ||
    (uintptr_t)out>UINTPTR_MAX-sizeof(result))return EINVAL;
 memcpy(&r,request,sizeof(r));
 if(r.fragment.profile!=REMESA_TA_FRAGMENT_AUTHORED_CONSTANT ||
    r.viewport.path!=REMESA_TA_VIEWPORT_INITIAL_ZERO ||
    r.scalar_profile!=REMESA_TA_SCALAR_POSITION4)return ENOTSUP;
 e=remesa_isp_opaque_triangle_data_encode(&r.isp,isp);if(e)return e;
 e=remesa_ta_fragment_binding_encode(&r.fragment,fragment);if(e)return e;
 e=remesa_ta_window_viewport_encode(&r.viewport,viewport);if(e)return e;
 e=remesa_ta_scalar_groups_encode(r.scalar_profile,scalar,sizeof(scalar),&scalar_bytes);if(e)return e;
 if(scalar_bytes!=sizeof(scalar))return EIO;
 /* Original287ff selects each group in increasing bit order. Individual
  * headers are merged once; their payloads contain no padding DWORDs. */
 put32(result,read32(isp)|read32(fragment)|read32(viewport)|read32(scalar));
 memcpy(result+4,isp+4,4);memcpy(result+8,fragment+4,12);
 memcpy(result+20,viewport+4,24);memcpy(result+44,scalar+4,12);
 memcpy(out,result,sizeof(result));return 0;
}


int remesa_ta_constant_draw_after_zclear_encode(
 const struct remesa_ta_constant_draw_state_request *request,
 uint8_t *out,size_t capacity,size_t *written)
{
 uint8_t full[56],result[52];int e;uintptr_t a=(uintptr_t)out,b=(uintptr_t)written;
 if(!out || !capacity || capacity-1>UINTPTR_MAX-a || !written ||
    sizeof(*written)-1>UINTPTR_MAX-b || b%_Alignof(size_t))return EINVAL;
 if(a<=b?b-a<capacity:a-b<sizeof(*written))return EINVAL;
 if(capacity<sizeof(result))return ENOSPC;
 e=remesa_ta_constant_draw_state_encode(request,full);if(e)return e;
 /* Original28fff compares with the published54c5 record: group400 is
  * identical and removed. Original287ff emits the remaining13 DWORDs.
  * This API names that exact predecessor; it does not infer cached state. */
 memcpy(result,full,48);put32(result,UINT32_C(0xb41));
 memcpy(result+48,full+52,4);
 memcpy(out,result,sizeof(result));*written=sizeof(result);return 0;
}

int remesa_ta_scene_zclear_state_encode(
 const struct remesa_ta_fragment_binding_request *request,uint8_t out[44])
{
 struct remesa_ta_fragment_binding_request r;uint8_t fragment[16],scalar[16],result[44];
 size_t scalar_bytes=0;int e;
 if(!request || !out || (uintptr_t)request>UINTPTR_MAX-sizeof(r) ||
    (uintptr_t)out>UINTPTR_MAX-sizeof(result))return EINVAL;
 memcpy(&r,request,sizeof(r));
 if(r.profile!=REMESA_TA_FRAGMENT_SCENE_ZCLEAR)return ENOTSUP;
 e=remesa_ta_fragment_binding_encode(&r,fragment);if(e)return e;
 e=remesa_ta_scalar_groups_encode(REMESA_TA_SCALAR_POSITION4,scalar,sizeof(scalar),&scalar_bytes);if(e)return e;
 if(scalar_bytes!=sizeof(scalar))return EIO;
 /* Original2a9d8..2aad2 zeroes its state and selects these exact groups.
  * ISPword0=01e00100 and word2=0e000000 are the demonstrated special CPU
  * constructor's fixed values, NOT a new generic physical instruction rule.
  */
 put32(result,read32(fragment)|(UINT32_C(1)<<0)|(UINT32_C(1)<<2)|
  (UINT32_C(1)<<7)|(UINT32_C(1)<<10)|(UINT32_C(1)<<12)|(UINT32_C(1)<<14));
 put32(result+4,UINT32_C(0x01e00100));put32(result+8,UINT32_C(0x0e000000));
 memcpy(result+12,fragment+4,12);put32(result+24,0);put32(result+28,0);
 memcpy(result+32,scalar+8,4);put32(result+36,UINT32_C(0x10000));put32(result+40,0);
 memcpy(out,result,sizeof(result));return 0;
}
