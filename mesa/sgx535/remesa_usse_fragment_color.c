/* SPDX-License-Identifier: MIT */
#include "remesa_usse_fragment_color.h"
#include "remesa_usse.h"
#include "remesa_nir.h"
#include <errno.h>
#include <string.h>
static int overlaps(const void *a,size_t an,const void *b,size_t bn)
{
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 if(an>UINTPTR_MAX-x||bn>UINTPTR_MAX-y)return 1;
 return x<y+bn&&y<x+an;
}
int remesa_usse_fragment_color_encode(
 const struct remesa_usse_fragment_color_request *request,void *out,
 size_t capacity,struct remesa_usse_fragment_color_extent *extent)
{
 if(!request||!out||!extent||
  overlaps(request,sizeof(*request),out,capacity)||
  overlaps(request,sizeof(*request),extent,sizeof(*extent))||
  overlaps(out,capacity,extent,sizeof(*extent)))return EINVAL;
 if((request->input_slot!=1&&request->input_slot!=2)||request->link_key)
  return ENOTSUP;
 if(capacity<REMESA_USSE_CPU_MOVE_BYTES)return ENOSPC;
 /* Compiler supplies controls12/23. Whole original builder ELF26654
  * sets bit18 on the last record; no link prefix/pack/tail for this key. */
 const struct remesa_usse_cpu_move move={.source={2,0},.destination={1,0},
  .control12=1,.control23=1,.control18=1};
 unsigned char code[REMESA_USSE_CPU_MOVE_BYTES];size_t written=0;
 int error=remesa_usse_cpu_move_encode(&move,code,sizeof(code),&written);
 if(error)return error;
 if(written!=sizeof(code))return EINVAL;
 const struct remesa_usse_fragment_color_extent result={8,16,32,request->input_slot+9,4,3,0};
 memcpy(out,code,sizeof(code));*extent=result;return 0;
}

int remesa_usse_fragment_color_encode_nir(const struct nir_shader *nir,void *out,
 size_t capacity,struct remesa_usse_fragment_color_extent *extent)
{
 unsigned slot;
 if(!nir||!out||!extent)return EINVAL;
 int error=remesa_nir_fragment_color_input(nir,&slot);
 if(error)return error;
 const struct remesa_usse_fragment_color_request request={slot,0};
 return remesa_usse_fragment_color_encode(&request,out,capacity,extent);
}
