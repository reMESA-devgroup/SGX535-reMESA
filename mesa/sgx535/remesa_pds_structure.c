/* SPDX-License-Identifier: MIT
 * Original bounded reconstruction from CPU def-use observations. See header.
 * This file supplies no physical PDS source/port, dispatch or end semantics.
 */
#include "remesa_pds_structure.h"
#include <errno.h>
#include <stdbool.h>
#include <string.h>

static int structure_valid(const struct remesa_pds_structure *);
static uint32_t cell0(uint32_t index)
{
 return (index / 8u) * 16u + index % 8u;
}
static uint32_t cell1(uint32_t index)
{
 return cell0(index) + 8u;
}
static int disjoint(const void *a,size_t a_size,const void *b,size_t b_size)
{
 uintptr_t aa=(uintptr_t)a,bb=(uintptr_t)b;
 if(aa>UINTPTR_MAX-a_size||bb>UINTPTR_MAX-b_size)return 0;
 return aa+a_size<=bb||bb+b_size<=aa;
}
int remesa_pds_structure_a2(uint32_t j,uint32_t *word)
{
 if(!word)return EINVAL;
 if(j>63u)return ERANGE;
 *word=UINT32_C(0x070400a2)|((j>>1u)<<13u)|(((j&1u)+2u)<<11u);
 return 0;
}
int remesa_pds_structure_class64(uint32_t a,uint32_t b,uint32_t *word)
{
 if(!word)return EINVAL;
 if(a>63u||b>63u)return ERANGE;
 *word=UINT32_C(0x07000064)|((a>>1u)<<18u)|((b>>1u)<<13u)|
       ((a&1u)<<11u)|(((a+1u)&1u)<<9u)|(((b&1u)+2u)<<7u);
 return 0;
}
/* Invert the CPU writer's interleaving, not a hypothetical GPU register map.
 * Checking the bank before removing its offset prevents cell7/cell8 aliasing. */
static int data_index(uint32_t cell,uint32_t bank,uint32_t data_dwords,
 uint32_t *index)
{
 if(cell>=data_dwords)return ERANGE;
 uint32_t lane=cell%16u;
 if(lane/8u!=bank)return EINVAL;
 *index=(cell/16u)*8u+lane%8u;
 return *index>63u?ERANGE:0;
}
static int texture_output_instructions(
 const struct remesa_pds_texture_output *output,uint32_t data_dwords,
 struct remesa_pds_code_instruction ir[2])
{
 if(data_dwords<12u||data_dwords>REMESA_PDS_STRUCTURE_DATA_CELLS)
  return ERANGE;
 if(data_dwords&3u)return EINVAL;
 if(output->order!=REMESA_PDS_TEXTURE_DESCRIPTOR_BEFORE_ADDRESS&&
    output->order!=REMESA_PDS_TEXTURE_DESCRIPTOR_AFTER_ADDRESS)return ENOTSUP;
 uint32_t a,a_next,b,j;
 int error=data_index(output->state_cells[0],0u,data_dwords,&a);
 if(error)return error;
 error=data_index(output->state_cells[1],0u,data_dwords,&a_next);
 if(error)return error;
 error=data_index(output->texture_cell,1u,data_dwords,&b);
 if(error)return error;
 error=data_index(output->descriptor_cell,1u,data_dwords,&j);
 if(error)return error;
 /* DATA0/1 and DATA8 belong to task setup in both audited constructors.
  * Only the observed even state-pair producer is accepted by this API. */
 if(a<2u||(a&1u)||a_next!=a+1u||!b||!j)return EINVAL;
 if(output->order==REMESA_PDS_TEXTURE_DESCRIPTOR_BEFORE_ADDRESS) {
  if(j+1u!=b)return EINVAL;
 } else if(b+1u!=j)return EINVAL;
 ir[0]=(struct remesa_pds_code_instruction){REMESA_PDS_STRUCTURE_A2,
  REMESA_PDS_STRUCTURE_NO_CELL,j};
 ir[1]=(struct remesa_pds_code_instruction){REMESA_PDS_STRUCTURE_CLASS64,a,b};
 return 0;
}
static int instruction_word(const struct remesa_pds_code_instruction *ir,
 uint32_t *word);
static int entry_valid(const struct remesa_pds_structure_entry *e)
{
 if(e->id>255u)return ERANGE;
 if(e->kind==REMESA_PDS_STRUCTURE_R) {
  if(e->components<1u||e->components>4u||e->mode>255u||e->flag_r>1u||
     e->format_present>1u)return ERANGE;
  if(e->flag_a||e->flag_b||e->state_words[0]||e->state_words[1]||
     e->reloc_arg3||e->reloc_arg4)return EINVAL;
  if(e->id==10u||e->id==11u) {
   if(!e->format_present)return EINVAL;
  } else if(e->format_present||e->explicit_format)return EINVAL;
 } else if(e->kind==REMESA_PDS_STRUCTURE_T) {
  if(e->flag_a>1u||e->flag_b>1u)return ERANGE;
  if(e->components||e->mode||e->flag_r||e->format_present||e->explicit_format)
   return EINVAL;
 } else return EINVAL;
 return 0;
}
static uint32_t r_fields(const struct remesa_pds_structure_entry *r)
{
 uint32_t format=(r->id==10u||r->id==11u)?r->explicit_format:UINT32_C(0x0c000000);
 return UINT32_C(0x01000000)|(r->id<<12u)|((r->components-1u)<<22u)|format|
        (r->flag_r?UINT32_C(0x00010000):0u)|
        (r->mode==3u?0u:UINT32_C(0x00100000));
}
static uint32_t t_fields(const struct remesa_pds_structure_entry *t)
{
 return t->id|(t->flag_a?UINT32_C(0x10):0u)|
        (t->flag_b?UINT32_C(0x200):UINT32_C(0x100));
}
static void write_cell(struct remesa_pds_structure *o,uint32_t cell,uint32_t word)
{
 o->data[cell]=word;
 o->data_known_mask[cell]=UINT32_MAX;
}
static void add_descriptor(struct remesa_pds_structure *o,uint32_t first_input,
 const struct remesa_pds_structure_entry *r,const struct remesa_pds_structure_entry *t,
 uint32_t *next0,uint32_t *next1)
{
 struct remesa_pds_structure_descriptor *d=&o->descriptors[o->descriptor_count++];
 d->first_input=first_input;
 d->has_r=r!=NULL;
 d->has_t=t!=NULL;
 d->ds1_index=*next1;
 d->data_cell=cell1(*next1);
 d->ds0_index=t?*next0:REMESA_PDS_STRUCTURE_NO_CELL;
 uint32_t value;
 if(r)value=r_fields(r)|(t?t_fields(t):UINT32_C(0xf));
 else value=UINT32_C(0x0c00f000)|t_fields(t);
 write_cell(o,d->data_cell,value);
 if(r)o->last_r_cell=d->data_cell;
 if(t) {
  struct remesa_pds_structure_relocation *reloc=&o->relocations[o->relocation_count++];
  write_cell(o,cell0(*next0),t->state_words[0]);
  write_cell(o,cell0(*next0+1u),t->state_words[1]);
  reloc->kind=REMESA_PDS_STRUCTURE_TEXTURE;
  reloc->data_cell=cell1(*next1+1u);
  reloc->arg3=t->reloc_arg3;
  reloc->arg4=t->reloc_arg4;
  /* The site is symbolic; helper arguments are retained without pretending
   * that these CPU-side identifiers constitute a resolved device address. */
  o->last_t_cell=d->data_cell;
  *next0+=2u;
  *next1+=2u;
 } else (*next1)++;
}
static void append_slot(struct remesa_pds_structure *o,uint32_t kind,
 uint32_t word,uint32_t ds0,uint32_t ds1)
{
 struct remesa_pds_structure_slot *s=&o->code[o->code_slot_count];
 s->kind=kind;
 s->word=word;
 s->known_mask=(kind==REMESA_PDS_STRUCTURE_A2||kind==REMESA_PDS_STRUCTURE_CLASS64)?UINT32_MAX:0u;
 s->byte_offset=(o->data_dwords+o->code_slot_count)*4u;
 s->ds0_index=ds0;
 s->ds1_index=ds1;
 o->code_slot_count++;
}
static int request_valid(const struct remesa_pds_structure_request *request)
{
 if(request->code_policy>REMESA_PDS_STRUCTURE_CODE_CPU_COMPLETE)return ENOTSUP;
 if(request->entry_count>REMESA_PDS_STRUCTURE_INPUTS)return E2BIG;
 if(request->extra_r>1u)return EINVAL;
 /* The packed word must retain the original owner.temp_count OR. Other
  * caller bits remain opaque; this is a CPU consistency requirement. */
 uint32_t temp_bits=request->temp_count<<27u;
 if((request->task_word1&temp_bits)!=temp_bits)return EINVAL;
 uint32_t shared_format=0u;
 int have_format=0;
 for(uint32_t i=0;i<request->entry_count;i++) {
  const struct remesa_pds_structure_entry *e=&request->entries[i];
  int error=entry_valid(e);
  if(error)return error;
  /* ids10/11 both read the SAME state+0x124 in one original invocation. */
  if(e->format_present) {
   if(have_format&&shared_format!=e->explicit_format)return EINVAL;
   shared_format=e->explicit_format;
   have_format=1;
  }
 }
 return 0;
}
int remesa_pds_structure_encode(const struct remesa_pds_structure_request *request,
 struct remesa_pds_structure *out)
{
 if(!request||!out)return EINVAL;
 if(!disjoint(request,sizeof(*request),out,sizeof(*out)))return EINVAL;
 int error=request_valid(request);
 if(error)return error;
 struct remesa_pds_structure o;
 memset(&o,0,sizeof(o));
 o.last_r_cell=REMESA_PDS_STRUCTURE_NO_CELL;
 o.last_t_cell=REMESA_PDS_STRUCTURE_NO_CELL;
 /* Reserve the one USE relocation. DATA0 remains deliberately unresolved. */
 o.relocation_count=1u;
 o.relocations[0].kind=REMESA_PDS_STRUCTURE_USE;
 uint32_t next0=2u,next1=1u;
 for(uint32_t i=0;i<request->entry_count;i++) {
  const struct remesa_pds_structure_entry *r=NULL,*t=NULL;
  uint32_t first=i;
  if(request->entries[i].kind==REMESA_PDS_STRUCTURE_R) {
   r=&request->entries[i];
   if(i+1u<request->entry_count&&request->entries[i+1u].kind==REMESA_PDS_STRUCTURE_T)
    t=&request->entries[++i];
  } else t=&request->entries[i];
  add_descriptor(&o,first,r,t,&next0,&next1);
 }
 if(request->extra_r) {
  struct remesa_pds_structure_entry extra;
  memset(&extra,0,sizeof(extra));
  extra.kind=REMESA_PDS_STRUCTURE_R;
  extra.id=14u;
  extra.components=3u;
  extra.mode=3u;
  add_descriptor(&o,request->entry_count,&extra,NULL,&next0,&next1);
 }
 if(o.last_t_cell!=REMESA_PDS_STRUCTURE_NO_CELL) {
  o.data[o.last_t_cell]|=UINT32_C(0x800);
  o.use_background|=UINT32_C(0x100000);
 }
 if(o.last_r_cell!=REMESA_PDS_STRUCTURE_NO_CELL) {
  o.data[o.last_r_cell]|=UINT32_C(0x02000000);
  o.use_background|=UINT32_C(0x80000);
 }
 o.relocations[0].background=o.use_background;
 write_cell(&o,1u,request->task_word1);
 write_cell(&o,8u,(request->temp_count>>5u)|UINT32_C(0x20));
 uint32_t limit0=cell0(next0),limit1=cell1(next1);
 o.data_dwords=((limit0>limit1?limit0:limit1)+3u)&~UINT32_C(3);
 append_slot(&o,REMESA_PDS_STRUCTURE_INITIAL_HOLE,0u,
             REMESA_PDS_STRUCTURE_NO_CELL,REMESA_PDS_STRUCTURE_NO_CELL);
 uint32_t texture_reloc=1u;
 for(uint32_t i=0;i<o.descriptor_count;i++) {
  const struct remesa_pds_structure_descriptor *d=&o.descriptors[i];
  uint32_t word;
  if(d->has_t) {
   const struct remesa_pds_texture_output output={
    REMESA_PDS_TEXTURE_DESCRIPTOR_BEFORE_ADDRESS,
    {cell0(d->ds0_index),cell0(d->ds0_index+1u)},
    o.relocations[texture_reloc++].data_cell,d->data_cell};
   struct remesa_pds_code_instruction ir[2];
   int error=texture_output_instructions(&output,o.data_dwords,ir);
   if(error)return error;
   for(uint32_t k=0;k<2u;k++) {
    error=instruction_word(&ir[k],&word);
    if(error)return error;
    append_slot(&o,ir[k].kind,word,ir[k].ds0_index,ir[k].ds1_index);
   }
   o.class64_count++;
  } else {
   /* Internal indexes are bounded by the admitted sixteen source entries. */
   if(remesa_pds_structure_a2(d->ds1_index,&word))return ERANGE;
   append_slot(&o,REMESA_PDS_STRUCTURE_A2,word,
               REMESA_PDS_STRUCTURE_NO_CELL,d->ds1_index);
  }
  o.a2_count++;
 }
 append_slot(&o,REMESA_PDS_STRUCTURE_FINAL_HOLE,0u,
             REMESA_PDS_STRUCTURE_NO_CELL,REMESA_PDS_STRUCTURE_NO_CELL);
 if(request->code_policy==REMESA_PDS_STRUCTURE_CODE_CPU_COMPLETE) {
  struct remesa_pds_structure resolved;
  error=remesa_pds_structure_resolve_initial(&o,&resolved);if(error)return error;
  struct remesa_pds_structure_slot *tail=&resolved.code[resolved.code_slot_count-1u];
  const struct remesa_pds_code_instruction ir={REMESA_PDS_STRUCTURE_CPU_TRAILER_AF,
   REMESA_PDS_STRUCTURE_NO_CELL,REMESA_PDS_STRUCTURE_NO_CELL};
  error=instruction_word(&ir,&tail->word);if(error)return error;
  tail->kind=ir.kind;tail->known_mask=UINT32_MAX;
  error=structure_valid(&resolved);if(error)return error;
  o=resolved;
 }
 *out=o;
 return 0;
}

/* Keep byte order explicit: CPU-native output and CPU-native endian are
 * different contracts, including when this runs on a non-i386 build host. */
static void store_le32(uint8_t *bytes,uint32_t value)
{
 for(unsigned i=0;i<4u;i++)bytes[i]=(uint8_t)(value>>(8u*i));
}
static int range_valid(const void *pointer,size_t size)
{
 return (!size||pointer)&&(uintptr_t)pointer<=UINTPTR_MAX-size;
}
int remesa_pds_structure_dma_partition(uint32_t n,
 struct remesa_pds_dma_partition *out)
{
 if(!out||((uintptr_t)out%_Alignof(struct remesa_pds_dma_partition))||
    !range_valid(out,sizeof(*out)))return EINVAL;
 if(!n)return EINVAL;
 /* Original coefficient constructor29385 asserts selected length<=128
  * DWORDs at29606. Do not extrapolate the helper's arithmetic beyond it. */
 if(n>128u)return ENOTSUP;
 struct remesa_pds_dma_partition p={.count=1};
 uint32_t width=n,rows=1;
 if(n>16u) {
  /*381c5..38200: descending trial widths15..2. Stop at the FIRST divisor,
   * even when its quotient exceeds16 (then use the split path). */
  width=0;
  for(uint32_t d=15u;d>1u;d--)if(!(n%d)){width=d;rows=n/d;break;}
 }
 if(width&&rows<=16u) {
  p.control[0]=UINT32_C(0x80000000)|((width-1u)<<21u)|
   ((rows-1u)<<4u)|(width-1u);
 } else {
  /*38202..38278: full16-wide prefix, optional remainder. Only the final
   * returned control receives bit31. Its physical meaning is not assigned. */
  uint32_t prefix=n&~15u,remainder=n&15u;
  p.control[0]=(prefix-16u)|UINT32_C(0x01e0000f);
  if(remainder) {
   p.count=2;p.source_word_offset[1]=prefix;
   p.control[1]=UINT32_C(0x80000000)|((remainder-1u)<<21u)|
    (prefix<<8u)|(remainder-1u);
  } else p.control[0]|=UINT32_C(0x80000000);
 }
 *out=p;return 0;
}
int remesa_pds_structure_dma_control(uint32_t word_count,uint32_t *word)
{
 if(!word||((uintptr_t)word%_Alignof(uint32_t))||
    !range_valid(word,sizeof(*word)))return EINVAL;
 if(!word_count)return EINVAL;
 if(word_count>16u)return ENOTSUP;
 struct remesa_pds_dma_partition p;
 int err=remesa_pds_structure_dma_partition(word_count,&p);if(err)return err;
 *word=p.control[0];
 return 0;
}
int remesa_pds_structure_pair_single(uint32_t a,uint32_t b,uint32_t *word)
{
 if(!word||((uintptr_t)word%_Alignof(uint32_t))||
    !range_valid(word,sizeof(*word)))return EINVAL;
 if(a>63u||b>63u)return ERANGE;
 if(a&1u)return EINVAL;
 if(a>4u||b>3u||(a<4u&&b>1u&&!(a==2u&&b==2u)))return ENOTSUP;
 /* The additional (2,2) tuple is corroborated by the original state-upload
  * final producer ELF28fd5 and retained three-word state CODE. It does not
  * admit other previously unobserved tuples. */
 /* These are the original producer's independent shifts and selections.
  * 0x07000345 is not an input/template: it follows at a=b=0 from the
  * demonstrated base and both even-index branches. */
 uint32_t low=b&1u;
 *word=UINT32_C(0x07000205)|(a<<17u)|((b>>1u)<<13u)|
       (low?UINT32_C(0x60):UINT32_C(0x40))|
       (low?UINT32_C(0x180):UINT32_C(0x100));
 return 0;
}
int remesa_pds_structure_forward_index(uint32_t current,uint32_t delta,
 uint32_t total,uint32_t *word)
{
 if(!word||((uintptr_t)word%_Alignof(uint32_t))||
    !range_valid(word,sizeof(*word)))return EINVAL;
 if(!total||current>=total)return EINVAL;
 if(total>REMESA_PDS_STRUCTURE_CODE_SLOTS)return E2BIG;
 if(delta!=3u&&delta!=4u&&delta!=13u)return ENOTSUP;
 if(delta>=total-current)return ERANGE;
 /* psb39671..39685 and396ab..396bc subtract CODE base, divide by four,
  * add3/13, then OR0x90000000. Windows100a71d6..100a71f0 independently
  * forms current+4 and the same class. No physical branch meaning follows
  * from these CPU stores; this only calculates an in-buffer word index. */
 *word=UINT32_C(0x90000000)|(current+delta);
 return 0;
}
static int instruction_word(const struct remesa_pds_code_instruction *ir,
 uint32_t *word)
{
 if(ir->kind==REMESA_PDS_STRUCTURE_CPU_RECORD_LAST_E6) {
  if(ir->ds1_index!=REMESA_PDS_STRUCTURE_NO_CELL)return EINVAL;
  if(ir->ds0_index&1u)return EINVAL;
  if(ir->ds0_index>66u)return ENOTSUP;
  /* Windows100a6d03..100a6d30 rounds the record cursor to even, adds
   * one, shifts by9 and ORs this fixed class/control form. Whole original
   * constructor replays cover every even cursor0..66. The last logical
   * DATA record is written through its interleaved row mapping at100a6c0e.
   * This does not establish physical DS reads or admit the event lowE0,
   * Windows fixed low68, or other unresolved class17 control fields. */
  *word=UINT32_C(0x170000e6)|((ir->ds0_index+1u)<<9u);
  return 0;
 }
 if(ir->kind==REMESA_PDS_STRUCTURE_CPU_VERTEX_SETUP) {
  if(ir->ds0_index!=REMESA_PDS_STRUCTURE_NO_CELL||
     ir->ds1_index!=REMESA_PDS_STRUCTURE_NO_CELL)return EINVAL;
  /* Fixed operand-free CPU profile entry at psb406ce and42845. This is
   * not identification of an architectural index/read operation. */
  *word=UINT32_C(0x67000000)|UINT32_C(0x00800000)|UINT32_C(0x70);
  return 0;
 }
 if(ir->kind==REMESA_PDS_STRUCTURE_CPU_VERTEX_PAIR) {
  if(ir->ds1_index!=REMESA_PDS_STRUCTURE_NO_CELL)return EINVAL;
  if(ir->ds0_index>63u)return ERANGE;
  if(ir->ds0_index!=0u&&ir->ds0_index!=2u)return ENOTSUP;
  /* First/optional second descriptor in both psb40355 and4252d. The
   * same source-pair contribution is independently parameterized in GLES
   * 18b00..18b11; its other fixed source selector differs from this profile. */
  *word=UINT32_C(0x2f000000)|UINT32_C(0x00030000)|
        UINT32_C(0x00000343)|(ir->ds0_index<<17u);
  return 0;
 }
 if(ir->kind==REMESA_PDS_STRUCTURE_CPU_VERTEX_TASK) {
  if(ir->ds0_index>63u||ir->ds1_index>63u)return ERANGE;
  if(ir->ds0_index!=4u||ir->ds1_index!=1u)return ENOTSUP;
  /* Independent GLES18bc0..18bfd uses the same shifts/parity choices as
   * the 07 class but an observed 03 class pattern. Only the tuple emitted
   * by both psb vertex producers (406ed/42864) is admitted. */
  uint32_t a=ir->ds0_index,b=ir->ds1_index,low=b&1u;
  *word=UINT32_C(0x03000205)|(a<<17u)|((b>>1u)<<13u)|
        (low?UINT32_C(0x60):UINT32_C(0x40))|
        (low?UINT32_C(0x180):UINT32_C(0x100));
  return 0;
 }
 if(ir->kind==REMESA_PDS_STRUCTURE_CPU_TRANSFER_PAIR) {
  if(ir->ds1_index!=REMESA_PDS_STRUCTURE_NO_CELL)return EINVAL;
  if(ir->ds0_index>63u)return ERANGE;
  if(ir->ds0_index!=0u&&ir->ds0_index!=2u)return ENOTSUP;
  /* CPU class, not a hardware mnemonic: original GLES17890 copies the
   * first DATA pair at178b2/178b7 and the second at178c8/178ce. Its stores
   * 17940/1795d differ by bit18 for pair indices0/2; psb28fba independently
   * emits the first form. The constants below are fixed observed CPU
   * class/selector patterns, not inferred architectural fields. Only these
   * two pair indices are admitted; overlap uses the original OR form. */
  *word=UINT32_C(0x07000000)|UINT32_C(0x00030000)|
        UINT32_C(0x00000223)|(ir->ds0_index<<17u);
  return 0;
 }
 if(ir->kind==REMESA_PDS_STRUCTURE_A2) {
  if(ir->ds0_index!=REMESA_PDS_STRUCTURE_NO_CELL)return EINVAL;
  return remesa_pds_structure_a2(ir->ds1_index,word);
 }
 if(ir->kind==REMESA_PDS_STRUCTURE_CLASS64)
  return remesa_pds_structure_class64(ir->ds0_index,ir->ds1_index,word);
 if(ir->kind==REMESA_PDS_STRUCTURE_PAIR_SINGLE)
  return remesa_pds_structure_pair_single(ir->ds0_index,ir->ds1_index,word);
 if(ir->kind==REMESA_PDS_STRUCTURE_CPU_TRAILER_AF||
    ir->kind==REMESA_PDS_STRUCTURE_CPU_EMPTY_SECONDARY_AF) {
  if(ir->ds0_index!=REMESA_PDS_STRUCTURE_NO_CELL||
     ir->ds1_index!=REMESA_PDS_STRUCTURE_NO_CELL)return EINVAL;
  /* psb ELF263e4 unconditionally writes this fixed CPU trailer before
   * commit. The separate empty-secondary producer ELF3fafa writes the same
   * fixed word, then commits four bytes and clears metadata+0c/+10/+14.
   * No AF subfields or hardware operation are inferred here. */
  *word=UINT32_C(0xaf000000);return 0;
 }
 return ENOTSUP;
}
int remesa_pds_structure_encode_code(
 const struct remesa_pds_code_instruction *instructions,size_t count,
 uint8_t *bytes,size_t capacity,size_t *written)
{
 if(count>REMESA_PDS_STRUCTURE_CODE_SLOTS)return E2BIG;
 size_t input_size=count*sizeof(*instructions),size=count*4u;
 if(!written||((uintptr_t)written%_Alignof(size_t))||
    (count&&((uintptr_t)instructions%_Alignof(struct remesa_pds_code_instruction)))||
    !range_valid(written,sizeof(*written))||
    !range_valid(instructions,input_size)||!range_valid(bytes,capacity))return EINVAL;
 if(!disjoint(instructions,input_size,bytes,capacity)||
    !disjoint(instructions,input_size,written,sizeof(*written))||
    !disjoint(bytes,capacity,written,sizeof(*written)))return EINVAL;
 if(capacity<size)return ENOSPC;
 uint8_t encoded[REMESA_PDS_STRUCTURE_CODE_SLOTS*4u];
 for(size_t i=0;i<count;i++) {
  if(instructions[i].kind==REMESA_PDS_STRUCTURE_CPU_EMPTY_SECONDARY_AF&&
     count!=1u)return EINVAL;
  if(instructions[i].kind==REMESA_PDS_STRUCTURE_CPU_TRAILER_AF) {
   int primary=count>=2u&&
    instructions[0].kind==REMESA_PDS_STRUCTURE_PAIR_SINGLE&&
    !instructions[0].ds0_index&&!instructions[0].ds1_index;
   /* Exact original one-descriptor state-upload grammar ELF28fba..28fdb.
    * The two-descriptor variant adds a different, unresolved CPU class. */
   int state=count==3u&&
    instructions[0].kind==REMESA_PDS_STRUCTURE_CPU_TRANSFER_PAIR&&
    instructions[0].ds0_index==0u&&
    instructions[0].ds1_index==REMESA_PDS_STRUCTURE_NO_CELL&&
    instructions[1].kind==REMESA_PDS_STRUCTURE_PAIR_SINGLE&&
    instructions[1].ds0_index==2u&&instructions[1].ds1_index==2u;
   int vertex=(count==4u||count==5u)&&
    instructions[0].kind==REMESA_PDS_STRUCTURE_CPU_VERTEX_SETUP&&
    instructions[1].kind==REMESA_PDS_STRUCTURE_CPU_VERTEX_PAIR&&
    instructions[1].ds0_index==0u&&
    instructions[1].ds1_index==REMESA_PDS_STRUCTURE_NO_CELL&&
    instructions[count-2u].kind==REMESA_PDS_STRUCTURE_CPU_VERTEX_TASK&&
    instructions[count-2u].ds0_index==4u&&
    instructions[count-2u].ds1_index==1u;
   if(vertex&&count==5u)vertex=
    instructions[2].kind==REMESA_PDS_STRUCTURE_CPU_VERTEX_PAIR&&
    instructions[2].ds0_index==2u&&
    instructions[2].ds1_index==REMESA_PDS_STRUCTURE_NO_CELL;
   if(i+1u!=count||(!primary&&!state&&!vertex))return EINVAL;
  }
  uint32_t word;
  int error;
  if(instructions[i].kind==REMESA_PDS_STRUCTURE_CPU_FORWARD_INDEX) {
   if(instructions[i].ds1_index!=REMESA_PDS_STRUCTURE_NO_CELL)return EINVAL;
   error=remesa_pds_structure_forward_index((uint32_t)i,
    instructions[i].ds0_index,(uint32_t)count,&word);
  } else error=instruction_word(&instructions[i],&word);
  if(error)return error;
  store_le32(&encoded[i*4u],word);
 }
 if(size)memcpy(bytes,encoded,size);
 *written=size;
 return 0;
}
int remesa_pds_structure_encode_texture_output(
 const struct remesa_pds_texture_output *output,uint32_t data_dwords,
 uint8_t *bytes,size_t capacity,size_t *written)
{
 if(!output||!written||
    ((uintptr_t)output%_Alignof(struct remesa_pds_texture_output))||
    ((uintptr_t)written%_Alignof(size_t))||
    !range_valid(output,sizeof(*output))||
    !range_valid(written,sizeof(*written))||!range_valid(bytes,capacity))
  return EINVAL;
 if(!disjoint(output,sizeof(*output),bytes,capacity)||
    !disjoint(output,sizeof(*output),written,sizeof(*written))||
    !disjoint(bytes,capacity,written,sizeof(*written)))return EINVAL;
 struct remesa_pds_code_instruction ir[2];
 int error=texture_output_instructions(output,data_dwords,ir);
 if(error)return error;
 return remesa_pds_structure_encode_code(ir,2u,bytes,capacity,written);
}
int remesa_pds_structure_encode_pair_single_output(
 const struct remesa_pds_pair_single_output *output,uint32_t data_dwords,
 uint8_t *bytes,size_t capacity,size_t *written)
{
 if(!output||!written||
    ((uintptr_t)output%_Alignof(struct remesa_pds_pair_single_output))||
    ((uintptr_t)written%_Alignof(size_t))||
    !range_valid(output,sizeof(*output))||
    !range_valid(written,sizeof(*written))||!range_valid(bytes,capacity))return EINVAL;
 if(!disjoint(output,sizeof(*output),bytes,capacity)||
    !disjoint(output,sizeof(*output),written,sizeof(*written))||
    !disjoint(bytes,capacity,written,sizeof(*written)))return EINVAL;
 if(data_dwords<12u||data_dwords>REMESA_PDS_STRUCTURE_DATA_CELLS)return ERANGE;
 if(data_dwords&3u)return EINVAL;
 uint32_t a,next,b;
 int e=data_index(output->pair_cells[0],0u,data_dwords,&a);if(e)return e;
 e=data_index(output->pair_cells[1],0u,data_dwords,&next);if(e)return e;
 e=data_index(output->single_cell,1u,data_dwords,&b);if(e)return e;
 if(next!=a+1u)return EINVAL;
 const struct remesa_pds_code_instruction ir={REMESA_PDS_STRUCTURE_PAIR_SINGLE,a,b};
 return remesa_pds_structure_encode_code(&ir,1u,bytes,capacity,written);
}
static int slot_valid(const struct remesa_pds_structure *s,uint32_t index,
 uint32_t kind,uint32_t ds0,uint32_t ds1)
{
 if(index>=s->code_slot_count)return EINVAL;
 const struct remesa_pds_structure_slot *slot=&s->code[index];
 if(slot->kind!=kind) {
  if(slot->kind!=REMESA_PDS_STRUCTURE_A2&&
     slot->kind!=REMESA_PDS_STRUCTURE_CLASS64&&
     slot->kind!=REMESA_PDS_STRUCTURE_INITIAL_HOLE&&
     slot->kind!=REMESA_PDS_STRUCTURE_FINAL_HOLE&&
     slot->kind!=REMESA_PDS_STRUCTURE_PAIR_SINGLE&&
     slot->kind!=REMESA_PDS_STRUCTURE_CPU_TRAILER_AF)return ENOTSUP;
  return EINVAL;
 }
 if(slot->byte_offset!=(s->data_dwords+index)*4u||
    slot->ds0_index!=ds0||slot->ds1_index!=ds1)return EINVAL;
 if(kind==REMESA_PDS_STRUCTURE_INITIAL_HOLE||
    kind==REMESA_PDS_STRUCTURE_FINAL_HOLE)
  return slot->word||slot->known_mask?EINVAL:0;
 struct remesa_pds_code_instruction ir={kind,ds0,ds1};
 uint32_t word;
 int error=instruction_word(&ir,&word);
 if(error)return error;
 return slot->known_mask!=UINT32_MAX||slot->word!=word?EINVAL:0;
}
/* This is a boundary validator for serialization of a mutable CPU analysis
 * result. It does not infer DATA field meaning, resolve a device address or
 * convert holes into supported instructions. */
static int structure_valid(const struct remesa_pds_structure *s)
{
 if(s->data_dwords<12u||s->data_dwords>REMESA_PDS_STRUCTURE_DATA_CELLS||
    (s->data_dwords&3u)||s->code_slot_count<2u||
    s->code_slot_count>REMESA_PDS_STRUCTURE_CODE_SLOTS||
    s->descriptor_count>REMESA_PDS_STRUCTURE_DESCRIPTORS||
    !s->relocation_count||s->relocation_count>REMESA_PDS_STRUCTURE_RELOCATIONS)
  return EINVAL;
 uint32_t masks[REMESA_PDS_STRUCTURE_DATA_CELLS]={0};
 masks[1]=masks[8]=UINT32_MAX;
 uint32_t next0=2u,next1=1u,next_input=0u,next_slot=1u,next_reloc=1u;
 uint32_t last_r=REMESA_PDS_STRUCTURE_NO_CELL,last_t=last_r,t_count=0u;
 int error;
 if(s->code[0].kind==REMESA_PDS_STRUCTURE_PAIR_SINGLE)
  error=slot_valid(s,0u,REMESA_PDS_STRUCTURE_PAIR_SINGLE,0u,0u);
 else error=slot_valid(s,0u,REMESA_PDS_STRUCTURE_INITIAL_HOLE,
                       REMESA_PDS_STRUCTURE_NO_CELL,REMESA_PDS_STRUCTURE_NO_CELL);
 if(error)return error;
 for(uint32_t i=0;i<s->descriptor_count;i++) {
  const struct remesa_pds_structure_descriptor *d=&s->descriptors[i];
  if(d->has_r>1u||d->has_t>1u||!(d->has_r||d->has_t)||
     d->first_input!=next_input||d->ds1_index!=next1||
     d->data_cell!=cell1(next1)||
     d->ds0_index!=(d->has_t?next0:REMESA_PDS_STRUCTURE_NO_CELL))return EINVAL;
  next_input+=d->has_r+d->has_t;
  if(next_input>REMESA_PDS_STRUCTURE_INPUTS&&
     (next_input!=REMESA_PDS_STRUCTURE_INPUTS+1u||
      i+1u!=s->descriptor_count||!d->has_r||d->has_t))return EINVAL;
  masks[d->data_cell]=UINT32_MAX;
  if(d->has_r)last_r=d->data_cell;
  if(d->has_t) {
   if(next_reloc>=s->relocation_count)return EINVAL;
   const struct remesa_pds_structure_relocation *reloc=&s->relocations[next_reloc++];
   if(reloc->kind!=REMESA_PDS_STRUCTURE_TEXTURE||
      reloc->data_cell!=cell1(next1+1u)||reloc->background)return EINVAL;
   masks[cell0(next0)]=masks[cell0(next0+1u)]=UINT32_MAX;
   const struct remesa_pds_texture_output output={
    REMESA_PDS_TEXTURE_DESCRIPTOR_BEFORE_ADDRESS,
    {cell0(next0),cell0(next0+1u)},reloc->data_cell,d->data_cell};
   struct remesa_pds_code_instruction ir[2];
   error=texture_output_instructions(&output,s->data_dwords,ir);
   if(error)return error;
   for(uint32_t k=0;k<2u;k++) {
    error=slot_valid(s,next_slot++,ir[k].kind,ir[k].ds0_index,ir[k].ds1_index);
    if(error)return error;
   }
   last_t=d->data_cell;
   next0+=2u;next1+=2u;t_count++;
  } else {
   error=slot_valid(s,next_slot++,REMESA_PDS_STRUCTURE_A2,
                    REMESA_PDS_STRUCTURE_NO_CELL,next1);
   if(error)return error;
   next1++;
  }
 }
 if(next_slot>=s->code_slot_count)return EINVAL;
 uint32_t tail_kind=s->code[next_slot].kind;
 if(tail_kind==REMESA_PDS_STRUCTURE_CPU_TRAILER_AF&&
    s->code[0].kind!=REMESA_PDS_STRUCTURE_PAIR_SINGLE)return EINVAL;
 if(tail_kind!=REMESA_PDS_STRUCTURE_CPU_TRAILER_AF)
  tail_kind=REMESA_PDS_STRUCTURE_FINAL_HOLE;
 error=slot_valid(s,next_slot++,tail_kind,
                  REMESA_PDS_STRUCTURE_NO_CELL,REMESA_PDS_STRUCTURE_NO_CELL);
 if(error)return error;
 uint32_t limit0=cell0(next0),limit1=cell1(next1);
 uint32_t extent=((limit0>limit1?limit0:limit1)+3u)&~UINT32_C(3);
 uint32_t background=(last_r!=REMESA_PDS_STRUCTURE_NO_CELL?UINT32_C(0x80000):0u)|
                     (last_t!=REMESA_PDS_STRUCTURE_NO_CELL?UINT32_C(0x100000):0u);
 if(s->data_dwords!=extent||s->code_slot_count!=next_slot||
    s->a2_count!=s->descriptor_count||s->class64_count!=t_count||
    s->relocation_count!=next_reloc||s->last_r_cell!=last_r||
    s->last_t_cell!=last_t||s->use_background!=background)return EINVAL;
 if(last_r!=REMESA_PDS_STRUCTURE_NO_CELL&&
    !(s->data[last_r]&UINT32_C(0x02000000)))return EINVAL;
 if(last_t!=REMESA_PDS_STRUCTURE_NO_CELL&&
    !(s->data[last_t]&UINT32_C(0x800)))return EINVAL;
 const struct remesa_pds_structure_relocation *use=&s->relocations[0];
 if(use->kind!=REMESA_PDS_STRUCTURE_USE||use->data_cell||use->arg3||
    use->arg4||use->background!=background)return EINVAL;
 for(uint32_t i=0;i<REMESA_PDS_STRUCTURE_DATA_CELLS;i++)
  if(s->data_known_mask[i]!=masks[i]||(!masks[i]&&s->data[i]))return EINVAL;
 return 0;
}
int remesa_pds_structure_serialize(const struct remesa_pds_structure *s,
 uint8_t *bytes,uint8_t *known_mask,size_t capacity,size_t *written)
{
 if(!s||!written||((uintptr_t)s%_Alignof(struct remesa_pds_structure))||
    ((uintptr_t)written%_Alignof(size_t))||!range_valid(s,sizeof(*s))||
    !range_valid(written,sizeof(*written))||!range_valid(bytes,capacity)||
    !range_valid(known_mask,capacity))return EINVAL;
 if(!disjoint(s,sizeof(*s),bytes,capacity)||
    !disjoint(s,sizeof(*s),known_mask,capacity)||
    !disjoint(s,sizeof(*s),written,sizeof(*written))||
    !disjoint(bytes,capacity,known_mask,capacity)||
    !disjoint(bytes,capacity,written,sizeof(*written))||
    !disjoint(known_mask,capacity,written,sizeof(*written)))return EINVAL;
 int error=structure_valid(s);
 if(error)return error;
 size_t size=(s->data_dwords+s->code_slot_count)*4u;
 if(capacity<size)return ENOSPC;
 for(uint32_t i=0;i<s->data_dwords;i++) {
  store_le32(&bytes[i*4u],s->data[i]&s->data_known_mask[i]);
  store_le32(&known_mask[i*4u],s->data_known_mask[i]);
 }
 for(uint32_t i=0;i<s->code_slot_count;i++) {
  size_t offset=(s->data_dwords+i)*4u;
  store_le32(&bytes[offset],s->code[i].word&s->code[i].known_mask);
  store_le32(&known_mask[offset],s->code[i].known_mask);
 }
 *written=size;
 return 0;
}

/* CPU allocator subset: Linux ELF2383dd/238c46 retain old logical starts;
 * ELF238412..2385ae and23872d..238be8 prune/trim using liveness; selector
 * ELF2352a0 and loop2391fe..2395ba select stable R/non-R alternation.
 * This implementation derives no ISA semantics or liveness facts. */
int remesa_pds_structure_allocate(const struct remesa_pds_allocator_request *input,
 struct remesa_pds_allocation *out)
{
 if(!input||!out||
    ((uintptr_t)input%_Alignof(struct remesa_pds_allocator_request))||
    ((uintptr_t)out%_Alignof(struct remesa_pds_allocation))||
    !range_valid(input,sizeof(*input))||!range_valid(out,sizeof(*out))||
    !disjoint(input,sizeof(*input),out,sizeof(*out)))return EINVAL;
 if(input->flags)return ENOTSUP;
 if(input->source.entry_count>REMESA_PDS_STRUCTURE_INPUTS)return E2BIG;
 for(uint32_t i=0;i<input->source.entry_count;i++) {
  const struct remesa_pds_structure_entry *entry=&input->source.entries[i];
  if(entry->kind!=REMESA_PDS_STRUCTURE_R&&entry->kind!=REMESA_PDS_STRUCTURE_T)
   return ENOTSUP;
  if(entry->kind==REMESA_PDS_STRUCTURE_R&&entry->mode)return ENOTSUP;
 }
 int error=request_valid(&input->source);
 if(error)return error;
 struct remesa_pds_allocation allocation;
 memset(&allocation,0,sizeof(allocation));
 allocation.request=input->source;
 allocation.request.entry_count=0u;
 memset(allocation.request.entries,0,sizeof(allocation.request.entries));
 for(uint32_t i=0;i<REMESA_PDS_STRUCTURE_INPUTS;i++) {
  allocation.source_to_entry[i]=REMESA_PDS_STRUCTURE_NO_CELL;
  allocation.entry_to_source[i]=REMESA_PDS_STRUCTURE_NO_CELL;
  allocation.source_base[i]=REMESA_PDS_STRUCTURE_NO_CELL;
  allocation.entry_base[i]=REMESA_PDS_STRUCTURE_NO_CELL;
 }
 for(uint32_t i=0;i<REMESA_PDS_ALLOCATOR_LOGICAL_WORDS;i++)
  allocation.logical_map[i]=REMESA_PDS_STRUCTURE_NO_CELL;
 uint32_t widths[REMESA_PDS_STRUCTURE_INPUTS]={0};
 for(uint32_t i=0;i<REMESA_PDS_STRUCTURE_INPUTS;i++) {
  if(i>=input->source.entry_count) {
   if(input->logical_width[i]||input->live_components[i])return EINVAL;
   continue;
  }
  const struct remesa_pds_structure_entry *entry=&input->source.entries[i];
  uint32_t width=input->logical_width[i],live=input->live_components[i];
  if(width<1u||width>4u)return ERANGE;
  if(entry->kind==REMESA_PDS_STRUCTURE_R&&width!=entry->components)
   return EINVAL;
  if(live&~((1u<<width)-1u))return EINVAL;
  allocation.source_base[i]=allocation.logical_words_before;
  allocation.logical_words_before+=width;
  if(!live)continue;
  widths[i]=width;
  if(entry->kind==REMESA_PDS_STRUCTURE_R&&entry->id!=10u&&entry->id!=11u)
   while(!(live&(1u<<(widths[i]-1u))))widths[i]--;
 }
 uint32_t prefer_r=1u;
 for(;;) {
  uint32_t selected=REMESA_PDS_STRUCTURE_NO_CELL;
  /* First remaining preferred-kind entry; fallback does not disturb the
   * relative order within either group. Preference toggles every iteration. */
  for(uint32_t i=0;i<input->source.entry_count;i++)
   if(widths[i]&&((input->source.entries[i].kind==REMESA_PDS_STRUCTURE_R)==
                 (prefer_r!=0u))) {selected=i;break;}
  if(selected==REMESA_PDS_STRUCTURE_NO_CELL)
   for(uint32_t i=0;i<input->source.entry_count;i++)
    if(widths[i]) {selected=i;break;}
  if(selected==REMESA_PDS_STRUCTURE_NO_CELL)break;
  prefer_r^=1u;
  uint32_t index=allocation.request.entry_count++,width=widths[selected];
  struct remesa_pds_structure_entry *entry=&allocation.request.entries[index];
  *entry=input->source.entries[selected];
  if(entry->kind==REMESA_PDS_STRUCTURE_R&&entry->id!=10u&&entry->id!=11u)
   entry->components=width;
  allocation.source_to_entry[selected]=index;
  allocation.entry_to_source[index]=selected;
  allocation.entry_base[index]=allocation.logical_words_after;
  allocation.entry_width[index]=width;
  allocation.entry_live_components[index]=input->live_components[selected];
  for(uint32_t k=0;k<width;k++)
   allocation.logical_map[allocation.source_base[selected]+k]=
    allocation.logical_words_after+k;
  allocation.logical_words_after+=width;
  widths[selected]=0u;
 }
 error=remesa_pds_structure_encode(&allocation.request,&allocation.structure);
 if(error)return error;
 *out=allocation;
 return 0;
}
int remesa_pds_structure_resolve_initial(const struct remesa_pds_structure *source,
 struct remesa_pds_structure *out)
{
 if(!source||!out||((uintptr_t)source%_Alignof(struct remesa_pds_structure))||
    ((uintptr_t)out%_Alignof(struct remesa_pds_structure))||
    !range_valid(source,sizeof(*source))||!range_valid(out,sizeof(*out))||
    !disjoint(source,sizeof(*source),out,sizeof(*out)))return EINVAL;
 int e=structure_valid(source);if(e)return e;
 if(source->code[0].kind!=REMESA_PDS_STRUCTURE_INITIAL_HOLE)return EBUSY;
 /* The existing psb grammar independently fixes task pair at DATA0/1 and
  * single at DATA8. Match their DATA references through the new codec; the
  * relocation at DATA0 remains symbolic and is not read as an address. */
 const struct remesa_pds_pair_single_output refs={{0u,1u},8u};
 uint8_t encoded[4];size_t written;
 e=remesa_pds_structure_encode_pair_single_output(&refs,source->data_dwords,
    encoded,sizeof(encoded),&written);if(e)return e;
 struct remesa_pds_structure resolved=*source;
 struct remesa_pds_structure_slot *slot=&resolved.code[0];
 slot->kind=REMESA_PDS_STRUCTURE_PAIR_SINGLE;
 slot->word=(uint32_t)encoded[0]|((uint32_t)encoded[1]<<8u)|
            ((uint32_t)encoded[2]<<16u)|((uint32_t)encoded[3]<<24u);
 slot->known_mask=UINT32_MAX;slot->ds0_index=slot->ds1_index=0u;
 e=structure_valid(&resolved);if(e)return e;
 *out=resolved;
 return 0;
}

int remesa_pds_structure_encode_background(
 const struct remesa_pds_background_request *request,uint8_t *bytes,
 uint8_t *known_mask,size_t capacity,size_t *written)
{
 if(!request||!written||
    (uintptr_t)request%_Alignof(struct remesa_pds_background_request)||
    (uintptr_t)written%_Alignof(size_t)||
    !range_valid(request,sizeof(*request))||!range_valid(written,sizeof(*written))||
    !range_valid(bytes,capacity)||!range_valid(known_mask,capacity))return EINVAL;
 if(!disjoint(request,sizeof(*request),bytes,capacity)||
    !disjoint(request,sizeof(*request),known_mask,capacity)||
    !disjoint(request,sizeof(*request),written,sizeof(*written))||
    !disjoint(bytes,capacity,known_mask,capacity)||
    !disjoint(bytes,capacity,written,sizeof(*written))||
    !disjoint(known_mask,capacity,written,sizeof(*written)))return EINVAL;
 uint32_t format;
 switch(request->surface_tag) {
 case 1:format=0;break;
 case 2:format=UINT32_C(0x05000000);break;
 case 4:format=UINT32_C(0x0c000000);break;
 default:return ENOTSUP;
 }
 if(!request->width||request->width>4096u||!request->height||request->height>4096u||
    request->use_lower19>UINT32_C(0x7ffff)||request->color_address>UINT32_MAX)return ERANGE;
 if(capacity<REMESA_PDS_STRUCTURE_BACKGROUND_BYTES)return ENOSPC;
 uint8_t image[REMESA_PDS_STRUCTURE_BACKGROUND_BYTES]={0};
 uint8_t mask[REMESA_PDS_STRUCTURE_BACKGROUND_BYTES]={0};
 uint32_t data[12]={0};
 /* These DATA constants are direct CPU state stores, not shader templates.
  * The USE prefix comes from the original three-record relocation producer. */
 data[0]=UINT32_C(0x100000)|request->use_lower19;
 data[2]=UINT32_C(0x001e0092);
 data[3]=UINT32_C(0x60000000)|format|((request->width-1u)<<12u)|(request->height-1u);
 data[8]=UINT32_C(0x20);data[9]=UINT32_C(0xf800);
 data[10]=(uint32_t)request->color_address;
 for(unsigned i=0;i<12u;i++) {
  store_le32(image+4u*i,data[i]);
  if(i<4u||i==8u||i==9u||i==10u)store_le32(mask+4u*i,UINT32_MAX);
 }
 const uint32_t none=REMESA_PDS_STRUCTURE_NO_CELL;
 const struct remesa_pds_code_instruction code[]={
  {REMESA_PDS_STRUCTURE_PAIR_SINGLE,0,0},
  {REMESA_PDS_STRUCTURE_A2,none,1},
  {REMESA_PDS_STRUCTURE_CLASS64,2,2},
  {REMESA_PDS_STRUCTURE_CPU_TRAILER_AF,none,none}};
 size_t code_bytes;
 int error=remesa_pds_structure_encode_code(code,4,image+48,16,&code_bytes);
 if(error)return error;
 if(code_bytes!=16u)return EPROTO;
 memset(mask+48,0xff,16);
 memcpy(bytes,image,sizeof(image));memcpy(known_mask,mask,sizeof(mask));
 *written=sizeof(image);return 0;
}

int remesa_pds_structure_encode_state_image(
 const struct remesa_pds_state_image_request *request,uint8_t *bytes,
 uint8_t *known_mask,size_t capacity,size_t *written)
{
 if(!request||!written||
    (uintptr_t)request%_Alignof(struct remesa_pds_state_image_request)||
    (uintptr_t)written%_Alignof(size_t)||
    !range_valid(request,sizeof(*request))||!range_valid(written,sizeof(*written))||
    !range_valid(bytes,capacity)||!range_valid(known_mask,capacity))return EINVAL;
 if(!disjoint(request,sizeof(*request),bytes,capacity)||
    !disjoint(request,sizeof(*request),known_mask,capacity)||
    !disjoint(request,sizeof(*request),written,sizeof(*written))||
    !disjoint(bytes,capacity,known_mask,capacity)||
    !disjoint(bytes,capacity,written,sizeof(*written))||
    !disjoint(known_mask,capacity,written,sizeof(*written)))return EINVAL;
 const struct remesa_pds_state_image_request r=*request;
 if(r.task_profile!=REMESA_PDS_STATE_TASK_ISP&&
    r.task_profile!=REMESA_PDS_STATE_TASK_PARAMETERS)return ENOTSUP;
 uint32_t dma;
 int error;
 if(r.task_profile==REMESA_PDS_STATE_TASK_PARAMETERS) {
  struct remesa_pds_dma_partition partition;
  error=remesa_pds_structure_dma_partition(r.state_dwords,&partition);
  if(error)return error;
  /*29385 calls381a0 for the committed coefficient length. Its one-row
   * and factored single-descriptor paths have the same CODE grammar.
   * The second-descriptor instruction remains a separate unknown class. */
  if(partition.count!=1u)return ENOTSUP;
  dma=partition.control[0];
 } else error=remesa_pds_structure_dma_control(r.state_dwords,&dma);
 if(error)return error;
 if(r.use_lower19>UINT32_C(0x7ffff))return ERANGE;
 if(!r.source_allocation_address||r.source_allocation_address>UINT32_MAX||
    (r.source_allocation_address&3u)||(r.source_offset&3u))return EINVAL;
 if(!r.source_allocation_bytes||
    r.source_allocation_bytes>UINT64_C(0x100000000)-r.source_allocation_address)
  return EOVERFLOW;
 uint64_t source_bytes=(uint64_t)r.state_dwords*4u;
 if(r.source_offset>r.source_allocation_bytes||
    source_bytes>r.source_allocation_bytes-r.source_offset)return ENOSPC;
 if(capacity<REMESA_PDS_STRUCTURE_STATE_BYTES)return ENOSPC;
 uint8_t image[REMESA_PDS_STRUCTURE_STATE_BYTES]={0};
 uint8_t mask[REMESA_PDS_STRUCTURE_STATE_BYTES]={0};
 /*28ef1/28ef6 source BO relocation and independent DMA control.28fa4
  * uses a DIFFERENT auxiliary allocation with original background bit21. */
 store_le32(image,(uint32_t)(r.source_allocation_address+r.source_offset));
 store_le32(image+4,dma);
 store_le32(image+8,UINT32_C(0x200000)|r.use_lower19);
 memset(mask,0xff,12);
 if(r.task_profile==REMESA_PDS_STATE_TASK_PARAMETERS) {
  /* Original29385:29cf2/29cf9. These are CPU task fields, not proof
   * of physical DS bank selection or auxiliary USE execution. */
  store_le32(image+12,UINT32_C(0x04000000));
  store_le32(image+40,0);
  memset(mask+12,0xff,4);memset(mask+40,0xff,4);
 }
 const uint32_t none=REMESA_PDS_STRUCTURE_NO_CELL;
 const struct remesa_pds_code_instruction code[]={
  {REMESA_PDS_STRUCTURE_CPU_TRANSFER_PAIR,0,none},
  {REMESA_PDS_STRUCTURE_PAIR_SINGLE,2,2},
  {REMESA_PDS_STRUCTURE_CPU_TRAILER_AF,none,none}};
 size_t code_bytes=0;
 error=remesa_pds_structure_encode_code(code,3,image+48,12,&code_bytes);
 if(error)return error;
 if(code_bytes!=12)return EPROTO;
 memset(mask+48,0xff,12);
 memcpy(bytes,image,sizeof(image));memcpy(known_mask,mask,sizeof(mask));
 *written=sizeof(image);return 0;
}

int remesa_pds_structure_encode_parameter_source(
 const struct remesa_pds_parameter_source_request *request,
 const uint8_t *coefficients,size_t coefficient_bytes_size,
 uint8_t *bytes,size_t capacity,size_t *written)
{
 if(!request||!written||
    (uintptr_t)request%_Alignof(struct remesa_pds_parameter_source_request)||
    (uintptr_t)written%_Alignof(size_t)||
    !range_valid(request,sizeof(*request))||!range_valid(written,sizeof(*written))||
    !range_valid(coefficients,coefficient_bytes_size)||!range_valid(bytes,capacity))
  return EINVAL;
 if(!disjoint(request,sizeof(*request),bytes,capacity)||
    !disjoint(request,sizeof(*request),written,sizeof(*written))||
    !disjoint(coefficients,coefficient_bytes_size,bytes,capacity)||
    !disjoint(coefficients,coefficient_bytes_size,written,sizeof(*written))||
    !disjoint(bytes,capacity,written,sizeof(*written)))return EINVAL;
 const struct remesa_pds_parameter_source_request r=*request;
 if(!r.coefficient_dwords)return EINVAL;
 if(r.coefficient_dwords>127u||r.transfer_dwords>128u)return ENOTSUP;
 const uint32_t effective=1u+r.coefficient_dwords;
 if(r.transfer_dwords!=effective&&r.transfer_dwords!=((effective+3u)&~3u))
  return EINVAL;
 if(coefficient_bytes_size!=(size_t)r.coefficient_dwords*4u)return EINVAL;
 struct remesa_pds_dma_partition partition;
 int error=remesa_pds_structure_dma_partition(r.transfer_dwords,&partition);
 if(error)return error;
 if(partition.count!=1u)return ENOTSUP;
 if(!r.source_allocation_address||r.source_allocation_address>UINT32_MAX||
    (r.source_allocation_address&3u)||(r.source_offset&3u))return EINVAL;
 if(!r.source_allocation_bytes||
    r.source_allocation_bytes>UINT64_C(0x100000000)-r.source_allocation_address)
  return EOVERFLOW;
 const size_t transfer_bytes=(size_t)r.transfer_dwords*4u;
 if(r.source_offset>r.source_allocation_bytes||
    transfer_bytes>r.source_allocation_bytes-r.source_offset||capacity<transfer_bytes)
  return ENOSPC;
 uint8_t source[128u*4u]={0};
 store_le32(source,(uint32_t)(r.source_allocation_address+r.source_offset));
 memcpy(source+4,coefficients,coefficient_bytes_size);
 memcpy(bytes,source,transfer_bytes);*written=transfer_bytes;return 0;
}

static int encode_vertex_image(
 const struct remesa_pds_vertex_image_request *request,uint8_t *bytes,
 uint8_t *known_mask,size_t capacity,size_t *written,bool compiled_program)
{
 if(!request||!written||
    (uintptr_t)request%_Alignof(struct remesa_pds_vertex_image_request)||
    (uintptr_t)written%_Alignof(size_t)||
    !range_valid(request,sizeof(*request))||!range_valid(written,sizeof(*written))||
    !range_valid(bytes,capacity)||!range_valid(known_mask,capacity))return EINVAL;
 if(!disjoint(request,sizeof(*request),bytes,capacity)||
    !disjoint(request,sizeof(*request),known_mask,capacity)||
    !disjoint(request,sizeof(*request),written,sizeof(*written))||
    !disjoint(bytes,capacity,known_mask,capacity)||
    !disjoint(bytes,capacity,written,sizeof(*written))||
    !disjoint(known_mask,capacity,written,sizeof(*written)))return EINVAL;
 const struct remesa_pds_vertex_image_request r=*request;
 uint32_t dma;
 int error=remesa_pds_structure_dma_control(r.vertex_stride_dwords,&dma);
 if(error)return error;
 if(r.use_lower19>UINT32_C(0x7ffff))return ERANGE;
 if(!r.source_allocation_address||r.source_allocation_address>UINT32_MAX||
    (r.source_allocation_address&3u)||(r.source_offset&3u))return EINVAL;
 if(!r.source_allocation_bytes||
    r.source_allocation_bytes>UINT64_C(0x100000000)-r.source_allocation_address)
  return EOVERFLOW;
 uint64_t source_bytes=(uint64_t)r.vertex_stride_dwords*4u;
 if(r.source_offset>r.source_allocation_bytes||
    source_bytes>r.source_allocation_bytes-r.source_offset)return ENOSPC;
 if(capacity<REMESA_PDS_STRUCTURE_VERTEX_BYTES)return ENOSPC;
 uint8_t image[REMESA_PDS_STRUCTURE_VERTEX_BYTES]={0};
 uint8_t mask[REMESA_PDS_STRUCTURE_VERTEX_BYTES]={0};
 /*40355 single-DMA vertex profile.405f5 source BO relocation,405fd
  * DMA control,406bb distinct auxiliary USE relocation;4067b byte stride.
  * This describes a record, not committed vertex count or fetch format. */
 store_le32(image,(uint32_t)(r.source_allocation_address+r.source_offset));
 store_le32(image+4,dma);
 store_le32(image+16,UINT32_C(0x200000)|r.use_lower19);
 store_le32(image+32,r.vertex_stride_dwords*4u);
 memset(mask,0xff,8);memset(mask+16,0xff,4);memset(mask+32,0xff,4);
 if(compiled_program) {
  /* Main compiled-VS producer4252d differs from upload/copy producer40355:
   * original42834/4283b write these two DATA fields. CODE is identical in
   * the single-DMA case; physical task-mode meanings remain unqualified. */
  store_le32(image+20,UINT32_C(0xe0000000));store_le32(image+36,12);
  memset(mask+20,0xff,4);memset(mask+36,0xff,4);
 }
 const uint32_t none=REMESA_PDS_STRUCTURE_NO_CELL;
 const struct remesa_pds_code_instruction code[]={
  {REMESA_PDS_STRUCTURE_CPU_VERTEX_SETUP,none,none},
  {REMESA_PDS_STRUCTURE_CPU_VERTEX_PAIR,0,none},
  {REMESA_PDS_STRUCTURE_CPU_VERTEX_TASK,4,1},
  {REMESA_PDS_STRUCTURE_CPU_TRAILER_AF,none,none}};
 size_t code_bytes=0;
 error=remesa_pds_structure_encode_code(code,4,image+48,16,&code_bytes);
 if(error)return error;
 if(code_bytes!=16)return EPROTO;
 memset(mask+48,0xff,16);
 memcpy(bytes,image,sizeof(image));memcpy(known_mask,mask,sizeof(mask));
 *written=sizeof(image);return 0;
}

int remesa_pds_structure_encode_vertex_image(
 const struct remesa_pds_vertex_image_request *request,uint8_t *bytes,
 uint8_t *known_mask,size_t capacity,size_t *written)
{return encode_vertex_image(request,bytes,known_mask,capacity,written,false);}
int remesa_pds_structure_encode_compiled_vertex_image(
 const struct remesa_pds_vertex_image_request *request,uint8_t *bytes,
 uint8_t *known_mask,size_t capacity,size_t *written)
{return encode_vertex_image(request,bytes,known_mask,capacity,written,true);}

int remesa_pds_structure_encode_event_data(
 const struct remesa_pds_event_data_request *request,uint8_t *bytes,
 uint8_t *known_mask,size_t capacity,size_t *written)
{
 if(!request||!written||
    (uintptr_t)request%_Alignof(struct remesa_pds_event_data_request)||
    (uintptr_t)written%_Alignof(size_t)||
    !range_valid(request,sizeof(*request))||!range_valid(written,sizeof(*written))||
    !range_valid(bytes,capacity)||!range_valid(known_mask,capacity))return EINVAL;
 if(!disjoint(request,sizeof(*request),bytes,capacity)||
    !disjoint(request,sizeof(*request),known_mask,capacity)||
    !disjoint(request,sizeof(*request),written,sizeof(*written))||
    !disjoint(bytes,capacity,known_mask,capacity)||
    !disjoint(bytes,capacity,written,sizeof(*written))||
    !disjoint(known_mask,capacity,written,sizeof(*written)))return EINVAL;
 const struct remesa_pds_event_data_request r=*request;
 if(r.use_lower19>UINT32_C(0x7ffff))return ERANGE;
 if(!r.source_allocation_address||r.source_allocation_address>UINT32_MAX||
    (r.source_allocation_address&3u)||(r.source_offset&3u))return EINVAL;
 if(!r.source_allocation_bytes||
    r.source_allocation_bytes>UINT64_C(0x100000000)-r.source_allocation_address)
  return EOVERFLOW;
 if(r.source_offset>r.source_allocation_bytes||
    UINT64_C(16)>r.source_allocation_bytes-r.source_offset)return ENOSPC;
 if(capacity<REMESA_PDS_STRUCTURE_EVENT_DATA_BYTES)return ENOSPC;
 uint8_t image[REMESA_PDS_STRUCTURE_EVENT_DATA_BYTES]={0};
 uint8_t mask[REMESA_PDS_STRUCTURE_EVENT_DATA_BYTES]={0};
 /* Original target relocation8126 and event USE relocation8165. The other
  * stores below are DATA, not an embedded historical instruction program. */
 store_le32(image,(uint32_t)(r.source_allocation_address+r.source_offset));
 store_le32(image+4,UINT32_C(0x80000003));
 store_le32(image+8,UINT32_C(0x200000)|r.use_lower19);
 memset(mask,0xff,16); /* DATA3 is explicitly written zero at8170. */
 store_le32(image+32,2);store_le32(image+36,UINT32_C(0x10000000));
 store_le32(image+40,UINT32_C(0x0007f800));
 store_le32(image+44,UINT32_C(0x07f00000));
 store_le32(image+48,UINT32_C(0x08000000));memset(mask+32,0xff,20);
 memcpy(bytes,image,sizeof(image));memcpy(known_mask,mask,sizeof(mask));
 *written=sizeof(image);return 0;
}
