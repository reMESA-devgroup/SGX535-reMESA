/* SPDX-License-Identifier: MIT */
#include "remesa_ta_index.h"
#include "remesa_render_target.h"
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
static int span(uint64_t base,uint64_t size,uint64_t offset,uint64_t bytes)
{
 return base && size && base<=UINT32_MAX && size<=UINT64_C(0x100000000)-base &&
        offset<=size && bytes<=size-offset;
}
static void le32(uint8_t *p,uint32_t v)
{
 p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);
}
static int index_triangles_encode(const struct remesa_ta_index_request *in,
 uint8_t out[20],uint32_t compiled_output_bytes)
{
 struct remesa_ta_index_request r;uint8_t tmp[20];uint64_t ibegin,ibytes,iva,pva;
 uint32_t descriptor;
 if(!in||!out || (uintptr_t)in>UINTPTR_MAX-sizeof(*in) ||
    (uintptr_t)out>UINTPTR_MAX-20)return EINVAL;
 memcpy(&r,in,sizeof(r));
 if(r.index_count<3 || r.index_count>65535 || r.index_count%3 ||
    r.pds_data_dwords!=12 ||
    !r.descriptor_a_field10 || r.descriptor_a_field10>16 ||
    (compiled_output_bytes?(r.descriptor_b_field38!=compiled_output_bytes||
      (r.descriptor_a_field10!=4&&r.descriptor_a_field10!=8&&
       (compiled_output_bytes!=32||r.descriptor_a_field10!=12))):
     r.descriptor_b_field38!=r.descriptor_a_field10))return ENOTSUP;
 if(r.index_byte_offset>UINT64_MAX-2*(uint64_t)r.first_index)return EOVERFLOW;
 ibegin=r.index_byte_offset+2*(uint64_t)r.first_index;ibytes=2*(uint64_t)r.index_count;
 if(!span(r.index_bo_gpu_va,r.index_bo_bytes,ibegin,ibytes) ||
    !span(r.pds_bo_gpu_va,r.pds_bo_bytes,r.pds_byte_offset,r.pds_object_bytes) ||
    r.pds_object_bytes<64)return ERANGE;
 iva=r.index_bo_gpu_va+ibegin;pva=r.pds_bo_gpu_va+r.pds_byte_offset;
 if((iva&1)||(pva&15))return EINVAL;
 if(iva<pva+r.pds_object_bytes && pva<iva+ibytes)return EINVAL;
 descriptor=((r.pds_data_dwords*4+15)>>4) |
            (((r.descriptor_b_field38+3)>>2)<<25) |
            (((r.descriptor_a_field10*4+15)<<4)&UINT32_C(0x1ff00));
 le32(tmp,UINT32_C(0x81400000)|r.index_count);le32(tmp+4,(uint32_t)iva);
 le32(tmp+8,0);le32(tmp+12,(uint32_t)(pva>>4));le32(tmp+16,descriptor);
 memcpy(out,tmp,sizeof(tmp));return 0;
}

int remesa_ta_scene_close_data_encode(uint32_t width,uint32_t height,
 uint8_t out[8])
{
 uint8_t tmp[8];
 if(!out || (uintptr_t)out>UINTPTR_MAX-8)return EINVAL;
 if(!width||!height)return EINVAL;
 if(width>2048||height>2048)return ENOTSUP;
 /* Original2a5c0 selects group13;2a5ca..2a5e7 rounds both dimensions up.
  * This has a different rounding rule from the raster-prefix dimension word.
  */
 le32(tmp,UINT32_C(0x2000));
 le32(tmp+4,(((width+15)>>4)<<16)|((height+15)>>4));
 memcpy(out,tmp,sizeof(tmp));return 0;
}
int remesa_ta_scene_close_encode(const struct remesa_ta_scene_close_request *in,
 uint8_t out[12])
{
 struct remesa_ta_scene_close_request r;uint8_t tmp[12];int rc;
 if(!in||!out || (uintptr_t)in>UINTPTR_MAX-sizeof(*in) ||
    (uintptr_t)out>UINTPTR_MAX-12)return EINVAL;
 memcpy(&r,in,sizeof(r));
 if(!span(r.pds_bo_gpu_va,r.pds_bo_bytes,r.pds_byte_offset,r.pds_object_bytes)||
    r.pds_object_bytes<60)return ERANGE;
 rc=remesa_ta_state_reference_encode(r.pds_bo_gpu_va+r.pds_byte_offset,2,true,tmp);
 if(rc)return rc;
 /* Explicit original CPU emission contract2a46b, not an invented ISA opcode.
  *2a471 advances by4,384ab commits that end. Do not add reserved-but-unused4.
  */
 le32(tmp+8,UINT32_C(0xc0000000));
 memcpy(out,tmp,sizeof(tmp));return 0;
}

int remesa_ta_index_triangles_encode(const struct remesa_ta_index_request *in,uint8_t out[20])
{return index_triangles_encode(in,out,0);}
int remesa_ta_index_compiled_position_triangles_encode(const struct remesa_ta_index_request *in,
 uint8_t out[20])
{return index_triangles_encode(in,out,16);}

int remesa_ta_index_compiled_position_color_triangles_encode(const struct remesa_ta_index_request *in,
 uint8_t out[20])
{return index_triangles_encode(in,out,32);}
