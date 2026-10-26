/* SPDX-License-Identifier: MIT
 * CPU-only allocator reconstruction. The original compiler replay oracle is
 * an authored public Mesa ADD/TEX/ADD program, not imported USE/PDS bytecode.
 * Arbitrary liveness cases below test the explicitly supplied host contract;
 * they do not assert that NIR or hardware independently produces those masks. */
#include "../remesa_pds_structure.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if(!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while(0)

static struct remesa_pds_structure_entry r(uint32_t id,uint32_t components)
{
 struct remesa_pds_structure_entry e={0};
 e.kind=REMESA_PDS_STRUCTURE_R;e.id=id;e.components=components;
 if(id==10u||id==11u) {e.format_present=1u;e.explicit_format=UINT32_C(0x0c000000);}
 return e;
}
static struct remesa_pds_structure_entry t(uint32_t id)
{
 struct remesa_pds_structure_entry e={0};e.kind=REMESA_PDS_STRUCTURE_T;e.id=id;
 return e;
}
static struct remesa_pds_allocator_request original_inputs(void)
{
 struct remesa_pds_allocator_request a={0};a.source.entry_count=15u;
 for(unsigned i=0;i<14u;i++) {a.source.entries[i]=r(i,4u);a.logical_width[i]=4u;}
 a.source.entries[14]=t(0u);a.logical_width[14]=1u;
 a.live_components[10]=a.live_components[11]=15u;a.live_components[14]=1u;
 a.source.temp_count=4u;a.source.task_word1=UINT32_C(0x20000000);
 return a;
}
static void original_compiler_oracle(void)
{
 /* Original allocator entry has R0..13, T0; original authored frontend and
  * optimizer leave only R10/R11/T0 live. Original reordering yields R10,T0,R11.
  * Linux psb_dri ELF2383dd..238c46 and2391fe..2395ba are the CPU producers. */
 struct remesa_pds_allocator_request input=original_inputs();
 struct remesa_pds_allocation a;CHECK(!remesa_pds_structure_allocate(&input,&a));
 CHECK(a.logical_words_before==57u&&a.logical_words_after==9u);
 CHECK(a.request.entry_count==3u&&a.request.entries[0].id==10u&&
       a.request.entries[0].kind==REMESA_PDS_STRUCTURE_R&&
       a.request.entries[1].kind==REMESA_PDS_STRUCTURE_T&&a.request.entries[1].id==0u&&
       a.request.entries[2].kind==REMESA_PDS_STRUCTURE_R&&a.request.entries[2].id==11u);
 CHECK(a.source_to_entry[10]==0u&&a.source_to_entry[14]==1u&&a.source_to_entry[11]==2u);
 CHECK(a.entry_to_source[0]==10u&&a.entry_to_source[1]==14u&&a.entry_to_source[2]==11u);
 CHECK(a.entry_base[0]==0u&&a.entry_base[1]==4u&&a.entry_base[2]==5u);
 CHECK(a.source_base[10]==40u&&a.source_base[11]==44u&&a.source_base[14]==56u);
 for(unsigned k=0;k<4u;k++) {
  CHECK(a.logical_map[40u+k]==k);CHECK(a.logical_map[44u+k]==5u+k);
 }
 CHECK(a.logical_map[56]==4u&&a.logical_map[48]==REMESA_PDS_STRUCTURE_NO_CELL);
 CHECK(a.source_to_entry[0]==REMESA_PDS_STRUCTURE_NO_CELL&&
       a.source_to_entry[13]==REMESA_PDS_STRUCTURE_NO_CELL);
 const struct remesa_pds_structure *s=&a.structure;
 CHECK(s->data_dwords==12u&&s->code_slot_count==5u);
 CHECK(s->data[1]==UINT32_C(0x20000000)&&s->data[8]==UINT32_C(0x20)&&
       s->data[9]==UINT32_C(0x0dd0a900)&&s->data[11]==UINT32_C(0x0fd0b00f));
 CHECK(s->code[1].word==UINT32_C(0x070418a2)&&
       s->code[2].word==UINT32_C(0x07042364)&&s->code[3].word==UINT32_C(0x070438a2));
 CHECK(s->relocations[1].data_cell==10u&&s->use_background==UINT32_C(0x180000));
 uint8_t bytes[REMESA_PDS_STRUCTURE_IMAGE_BYTES],mask[sizeof(bytes)];size_t written=0u;
 CHECK(!remesa_pds_structure_serialize(s,bytes,mask,sizeof(bytes),&written));
 const uint8_t code[]={0xa2,0x18,0x04,0x07,0x64,0x23,0x04,0x07,0xa2,0x38,0x04,0x07};
 CHECK(written==68u&&!memcmp(bytes+52u,code,sizeof(code)));
 CHECK(!s->code[0].word&&!s->code[0].known_mask&&!s->code[4].word&&!s->code[4].known_mask);
 CHECK(!s->data_known_mask[0]&&!s->data_known_mask[10]);
}
static void prefix_trim_and_keep_width(void)
{
 struct remesa_pds_allocator_request input={0};input.source.entry_count=4u;
 input.source.entries[0]=r(4u,4u);input.logical_width[0]=4u;input.live_components[0]=5u;
 input.source.entries[1]=r(10u,4u);input.logical_width[1]=4u;input.live_components[1]=1u;
 input.source.entries[2]=t(2u);input.logical_width[2]=3u;input.live_components[2]=2u;
 input.source.entries[3]=r(5u,2u);input.logical_width[3]=2u;input.live_components[3]=0u;
 struct remesa_pds_allocation a;CHECK(!remesa_pds_structure_allocate(&input,&a));
 CHECK(a.logical_words_before==13u&&a.logical_words_after==10u);
 CHECK(a.request.entries[0].id==4u&&a.request.entries[0].components==3u);
 CHECK(a.entry_width[0]==3u&&a.entry_live_components[0]==5u);
 CHECK(a.request.entries[1].kind==REMESA_PDS_STRUCTURE_T&&a.entry_width[1]==3u);
 CHECK(a.request.entries[2].id==10u&&a.request.entries[2].components==4u&&a.entry_width[2]==4u);
 CHECK(a.logical_map[0]==0u&&a.logical_map[1]==1u&&a.logical_map[2]==2u&&
       a.logical_map[3]==REMESA_PDS_STRUCTURE_NO_CELL);
 CHECK(a.logical_map[8]==3u&&a.logical_map[9]==4u&&a.logical_map[10]==5u);
 CHECK(a.logical_map[4]==6u&&a.logical_map[7]==9u);
 CHECK(a.logical_map[11]==REMESA_PDS_STRUCTURE_NO_CELL&&
       a.logical_map[12]==REMESA_PDS_STRUCTURE_NO_CELL);
 /* Interior dead component one stays allocated; color and T retain all
  * their words even when the live bit occurs before the final word. */
 CHECK(a.logical_map[1]!=REMESA_PDS_STRUCTURE_NO_CELL&&a.logical_map[7]!=REMESA_PDS_STRUCTURE_NO_CELL);
 input.live_components[0]=input.live_components[1]=input.live_components[2]=0u;
 CHECK(!remesa_pds_structure_allocate(&input,&a));
 CHECK(!a.request.entry_count&&!a.logical_words_after&&a.logical_words_before==13u);
 CHECK(a.structure.code_slot_count==2u&&!a.structure.a2_count&&!a.structure.class64_count);
}
static void stable_alternation_and_fallback(void)
{
 struct remesa_pds_allocator_request input={0};input.source.entry_count=6u;
 input.source.entries[0]=t(5u);input.source.entries[1]=t(6u);
 input.source.entries[2]=r(3u,1u);input.source.entries[3]=r(4u,1u);
 input.source.entries[4]=r(5u,1u);input.source.entries[5]=t(7u);
 for(unsigned i=0;i<6u;i++)input.logical_width[i]=input.live_components[i]=1u;
 struct remesa_pds_allocation a;CHECK(!remesa_pds_structure_allocate(&input,&a));
 const uint32_t order[]={2u,0u,3u,1u,4u,5u};
 CHECK(a.request.entry_count==6u);
 for(unsigned i=0;i<6u;i++)CHECK(a.entry_to_source[i]==order[i]);
 /* Removing the middle T leaves fallback R selection without reversing R. */
 input.live_components[1]=0u;input.live_components[5]=0u;
 CHECK(!remesa_pds_structure_allocate(&input,&a));
 CHECK(a.request.entry_count==4u&&a.entry_to_source[0]==2u&&
       a.entry_to_source[1]==0u&&a.entry_to_source[2]==3u&&a.entry_to_source[3]==4u);
 input.live_components[2]=input.live_components[3]=input.live_components[4]=0u;
 CHECK(!remesa_pds_structure_allocate(&input,&a));
 CHECK(a.request.entry_count==1u&&a.entry_to_source[0]==0u&&a.logical_map[0]==0u);
}
static void rejection(const struct remesa_pds_allocator_request *input,int expected)
{
 struct remesa_pds_allocation out,before;
 struct remesa_pds_allocator_request input_before=*input;
 memset(&out,0xa5,sizeof(out));before=out;
 CHECK(remesa_pds_structure_allocate(input,&out)==expected);
 CHECK(!memcmp(&out,&before,sizeof(out)));
 CHECK(!memcmp(input,&input_before,sizeof(input_before)));
}
static void maximum_logical_range(void)
{
 struct remesa_pds_allocator_request input={0};
 input.source.entry_count=REMESA_PDS_STRUCTURE_INPUTS;
 for(unsigned i=0;i<REMESA_PDS_STRUCTURE_INPUTS;i++) {
  input.source.entries[i]=r(i,4u);input.logical_width[i]=4u;
  input.live_components[i]=15u;
 }
 struct remesa_pds_allocator_request before=input;
 struct remesa_pds_allocation out;CHECK(!remesa_pds_structure_allocate(&input,&out));
 CHECK(!memcmp(&input,&before,sizeof(input)));
 CHECK(out.logical_words_before==64u&&out.logical_words_after==64u&&
       out.source_base[15]==60u&&out.entry_base[15]==60u);
 uint32_t expected[REMESA_PDS_ALLOCATOR_LOGICAL_WORDS];
 for(unsigned i=0;i<REMESA_PDS_ALLOCATOR_LOGICAL_WORDS;i++)expected[i]=i;
 CHECK(!memcmp(out.logical_map,expected,sizeof(expected)));
 CHECK(out.source_to_entry[15]==15u&&out.entry_to_source[15]==15u);
}
static void validation_and_atomic_failure(void)
{
 struct remesa_pds_allocator_request good=original_inputs(),bad;
 bad=good;bad.flags=1u;rejection(&bad,ENOTSUP);
 bad=good;bad.flags=4u;rejection(&bad,ENOTSUP);
 for(unsigned mode=1u;mode<=3u;mode++) {bad=good;bad.source.entries[0].mode=mode;rejection(&bad,ENOTSUP);}
 bad=good;bad.source.entries[0].kind=99u;rejection(&bad,ENOTSUP);
 bad=good;bad.logical_width[0]=0u;rejection(&bad,ERANGE);
 bad=good;bad.logical_width[0]=5u;rejection(&bad,ERANGE);
 bad=good;bad.logical_width[0]=3u;rejection(&bad,EINVAL);
 bad=good;bad.live_components[14]=2u;rejection(&bad,EINVAL);
 bad=good;bad.logical_width[15]=1u;rejection(&bad,EINVAL);
 bad=good;bad.live_components[15]=1u;rejection(&bad,EINVAL);
 bad=good;bad.source.entries[10].explicit_format=0u;rejection(&bad,EINVAL);
 bad=good;bad.source.entries[14].flag_r=1u;rejection(&bad,EINVAL);
 bad=good;bad.source.task_word1=0u;rejection(&bad,EINVAL);
 bad=good;bad.source.entry_count=17u;rejection(&bad,E2BIG);
 struct remesa_pds_allocation out;
 CHECK(remesa_pds_structure_allocate(NULL,&out)==EINVAL);
 CHECK(remesa_pds_structure_allocate(&good,NULL)==EINVAL);
 union { struct remesa_pds_allocator_request input;struct remesa_pds_allocation output; } alias;
 alias.input=good;CHECK(remesa_pds_structure_allocate(&alias.input,&alias.output)==EINVAL);
 CHECK(remesa_pds_structure_allocate((const struct remesa_pds_allocator_request *)(uintptr_t)(UINTPTR_MAX-3u),&out)==EINVAL);
 CHECK(remesa_pds_structure_allocate(&good,(struct remesa_pds_allocation *)(uintptr_t)(UINTPTR_MAX-3u))==EINVAL);
 union { struct remesa_pds_allocator_request alignment;
         uint8_t bytes[sizeof(struct remesa_pds_allocator_request)+8u]; } unaligned_input;
 memcpy(unaligned_input.bytes+1u,&good,sizeof(good));
 CHECK(remesa_pds_structure_allocate((const struct remesa_pds_allocator_request *)(const void *)(unaligned_input.bytes+1u),&out)==EINVAL);
 union { struct remesa_pds_allocation alignment;
         uint8_t bytes[sizeof(struct remesa_pds_allocation)+8u]; } unaligned_output;
 CHECK(remesa_pds_structure_allocate(&good,(struct remesa_pds_allocation *)(void *)(unaligned_output.bytes+1u))==EINVAL);
 /* Existing extra-R feature remains delegated, not silently part of this
  * input logical mapping. Its unresolved hardware delivery remains unknown. */
 bad=good;bad.source.extra_r=1u;CHECK(!remesa_pds_structure_allocate(&bad,&out));
 CHECK(out.logical_words_after==9u&&out.structure.descriptor_count==3u&&
       out.structure.last_r_cell==12u);
}
int main(void)
{
 original_compiler_oracle();prefix_trim_and_keep_width();stable_alternation_and_fallback();
 maximum_logical_range();validation_and_atomic_failure();
 printf("PDS_R_ALLOCATOR_PASS checks=%u gpu_submissions=0\n",checks);
 return 0;
}
