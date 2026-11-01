/* SPDX-License-Identifier: MIT. Original encoders feed the reference parser. */
#include "remesa/ta_references.h"
#include "remesa_render_target.h"
#include "remesa_ta_index.h"
#include <assert.h>
#include <stdio.h>
struct visitor {unsigned calls,reject;};
static int visit(void *opaque,const struct remesa_ta_reference *ref)
{
 struct visitor *v=opaque;v->calls++;
 assert(ref->minimum_bytes&&ref->address>=0x20000000&&ref->address<0x20100000);
 if(v->reject&&v->calls==v->reject)return EACCES;
 return 0;
}
static size_t build(unsigned draws,uint8_t *out)
{
 assert(!remesa_ta_state_reference_encode(0x20080140,11,false,out));
 struct remesa_ta_index_request r={.index_bo_gpu_va=0x20080000,.index_bo_bytes=4096,.index_byte_offset=144,
  .pds_bo_gpu_va=0x20080000,.pds_bo_bytes=4096,.pds_byte_offset=64,.pds_object_bytes=64,
  .index_count=3,.pds_data_dwords=12,.descriptor_a_field10=4,.descriptor_b_field38=4};
 assert(!remesa_ta_index_triangles_encode(&r,out+8));
 r.index_bo_gpu_va=0x20090000;r.index_byte_offset=128;r.pds_bo_gpu_va=0x200a0000;
 r.pds_byte_offset=0;r.descriptor_a_field10=12;r.descriptor_b_field38=32;
 for(unsigned i=0;i<draws;i++){
  assert(!remesa_ta_state_reference_encode(0x200b0100,14,false,out+28*(i+1)));
  assert(!remesa_ta_index_compiled_position_color_triangles_encode(&r,out+28*(i+1)+8));
 }
 struct remesa_ta_scene_close_request end={0x200c0000,4096,256,60};
 assert(!remesa_ta_scene_close_encode(&end,out+28*(draws+1)));return 40+28*draws;
}
int main(void)
{
 _Alignas(8) uint8_t image[40+28*17],saved[sizeof image];
 const unsigned sizes[]={1,2,16};
 struct remesa_ta_reference_counts counts,initial={99,98};
 for(unsigned s=0;s<3;s++){
  size_t bytes=build(sizes[s],image);struct visitor v={0};
  assert(!remesa_ta_references_walk(image,bytes,visit,&v,&counts));
  assert(counts.draws==sizes[s]&&counts.references==3*(sizes[s]+1)+1&&v.calls==counts.references);
  counts=initial;assert(!remesa_ta_references_walk(image,bytes,NULL,NULL,&counts));
  assert(counts.draws==sizes[s]);
 }
 size_t bytes=build(1,image);memcpy(saved,image,bytes);
 for(unsigned bad=0;bad<10;bad++){
  memcpy(image,saved,bytes);struct visitor v={0};counts=initial;
  if(bad==0)image[7]^=1;
  if(bad==1)image[8]=2;
  if(bad==2)image[16]=1;
  if(bad==3)image[24]^=1;
  if(bad==4)image[35]^=1;
  if(bad==5)image[52]^=1;
  if(bad==6)image[59]^=0x20;
  if(bad==7)image[60]^=1;
  if(bad==8)image[67]=0;
  if(bad==9){image[12]=0xfe;image[13]=image[14]=image[15]=0xff;}
  assert(remesa_ta_references_walk(image,bytes,visit,&v,&counts));
  assert(!v.calls&&!memcmp(&counts,&initial,sizeof counts));
 }
 memcpy(image,saved,bytes);struct visitor v={.reject=2};counts=initial;
 assert(remesa_ta_references_walk(image,bytes,visit,&v,&counts)==EACCES&&v.calls==2&&!memcmp(&counts,&initial,sizeof counts));
 v=(struct visitor){0};bytes=build(17,image);counts=initial;
 assert(remesa_ta_references_walk(image,bytes,visit,&v,&counts)==EINVAL&&!v.calls&&!memcmp(&counts,&initial,sizeof counts));
 bytes=build(1,image);
 assert(remesa_ta_references_walk(image,bytes-1,NULL,NULL,&counts)==EINVAL);
 assert(remesa_ta_references_walk(image,bytes+4,NULL,NULL,&counts)==EINVAL);
 assert(remesa_ta_references_walk(image,bytes,NULL,NULL,(void *)image)==EINVAL);
 assert(remesa_ta_references_walk((void *)(UINTPTR_MAX-1),bytes,NULL,NULL,&counts)==EINVAL);
 assert(!memcmp(&counts,&initial,sizeof counts));
 /* An initialization predecessor shares the exact clear INDEX descriptor;
  * it adds one state reference, preserving all16 compiled draw windows. */
 for(unsigned n=0;n<3;n++){
  size_t original=build(sizes[n],image);memmove(image+28,image,original);
  assert(!remesa_ta_state_reference_encode(0x20080340,11,false,image));
  memcpy(image+8,image+36,20);bytes=original+28;v=(struct visitor){0};
  assert(!remesa_ta_references_walk(image,bytes,visit,&v,&counts));
  assert(counts.draws==sizes[n]&&counts.references==3*(sizes[n]+1)+2&&v.calls==counts.references);
  memcpy(saved,image,bytes);
  image[12]^=2;v=(struct visitor){0};counts=initial;
  assert(remesa_ta_references_walk(image,bytes,visit,&v,&counts)==EINVAL&&!v.calls&&!memcmp(&counts,&initial,sizeof counts));
  memcpy(image,saved,bytes);memcpy(image,image+28,8);v=(struct visitor){0};
  assert(remesa_ta_references_walk(image,bytes,visit,&v,&counts)==EINVAL&&!v.calls&&!memcmp(&counts,&initial,sizeof counts));
 }
 puts("PASS TA reference grammar: original copy/compiled/close encoders,1/2/16 draws,52 legacy /53 initialized references, exact shared descriptor, full syntax before callbacks, transactional counts, aliases/wrap/malformed tails; minimum spans only, GPU0");return 0;
}
