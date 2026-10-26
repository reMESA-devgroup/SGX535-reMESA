/* SPDX-License-Identifier: MIT
 * CPU-only tests of calculated word classes and sparse serialization.
 * Literal reference bytes below are bounded independent original-x86 oracles,
 * not inputs accepted by the encoder. They deliberately omit initial/end holes.
 */
#include "../remesa_pds_structure.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while(0)

static struct remesa_pds_code_instruction a2(uint32_t index)
{
 struct remesa_pds_code_instruction ir={REMESA_PDS_STRUCTURE_A2,
  REMESA_PDS_STRUCTURE_NO_CELL,index};
 return ir;
}
static struct remesa_pds_code_instruction c64(uint32_t a,uint32_t b)
{
 struct remesa_pds_code_instruction ir={REMESA_PDS_STRUCTURE_CLASS64,a,b};
 return ir;
}
static void original_cpu_oracles(void)
{
 /* Full-constructor owner-B: T descriptor at j=1, texture word at DS0=2,
  * DS1=2, followed by the separate R descriptor j=3. */
 const uint8_t owner_b[]={0xa2,0x18,0x04,0x07,0x64,0x23,0x04,0x07,
                          0xa2,0x38,0x04,0x07};
 struct remesa_pds_code_instruction ir[]={a2(1u),c64(2u,2u),a2(3u)};
 uint8_t bytes[sizeof(owner_b)+3u];
 memset(bytes,0xa5,sizeof(bytes));
 size_t written=SIZE_MAX;
 CHECK(!remesa_pds_structure_encode_code(ir,3u,bytes,sizeof(bytes),&written));
 CHECK(written==sizeof(owner_b)&&!memcmp(bytes,owner_b,sizeof(owner_b)));
 CHECK(bytes[written]==0xa5&&bytes[written+1u]==0xa5&&bytes[written+2u]==0xa5);
 /* Original block replay (not full-constructor inputs): alternating parity
  * and row carry, including the largest admitted nonoverlapping operand. */
 const uint8_t parity[]={0xa2,0x10,0x04,0x07,0xa2,0x18,0x04,0x07,
                         0xa2,0x30,0x04,0x07,0xa2,0x38,0x04,0x07,
                         0xa2,0xf8,0x07,0x07};
 struct remesa_pds_code_instruction rows[]={a2(0u),a2(1u),a2(2u),a2(3u),a2(63u)};
 uint8_t row_bytes[sizeof(parity)];
 CHECK(!remesa_pds_structure_encode_code(rows,5u,row_bytes,sizeof(row_bytes),&written));
 CHECK(written==sizeof(parity)&&!memcmp(row_bytes,parity,sizeof(parity)));
 /* Original TRT case's second texture has both independent indices changed. */
 const uint8_t texture2[]={0x64,0x43,0x08,0x07};
 struct remesa_pds_code_instruction texture=c64(4u,4u);
 CHECK(!remesa_pds_structure_encode_code(&texture,1u,bytes,sizeof(bytes),&written));
 CHECK(written==4u&&!memcmp(bytes,texture2,4u));
 /* Independent producer: original Xpsb82c0, T=1 and T=2 replays read
  * (a,b)=(2,1),(4,3) at ELF89cf/89d2. A variable field crosses the
  * low byte: e4 is valid and must not be rejected as a different class. */
 const uint8_t xpsb_odd[]={0xe4,0x03,0x04,0x07,0xe4,0x23,0x08,0x07};
 struct remesa_pds_code_instruction odd[]={c64(2u,1u),c64(4u,3u)};
 CHECK(!remesa_pds_structure_encode_code(odd,2u,bytes,sizeof(bytes),&written));
 CHECK(written==sizeof(xpsb_odd)&&!memcmp(bytes,xpsb_odd,sizeof(xpsb_odd)));
}
static void code_rejection_is_atomic(void)
{
 struct remesa_pds_code_instruction ir[]={a2(1u),a2(3u)};
 uint8_t bytes[16],saved[16];
 memset(bytes,0x6d,sizeof(bytes));memcpy(saved,bytes,sizeof(bytes));
 size_t written=123u;
 CHECK(remesa_pds_structure_encode_code(ir,2u,bytes,7u,&written)==ENOSPC);
 CHECK(written==123u&&!memcmp(bytes,saved,sizeof(bytes)));
 ir[1].ds1_index=64u;
 CHECK(remesa_pds_structure_encode_code(ir,2u,bytes,sizeof(bytes),&written)==ERANGE);
 CHECK(written==123u&&!memcmp(bytes,saved,sizeof(bytes)));
 ir[1]=c64(64u,0u);
 CHECK(remesa_pds_structure_encode_code(ir,2u,bytes,sizeof(bytes),&written)==ERANGE);
 ir[1]=c64(0u,64u);
 CHECK(remesa_pds_structure_encode_code(ir,2u,bytes,sizeof(bytes),&written)==ERANGE);
 ir[1]=a2(3u);ir[1].ds0_index=0u;
 CHECK(remesa_pds_structure_encode_code(ir,2u,bytes,sizeof(bytes),&written)==EINVAL);
 const uint32_t unsupported[]={REMESA_PDS_STRUCTURE_INITIAL_HOLE,
  REMESA_PDS_STRUCTURE_FINAL_HOLE,0u,UINT32_MAX};
 for(size_t i=0;i<sizeof(unsupported)/sizeof(unsupported[0]);i++) {
  ir[1]=a2(3u);ir[1].kind=unsupported[i];
  CHECK(remesa_pds_structure_encode_code(ir,2u,bytes,sizeof(bytes),&written)==ENOTSUP);
  CHECK(written==123u&&!memcmp(bytes,saved,sizeof(bytes)));
 }
 CHECK(remesa_pds_structure_encode_code(NULL,1u,bytes,sizeof(bytes),&written)==EINVAL);
 CHECK(remesa_pds_structure_encode_code(ir,1u,NULL,0u,&written)==ENOSPC);
 CHECK(remesa_pds_structure_encode_code(ir,1u,bytes,sizeof(bytes),NULL)==EINVAL);
 CHECK(remesa_pds_structure_encode_code(ir,36u,bytes,sizeof(bytes),&written)==E2BIG);
 CHECK(!remesa_pds_structure_encode_code(NULL,0u,NULL,0u,&written)&&!written);
}
static void code_alias_and_wrap(void)
{
 union { struct remesa_pds_code_instruction ir[3]; size_t written;
         uint8_t bytes[64]; } alias;
 memset(&alias,0,sizeof(alias));alias.ir[0]=a2(1u);
 size_t written=123u;
 CHECK(remesa_pds_structure_encode_code(alias.ir,1u,alias.bytes,
       sizeof(alias.bytes),&written)==EINVAL&&written==123u);
 uint8_t bytes[16];memset(bytes,0x6d,sizeof(bytes));
 CHECK(remesa_pds_structure_encode_code(alias.ir,1u,bytes,
       sizeof(bytes),&alias.written)==EINVAL);
 CHECK(remesa_pds_structure_encode_code(alias.ir,1u,alias.bytes,
       sizeof(alias.bytes),&alias.written)==EINVAL);
 CHECK(remesa_pds_structure_encode_code(alias.ir,1u,bytes,sizeof(bytes),
       (size_t *)(void *)bytes)==EINVAL);
 CHECK(remesa_pds_structure_encode_code(alias.ir,1u,
       (uint8_t *)(uintptr_t)(UINTPTR_MAX-1u),16u,&written)==EINVAL);
 CHECK(remesa_pds_structure_encode_code(
       (const struct remesa_pds_code_instruction *)(uintptr_t)(UINTPTR_MAX-3u),
       1u,bytes,sizeof(bytes),&written)==EINVAL);
 CHECK(remesa_pds_structure_encode_code(alias.ir,1u,bytes,sizeof(bytes),
       (size_t *)(uintptr_t)(UINTPTR_MAX-1u))==EINVAL);
 for(size_t i=0;i<sizeof(bytes);i++)CHECK(bytes[i]==0x6d);
}
static struct remesa_pds_structure owner_b(void)
{
 struct remesa_pds_structure_request request={0};
 struct remesa_pds_structure s;
 request.entry_count=1u;request.extra_r=1u;
 request.entries[0].kind=REMESA_PDS_STRUCTURE_T;
 request.entries[0].id=4u;
 request.entries[0].state_words[0]=UINT32_C(0x001e0002);
 request.entries[0].state_words[1]=UINT32_C(0x60000000);
 request.entries[0].reloc_arg3=UINT32_C(0x100);
 request.entries[0].reloc_arg4=UINT32_C(0x200);
 CHECK(!remesa_pds_structure_encode(&request,&s));
 return s;
}
static void sparse_serialization(void)
{
 struct remesa_pds_structure s=owner_b();
 uint8_t bytes[REMESA_PDS_STRUCTURE_IMAGE_BYTES+1u];
 uint8_t masks[sizeof(bytes)];
 memset(bytes,0x4d,sizeof(bytes));memset(masks,0x8e,sizeof(masks));
 size_t written=123u;
 CHECK(!remesa_pds_structure_serialize(&s,bytes,masks,sizeof(bytes),&written));
 CHECK(written==68u&&bytes[written]==0x4d&&masks[written]==0x8e);
 /* Independent historical owner-B DATA descriptor and emitted word bytes. */
 const uint8_t data_t[]={0x04,0xf9,0x00,0x0c};
 const uint8_t data_r[]={0x0f,0xe0,0x80,0x0f};
 const uint8_t calculated[]={0xa2,0x18,0x04,0x07,0x64,0x23,0x04,0x07,
                            0xa2,0x38,0x04,0x07};
 CHECK(!memcmp(bytes+36u,data_t,4u)&&!memcmp(bytes+44u,data_r,4u));
 CHECK(!memcmp(bytes+52u,calculated,sizeof(calculated)));
 for(size_t i=0;i<4u;i++) {
  CHECK(!bytes[i]&&!masks[i]);                 /* symbolic USE relocation */
  CHECK(!bytes[40u+i]&&!masks[40u+i]);         /* symbolic texture relocation */
  CHECK(!bytes[48u+i]&&!masks[48u+i]);         /* initial hole */
  CHECK(!bytes[64u+i]&&!masks[64u+i]);         /* final hole */
  CHECK(masks[36u+i]==0xff&&masks[44u+i]==0xff);
 }
 for(size_t i=52u;i<64u;i++)CHECK(masks[i]==0xff);
 CHECK(s.relocations[0].background==UINT32_C(0x180000));
 CHECK(s.relocations[1].data_cell==10u&&s.relocations[1].arg3==0x100u&&
       s.relocations[1].arg4==0x200u);
 /* Arbitrarily aligned output is valid because output is byte addressed. */
 CHECK(!remesa_pds_structure_serialize(&s,bytes+1u,masks+1u,sizeof(bytes)-1u,&written));
}
static void serialization_negative(void)
{
 struct remesa_pds_structure valid=owner_b(),bad;
 uint8_t bytes[REMESA_PDS_STRUCTURE_IMAGE_BYTES],masks[sizeof(bytes)];
 memset(bytes,0x4d,sizeof(bytes));memset(masks,0x8e,sizeof(masks));
 size_t written=123u;
 CHECK(remesa_pds_structure_serialize(&valid,bytes,masks,67u,&written)==ENOSPC);
 for(unsigned test=0;test<16u;test++) {
  bad=valid;
  switch(test) {
  case 0: bad.data_dwords=UINT32_MAX;break;
  case 1: bad.code_slot_count=36u;break;
  case 2: bad.code[1].word^=1u;break;
  case 3: bad.code[1].byte_offset++;break;
  case 4: bad.code[1].ds1_index=64u;break;
  case 5: bad.code[0].word=1u;break;
  case 6: bad.code[0].known_mask=UINT32_MAX;break;
  case 7: bad.data_known_mask[0]=UINT32_MAX;break;
  case 8: bad.data_known_mask[9]=0u;break;
  case 9: bad.data[10]=1u;break;
  case 10: bad.relocations[1].data_cell=128u;break;
  case 11: bad.relocations[0].background=0u;break;
  case 12: bad.last_r_cell=UINT32_MAX;break;
  case 13: bad.descriptors[0].ds1_index=UINT32_MAX;break;
  case 14: bad.descriptor_count=18u;break;
  default: bad.a2_count++;break;
  }
  CHECK(remesa_pds_structure_serialize(&bad,bytes,masks,sizeof(bytes),&written)==EINVAL);
  CHECK(written==123u);
  for(size_t i=0;i<sizeof(bytes);i++)CHECK(bytes[i]==0x4d&&masks[i]==0x8e);
 }
 bad=valid;bad.code[1].kind=UINT32_MAX;
 CHECK(remesa_pds_structure_serialize(&bad,bytes,masks,sizeof(bytes),&written)==ENOTSUP);
 CHECK(remesa_pds_structure_serialize(&valid,bytes,bytes,sizeof(bytes),&written)==EINVAL);
 CHECK(remesa_pds_structure_serialize(&valid,(uint8_t *)(void *)&valid,
       masks,sizeof(bytes),&written)==EINVAL);
 CHECK(remesa_pds_structure_serialize(&valid,bytes,masks,sizeof(bytes),
       (size_t *)(void *)&valid)==EINVAL);
 CHECK(remesa_pds_structure_serialize(&valid,bytes,masks,sizeof(bytes),
       (size_t *)(void *)masks)==EINVAL);
 CHECK(remesa_pds_structure_serialize(&valid,bytes,
       (uint8_t *)(uintptr_t)(UINTPTR_MAX-1u),sizeof(bytes),&written)==EINVAL);
 CHECK(remesa_pds_structure_serialize(
       (const struct remesa_pds_structure *)(uintptr_t)(UINTPTR_MAX-3u),
       bytes,masks,sizeof(bytes),&written)==EINVAL);
 CHECK(written==123u);
}
static void layouts_and_boundaries(void)
{
 struct remesa_pds_structure_request request={0};
 struct remesa_pds_structure s;
 uint8_t bytes[REMESA_PDS_STRUCTURE_IMAGE_BYTES],masks[sizeof(bytes)];
 size_t written;
 CHECK(!remesa_pds_structure_encode(&request,&s));
 CHECK(!remesa_pds_structure_serialize(&s,bytes,masks,sizeof(bytes),&written));
 CHECK(written==56u);
 request.entry_count=REMESA_PDS_STRUCTURE_INPUTS;request.extra_r=1u;
 for(unsigned i=0;i<request.entry_count;i++) {
  request.entries[i].kind=REMESA_PDS_STRUCTURE_T;
  request.entries[i].id=i;
 }
 CHECK(!remesa_pds_structure_encode(&request,&s));
 CHECK(!remesa_pds_structure_serialize(&s,bytes,masks,sizeof(bytes),&written));
 CHECK(written==444u&&s.data_dwords==76u&&s.code_slot_count==35u);
 for(uint32_t i=0;i<s.relocation_count;i++) {
  CHECK(s.relocations[i].data_cell<s.data_dwords);
  CHECK(!masks[s.relocations[i].data_cell*4u]);
 }
}
int main(void)
{
 original_cpu_oracles();code_rejection_is_atomic();code_alias_and_wrap();
 sparse_serialization();serialization_negative();layouts_and_boundaries();
 puts("PDS CODE subset: independent CPU oracles, transactional IR encoding, sparse DATA/CODE, fixups and bounds passed");
 return 0;
}
