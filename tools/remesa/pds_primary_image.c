/* SPDX-License-Identifier: MIT
 * Independently generate the restricted primary FS CPU image, using the
 * existing typed CODE/DATA and USE relocation codecs. Output is not a submit
 * packet or execution authorization. The caller supplies the retained USE VA
 * and bank. Unknown DATA cells retain the existing zero-initialization policy.
 */
#include "remesa_pds_structure.h"
#include "remesa_program_registers.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int number(const char *s,uint64_t *v)
{
 char *end;unsigned long long n;
 if(!s||!*s||*s=='-'||*s=='+')return EINVAL;
 errno=0;n=strtoull(s,&end,0);if(errno||*end)return EINVAL;
 *v=(uint64_t)n;return 0;
}
int main(int argc,char **argv)
{
 if(argc==9&&!strcmp(argv[1],"--background-image")) {
  uint64_t color,width,height,tag,use,offset,bank;
  if(number(argv[2],&color)||number(argv[3],&width)||number(argv[4],&height)||
     number(argv[5],&tag)||number(argv[6],&use)||number(argv[7],&offset)||
     number(argv[8],&bank)||width>UINT32_MAX||height>UINT32_MAX||
     tag>UINT32_MAX||bank>15)return EINVAL;
  const struct remesa_use_entry_request entry_request={.allocation_address=use,
   .allocation_bytes=32768,.entry_offset=offset,.program_bytes=8,
   .bank=(uint32_t)bank,.data_master=REMESA_USE_PIXEL};
  struct remesa_use_entry_plan entry;uint32_t fixup;
  int e=remesa_use_entry_prepare(&entry_request,&entry);if(e)return e;
  e=remesa_use_entry_preview_fixup(&entry,&fixup);if(e)return e;
  const struct remesa_pds_background_request request={.color_address=color,
   .use_lower19=fixup,.surface_tag=(uint32_t)tag,
   .width=(uint32_t)width,.height=(uint32_t)height};
  unsigned char image[64],known[64];size_t length=0;
  e=remesa_pds_structure_encode_background(&request,image,known,sizeof(image),&length);
  if(e)return e;
  if(length!=sizeof(image))return EPROTO;
  return fwrite(image,1,length,stdout)==length&&!fflush(stdout)?0:EIO;
 }
 /* Auxiliary CPU profiles from psb40355/287ff. Source VA is an explicit
  * caller binding, not authenticated by this CPU-only emitter. Unwritten
  * DATA cells use our zero initialization policy, not inferred ISA meaning. */
 if(argc==7&&(!strcmp(argv[1],"--vertex-image")||!strcmp(argv[1],"--state-image"))) {
  uint64_t source,words,use,offset,slot;
  if(number(argv[2],&source)||number(argv[3],&words)||number(argv[4],&use)||
     number(argv[5],&offset)||number(argv[6],&slot)||source>UINT32_MAX||
     !source||(source&3u)||slot>15)return EINVAL;
  if(words>UINT32_MAX)return ERANGE;
  uint32_t dma,fixup;
  int e=remesa_pds_structure_dma_control((uint32_t)words,&dma);if(e)return e;
  if(source+words*4u>UINT64_C(0x100000000))return ERANGE;
  const struct remesa_use_entry_request request={.allocation_address=use,
   .allocation_bytes=32768,.entry_offset=offset,.program_bytes=16,
   .bank=(uint32_t)slot,.data_master=REMESA_USE_VERTEX};
  struct remesa_use_entry_plan entry;
  e=remesa_use_entry_prepare(&request,&entry);if(e)return e;
  e=remesa_use_entry_preview_fixup(&entry,&fixup);if(e)return e;
  const int vertex=!strcmp(argv[1],"--vertex-image");
  if(!vertex) {
   const struct remesa_pds_state_image_request state={
    .source_allocation_address=source,.source_allocation_bytes=words*4u,
    .state_dwords=(uint32_t)words,.use_lower19=fixup};
   unsigned char image[REMESA_PDS_STRUCTURE_STATE_BYTES],known[REMESA_PDS_STRUCTURE_STATE_BYTES];
   size_t length=0;
   e=remesa_pds_structure_encode_state_image(&state,image,known,sizeof(image),&length);
   if(e)return e;
   if(length!=sizeof(image))return EPROTO;
   return fwrite(image,1,length,stdout)==length&&!fflush(stdout)?0:EIO;
  }
  const struct remesa_pds_vertex_image_request vertex_request={
   .source_allocation_address=source,.source_allocation_bytes=words*4u,
   .vertex_stride_dwords=(uint32_t)words,.use_lower19=fixup};
  unsigned char image[REMESA_PDS_STRUCTURE_VERTEX_BYTES],known[REMESA_PDS_STRUCTURE_VERTEX_BYTES];
  size_t length=0;
  e=remesa_pds_structure_encode_vertex_image(&vertex_request,image,known,sizeof(image),&length);
  if(e)return e;
  if(length!=sizeof(image))return EPROTO;
  return fwrite(image,1,length,stdout)==length&&!fflush(stdout)?0:EIO;
 }
 if(argc==2&&(!strcmp(argv[1],"--state-code")||!strcmp(argv[1],"--background-code"))) {
  const uint32_t none=REMESA_PDS_STRUCTURE_NO_CELL;
  const struct remesa_pds_code_instruction state[]={
   {REMESA_PDS_STRUCTURE_CPU_TRANSFER_PAIR,0,none},
   {REMESA_PDS_STRUCTURE_PAIR_SINGLE,2,2},
   {REMESA_PDS_STRUCTURE_CPU_TRAILER_AF,none,none}};
  const struct remesa_pds_code_instruction background[]={
   {REMESA_PDS_STRUCTURE_PAIR_SINGLE,0,0},
   {REMESA_PDS_STRUCTURE_A2,none,1},
   {REMESA_PDS_STRUCTURE_CLASS64,2,2},
   {REMESA_PDS_STRUCTURE_CPU_TRAILER_AF,none,none}};
  const int is_state=!strcmp(argv[1],"--state-code");
  unsigned char bytes[16];size_t length=0;
  int e=remesa_pds_structure_encode_code(is_state?state:background,
   is_state?3u:4u,bytes,sizeof(bytes),&length);
  if(e)return e;
  if(length!=(is_state?12u:16u))return EPROTO;
  return fwrite(bytes,1,length,stdout)==length&&!fflush(stdout)?0:EIO;
 }
 if(argc==2&&!strcmp(argv[1],"--empty-secondary")) {
  const struct remesa_pds_code_instruction ir={
   REMESA_PDS_STRUCTURE_CPU_EMPTY_SECONDARY_AF,
   REMESA_PDS_STRUCTURE_NO_CELL,REMESA_PDS_STRUCTURE_NO_CELL};
  unsigned char bytes[4];size_t length=0;
  int e=remesa_pds_structure_encode_code(&ir,1,bytes,sizeof(bytes),&length);
  if(e)return e;
  if(length!=sizeof(bytes))return EPROTO;
  return fwrite(bytes,1,length,stdout)==length&&!fflush(stdout)?0:EIO;
 }
 uint64_t address,bank;
 if(argc!=3||number(argv[1],&address)||number(argv[2],&bank)||bank>15)return EINVAL;
 const struct remesa_use_entry_request request={.allocation_address=address,
  .allocation_bytes=32768,.program_bytes=8,.bank=(uint32_t)bank,
  .data_master=REMESA_USE_PIXEL};
 struct remesa_use_entry_plan entry;uint32_t fixup;
 int e=remesa_use_entry_prepare(&request,&entry);if(e)return e;
 e=remesa_use_entry_preview_fixup(&entry,&fixup);if(e)return e;
 const struct remesa_pds_structure_request ir={.code_policy=REMESA_PDS_STRUCTURE_CODE_CPU_COMPLETE};
 struct remesa_pds_structure structure;
 e=remesa_pds_structure_encode(&ir,&structure);if(e)return e;
 unsigned char bytes[REMESA_PDS_STRUCTURE_IMAGE_BYTES],mask[sizeof(bytes)];size_t length;
 e=remesa_pds_structure_serialize(&structure,bytes,mask,sizeof(bytes),&length);if(e)return e;
 if(structure.relocation_count!=1||structure.relocations[0].data_cell!=0||
    structure.relocations[0].kind!=REMESA_PDS_STRUCTURE_USE||
    length!=56||structure.data_dwords!=12)return EPROTO;
 uint32_t word=structure.relocations[0].background|fixup;
 for(unsigned i=0;i<4;i++)bytes[i]=(unsigned char)(word>>(8u*i));
 /* No historical CODE words or shader arrays are inputs to this emitter. */
 if(fwrite(bytes,1,length,stdout)!=length||fflush(stdout))return EIO;
 return 0;
}
