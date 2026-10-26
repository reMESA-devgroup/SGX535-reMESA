/* SPDX-License-Identifier: MIT
 * Independent CPU-constructor observations are assertions, never encoder input.
 * No AF word or physical ISA semantics is supplied by this test. */
#include "../remesa_pds_structure.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do {checks++;if(!(v)){fprintf(stderr,"%s:%d %s\n",__FILE__,__LINE__,#v);exit(1);}}while(0)
static uint32_t le(const uint8_t *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void fields(void)
{
 /* Original complete constructor13f470 ABI replays; Windows independently
  * corroborates the six cases with b0/1. No generic ISA domain is inferred. */
 const uint32_t cases[][3]={{0,0,0x07000345},{0,1,0x070003e5},
  {2,0,0x07040345},{2,1,0x070403e5},{4,0,0x07080345},
  {4,1,0x070803e5},{4,2,0x07082345},{4,3,0x070823e5}};
 for(unsigned i=0;i<8;i++){
  uint32_t a=cases[i][0],b=cases[i][1],word=0;
  CHECK(!remesa_pds_structure_pair_single(a,b,&word)&&word==cases[i][2]);
  const struct remesa_pds_pair_single_output refs={{a,a+1},8+b};
  uint8_t bytes[8];memset(bytes,0xa5,sizeof(bytes));size_t n=73;
  CHECK(!remesa_pds_structure_encode_pair_single_output(&refs,12,bytes,sizeof(bytes),&n));
  CHECK(n==4&&le(bytes)==cases[i][2]&&le(bytes+4)==0xa5a5a5a5);
 }
 uint32_t word=73;
 CHECK(remesa_pds_structure_pair_single(1,0,&word)==EINVAL&&word==73);
 CHECK(remesa_pds_structure_pair_single(6,0,&word)==ENOTSUP&&word==73);
 CHECK(remesa_pds_structure_pair_single(0,2,&word)==ENOTSUP&&word==73);
 CHECK(remesa_pds_structure_pair_single(4,4,&word)==ENOTSUP&&word==73);
 CHECK(remesa_pds_structure_pair_single(64,0,&word)==ERANGE&&word==73);
 CHECK(remesa_pds_structure_pair_single(0,64,&word)==ERANGE&&word==73);
 CHECK(remesa_pds_structure_pair_single(0,0,NULL)==EINVAL);
 CHECK(remesa_pds_structure_pair_single(0,0,(void *)(UINTPTR_MAX-1))==EINVAL);
}
static void failures(void)
{
 const struct remesa_pds_pair_single_output good={{0,1},8};
 for(unsigned i=0;i<9;i++){
  struct remesa_pds_pair_single_output r=good;
  uint8_t bytes[8],old[8];memset(bytes,0xa5,sizeof(bytes));memcpy(old,bytes,sizeof(bytes));
  uint32_t extent=12;size_t n=73,capacity=sizeof(bytes);int want=EINVAL;
  if(i==0){r.pair_cells[0]=8;}
  if(i==1){r.pair_cells[1]=2;}
  if(i==2){r.single_cell=0;}
  if(i==3){extent=11;want=ERANGE;}
  if(i==4){extent=13;}
  if(i==5){r.single_cell=10;want=ENOTSUP;}
  if(i==6){r.pair_cells[0]=6;r.pair_cells[1]=7;want=ENOTSUP;}
  if(i==7){r.single_cell=12;want=ERANGE;}
  if(i==8){capacity=3;want=ENOSPC;}
  CHECK(remesa_pds_structure_encode_pair_single_output(&r,extent,bytes,capacity,&n)==want);
  CHECK(n==73&&!memcmp(bytes,old,sizeof(bytes)));
 }
 struct remesa_pds_code_instruction ir[]={{REMESA_PDS_STRUCTURE_PAIR_SINGLE,0,0},
  {REMESA_PDS_STRUCTURE_A2,REMESA_PDS_STRUCTURE_NO_CELL,1},
  {REMESA_PDS_STRUCTURE_CLASS64,2,2},{REMESA_PDS_STRUCTURE_FINAL_HOLE,0,0}};
 uint8_t b[16],old[16];memset(b,0xa5,sizeof(b));memcpy(old,b,sizeof(b));size_t n=73;
 CHECK(remesa_pds_structure_encode_code(ir,4,b,sizeof(b),&n)==ENOTSUP);
 CHECK(n==73&&!memcmp(b,old,sizeof(b)));
 CHECK(!remesa_pds_structure_encode_code(ir,3,b,sizeof(b),&n));
 CHECK(n==12&&le(b)==0x07000345&&le(b+4)==0x070418a2&&le(b+8)==0x07042364);
}
static void allocation_integration(void)
{
 struct remesa_pds_allocator_request in={0};in.source.entry_count=15;
 for(unsigned i=0;i<14;i++){
  in.source.entries[i]=(struct remesa_pds_structure_entry){.kind=REMESA_PDS_STRUCTURE_R,.id=i,.components=4};
  if(i==10||i==11){in.source.entries[i].format_present=1;in.source.entries[i].explicit_format=0xc000000;}
  in.logical_width[i]=4;
 }
 in.source.entries[14].kind=REMESA_PDS_STRUCTURE_T;in.logical_width[14]=1;
 in.live_components[10]=in.live_components[11]=15;in.live_components[14]=1;
 struct remesa_pds_allocation plan;CHECK(!remesa_pds_structure_allocate(&in,&plan));
 struct remesa_pds_structure resolved,before=plan.structure;
 CHECK(!remesa_pds_structure_resolve_initial(&plan.structure,&resolved));
 CHECK(!memcmp(&before,&plan.structure,sizeof(before)));
 CHECK(resolved.code[0].word==0x07000345&&resolved.code[0].known_mask==UINT32_MAX);
 CHECK(resolved.code[resolved.code_slot_count-1].kind==REMESA_PDS_STRUCTURE_FINAL_HOLE&&
       !resolved.code[resolved.code_slot_count-1].known_mask);
 CHECK(!memcmp(resolved.relocations,plan.structure.relocations,sizeof(resolved.relocations)));
 struct remesa_pds_structure original=resolved;original.code[0]=plan.structure.code[0];
 CHECK(!memcmp(&original,&plan.structure,sizeof(original)));
 uint8_t image[REMESA_PDS_STRUCTURE_IMAGE_BYTES],mask[sizeof(image)];size_t n=73;
 CHECK(!remesa_pds_structure_serialize(&resolved,image,mask,sizeof(image),&n));
 size_t off=4*resolved.data_dwords;
 CHECK(le(image+off)==0x07000345&&le(mask+off)==UINT32_MAX);
 CHECK(le(image+n-4)==0&&le(mask+n-4)==0); /* no invented AF */
 CHECK(plan.logical_map[56]==4&&resolved.relocations[1].data_cell==10);
 struct remesa_pds_structure out;memset(&out,0xa5,sizeof(out));before=out;
 CHECK(remesa_pds_structure_resolve_initial(&resolved,&out)==EBUSY&&!memcmp(&out,&before,sizeof(out)));
 original=plan.structure;original.code[1].word^=1;
 CHECK(remesa_pds_structure_resolve_initial(&original,&out)!=0&&!memcmp(&out,&before,sizeof(out)));
 CHECK(remesa_pds_structure_resolve_initial(&plan.structure,&plan.structure)==EINVAL);
 CHECK(remesa_pds_structure_resolve_initial(NULL,&out)==EINVAL);
 resolved.code[0].ds1_index=1;
 CHECK(remesa_pds_structure_serialize(&resolved,image,mask,sizeof(image),&n)==EINVAL);
}
int main(void){fields();failures();allocation_integration();printf("PDS_PAIR_SINGLE_PASS checks=%u complete_program=ENOTSUP gpu_submissions=0\n",checks);return 0;}
