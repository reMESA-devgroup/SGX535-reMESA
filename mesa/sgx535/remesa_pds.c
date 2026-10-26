/* SPDX-License-Identifier: MIT; CPU field codecs do not establish PDS ISA. */
#include "remesa_pds.h"
#include "remesa_pds_candidate.h"
#include "remesa_pds_structure.h"
#include <errno.h>
#include <string.h>
static int overlaps(const void *a,size_t na,const void *b,size_t nb)
{
 if(!na||!nb)return 0;
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 if(na>UINTPTR_MAX-x||nb>UINTPTR_MAX-y)return 1;
 return x<=y?y-x<na:x-y<nb;
}
static int binding_valid(const struct remesa_pds_binding *b,uint64_t owner)
{
 if(!b->allocation_cookie||b->owner_cookie!=owner||!b->mapping_generation||
    !b->address||!b->bytes||!b->allocation_bytes)return EINVAL;
 if(b->offset>=b->allocation_bytes||b->bytes>b->allocation_bytes-b->offset||
    b->allocation_bytes>UINT64_MAX-b->address)return EOVERFLOW;
 return 0;
}
static int request_valid(const struct remesa_pds_request *r)
{
 if(!r||!r->operation||!r->generation||!r->owner_cookie)return EINVAL;
 int ret=binding_valid(&r->code,r->owner_cookie);if(ret)return ret;
 ret=binding_valid(&r->data,r->owner_cookie);if(ret)return ret;
 /* One retained allocation cannot carry contradictory mapping identities.
  * Overlap is rejected so a data update cannot corrupt symbolic code storage.
  * Distinct allocations still cannot alias declared address ranges. */
 if(r->code.allocation_cookie==r->data.allocation_cookie) {
  if(r->code.mapping_generation!=r->data.mapping_generation||
     r->code.address!=r->data.address||r->code.allocation_bytes!=r->data.allocation_bytes)return ESTALE;
 } else {
  /* Distinct retained ownership identities must describe disjoint complete
   * allocations in this request's address space. Selected windows alone do
   * not establish that a data allocation cannot alias code backing. Both
   * endpoints are representable after binding_valid above. */
  if(r->code.address<r->data.address+r->data.allocation_bytes&&
     r->data.address<r->code.address+r->code.allocation_bytes)return EINVAL;
 }
 uint64_t c=r->code.address+r->code.offset,d=r->data.address+r->data.offset;
 if(c<d+r->data.bytes&&d<c+r->code.bytes)return EINVAL;
 return 0;
}
int remesa_pds_request_from_block(const struct remesa_pds_block_request *block,
                                 struct remesa_pds_request *out)
{
 if(!block||!out||overlaps(block,sizeof(*block),out,sizeof(*out)))return EINVAL;
 if(!block->operation||!block->generation||!block->allocation.owner_cookie||
    !block->data_bytes||!block->code_bytes)return EINVAL;
 uint64_t alignment=block->code_alignment;
 if(!alignment||(alignment&(alignment-1)))return EINVAL;
 int ret=binding_valid(&block->allocation,block->allocation.owner_cookie);if(ret)return ret;
 const struct remesa_pds_binding *b=&block->allocation;
 if(block->data_bytes>b->bytes)return EOVERFLOW;
 /* Binding bounds make start + data_bytes representable. Padding is checked
  * separately so alignment cannot wrap into another allocation/window. */
 uint64_t start=b->address+b->offset,after_data=start+block->data_bytes;
 uint64_t padding=(alignment-(after_data&(alignment-1)))&(alignment-1);
 if(padding>UINT64_MAX-after_data)return EOVERFLOW;
 uint64_t code_address=after_data+padding,used=block->data_bytes;
 if(padding>b->bytes-used)return EOVERFLOW;
 used+=padding;
 if(block->code_bytes>b->bytes-used)return EOVERFLOW;
 struct remesa_pds_request r={.operation=block->operation,
  .generation=block->generation,.owner_cookie=b->owner_cookie,
  .code=*b,.data=*b};
 r.data.bytes=block->data_bytes;
 r.code.offset=code_address-b->address;r.code.bytes=block->code_bytes;
 ret=request_valid(&r);if(ret)return ret;
 *out=r;return 0;
}
int remesa_pds_prepare(const struct remesa_usse_program *program,const struct remesa_pds_request *request,struct remesa_pds_plan *out)
{
 if(!out||!program||!request||overlaps(out,sizeof(*out),program,sizeof(*program))||
    overlaps(out,sizeof(*out),request,sizeof(*request)))return EINVAL;
 uint64_t identity;int ret=remesa_usse_identity(program,&identity);if(ret)return ret;
 ret=request_valid(request);if(ret)return ret;
 struct remesa_pds_plan p={.schema=REMESA_PDS_SCHEMA,.stage=program->stage,
  .program_identity=identity,.program=*program,.request=*request,.relocation_count=REMESA_PDS_RELOCATIONS};
 p.relocations[0].role=REMESA_PDS_CODE;p.relocations[1].role=REMESA_PDS_DATA;
 p.relocations[0].kind=p.relocations[1].kind=REMESA_PDS_SYMBOLIC_ADDRESS;
 p.relocations[0].address=request->code.address+request->code.offset;
 p.relocations[0].bytes=request->code.bytes;
 p.relocations[1].address=request->data.address+request->data.offset;
 p.relocations[1].bytes=request->data.bytes;
 *out=p;return 0;
}
int remesa_pds_validate(const struct remesa_usse_program *program,const struct remesa_pds_plan *plan)
{
 if(!plan)return EINVAL;
 uint64_t identity;int ret=remesa_usse_identity(program,&identity);if(ret)return ret;
 if(plan->schema!=REMESA_PDS_SCHEMA||plan->reserved||
    plan->relocation_count!=REMESA_PDS_RELOCATIONS)return ENOTSUP;
 ret=remesa_usse_validate(&plan->program);if(ret)return ret;
 if(plan->program_identity!=identity||plan->stage!=program->stage||
    plan->program.schema!=program->schema||plan->program.stage!=program->stage||
    plan->program.count!=program->count||
    memcmp(plan->program.instructions,program->instructions,sizeof(program->instructions)))return ESTALE;
 ret=request_valid(&plan->request);if(ret)return ret;
 for(unsigned n=0;n<REMESA_PDS_RELOCATIONS;n++) {
  const struct remesa_pds_relocation *r=&plan->relocations[n];
  const struct remesa_pds_binding *b=n?&plan->request.data:&plan->request.code;
  if(r->role!=(n?REMESA_PDS_DATA:REMESA_PDS_CODE)||r->kind!=REMESA_PDS_SYMBOLIC_ADDRESS||
     r->address!=b->address+b->offset||r->bytes!=b->bytes)return EINVAL;
 }
 return 0;
}
static int binding_equal(const struct remesa_pds_binding *a,const struct remesa_pds_binding *b)
{
 return a->allocation_cookie==b->allocation_cookie&&a->owner_cookie==b->owner_cookie&&
  a->mapping_generation==b->mapping_generation&&a->address==b->address&&
  a->offset==b->offset&&a->bytes==b->bytes&&a->allocation_bytes==b->allocation_bytes;
}
int remesa_pds_validate_binding(const struct remesa_usse_program *program,const struct remesa_pds_plan *plan,const struct remesa_pds_request *current)
{
 int ret=remesa_pds_validate(program,plan);if(ret)return ret;
 ret=request_valid(current);if(ret)return ret;
 const struct remesa_pds_request *old=&plan->request;
 if(old->operation!=current->operation||old->generation!=current->generation||
    old->owner_cookie!=current->owner_cookie||!binding_equal(&old->code,&current->code)||
    !binding_equal(&old->data,&current->data))return ESTALE;
 return 0;
}
/* Call only after both bindings have passed overflow/bounds validation. */
static int storage_disjoint(const struct remesa_pds_binding *a,
                            const struct remesa_pds_binding *b)
{
 if(a->allocation_cookie==b->allocation_cookie) {
  if(a->address!=b->address||a->allocation_bytes!=b->allocation_bytes||
     a->mapping_generation!=b->mapping_generation)return ESTALE;
 } else if(a->address<b->address+b->allocation_bytes&&
           b->address<a->address+a->allocation_bytes)return EINVAL;
 uint64_t x=a->address+a->offset,y=b->address+b->offset;
 return x<y+b->bytes&&y<x+a->bytes?EINVAL:0;
}
int remesa_pds_prepare_use_links(const struct remesa_pds_plan plans[2],
 const struct remesa_pds_binding programs[2],const uint32_t banks[2],
 const uint64_t offsets[2],struct remesa_pds_use_link links[2])
{
 if(!plans||!programs||!banks||!offsets||!links||
    overlaps(links,sizeof(*links)*2,plans,sizeof(*plans)*2)||
    overlaps(links,sizeof(*links)*2,programs,sizeof(*programs)*2)||
    overlaps(links,sizeof(*links)*2,banks,sizeof(*banks)*2)||
    overlaps(links,sizeof(*links)*2,offsets,sizeof(*offsets)*2))return EINVAL;
 struct remesa_use_entry_request requests[2]={{0}};
 struct remesa_pds_use_link result[2]={0};
 const enum remesa_ir_stage stages[2]={REMESA_IR_VERTEX,REMESA_IR_FRAGMENT};
 for(unsigned i=0;i<2;i++) {
  const struct remesa_pds_request *r=&plans[i].request;
  int error=remesa_pds_validate(&plans[i].program,&plans[i]);if(error)return error;
  if(plans[i].stage!=stages[i])return EINVAL;
  if(r->owner_cookie!=plans[0].request.owner_cookie||
     r->operation!=plans[0].request.operation||
     r->generation!=plans[0].request.generation)return ESTALE;
  error=binding_valid(&programs[i],r->owner_cookie);if(error)return error;
  if(offsets[i]%4||offsets[i]>r->data.bytes||4>r->data.bytes-offsets[i])return EINVAL;
  uint64_t address=r->data.address+r->data.offset+offsets[i];
  if(address%4)return EINVAL;
  requests[i]=(struct remesa_use_entry_request){
   .allocation_address=programs[i].address,.allocation_bytes=programs[i].allocation_bytes,
   .entry_offset=programs[i].offset,.program_bytes=programs[i].bytes,
   .bank=banks[i],.data_master=i==0?REMESA_USE_VERTEX:REMESA_USE_PIXEL
  };
  result[i].request=*r;result[i].program=programs[i];result[i].stage=stages[i];
  result[i].program_identity=plans[i].program_identity;
  result[i].data_byte_offset=offsets[i];result[i].data_address=address;
 }
 /* Matching source handles alone cannot prove disjointness from either PDS
  * allocation. Check complete extents and all selected windows pairwise. */
 const struct remesa_pds_binding *regions[6]={&programs[0],&programs[1],
  &plans[0].request.code,&plans[0].request.data,
  &plans[1].request.code,&plans[1].request.data};
 for(unsigned i=0;i<6;i++)for(unsigned j=i+1;j<6;j++) {
  int error=storage_disjoint(regions[i],regions[j]);if(error)return error;
 }
 struct remesa_use_entry_plan entries[2];
 int error=remesa_use_entry_pair_prepare(requests,entries);if(error)return error;
 for(unsigned i=0;i<2;i++) {
  result[i].entry=entries[i];
  error=remesa_use_entry_preview_fixup(&entries[i],&result[i].address_bits);
  if(error)return error;
 }
 links[0]=result[0];links[1]=result[1];return 0;
}
static int entry_equal(const struct remesa_use_entry_plan *a,
                       const struct remesa_use_entry_plan *b)
{
 return a->base_address==b->base_address&&a->entry_address==b->entry_address&&
  a->program_end==b->program_end&&a->bank==b->bank&&
  a->register_offset==b->register_offset&&a->register_word==b->register_word&&
  a->entry_byte_offset==b->entry_byte_offset;
}
int remesa_pds_validate_use_links(const struct remesa_pds_plan plans[2],
 const struct remesa_pds_binding programs[2],const struct remesa_pds_use_link links[2])
{
 if(!links||sizeof(*links)*2>UINTPTR_MAX-(uintptr_t)links)return EINVAL;
 const uint32_t banks[2]={links[0].entry.bank,links[1].entry.bank};
 const uint64_t offsets[2]={links[0].data_byte_offset,links[1].data_byte_offset};
 struct remesa_pds_use_link current[2];
 int error=remesa_pds_prepare_use_links(plans,programs,banks,offsets,current);if(error)return error;
 for(unsigned i=0;i<2;i++) {
  const struct remesa_pds_use_link *a=&links[i],*b=&current[i];
  if(a->stage!=b->stage||a->program_identity!=b->program_identity||
     a->data_address!=b->data_address||a->address_bits!=b->address_bits||
     !entry_equal(&a->entry,&b->entry)||!binding_equal(&a->program,&b->program)||
     a->request.owner_cookie!=b->request.owner_cookie||
     a->request.operation!=b->request.operation||a->request.generation!=b->request.generation||
     !binding_equal(&a->request.code,&b->request.code)||
     !binding_equal(&a->request.data,&b->request.data))return ESTALE;
 }
 return 0;
}
int remesa_pds_preview_record_pair(const struct remesa_pds_plan *plans,
 const struct remesa_pds_binding *programs,const struct remesa_pds_use_link *links,
 struct remesa_pds_record_preview *out)
{
 struct remesa_pds_record_preview result[2]={{0}};
 if(!plans||!programs||!links||!out||
    overlaps(out,sizeof(result),plans,2*sizeof(*plans))||
    overlaps(out,sizeof(result),programs,2*sizeof(*programs))||
    overlaps(out,sizeof(result),links,2*sizeof(*links))||
    sizeof(result)>UINTPTR_MAX-(uintptr_t)out)return EINVAL;
 int error=remesa_pds_validate_use_links(plans,programs,links);if(error)return error;
 for(unsigned i=0;i<2;i++) {
  const struct remesa_pds_request *r=&plans[i].request;
  const struct remesa_pds_binding *d=&r->data,*c=&r->code;
  /* This CPU projection is only defined for one contiguous retained block.
   * Other symbolic PDS layouts may be valid, but have no admitted projection. */
  if(d->allocation_cookie!=c->allocation_cookie)return ENOTSUP;
  uint64_t b=d->address+d->offset,code=c->address+c->offset;
  if(b>UINT32_MAX||code>UINT32_MAX||c->bytes>UINT32_MAX-code)return ERANGE;
  if(code<b||code-b<d->bytes)return EINVAL;
  uint64_t q=code-b;
  if((b&15)||(q&15))return EINVAL;
  if(!q||q>=0x400)return ERANGE;
  struct remesa_pds_record_preview *p=&result[i];
  p->operation=r->operation;p->generation=r->generation;p->owner_cookie=r->owner_cookie;
  p->allocation_cookie=d->allocation_cookie;p->mapping_generation=d->mapping_generation;
  p->block_address=b;p->prefix_bytes=q;p->code_window_address=code;p->code_bytes=c->bytes;
  p->use_entry_address=links[i].entry.entry_address;p->use_program_bytes=programs[i].bytes;
  p->data_fixup_address=links[i].data_address;p->use_address_bits=links[i].address_bits;
  /* Audited Xpsb primary fragment relocation: mask00ffffff and size<<26.
   * Public psb fixup replaces the complete word, so bits25:24 are zero.
   * The vertex projection has no equivalent qualified producer here. */
  p->address_prefix_known_mask=i==1?UINT32_MAX:UINT32_C(0xfcffffff);
  p->address_prefix_bits=((uint32_t)(b>>4)&UINT32_C(0x00ffffff))|((uint32_t)q<<22);
  p->high_address_region=(uint32_t)b&UINT32_C(0xf0000000);
 }
 out[0]=result[0];out[1]=result[1];return 0;
}
static int host_entry_targets(const struct remesa_pds_plan *plans,
 const struct remesa_pds_binding *programs,const struct remesa_pds_use_link *links,
 const struct remesa_pds_host_data *data,unsigned char *target[2])
{
 if(!data||2*sizeof(*data)>UINTPTR_MAX-(uintptr_t)data)return EINVAL;
 /* Entry fixups require validated USE links and bounded DATA, not a T+8
  * projection. CODE/DATA can be separate symbolic allocations; no receiver
  * framing or first-PC choice is needed to patch the owned host bytes. */
 int error=remesa_pds_validate_use_links(plans,programs,links);if(error)return error;
 for(unsigned i=0;i<2;i++) {
  const struct remesa_pds_host_data *d=&data[i];
  if(!binding_equal(&d->binding,&plans[i].request.data))return ESTALE;
  if(!d->bytes||d->byte_length!=d->binding.bytes)return EINVAL;
  if(overlaps(d->bytes,d->byte_length,plans,2*sizeof(*plans))||
     overlaps(d->bytes,d->byte_length,programs,2*sizeof(*programs))||
     overlaps(d->bytes,d->byte_length,links,2*sizeof(*links))||
     overlaps(d->bytes,d->byte_length,data,2*sizeof(*data)))return EINVAL;
  if(i&&overlaps(d->bytes,d->byte_length,data[0].bytes,data[0].byte_length))return EINVAL;
  uint64_t offset=links[i].data_byte_offset;
  /* Revalidation already bounds this four-byte location; retain a local
   * subtraction check so publication cannot depend on unchecked arithmetic. */
  if(offset>d->byte_length||4>d->byte_length-offset)return ERANGE;
  target[i]=(unsigned char *)d->bytes+(size_t)offset;
 }
 return 0;
}
int remesa_pds_validate_host_entry_fixups(const struct remesa_pds_plan *plans,
 const struct remesa_pds_binding *programs,const struct remesa_pds_use_link *links,
 const struct remesa_pds_host_data *data)
{
 unsigned char *target[2];
 int error=host_entry_targets(plans,programs,links,data,target);if(error)return error;
 for(unsigned i=0;i<2;i++) {
  uint32_t word=(uint32_t)target[i][0]|((uint32_t)target[i][1]<<8)|
   ((uint32_t)target[i][2]<<16)|((uint32_t)target[i][3]<<24);
  if((word&UINT32_C(0x7ffff))!=links[i].address_bits)return EPROTO;
 }
 return 0;
}
int remesa_pds_apply_host_entry_fixups(const struct remesa_pds_plan *plans,
 const struct remesa_pds_binding *programs,const struct remesa_pds_use_link *links,
 const struct remesa_pds_host_data *data)
{
 unsigned char *target[2];uint32_t words[2];
 int error=host_entry_targets(plans,programs,links,data,target);if(error)return error;
 for(unsigned i=0;i<2;i++) {
  uint32_t previous=(uint32_t)target[i][0]|((uint32_t)target[i][1]<<8)|
   ((uint32_t)target[i][2]<<16)|((uint32_t)target[i][3]<<24);
  words[i]=(previous&~UINT32_C(0x7ffff))|links[i].address_bits;
 }
 /* No fallible operation remains. This is transactional on errors, not a
  * concurrency primitive or CPU/GPU cache-publication operation. */
 for(unsigned i=0;i<2;i++)for(unsigned byte=0;byte<4;byte++)
  target[i][byte]=(unsigned char)(words[i]>>(8*byte));
 return 0;
}
/* Independent CPU arithmetic, not a copied task/program template.
 * Xpsb ELF0x893e loads metadata+0x18;0x8948 shifts27;0x894b writes DATA+4.
 * Evidence: phase9-execution-contract-20261018/evidence/
 * CPU-TASK-CONTRACT-EVIDENCE.json. Only observed values0/1 are admitted;
 * the field's hardware resource/termination meaning is unknown. */
int remesa_pds_cpu_meta6_pack(uint32_t metadata6,uint32_t *word)
{
 if(!word||sizeof(*word)>UINTPTR_MAX-(uintptr_t)word)return EINVAL;
 if(metadata6>1)return ERANGE;
 *word=metadata6<<27;return 0;
}
int remesa_pds_cpu_meta6_unpack(uint32_t word,uint32_t *metadata6)
{
 if(!metadata6||sizeof(*metadata6)>UINTPTR_MAX-(uintptr_t)metadata6)return EINVAL;
 if(word&~UINT32_C(0x08000000))return EPROTO;
 *metadata6=word>>27;return 0;
}
static int host_data_recipes(const struct remesa_pds_plan *plans,
 const struct remesa_pds_binding *programs,const struct remesa_pds_use_link *links,
 const struct remesa_pds_host_data_recipe *recipes,
 const struct remesa_pds_host_data *data)
{
 if(!recipes||2*sizeof(*recipes)>UINTPTR_MAX-(uintptr_t)recipes)return EINVAL;
 unsigned char *targets[2];
 int error=host_entry_targets(plans,programs,links,data,targets);if(error)return error;
 for(unsigned i=0;i<2;i++)
  if(overlaps(data[i].bytes,data[i].byte_length,recipes,2*sizeof(*recipes)))return EINVAL;
 for(unsigned i=0;i<2;i++) {
  const struct remesa_pds_host_data_recipe *r=&recipes[i];
  if(!r->cells||!r->cell_count||r->cell_count>SIZE_MAX/sizeof(*r->cells))return EINVAL;
  size_t source_bytes=r->cell_count*sizeof(*r->cells);
  if(source_bytes>UINTPTR_MAX-(uintptr_t)r->cells)return EINVAL;
  if(data[i].byte_length%4||r->cell_count!=data[i].byte_length/4)return EINVAL;
  if(overlaps(r->cells,source_bytes,plans,2*sizeof(*plans))||
     overlaps(r->cells,source_bytes,programs,2*sizeof(*programs))||
     overlaps(r->cells,source_bytes,links,2*sizeof(*links))||
     overlaps(r->cells,source_bytes,recipes,2*sizeof(*recipes))||
     overlaps(r->cells,source_bytes,data,2*sizeof(*data)))return EINVAL;
  for(unsigned j=0;j<2;j++)
   if(overlaps(data[j].bytes,data[j].byte_length,r->cells,source_bytes))return EINVAL;
  unsigned relocations=0;
  for(size_t cell=0;cell<r->cell_count;cell++) {
   const struct remesa_pds_data_cell *c=&r->cells[cell];
   switch(c->kind) {
   case REMESA_PDS_DATA_LITERAL32:break;
   case REMESA_PDS_DATA_CPU_META6:
    if(c->value>1)return ERANGE;
    break;
   case REMESA_PDS_DATA_USE_ENTRY_LOWER19:
    if((c->value&UINT32_C(0x7ffff))||cell*4!=links[i].data_byte_offset)return EINVAL;
    relocations++;break;
   default:return ENOTSUP;
   }
  }
  if(relocations!=1)return EINVAL;
 }
 return 0;
}
static uint32_t host_data_cell_word(const struct remesa_pds_data_cell *cell,
 const struct remesa_pds_use_link *link)
{
 if(cell->kind==REMESA_PDS_DATA_CPU_META6)return cell->value<<27;
 return cell->value|(cell->kind==REMESA_PDS_DATA_USE_ENTRY_LOWER19?
  link->address_bits:0);
}
int remesa_pds_build_host_data_pair(const struct remesa_pds_plan *plans,
 const struct remesa_pds_binding *programs,const struct remesa_pds_use_link *links,
 const struct remesa_pds_host_data_recipe *recipes,
 const struct remesa_pds_host_data *data)
{
 int error=host_data_recipes(plans,programs,links,recipes,data);if(error)return error;
 /* All fallible validation precedes publication. Literals carry no hardware
  * meaning; only lower19 entry relocation and the observed bounded CPU
  * metadata6 field packing are supplied. */
 for(unsigned i=0;i<2;i++)for(size_t cell=0;cell<recipes[i].cell_count;cell++) {
  uint32_t word=host_data_cell_word(&recipes[i].cells[cell],&links[i]);
  unsigned char *p=(unsigned char *)data[i].bytes+cell*4;
  for(unsigned byte=0;byte<4;byte++)p[byte]=(unsigned char)(word>>(8*byte));
 }
 return 0;
}
int remesa_pds_validate_host_data_pair(const struct remesa_pds_plan *plans,
 const struct remesa_pds_binding *programs,const struct remesa_pds_use_link *links,
 const struct remesa_pds_host_data_recipe *recipes,
 const struct remesa_pds_host_data *data)
{
 int error=host_data_recipes(plans,programs,links,recipes,data);if(error)return error;
 for(unsigned i=0;i<2;i++)for(size_t cell=0;cell<recipes[i].cell_count;cell++) {
  uint32_t word=host_data_cell_word(&recipes[i].cells[cell],&links[i]);
  const unsigned char *p=(const unsigned char *)data[i].bytes+cell*4;
  for(unsigned byte=0;byte<4;byte++)if(p[byte]!=(unsigned char)(word>>(8*byte)))return EPROTO;
 }
 return 0;
}
/* No PDS ISA or architectural DS bank is represented by this planner. It
 * resolves the existing original DATA recipe into an explicit host value
 * graph, retaining the exact CPU relocation evidence and its unknown bits. */
static int host_task_ranges(const struct remesa_pds_plan *plans,
 const struct remesa_pds_binding *programs,const struct remesa_pds_use_link *links,
 const struct remesa_pds_host_data_recipe *recipes,
 const struct remesa_pds_host_transfer_request *requests,
 const struct remesa_pds_host_task_plan *tasks)
{
 const void *descriptors[5]={plans,programs,links,recipes,requests};
 const size_t lengths[5]={2*sizeof(*plans),2*sizeof(*programs),2*sizeof(*links),
  2*sizeof(*recipes),2*sizeof(*requests)};
 if(!tasks||2*sizeof(*tasks)>UINTPTR_MAX-(uintptr_t)tasks)return EINVAL;
 for(unsigned i=0;i<5;i++) {
  if(!descriptors[i]||lengths[i]>UINTPTR_MAX-(uintptr_t)descriptors[i]||
     overlaps(tasks,2*sizeof(*tasks),descriptors[i],lengths[i]))return EINVAL;
 }
 /* Check array ranges before dereferencing a cell or transfer. The bounded
  * host limit is independent of PDS/DS register capacity or instruction width. */
 for(unsigned i=0;i<2;i++) {
  const struct remesa_pds_host_data_recipe *r=&recipes[i];
  const struct remesa_pds_host_transfer_request *t=&requests[i];
  if(!r->cells||!r->cell_count||!t->transfers||!t->transfer_count||
     r->cell_count>SIZE_MAX/sizeof(*r->cells)||
     t->transfer_count>SIZE_MAX/sizeof(*t->transfers))return EINVAL;
  size_t rb=r->cell_count*sizeof(*r->cells),tb=t->transfer_count*sizeof(*t->transfers);
  if(rb>UINTPTR_MAX-(uintptr_t)r->cells||tb>UINTPTR_MAX-(uintptr_t)t->transfers||
     overlaps(tasks,2*sizeof(*tasks),r->cells,rb)||
     overlaps(tasks,2*sizeof(*tasks),t->transfers,tb))return EINVAL;
  for(unsigned j=0;j<5;j++)
   if(overlaps(r->cells,rb,descriptors[j],lengths[j])||
      overlaps(t->transfers,tb,descriptors[j],lengths[j]))return EINVAL;
  if(r->cell_count>REMESA_PDS_HOST_TASK_SLOTS||
     t->transfer_count>REMESA_PDS_HOST_TASK_SLOTS)return E2BIG;
  if(r->cell_count!=t->transfer_count)return EINVAL;
 }
 for(unsigned i=0;i<2;i++)for(unsigned j=0;j<2;j++)
  if(overlaps(recipes[i].cells,recipes[i].cell_count*sizeof(*recipes[i].cells),
     requests[j].transfers,requests[j].transfer_count*sizeof(*requests[j].transfers)))return EINVAL;
 return 0;
}
int remesa_pds_prepare_task_pair(const struct remesa_pds_plan *plans,
 const struct remesa_pds_binding *programs,const struct remesa_pds_use_link *links,
 const struct remesa_pds_host_data_recipe *recipes,
 const struct remesa_pds_host_transfer_request *requests,
 struct remesa_pds_host_task_plan *out)
{
 int error=host_task_ranges(plans,programs,links,recipes,requests,out);if(error)return error;
 error=remesa_pds_validate_use_links(plans,programs,links);if(error)return error;
 struct remesa_pds_host_task_plan result[2];memset(result,0,sizeof(result));
 for(unsigned i=0;i<2;i++) {
  const struct remesa_pds_host_data_recipe *r=&recipes[i];
  const struct remesa_pds_host_transfer_request *t=&requests[i];
  const struct remesa_pds_binding *data=&plans[i].request.data;
  if(data->bytes%4||r->cell_count!=data->bytes/4)return EINVAL;
  struct remesa_pds_host_task_plan *p=&result[i];
  p->schema=REMESA_PDS_HOST_TASK_SCHEMA;p->stage=plans[i].stage;p->link=links[i];
  p->slot_count=(uint32_t)r->cell_count;
  p->data_to_ds_error=p->dispatch_error=p->termination_error=ENOTSUP;
  unsigned sources[REMESA_PDS_HOST_TASK_SLOTS]={0};
  unsigned destinations[REMESA_PDS_HOST_TASK_SLOTS]={0};
  unsigned entries=0;
  for(size_t n=0;n<t->transfer_count;n++) {
   const struct remesa_pds_host_transfer *x=&t->transfers[n];
   if(x->data_cell>=r->cell_count||x->logical_slot>=REMESA_PDS_HOST_TASK_SLOTS)return ERANGE;
   if(sources[x->data_cell]++||destinations[x->logical_slot]++)return EINVAL;
   const struct remesa_pds_data_cell *cell=&r->cells[x->data_cell];
   struct remesa_pds_host_task_slot *slot=&p->slots[x->data_cell];
   slot->data_cell=x->data_cell;slot->logical_slot=x->logical_slot;
   slot->data_kind=cell->kind;slot->value=cell->value;
   slot->data_address=data->address+data->offset+(uint64_t)x->data_cell*4;
   switch(cell->kind) {
   case REMESA_PDS_DATA_LITERAL32:break;
   case REMESA_PDS_DATA_CPU_META6:
    error=remesa_pds_cpu_meta6_pack(cell->value,&slot->value);if(error)return error;
    break;
   case REMESA_PDS_DATA_USE_ENTRY_LOWER19:
    if((cell->value&UINT32_C(0x7ffff))||
       (uint64_t)x->data_cell*4!=links[i].data_byte_offset)return EINVAL;
    entries++;p->entry_logical_slot=x->logical_slot;
    slot->value|=links[i].address_bits;
    /* This mask identifies software relocation fields only. It is not
     * evidence that the remaining task bits are valid hardware controls. */
    slot->cpu_relocation_mask=UINT32_C(0x7ffff);break;
   default:return ENOTSUP;
   }
  }
  if(entries!=1)return EINVAL;
  for(size_t n=0;n<r->cell_count;n++)if(sources[n]!=1)return EINVAL;
 }
 out[0]=result[0];out[1]=result[1];return 0;
}
int remesa_pds_validate_task_pair(const struct remesa_pds_plan *plans,
 const struct remesa_pds_binding *programs,const struct remesa_pds_use_link *links,
 const struct remesa_pds_host_data_recipe *recipes,
 const struct remesa_pds_host_transfer_request *requests,
 const struct remesa_pds_host_task_plan *tasks)
{
 int error=host_task_ranges(plans,programs,links,recipes,requests,tasks);if(error)return error;
 struct remesa_pds_host_task_plan current[2];
 error=remesa_pds_prepare_task_pair(plans,programs,links,recipes,requests,current);if(error)return error;
 struct remesa_pds_use_link retained[2]={tasks[0].link,tasks[1].link};
 error=remesa_pds_validate_use_links(plans,programs,retained);if(error)return error;
 for(unsigned i=0;i<2;i++) {
  const struct remesa_pds_host_task_plan *a=&tasks[i],*b=&current[i];
  /* A retained link can be independently valid for a different bank/offset.
   * It must also equal the exact live link used to rebuild these slot values. */
  if(!entry_equal(&a->link.entry,&b->link.entry)||
     a->link.data_byte_offset!=b->link.data_byte_offset)return ESTALE;
  if(a->schema!=b->schema||a->reserved||a->stage!=b->stage||
     a->slot_count!=b->slot_count||a->entry_logical_slot!=b->entry_logical_slot||
     a->data_to_ds_error!=b->data_to_ds_error||a->dispatch_error!=b->dispatch_error||
     a->termination_error!=b->termination_error)return ESTALE;
  for(unsigned n=0;n<REMESA_PDS_HOST_TASK_SLOTS;n++) {
   const struct remesa_pds_host_task_slot *x=&a->slots[n],*y=&b->slots[n];
   if(x->data_cell!=y->data_cell||x->logical_slot!=y->logical_slot||
      x->data_kind!=y->data_kind||x->value!=y->value||
      x->cpu_relocation_mask!=y->cpu_relocation_mask||
      x->data_address!=y->data_address)return ESTALE;
  }
 }
 return 0;
}
/* Scalar CPU profile, reconstructed from Xpsb zero-texture DATA producers:
 * background0x100000 is preserved by lower19 op5/op4 relocations, metadata6
 * is shifted27 and written at DATA+4, literal0x20 is written at DATA+0x20.
 * No GPU instruction bytes or hardware dispatch semantics are used here. */
int remesa_pds_cpu_pixel_task_pack(const struct remesa_use_entry_plan *entry,
 uint32_t metadata6,struct remesa_pds_cpu_pixel_task *out)
{
 if(!entry||!out||sizeof(*entry)>UINTPTR_MAX-(uintptr_t)entry||
    sizeof(*out)>UINTPTR_MAX-(uintptr_t)out||
    overlaps(entry,sizeof(*entry),out,sizeof(*out)))return EINVAL;
 struct remesa_pds_cpu_pixel_task task={{0}};
 uint32_t address_bits;uint64_t base;enum remesa_use_data_master master;
 int error=remesa_use_entry_preview_fixup(entry,&address_bits);if(error)return error;
 error=remesa_use_base_unpack_address(entry->register_word,&base,&master);if(error)return error;
 if(master!=REMESA_USE_PIXEL)return ENOTSUP;
 error=remesa_pds_cpu_meta6_pack(metadata6,&task.words[1]);if(error)return error;
 task.words[0]=UINT32_C(0x00100000)|address_bits;
 task.words[2]=UINT32_C(0x20);
 *out=task;return 0;
}
int remesa_pds_cpu_pixel_task_unpack(const struct remesa_pds_cpu_pixel_task *task,
 struct remesa_pds_cpu_pixel_task_fields *out)
{
 if(!task||!out||sizeof(*task)>UINTPTR_MAX-(uintptr_t)task||
    sizeof(*out)>UINTPTR_MAX-(uintptr_t)out||
    overlaps(task,sizeof(*task),out,sizeof(*out)))return EINVAL;
 if((task->words[0]&~UINT32_C(0x7ffff))!=UINT32_C(0x00100000)||
    task->words[2]!=UINT32_C(0x20))return EPROTO;
 struct remesa_pds_cpu_pixel_task_fields fields={0};
 int error=remesa_use_entry_decode_fixup(task->words[0]&UINT32_C(0x7ffff),&fields.use);
 if(error)return error;
 error=remesa_pds_cpu_meta6_unpack(task->words[1],&fields.metadata6);if(error)return error;
 *out=fields;return 0;
}
int remesa_pds_cpu_pixel_task_recipe(uint32_t metadata6,
 struct remesa_pds_data_cell out[REMESA_PDS_CPU_PIXEL_DATA_CELLS])
{
 const size_t bytes=sizeof(*out)*REMESA_PDS_CPU_PIXEL_DATA_CELLS;
 if(!out||bytes>UINTPTR_MAX-(uintptr_t)out)return EINVAL;
 uint32_t word;int error=remesa_pds_cpu_meta6_pack(metadata6,&word);if(error)return error;
 struct remesa_pds_data_cell cells[REMESA_PDS_CPU_PIXEL_DATA_CELLS];
 for(unsigned i=0;i<REMESA_PDS_CPU_PIXEL_DATA_CELLS;i++)
  cells[i]=(struct remesa_pds_data_cell){REMESA_PDS_DATA_LITERAL32,0};
 cells[0]=(struct remesa_pds_data_cell){REMESA_PDS_DATA_USE_ENTRY_LOWER19,UINT32_C(0x00100000)};
 cells[1]=(struct remesa_pds_data_cell){REMESA_PDS_DATA_CPU_META6,metadata6};
 cells[8].value=UINT32_C(0x20);
 memcpy(out,cells,bytes);return 0;
}
int remesa_pds_cpu_pixel_task_prepare(const struct remesa_pds_host_task_plan *plan,
 struct remesa_pds_cpu_pixel_task_output *out)
{
 if(!plan||!out||sizeof(*plan)>UINTPTR_MAX-(uintptr_t)plan||
    sizeof(*out)>UINTPTR_MAX-(uintptr_t)out||
    overlaps(plan,sizeof(*plan),out,sizeof(*out)))return EINVAL;
 if(plan->schema!=REMESA_PDS_HOST_TASK_SCHEMA||plan->reserved||
    plan->stage!=REMESA_IR_FRAGMENT||plan->link.stage!=REMESA_IR_FRAGMENT||
    plan->slot_count!=REMESA_PDS_CPU_PIXEL_DATA_CELLS||
    plan->data_to_ds_error!=ENOTSUP||plan->dispatch_error!=ENOTSUP||
    plan->termination_error!=ENOTSUP)return EPROTO;
 const struct remesa_pds_use_link *link=&plan->link;
 int error=request_valid(&link->request);if(error)return error;
 error=binding_valid(&link->program,link->request.owner_cookie);if(error)return error;
 const struct remesa_pds_binding *data=&link->request.data;
 if(!link->program_identity||data->bytes!=4*REMESA_PDS_CPU_PIXEL_DATA_CELLS||
    link->data_byte_offset||data->address%4||data->offset%4)return EPROTO;
 uint64_t data_address=data->address+data->offset;
 if(link->data_address!=data_address)return ESTALE;
 const struct remesa_pds_binding *regions[3]={&link->request.code,data,&link->program};
 for(unsigned i=0;i<3;i++)for(unsigned j=i+1;j<3;j++) {
  error=storage_disjoint(regions[i],regions[j]);if(error)return error;
 }
 struct remesa_use_entry_request entry_request={
  .allocation_address=link->program.address,.allocation_bytes=link->program.allocation_bytes,
  .entry_offset=link->program.offset,.program_bytes=link->program.bytes,
  .bank=link->entry.bank,.data_master=REMESA_USE_PIXEL
 };
 struct remesa_use_entry_plan current_entry;
 error=remesa_use_entry_prepare(&entry_request,&current_entry);if(error)return error;
 if(!entry_equal(&current_entry,&link->entry))return ESTALE;
 uint32_t address_bits;
 error=remesa_use_entry_preview_fixup(&current_entry,&address_bits);if(error)return error;
 if(link->address_bits!=address_bits)return ESTALE;
 uint32_t metadata6;
 error=remesa_pds_cpu_meta6_unpack(plan->slots[1].value,&metadata6);if(error)return error;
 struct remesa_pds_cpu_pixel_task_output result;memset(&result,0,sizeof(result));
 error=remesa_pds_cpu_pixel_task_pack(&current_entry,metadata6,&result.task);if(error)return error;
 unsigned destinations[REMESA_PDS_HOST_TASK_SLOTS]={0};
 for(unsigned i=0;i<REMESA_PDS_HOST_TASK_SLOTS;i++) {
  const struct remesa_pds_host_task_slot *slot=&plan->slots[i];
  if(i>=REMESA_PDS_CPU_PIXEL_DATA_CELLS) {
   if(slot->data_cell||slot->logical_slot||slot->data_kind||slot->value||
      slot->cpu_relocation_mask||slot->data_address)return EPROTO;
   continue;
  }
  uint32_t kind=REMESA_PDS_DATA_LITERAL32,value=0,mask=0;
  if(i==0) {kind=REMESA_PDS_DATA_USE_ENTRY_LOWER19;value=result.task.words[0];mask=UINT32_C(0x7ffff);}
  else if(i==1) {kind=REMESA_PDS_DATA_CPU_META6;value=result.task.words[1];}
  else if(i==8)value=result.task.words[2];
  if(slot->data_cell!=i||slot->logical_slot>=REMESA_PDS_HOST_TASK_SLOTS||
     slot->data_kind!=kind||slot->value!=value||slot->cpu_relocation_mask!=mask||
     slot->data_address!=data_address+(uint64_t)i*4)return EPROTO;
  if(destinations[slot->logical_slot]++)return EPROTO;
 }
 if(plan->entry_logical_slot!=plan->slots[0].logical_slot)return EPROTO;
 const unsigned cells[REMESA_PDS_CPU_PIXEL_TASK_WORDS]={0,1,8};
 for(unsigned i=0;i<REMESA_PDS_CPU_PIXEL_TASK_WORDS;i++) {
  const struct remesa_pds_host_task_slot *slot=&plan->slots[cells[i]];
  result.sources[i].data_cell=slot->data_cell;result.sources[i].logical_slot=slot->logical_slot;
  result.sources[i].value=slot->value;result.sources[i].data_address=slot->data_address;
 }
 result.schema=REMESA_PDS_CPU_PIXEL_TASK_SCHEMA;
 result.cpu_word_count=REMESA_PDS_CPU_PIXEL_TASK_WORDS;
 result.data_to_ds_error=result.dispatch_error=result.termination_error=ENOTSUP;
 *out=result;return 0;
}
void remesa_pds_encoding_status(struct remesa_pds_encoding_status *status)
{if(status)memset(status,0,sizeof(*status));}
int remesa_pds_encode(const struct remesa_usse_program *program,const struct remesa_pds_plan *plan,void *out,size_t capacity,size_t *written)
{
 if(!written||(!out&&capacity))return EINVAL;
 int ret=remesa_pds_validate(program,plan);return ret?ret:ENOTSUP;
}

int remesa_pds_candidate_decode(uint32_t word,struct remesa_pds_candidate_request *out)
{return remesa_pds_candidate_decode_condition(word,7,out);}
int remesa_pds_candidate_decode_condition(uint32_t word,uint32_t condition,
 struct remesa_pds_candidate_request *out)
{
 if(!out)return EINVAL;
 if(condition!=3&&condition!=7)return ENOTSUP;
 if(word&UINT32_C(0x10))return EPROTO;
 if((word>>27)||((word>>24)&7)!=condition||(word&15)!=5)return ENOTSUP;
 struct remesa_pds_candidate_request r={.policy=REMESA_PDS_CANDIDATE_ANALYSIS_ONLY,
  .source={(word>>18)&63,(word>>13)&31}};
 for(unsigned n=0;n<4;n++)r.selector[n]=(word>>(11-2*n))&3;
 *out=r;return 0;
}
int remesa_pds_candidate_preview(const struct remesa_pds_candidate_request *r,
                               struct remesa_pds_candidate_preview *out)
{return remesa_pds_candidate_preview_condition(r,7,out);}
int remesa_pds_candidate_preview_condition(const struct remesa_pds_candidate_request *r,
 uint32_t condition,struct remesa_pds_candidate_preview *out)
{
 if(!r||!out||overlaps(r,sizeof(*r),out,sizeof(*out)))return EINVAL;
 if(r->policy!=REMESA_PDS_CANDIDATE_ANALYSIS_ONLY||
    (condition!=3&&condition!=7))return ENOTSUP;
 if(r->source[0]>63||r->source[1]>31)return ERANGE;
 struct remesa_pds_candidate_preview p={0};
 /* Original packing from the supplied candidate partition, not a template
  * extracted from a driver or a fragment program. No sequence is emitted. */
 p.word=(condition<<24)|(r->source[0]<<18)|(r->source[1]<<13)|5u;
 for(unsigned n=0;n<4;n++) {
  uint32_t selector=r->selector[n];if(selector>3)return ERANGE;
  p.word|=selector<<(11-2*n);
  uint32_t bank=selector>>1,half=selector&1;
  p.pair[n]=(struct remesa_pds_candidate_operand){bank,2*r->source[bank]+half};
  p.scalar[n]=(struct remesa_pds_candidate_operand){bank,r->source[bank]+half};
 }
 int error=remesa_pds_candidate_decode_condition(p.word,condition,&p.decoded);if(error)return error;
 *out=p;return 0;
}

/* Selected producer bytes. The old scalar API/ABI and all execution gates
 * remain unchanged: these plans authenticate no external mapping provider. */
static int selected_request_equal(const struct remesa_pds_selected_request *a,
 const struct remesa_pds_selected_request *b)
{
 return a->profile==b->profile&&a->use_bank==b->use_bank&&
  a->semantic_identity==b->semantic_identity&&a->source_records==b->source_records&&
  a->reserved==b->reserved&&a->storage.operation==b->storage.operation&&
  a->storage.generation==b->storage.generation&&a->storage.owner_cookie==b->storage.owner_cookie&&
  binding_equal(&a->storage.code,&b->storage.code)&&binding_equal(&a->storage.data,&b->storage.data)&&
  binding_equal(&a->use,&b->use)&&binding_equal(&a->vertex_source,&b->vertex_source);
}
static int selected_binding32(const struct remesa_pds_binding *b,uint64_t owner)
{
 int e=binding_valid(b,owner);if(e)return e;
 if(b->address>UINT32_MAX||b->allocation_bytes>UINT64_C(0x100000000)-b->address)return ERANGE;
 return 0;
}
static struct remesa_pds_binding selected_block(const struct remesa_pds_selected_plan *p)
{
 struct remesa_pds_binding b=p->request.storage.data;b.bytes=p->pds_reserved_bytes;return b;
}
int remesa_pds_selected_prepare(const struct remesa_pds_selected_request *r,
 struct remesa_pds_selected_plan *out)
{
 if(!r||!out||overlaps(r,sizeof(*r),out,sizeof(*out)))return EINVAL;
 struct remesa_pds_selected_plan p;memset(&p,0,sizeof(p));
 p.schema=REMESA_PDS_SELECTED_SCHEMA;p.request=*r;p.data_bytes=48;p.alignment=32;
 if(r->reserved)return ENOTSUP;
 if(!r->semantic_identity)return EINVAL;
 switch(r->profile) {
 case REMESA_PDS_SELECTED_VS_WHITE_LAMBERT3:
  p.stage=REMESA_IR_VERTEX;p.code_bytes=16;p.use_committed_bytes=64;
  p.use_reserved_bytes=144;p.pds_reserved_bytes=192;break;
 case REMESA_PDS_SELECTED_FS_INPUT_COL0:
  p.stage=REMESA_IR_FRAGMENT;p.code_bytes=12;p.use_committed_bytes=8;
  p.use_reserved_bytes=16;p.pds_reserved_bytes=64;break;
 case REMESA_PDS_SELECTED_FS_CONSTANT_DRIVER_BG0:
  p.stage=REMESA_IR_FRAGMENT;p.code_bytes=8;p.use_committed_bytes=8;
  p.use_reserved_bytes=8;p.pds_reserved_bytes=64;break;
 default:return ENOTSUP;
 }
 p.image_bytes=p.data_bytes+p.code_bytes;
 int e=request_valid(&r->storage);if(e)return e;
 const struct remesa_pds_binding *d=&r->storage.data,*c=&r->storage.code;
 if(d->allocation_cookie!=c->allocation_cookie||d->bytes!=48||c->bytes!=p.code_bytes||
    c->address+c->offset!=d->address+d->offset+48)return ENOTSUP;
 e=selected_binding32(d,r->storage.owner_cookie);if(e)return e;
 if((d->address+d->offset)&31)return EINVAL;
 if(p.pds_reserved_bytes>d->allocation_bytes-d->offset)return ENOSPC;
 e=selected_binding32(&r->use,r->storage.owner_cookie);if(e)return e;
 if(r->use.bytes!=p.use_reserved_bytes)return ENOTSUP;
 struct remesa_use_entry_request use={.allocation_address=r->use.address,
  .allocation_bytes=r->use.allocation_bytes,.entry_offset=r->use.offset,
  .program_bytes=p.use_reserved_bytes,.bank=r->use_bank,
  .data_master=p.stage==REMESA_IR_VERTEX?REMESA_USE_VERTEX:REMESA_USE_PIXEL};
 e=remesa_use_entry_prepare(&use,&p.use_entry);if(e)return e;
 if(p.use_entry.entry_address&31)return EINVAL; /* selected HOST policy */
 e=remesa_use_entry_preview_fixup(&p.use_entry,&p.use_address_bits);if(e)return e;
 struct remesa_pds_binding block=selected_block(&p);
 e=storage_disjoint(&block,&r->use);if(e)return e;
 if(p.stage==REMESA_IR_VERTEX) {
  if(!r->source_records)return EINVAL;
  e=selected_binding32(&r->vertex_source,r->storage.owner_cookie);if(e)return e;
  if((r->vertex_source.address+r->vertex_source.offset)&3)return EINVAL;
  if((uint64_t)r->source_records*48>r->vertex_source.bytes)return ENOSPC;
  e=storage_disjoint(&block,&r->vertex_source);if(e)return e;
  e=storage_disjoint(&r->use,&r->vertex_source);if(e)return e;
 } else {
  const struct remesa_pds_binding zero={0};
  if(r->source_records||!binding_equal(&r->vertex_source,&zero))return ENOTSUP;
 }
 struct remesa_pds_selected_relocation *v=&p.relocations[0];
 *v=(struct remesa_pds_selected_relocation){.kind=REMESA_PDS_SELECTED_USE_LOWER19,
  .data_byte_offset=p.stage==REMESA_IR_VERTEX?16:0,.mask=0x7ffff,
  .background=p.stage==REMESA_IR_VERTEX?0x200000:
   r->profile==REMESA_PDS_SELECTED_FS_INPUT_COL0?0x80000:0,
  .allocation_cookie=r->use.allocation_cookie,.mapping_generation=r->use.mapping_generation,
  .address=p.use_entry.entry_address,.bytes=p.use_reserved_bytes};
 p.relocation_count=1;
 if(p.stage==REMESA_IR_VERTEX) {
  p.relocations[1]=(struct remesa_pds_selected_relocation){
   .kind=REMESA_PDS_SELECTED_SOURCE_ADDRESS32,.data_byte_offset=0,.mask=UINT32_MAX,
   .allocation_cookie=r->vertex_source.allocation_cookie,
   .mapping_generation=r->vertex_source.mapping_generation,
   .address=r->vertex_source.address+r->vertex_source.offset,
   .bytes=(uint64_t)r->source_records*48};p.relocation_count=2;
 }
 *out=p;return 0;
}
int remesa_pds_selected_validate(const struct remesa_pds_selected_plan *p,
 const struct remesa_pds_selected_request *current)
{
 if(!p||!current)return EINVAL;
 if(p->schema!=REMESA_PDS_SELECTED_SCHEMA||p->reserved)return ENOTSUP;
 struct remesa_pds_selected_plan q;
 int e=remesa_pds_selected_prepare(&p->request,&q);if(e)return e;
 if(!selected_request_equal(&p->request,current))return ESTALE;
 if(p->stage!=q.stage||p->use_address_bits!=q.use_address_bits||p->data_bytes!=q.data_bytes||
    p->code_bytes!=q.code_bytes||p->image_bytes!=q.image_bytes||p->alignment!=q.alignment||
    p->use_committed_bytes!=q.use_committed_bytes||p->use_reserved_bytes!=q.use_reserved_bytes||
    p->pds_reserved_bytes!=q.pds_reserved_bytes||p->relocation_count!=q.relocation_count||
    !entry_equal(&p->use_entry,&q.use_entry))return EINVAL;
 for(unsigned i=0;i<2;i++) {
  const struct remesa_pds_selected_relocation *a=&p->relocations[i],*b=&q.relocations[i];
  if(a->kind!=b->kind||a->data_byte_offset!=b->data_byte_offset||a->mask!=b->mask||
     a->background!=b->background||a->allocation_cookie!=b->allocation_cookie||
     a->mapping_generation!=b->mapping_generation||a->address!=b->address||a->bytes!=b->bytes)return EINVAL;
 }
 return 0;
}
int remesa_pds_selected_pair_validate(const struct remesa_pds_selected_plan p[2])
{
 if(!p)return EINVAL;
 for(unsigned i=0;i<2;i++){int e=remesa_pds_selected_validate(p+i,&p[i].request);if(e)return e;}
 if(p[0].stage!=REMESA_IR_VERTEX||p[1].stage!=REMESA_IR_FRAGMENT)return EINVAL;
 if(p[0].request.storage.owner_cookie!=p[1].request.storage.owner_cookie||
    p[0].request.storage.operation!=p[1].request.storage.operation||
    p[0].request.storage.generation!=p[1].request.storage.generation)return ESTALE;
 struct remesa_pds_binding blocks[2]={selected_block(p),selected_block(p+1)};
 const struct remesa_pds_binding *regions[5]={blocks,blocks+1,&p[0].request.use,
  &p[1].request.use,&p[0].request.vertex_source};
 for(unsigned i=0;i<5;i++)for(unsigned j=i+1;j<5;j++) {
  int e=storage_disjoint(regions[i],regions[j]);if(e)return e;
 }
 struct remesa_use_entry_request r[2];struct remesa_use_entry_plan entries[2];
 for(unsigned i=0;i<2;i++)r[i]=(struct remesa_use_entry_request){
  p[i].request.use.address,p[i].request.use.allocation_bytes,p[i].request.use.offset,
  p[i].use_reserved_bytes,p[i].request.use_bank,i?REMESA_USE_PIXEL:REMESA_USE_VERTEX};
 return remesa_use_entry_pair_prepare(r,entries);
}
int remesa_pds_selected_encode(const struct remesa_pds_selected_plan *p,void *out,
 uint8_t *known_mask,size_t capacity,size_t *written)
{
 if(!p||!out||!known_mask||!written||overlaps(p,sizeof(*p),out,capacity)||
    overlaps(p,sizeof(*p),known_mask,capacity)||overlaps(p,sizeof(*p),written,sizeof(*written))||
    overlaps(out,capacity,known_mask,capacity)||overlaps(out,capacity,written,sizeof(*written))||
    overlaps(known_mask,capacity,written,sizeof(*written)))return EINVAL;
 int e=remesa_pds_selected_validate(p,&p->request);if(e)return e;
 if(capacity<p->image_bytes)return ENOSPC;
 uint8_t image[64],mask[64];size_t size=0;
 if(p->stage==REMESA_IR_VERTEX) {
  const struct remesa_pds_binding *v=&p->request.vertex_source;
  const struct remesa_pds_vertex_image_request r={v->address,v->allocation_bytes,
   v->offset,12,p->use_address_bits};
  e=remesa_pds_structure_encode_compiled_vertex_image(&r,image,mask,sizeof(image),&size);
 } else {
  struct remesa_pds_structure_request r={.code_policy=REMESA_PDS_STRUCTURE_CODE_CPU_COMPLETE};
  if(p->request.profile==REMESA_PDS_SELECTED_FS_INPUT_COL0) {
   r.entry_count=1;r.entries[0]=(struct remesa_pds_structure_entry){
    .kind=REMESA_PDS_STRUCTURE_R,.id=10,.components=4,.mode=3,
    .format_present=1,.explicit_format=0x0c000000};
  }
  struct remesa_pds_structure shape;
  e=remesa_pds_structure_encode(&r,&shape);
  if(!e)e=remesa_pds_structure_serialize(&shape,image,mask,sizeof(image),&size);
  if(!e) {
   if(shape.use_background!=p->relocations[0].background)return EPROTO;
   uint32_t word=shape.use_background|p->use_address_bits;
   for(unsigned i=0;i<4;i++){image[i]=(uint8_t)(word>>(i*8));mask[i]=255;}
  }
 }
 if(e)return e;
 if(size!=p->image_bytes)return EPROTO;
 for(size_t i=p->data_bytes;i<size;i++)if(mask[i]!=255)return EPROTO;
 memcpy(out,image,size);memcpy(known_mask,mask,size);*written=size;return 0;
}
