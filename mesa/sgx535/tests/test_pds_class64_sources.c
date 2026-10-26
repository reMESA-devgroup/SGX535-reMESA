/* SPDX-License-Identifier: MIT
 * CPU-only DATA-reference tests. Byte literals labelled original below are
 * observations from independent original constructors, not encoder inputs.
 * Row/bounds tests exercise API invariants; they do not prove GPU reads. */
#include "../remesa_pds_structure.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if(!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while(0)

static struct remesa_pds_texture_output refs(uint32_t order,uint32_t a,uint32_t b)
{
 uint32_t j=order==REMESA_PDS_TEXTURE_DESCRIPTOR_BEFORE_ADDRESS?b-1u:b+1u;
 struct remesa_pds_texture_output r={order,
  {(a/8u)*16u+a%8u,((a+1u)/8u)*16u+(a+1u)%8u},
  (b/8u)*16u+b%8u+8u,(j/8u)*16u+j%8u+8u};
 return r;
}
static void original_constructor_bytes(void)
{
 /* Distinct production constructors: fixed a=2, b=1 in Xpsb82c0 versus
  * b=2 in psb25970. This is cross-producer evidence, not independent b
  * variation within the same input program. Xpsb low byte e4 is expected. */
 const struct remesa_pds_texture_output source[]={
  {REMESA_PDS_TEXTURE_DESCRIPTOR_AFTER_ADDRESS,{2u,3u},9u,10u},
  {REMESA_PDS_TEXTURE_DESCRIPTOR_BEFORE_ADDRESS,{2u,3u},10u,9u},
  {REMESA_PDS_TEXTURE_DESCRIPTOR_AFTER_ADDRESS,{4u,5u},11u,12u},
  {REMESA_PDS_TEXTURE_DESCRIPTOR_BEFORE_ADDRESS,{4u,5u},12u,11u}
 };
 const uint8_t original[4][8]={
  {0xa2,0x30,0x04,0x07,0xe4,0x03,0x04,0x07},
  {0xa2,0x18,0x04,0x07,0x64,0x23,0x04,0x07},
  {0xa2,0x50,0x04,0x07,0xe4,0x23,0x08,0x07},
  {0xa2,0x38,0x04,0x07,0x64,0x43,0x08,0x07}
 };
 for(size_t i=0;i<4u;i++) {
  uint8_t bytes[11];memset(bytes,0x6d,sizeof(bytes));size_t written=111u;
  CHECK(!remesa_pds_structure_encode_texture_output(&source[i],16u,
        bytes,sizeof(bytes),&written));
  CHECK(written==8u&&!memcmp(bytes,original[i],8u));
  CHECK(bytes[8]==0x6d&&bytes[9]==0x6d&&bytes[10]==0x6d);
 }
}
static void rejected(struct remesa_pds_texture_output r,uint32_t extent,
 int expected)
{
 uint8_t bytes[12],before[12];memset(bytes,0x6d,sizeof(bytes));
 memcpy(before,bytes,sizeof(bytes));size_t written=111u;
 CHECK(remesa_pds_structure_encode_texture_output(&r,extent,
       bytes,sizeof(bytes),&written)==expected);
 CHECK(written==111u&&!memcmp(bytes,before,sizeof(bytes)));
}
static void logical_row_mapping(void)
{
 /* Logical neighbour 7->8 is physical cell15->24, not cell15->16.
  * This tests the CPU mapping invariant, not an independent byte oracle. */
 struct remesa_pds_texture_output after=refs(
  REMESA_PDS_TEXTURE_DESCRIPTOR_AFTER_ADDRESS,2u,7u);
 CHECK(after.texture_cell==15u&&after.descriptor_cell==24u);
 uint8_t bytes[8],raw[8];size_t written=111u,raw_written=111u;
 struct remesa_pds_code_instruction ir[]={
  {REMESA_PDS_STRUCTURE_A2,REMESA_PDS_STRUCTURE_NO_CELL,8u},
  {REMESA_PDS_STRUCTURE_CLASS64,2u,7u}};
 CHECK(!remesa_pds_structure_encode_texture_output(&after,28u,
       bytes,sizeof(bytes),&written));
 CHECK(!remesa_pds_structure_encode_code(ir,2u,raw,sizeof(raw),&raw_written));
 CHECK(written==raw_written&&!memcmp(bytes,raw,sizeof(raw)));
 after.descriptor_cell=after.texture_cell+1u;rejected(after,28u,EINVAL);
 struct remesa_pds_texture_output before=refs(
  REMESA_PDS_TEXTURE_DESCRIPTOR_BEFORE_ADDRESS,8u,8u);
 CHECK(before.state_cells[0]==16u&&before.state_cells[1]==17u&&
       before.descriptor_cell==15u&&before.texture_cell==24u);
 CHECK(!remesa_pds_structure_encode_texture_output(&before,28u,
       bytes,sizeof(bytes),&written));
 before.texture_cell=before.descriptor_cell+1u;rejected(before,28u,EINVAL);
 /* Largest admitted even pair reaches bank-0 logical63; this is policy,
  * not a claim that 63 is an architectural DS limit. */
 struct remesa_pds_texture_output last=refs(
  REMESA_PDS_TEXTURE_DESCRIPTOR_BEFORE_ADDRESS,62u,63u);
 CHECK(!remesa_pds_structure_encode_texture_output(&last,128u,
       bytes,sizeof(bytes),&written));
 last=refs(REMESA_PDS_TEXTURE_DESCRIPTOR_AFTER_ADDRESS,62u,63u);
 rejected(last,128u,ERANGE);
}
static void malformed_references(void)
{
 struct remesa_pds_texture_output good=refs(
  REMESA_PDS_TEXTURE_DESCRIPTOR_BEFORE_ADDRESS,2u,2u),bad;
 bad=good;bad.order=0u;rejected(bad,16u,ENOTSUP);
 bad=good;bad.order=UINT32_MAX;rejected(bad,16u,ENOTSUP);
 bad=good;bad.state_cells[0]=10u;rejected(bad,16u,EINVAL);
 bad=good;bad.state_cells[1]=11u;rejected(bad,16u,EINVAL);
 bad=good;bad.texture_cell=2u;rejected(bad,16u,EINVAL);
 bad=good;bad.descriptor_cell=3u;rejected(bad,16u,EINVAL);
 bad=good;bad.state_cells[0]=3u;bad.state_cells[1]=4u;rejected(bad,16u,EINVAL);
 bad=good;bad.state_cells[1]=4u;rejected(bad,16u,EINVAL);
 bad=good;bad.state_cells[0]=0u;bad.state_cells[1]=1u;rejected(bad,16u,EINVAL);
 bad=good;bad.texture_cell=8u;bad.descriptor_cell=9u;
 bad.order=REMESA_PDS_TEXTURE_DESCRIPTOR_AFTER_ADDRESS;rejected(bad,16u,EINVAL);
 bad=good;bad.descriptor_cell=8u;bad.texture_cell=9u;rejected(bad,16u,EINVAL);
 bad=good;bad.descriptor_cell=bad.texture_cell;rejected(bad,16u,EINVAL);
 bad=good;bad.descriptor_cell=11u;rejected(bad,16u,EINVAL);
 bad=good;bad.texture_cell=16u;rejected(bad,16u,ERANGE);
 bad=good;bad.state_cells[1]=UINT32_MAX;rejected(bad,16u,ERANGE);
 rejected(good,0u,ERANGE);rejected(good,8u,ERANGE);
 rejected(good,13u,EINVAL);rejected(good,132u,ERANGE);
 bad=refs(REMESA_PDS_TEXTURE_DESCRIPTOR_BEFORE_ADDRESS,4u,4u);
 rejected(bad,12u,ERANGE);
 /* Raw CPU expressions deliberately retain their previous operand policy. */
 uint32_t word=0u;CHECK(!remesa_pds_structure_class64(3u,0u,&word));
}
static void alias_and_capacity(void)
{
 struct remesa_pds_texture_output good=refs(
  REMESA_PDS_TEXTURE_DESCRIPTOR_BEFORE_ADDRESS,2u,2u);
 uint8_t bytes[32];memset(bytes,0x6d,sizeof(bytes));size_t written=111u;
 CHECK(remesa_pds_structure_encode_texture_output(&good,16u,bytes,7u,
       &written)==ENOSPC&&written==111u);
 CHECK(remesa_pds_structure_encode_texture_output(&good,16u,NULL,0u,
       &written)==ENOSPC&&written==111u);
 CHECK(remesa_pds_structure_encode_texture_output(NULL,16u,bytes,
       sizeof(bytes),&written)==EINVAL);
 CHECK(remesa_pds_structure_encode_texture_output(&good,16u,bytes,
       sizeof(bytes),NULL)==EINVAL);
 union { struct remesa_pds_texture_output output; size_t written;
         uint8_t bytes[64]; } alias;
 memset(&alias,0,sizeof(alias));alias.output=good;
 CHECK(remesa_pds_structure_encode_texture_output(&alias.output,16u,
       alias.bytes,sizeof(alias.bytes),&written)==EINVAL);
 CHECK(remesa_pds_structure_encode_texture_output(&alias.output,16u,
       bytes,sizeof(bytes),&alias.written)==EINVAL);
 CHECK(remesa_pds_structure_encode_texture_output(&good,16u,
       alias.bytes,sizeof(alias.bytes),&alias.written)==EINVAL);
 CHECK(remesa_pds_structure_encode_texture_output(&good,16u,
       (uint8_t *)(uintptr_t)(UINTPTR_MAX-3u),8u,&written)==EINVAL);
 CHECK(remesa_pds_structure_encode_texture_output(
       (const struct remesa_pds_texture_output *)(uintptr_t)(UINTPTR_MAX-3u),
       16u,bytes,sizeof(bytes),&written)==EINVAL);
 CHECK(remesa_pds_structure_encode_texture_output(&good,16u,bytes,
       sizeof(bytes),(size_t *)(uintptr_t)(UINTPTR_MAX-3u))==EINVAL);
 CHECK(written==111u);
 for(size_t i=0;i<sizeof(bytes);i++)CHECK(bytes[i]==0x6d);
}
static void existing_structure_consumer(void)
{
 /* Four real structural T inputs place the final descriptor/address across
  * a DATA row. The serializer must consume the same DATA-reference grammar. */
 struct remesa_pds_structure_request request={0};
 request.entry_count=4u;
 for(unsigned i=0;i<4u;i++) {
  request.entries[i].kind=REMESA_PDS_STRUCTURE_T;
  request.entries[i].id=i;
  request.entries[i].state_words[0]=UINT32_C(0x100)+i;
  request.entries[i].state_words[1]=UINT32_C(0x200)+i;
 }
 struct remesa_pds_structure s;CHECK(!remesa_pds_structure_encode(&request,&s));
 CHECK(s.descriptors[3].data_cell==15u&&s.relocations[4].data_cell==24u);
 uint8_t bytes[REMESA_PDS_STRUCTURE_IMAGE_BYTES],mask[sizeof(bytes)];
 size_t written=111u;
 CHECK(!remesa_pds_structure_serialize(&s,bytes,mask,sizeof(bytes),&written));
 struct remesa_pds_texture_output output={
  REMESA_PDS_TEXTURE_DESCRIPTOR_BEFORE_ADDRESS,{16u,17u},24u,15u};
 uint8_t pair[8];size_t pair_written=111u;
 CHECK(!remesa_pds_structure_encode_texture_output(&output,s.data_dwords,
       pair,sizeof(pair),&pair_written));
 size_t offset=s.code[7].byte_offset;
 CHECK(pair_written==8u&&offset+8u<=written&&!memcmp(bytes+offset,pair,8u));
 s.relocations[4].data_cell=16u;
 CHECK(remesa_pds_structure_serialize(&s,bytes,mask,sizeof(bytes),&written)==EINVAL);
}
int main(void)
{
 original_constructor_bytes();logical_row_mapping();malformed_references();
 alias_and_capacity();existing_structure_consumer();
 printf("PDS_CLASS64_DATA_SOURCES_PASS checks=%u gpu_submissions=0\n",checks);
 return 0;
}
