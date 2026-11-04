/* SPDX-License-Identifier: MIT */
#ifndef REMESA_MATRIX_DRAW_DOMAIN_H
#define REMESA_MATRIX_DRAW_DOMAIN_H
#include "ta_references.h"
/* Existing ordinary draw's finite in-clip domain, expressed in IEEE754 bits.
 * No floating point, transformed vertices, normalization or rendering. Counts
 * describe committed records, not allocation capacity. Packed GPU records add
 * the existing unused, zero-initialized float4; source attributes stay intact.
 * The caller retains immutable input and proves backing/ownership separately.
 * This predicate alone is not shader/state or GPU execution authority. */
static inline int remesa_matrix_draw_values(const void *vertices,size_t vertex_bytes,
 remesa_scene_u32 count,remesa_scene_u32 stride,const void *coefficients,size_t coefficient_bytes)
{
 const remesa_ta_ref_u8 *v=vertices,*p=coefficients;
 remesa_ta_ref_uptr limit=~(remesa_ta_ref_uptr)0;remesa_scene_u32 c[36];unsigned i,col,row,light=0;
 if(!count||count>65535||(stride!=32&&stride!=48))return EOPNOTSUPP;
 if(!vertices||!coefficients||vertex_bytes!=(size_t)count*stride||coefficient_bytes!=144||
    (remesa_ta_ref_uptr)vertices>limit-vertex_bytes||(remesa_ta_ref_uptr)coefficients>limit-144)return EINVAL;
 for(i=0;i<36;i++){
  c[i]=remesa_ta_ref_word(p+4*i);
  /* Magnitudes above4 include every infinity and NaN representation. */
  if((c[i]&0x7fffffffU)>0x40800000U)return EOPNOTSUPP;
 }
 for(col=0;col<3;col++)for(row=0;row<4;row++){
  remesa_scene_u32 magnitude=c[16+4*col+row]&0x7fffffffU;
  if(row==col){if(magnitude>0x3f800000U)return EOPNOTSUPP;}
  else if(magnitude)return EOPNOTSUPP;
 }
 for(i=28;i<31;i++)if((c[i]&0x7fffffffU)>0x3f000000U)return EOPNOTSUPP;
 if(c[31]!=0x40000000U)return EOPNOTSUPP;
 for(i=32;i<35;i++){
  remesa_scene_u32 magnitude=c[i]&0x7fffffffU;
  if(magnitude>0x3f800000U)return EOPNOTSUPP;
  if(magnitude>=0x3d800000U)light=1;
 }
 if(!light||(c[35]&0x7fffffffU))return EOPNOTSUPP;
 for(i=0;i<count;i++){
  const remesa_ta_ref_u8 *at=v+(size_t)i*stride;unsigned k,normal=0;
  for(k=0;k<8;k++){
   remesa_scene_u32 magnitude=remesa_ta_ref_word(at+4*k)&0x7fffffffU;
   if(magnitude>0x3f800000U)return EOPNOTSUPP;
   if(k>=4&&k<7&&magnitude>=0x3d800000U)normal=1;
  }
  if(remesa_ta_ref_word(at+12)!=0x3f800000U||!normal)return EOPNOTSUPP;
  {
   remesa_scene_u32 w=remesa_ta_ref_word(at+28);
   if((w&0x7fffffffU)&&w!=0x3f800000U)return EOPNOTSUPP;
  }
  if(stride==48)for(k=32;k<48;k++)if(at[k])return EOPNOTSUPP;
 }
 return 0;
}
/* Compare dyadic dimension values emitted by the existing viewport/clear
 * producers without using an FPU in kernel. This is not a viewport encoder
 * or transform: no output state/vertex is constructed or uploaded. */
static inline remesa_scene_u32 remesa_matrix_half_dimension(remesa_scene_u32 n)
{
 remesa_scene_u32 top=n,e=0;
 if(!n||n>2048)return 0;
 while(top>1){top>>=1;e++;}
 return ((e+126U)<<23)|((n-(1U<<e))<<(23-e));
}
/* Non-reference fields of the existing opaque LESS/write/full-RGBA state15,
 * full MATRIX_WINDOW viewport, near0/far1 and Z24. Fragment references are
 * authenticated by the existing held-graph closure, not by this predicate. */
static inline int remesa_matrix_draw_state_values(const void *input,size_t bytes,
 remesa_scene_u32 width,remesa_scene_u32 height)
{
 const remesa_ta_ref_u8 *p=input;remesa_scene_u32 x,y;
 if(!input||bytes!=60||(remesa_ta_ref_uptr)input>~(remesa_ta_ref_uptr)0-60)return EINVAL;
 if(!width||width>2048||!height||height>2048)return EOPNOTSUPP;
 x=remesa_matrix_half_dimension(width);y=remesa_matrix_half_dimension(height);
 if(remesa_ta_ref_word(p)!=0x1f41U||remesa_ta_ref_word(p+4)!=0x00400000U||
    remesa_ta_ref_word(p+20)!=x||remesa_ta_ref_word(p+24)!=x||
    remesa_ta_ref_word(p+28)!=y||remesa_ta_ref_word(p+32)!=(y|0x80000000U)||
    remesa_ta_ref_word(p+36)!=0x3f000000U||remesa_ta_ref_word(p+40)!=0x3f000000U||
    remesa_ta_ref_word(p+44)||remesa_ta_ref_word(p+48)!=0x08001800U||
    remesa_ta_ref_word(p+52)!=0x358637bdU||remesa_ta_ref_word(p+56))return EOPNOTSUPP;
 return 0;
}
/* Existing single-triangle Z-clear constructor's consumed state/geometry.
 * The fragment reference triple is checked by the held closure. Ignore all
 * unconsumed fourth-vertex/index capacity. No new geometry or CODE is emitted. */
static inline int remesa_matrix_prefix_geometry_values(const void *vertices,size_t vertex_bytes,
 const void *indices,size_t index_bytes,remesa_scene_u32 width,remesa_scene_u32 height)
{
 const remesa_ta_ref_u8 *v=vertices,*ix=indices;unsigned i;
 remesa_ta_ref_uptr limit=~(remesa_ta_ref_uptr)0;remesa_scene_u32 x,y;
 if(!v||!ix||vertex_bytes!=48||index_bytes!=6||
    (remesa_ta_ref_uptr)v>limit-48||(remesa_ta_ref_uptr)ix>limit-6)return EINVAL;
 if(!width||width>2048||!height||height>2048)return EOPNOTSUPP;
 x=remesa_matrix_half_dimension(width)+0x01000000U;
 y=remesa_matrix_half_dimension(height)+0x01000000U;
 for(i=0;i<3;i++){
  if(remesa_ta_ref_word(v+i*16)!=(i==1?x:0)||remesa_ta_ref_word(v+i*16+4)!=(i==2?y:0)||
     remesa_ta_ref_word(v+i*16+8)!=0x3f800000U||remesa_ta_ref_word(v+i*16+12)!=0x3f800000U||
     ix[2*i]!=i||ix[2*i+1])return EOPNOTSUPP;
 }
 return 0;
}
static inline int remesa_matrix_prefix_values(const void *state,size_t state_bytes,
 const void *vertices,size_t vertex_bytes,const void *indices,size_t index_bytes,
 remesa_scene_u32 width,remesa_scene_u32 height)
{
 const remesa_ta_ref_u8 *p=state,*v=vertices,*ix=indices;
 remesa_ta_ref_uptr limit=~(remesa_ta_ref_uptr)0;
 if(!p||!v||!ix||state_bytes!=44||vertex_bytes!=48||index_bytes!=6||
    (remesa_ta_ref_uptr)p>limit-44||(remesa_ta_ref_uptr)v>limit-48||
    (remesa_ta_ref_uptr)ix>limit-6)return EINVAL;
 if(!width||width>2048||!height||height>2048)return EOPNOTSUPP;
 if(remesa_ta_ref_word(p)!=0x54c5U||remesa_ta_ref_word(p+4)!=0x01e00100U||
    remesa_ta_ref_word(p+8)!=0x0e000000U||remesa_ta_ref_word(p+24)||remesa_ta_ref_word(p+28)||
    remesa_ta_ref_word(p+32)!=0x04001000U||remesa_ta_ref_word(p+36)!=0x10000U||
    remesa_ta_ref_word(p+40))return EOPNOTSUPP;
 return remesa_matrix_prefix_geometry_values(vertices,vertex_bytes,indices,index_bytes,width,height);
}
#endif
