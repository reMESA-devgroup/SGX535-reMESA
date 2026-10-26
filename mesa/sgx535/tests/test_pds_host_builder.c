/* SPDX-License-Identifier: MIT
 * Original host DATA recipe/relocation regression, no device or ISA fixture.
 */
#include "remesa_pds.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { assert(x);checks++; } while(0)
struct fixture {
 struct remesa_usse_program semantic[2];
 struct remesa_pds_plan plans[2];
 struct remesa_pds_binding programs[2];
 struct remesa_pds_use_link links[2];
 struct remesa_pds_data_cell cells[2][12];
 struct remesa_pds_host_data_recipe recipes[2];
 struct remesa_pds_host_data data[2];
 unsigned char host[2][96];
};
static uint32_t get32(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void prepare(struct fixture *f,int separated)
{
 memset(f,0,sizeof(*f));memset(f->host,0xa5,sizeof(f->host));
 for(unsigned i=0;i<2;i++) {
  struct remesa_usse_program *p=&f->semantic[i];
  p->schema=REMESA_USSE_SCHEMA;p->stage=i?REMESA_IR_FRAGMENT:REMESA_IR_VERTEX;
  p->count=i?6:8;
  for(unsigned c=0;c<(i?2u:4u);c++)p->instructions[c]=(struct remesa_usse_instruction){
   .op=i?REMESA_USSE_CONSTANT:REMESA_USSE_INPUT,.dst=c,
   .src=REMESA_USSE_NO_VALUE,.channel=i?0:c,.bits=i&&!c?UINT32_C(0x3f800000):0};
  for(unsigned c=0;c<4;c++)p->instructions[p->count-4+c]=(struct remesa_usse_instruction){
   .op=i?REMESA_USSE_EXPORT_COLOR:REMESA_USSE_EXPORT_POSITION,
   .dst=REMESA_USSE_NO_VALUE,.src=i?(c==1?1:0):c,.channel=c};
  CHECK(!remesa_usse_validate(p));
  struct remesa_pds_block_request block={.operation=11,.generation=7,
   .allocation={.allocation_cookie=10+i,.owner_cookie=3,.mapping_generation=2,
    .address=UINT64_C(0x21000000)+i*UINT64_C(0x10000),.offset=16,
    .bytes=80,.allocation_bytes=4096},.data_bytes=48,.code_bytes=16,.code_alignment=16};
  struct remesa_pds_request request;CHECK(!remesa_pds_request_from_block(&block,&request));
  if(separated) {
   request.code.allocation_cookie=20+i;
   request.code.address=UINT64_C(0x22000000)+i*UINT64_C(0x10000);
  }
  CHECK(!remesa_pds_prepare(p,&request,&f->plans[i]));
  f->programs[i]=(struct remesa_pds_binding){.allocation_cookie=1,.owner_cookie=3,
   .mapping_generation=4,.address=UINT64_C(0x20000000),.offset=i?32:0,
   .bytes=i?8:32,.allocation_bytes=4096};
  f->data[i]=(struct remesa_pds_host_data){.binding=request.data,
   .bytes=f->host[i]+16,.byte_length=48};
  for(unsigned c=0;c<12;c++)f->cells[i][c]=(struct remesa_pds_data_cell){
   .kind=REMESA_PDS_DATA_LITERAL32,.value=UINT32_C(0x82345600)+i*16+c};
  f->cells[i][1+i]=(struct remesa_pds_data_cell){
   .kind=REMESA_PDS_DATA_USE_ENTRY_LOWER19,.value=UINT32_C(0xa5800000)};
  f->recipes[i]=(struct remesa_pds_host_data_recipe){.cells=f->cells[i],.cell_count=12};
 }
 const uint32_t banks[2]={1,2};const uint64_t offsets[2]={4,8};
 CHECK(!remesa_pds_prepare_use_links(f->plans,f->programs,banks,offsets,f->links));
}
static void unchanged(const struct fixture *f,unsigned char value)
{for(unsigned i=0;i<2;i++)for(unsigned c=0;c<96;c++)CHECK(f->host[i][c]==value);}
static void positive(int separated)
{
 struct fixture f;prepare(&f,separated);
 CHECK(remesa_pds_validate_host_data_pair(f.plans,f.programs,f.links,f.recipes,f.data)==EPROTO);
 CHECK(!remesa_pds_build_host_data_pair(f.plans,f.programs,f.links,f.recipes,f.data));
 CHECK(!remesa_pds_validate_host_data_pair(f.plans,f.programs,f.links,f.recipes,f.data));
 CHECK(!remesa_pds_validate_host_entry_fixups(f.plans,f.programs,f.links,f.data));
 for(unsigned i=0;i<2;i++) {
  for(unsigned c=0;c<12;c++)CHECK(get32(f.host[i]+16+c*4)==
   (f.cells[i][c].value|(c==1+i?f.links[i].address_bits:0)));
  /* Prefix, explicit CODE window, and trailing bytes are never written. */
  for(unsigned c=0;c<96;c++)if(c<16||c>=64)CHECK(f.host[i][c]==0xa5);
 }
 for(unsigned i=0;i<2;i++)for(unsigned byte=0;byte<48;byte++) {
  f.host[i][16+byte]^=1;
  CHECK(remesa_pds_validate_host_data_pair(f.plans,f.programs,f.links,f.recipes,f.data)==EPROTO);
  f.host[i][16+byte]^=1;
 }
 CHECK(!remesa_pds_build_host_data_pair(f.plans,f.programs,f.links,f.recipes,f.data));
 size_t n=31;unsigned char code[16];memset(code,0xa5,sizeof(code));
 CHECK(remesa_pds_encode(&f.semantic[0],&f.plans[0],code,sizeof(code),&n)==ENOTSUP&&n==31);
 for(unsigned c=0;c<sizeof(code);c++)CHECK(code[c]==0xa5);
}
static void invalid(void)
{
 for(unsigned mode=0;mode<25;mode++) {
  struct fixture f;prepare(&f,mode&1);
  const struct remesa_pds_host_data_recipe *recipes=f.recipes;
  const struct remesa_pds_plan *plans=f.plans;
  const struct remesa_pds_binding *programs=f.programs;
  const struct remesa_pds_use_link *links=f.links;
  const struct remesa_pds_host_data *data=f.data;
  switch(mode) {
  case 0:recipes=NULL;break;
  case 1:f.recipes[1].cells=NULL;break;
  case 2:f.recipes[1].cell_count=0;break;
  case 3:f.recipes[1].cell_count=11;break;
  case 4:f.recipes[1].cell_count=SIZE_MAX;break;
  case 5:f.recipes[1].cells=(void *)(UINTPTR_MAX-3);break;
  case 6:f.cells[1][11].kind=99;break;
  case 7:f.cells[1][2].kind=REMESA_PDS_DATA_LITERAL32;break;
  case 8:f.cells[1][2].value|=1;break;
  case 9:f.cells[1][3].kind=REMESA_PDS_DATA_USE_ENTRY_LOWER19;f.cells[1][3].value=0;break;
  case 10:f.data[1].binding.mapping_generation++;break;
  case 11:f.programs[1].mapping_generation++;break;
  case 12:f.links[1].address_bits^=1;break;
  case 13:f.data[1].byte_length--;break;
  case 14:f.data[1].bytes=f.data[0].bytes;break;
  case 15:f.recipes[1].cells=(void *)f.host[0];break;
  case 16:f.recipes[1].cells=(void *)f.plans;break;
  case 17:recipes=(void *)(UINTPTR_MAX-3);break;
  case 18:recipes=(void *)f.host[0];break;
  case 19:plans=(void *)(UINTPTR_MAX-3);break;
  case 20:programs=(void *)(UINTPTR_MAX-3);break;
  case 21:links=(void *)(UINTPTR_MAX-3);break;
  case 22:data=(void *)(UINTPTR_MAX-3);break;
  case 23:f.data[1].bytes=(void *)(UINTPTR_MAX-3);break;
  case 24:f.recipes[0].cells=(void *)f.recipes;break;
  }
  CHECK(remesa_pds_build_host_data_pair(plans,programs,links,recipes,data)!=0);
  unchanged(&f,0xa5);
  CHECK(remesa_pds_validate_host_data_pair(plans,programs,links,recipes,data)!=0);
  unchanged(&f,0xa5);
 }
}
int main(void)
{
 positive(0);positive(1);invalid();
 printf("PASS PDS original host DATA builder: %u checks; no GPU execution\n",checks);
 return 0;
}
