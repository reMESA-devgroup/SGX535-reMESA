/* SPDX-License-Identifier: MIT; original host compilation/lifetime contract. */
#include "remesa_shader_program.h"
#include "remesa_fragment_body.h"
#include "remesa_usse_arithmetic.h"
#include "remesa/shader_request.h"
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
static int overlaps(const void *a,size_t an,const void *b,size_t bn)
{
 if(!a||!b||!an||!bn)return 0;
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 if(an>UINTPTR_MAX-x||bn>UINTPTR_MAX-y)return 1;
 return x<y+bn&&y<x+an;
}

int remesa_shader_program_prepare(const struct remesa_shader_ir *ir,
                                 struct remesa_shader_program *out)
{
 struct remesa_shader_program program={.schema=REMESA_SHADER_PROGRAM_SCHEMA,
                                      .status=REMESA_PROGRAM_SEMANTIC_ONLY};
 if(!ir||!out||overlaps(ir,sizeof(*ir),out,sizeof(*out)))return EINVAL;
 int error=remesa_shader_ir_validate(ir);
 if(error)return error;
 if(!remesa_shader_ir_white_lambert(ir)||!remesa_shader_ir_fragment_color_input(ir)||
    !remesa_shader_ir_gears_vertex(ir)||!remesa_shader_ir_fragment_flat_color_input(ir))program.status=REMESA_PROGRAM_COMPILE_OFFLINE;
 else {error=remesa_usse_lower(ir,&program.usse);if(error)return error;}
 program.semantic=*ir;
 *out=program;
 return 0;
}
int remesa_shader_program_prepare_color(uint32_t argb,struct remesa_shader_program *out)
{
 if(!out)return EINVAL;
 struct remesa_shader_ir ir={.schema=REMESA_SHADER_IR_SCHEMA,
  .stage=REMESA_IR_FRAGMENT,.count=2};
 ir.instructions[0]=(struct remesa_ir_instruction){.op=REMESA_IR_CONSTANT,
  .components=4,.type=REMESA_IR_FLOAT32};
 ir.instructions[1]=(struct remesa_ir_instruction){.op=REMESA_IR_COLOR,
  .dst=REMESA_SHADER_IR_NO_REGISTER,.components=4,.type=REMESA_IR_FLOAT32};
 const unsigned shifts[4]={16,8,0,24};
 for(unsigned c=0;c<4;c++) {
  uint32_t channel=(argb>>shifts[c])&255;
  if(channel!=0&&channel!=255)return ENOTSUP;
  ir.instructions[0].bits[c]=channel ? UINT32_C(0x3f800000) : 0;
 }
 return remesa_shader_program_prepare(&ir,out);
}
int remesa_shader_program_validate(const struct remesa_shader_program *program)
{
 struct remesa_usse_program expected={0};
 if(!program)return EINVAL;
 if(program->schema!=REMESA_SHADER_PROGRAM_SCHEMA)return ENOTSUP;
 if(program->status==REMESA_PROGRAM_COMPILE_OFFLINE) {
  int error=remesa_shader_ir_white_lambert(&program->semantic);
  if(error)error=remesa_shader_ir_fragment_color_input(&program->semantic);
  if(error)error=remesa_shader_ir_gears_vertex(&program->semantic);
  if(error)error=remesa_shader_ir_fragment_flat_color_input(&program->semantic);
  if(error)return error;
  return memcmp(&expected,&program->usse,sizeof(expected))?EPROTO:0;
 }
 if(program->status!=REMESA_PROGRAM_SEMANTIC_ONLY)return ENOTSUP;
 int error=remesa_usse_lower(&program->semantic,&expected);
 if(error)return error;
 error=remesa_usse_validate(&program->usse);
 if(error)return error;
 /* A separately valid lowering for another color/stage must not alias this
  * shader. Both structures are canonical host uint32 records, not binaries. */
 return memcmp(&expected,&program->usse,sizeof(expected)) ? EPROTO : 0;
}
int remesa_shader_program_report(const struct remesa_shader_program *program,
                                struct remesa_shader_program_report *out)
{
 if(!out||!program||overlaps(out,sizeof(*out),program,sizeof(*program)))return EINVAL;
 int error=remesa_shader_program_validate(program);
 if(error)return error;
 struct remesa_shader_program_report report={
  .stage=program->semantic.stage,.status=program->status,
  .virtual_instructions=program->usse.count,.launch_error=ENOTSUP
 };
 if(program->status==REMESA_PROGRAM_COMPILE_OFFLINE) {
  report.virtual_instructions=program->semantic.count;
  report.binary_error=0;report.binary_bytes=program->semantic.stage==REMESA_IR_VERTEX?
   (!remesa_shader_ir_gears_vertex(&program->semantic)?
    REMESA_USSE_CPU_GEARS_VERTEX_BODY_BYTES+8u:REMESA_USSE_CPU_WHITE_LAMBERT_VERTEX_BYTES):REMESA_USSE_CPU_MOVE_BYTES;
 } else report.binary_error=remesa_usse_encoded_size(&program->usse,&report.binary_bytes);
 remesa_program_register_evidence(&report.register_evidence);
 *out=report;
 return 0;
}
int remesa_shader_program_encode(const struct remesa_shader_program *program,
                                void *out,size_t capacity,size_t *written)
{
 if(!program||!written||(!out&&capacity)||
    sizeof(*program)>UINTPTR_MAX-(uintptr_t)program||
    sizeof(*written)>UINTPTR_MAX-(uintptr_t)written||
    capacity>UINTPTR_MAX-(uintptr_t)out||
    overlaps(program,sizeof(*program),out,capacity)||
    overlaps(program,sizeof(*program),written,sizeof(*written))||
    overlaps(out,capacity,written,sizeof(*written)))return EINVAL;
 int error=remesa_shader_program_validate(program);
 if(error)return error;
 if(program->status==REMESA_PROGRAM_COMPILE_OFFLINE) {
  if(!remesa_shader_ir_gears_vertex(&program->semantic)) {
   /* The full semantic profile fixes these bindings. The selected-resource
    * planner still rejects it until the actual VS parameter upload exists. */
   const struct remesa_ir_instruction *i=&program->semantic.instructions[0];
   const struct remesa_usse_cpu_gears_vertex_body request={i->input_slot,i->bits[0],i->bits[1],i->bits[3],0};
   uint8_t bytes[REMESA_USSE_CPU_GEARS_VERTEX_BODY_BYTES+8u];size_t n;
   if(capacity<sizeof(bytes))return ENOSPC;
   error=remesa_usse_cpu_gears_vertex_body_encode(&request,bytes,sizeof(bytes),&n);
   if(error)return error;
   if(n!=REMESA_USSE_CPU_GEARS_VERTEX_BODY_BYTES)return EPROTO;
   const struct remesa_usse_cpu_three_source tail={.source={{0,0},{2,0},{2,0}},
    .source_extension={0,1,1},.word0_control21=1,.word1_control12_13=1,.word1_control18=1};
   error=remesa_usse_cpu_three_source_encode(&tail,bytes+n,8,&n);if(error)return error;
   if(n!=8)return EPROTO;
   memcpy(out,bytes,sizeof(bytes));*written=sizeof(bytes);return 0;
  }
  if(program->semantic.stage==REMESA_IR_FRAGMENT) {
   if(capacity<REMESA_USSE_CPU_MOVE_BYTES)return ENOSPC;
   const struct remesa_usse_fragment_color_request request={1,0};
   struct remesa_usse_fragment_color_extent extent;
   error=remesa_usse_fragment_color_encode(&request,out,capacity,&extent);
   if(!error)*written=extent.logical_bytes;
   return error;
  }
  /* The admitted IR fixes three independent contiguous float4 inputs.
   * This selects only the checked CPU layout, not a device launch mapping. */
  const struct remesa_usse_cpu_white_lambert mapping={0,4,8,0};
  return remesa_usse_cpu_white_lambert_vertex_encode(&mapping,out,capacity,written);
 }
 return remesa_usse_encode(&program->usse,out,capacity,written);
}
int remesa_shader_program_fragment_color_extent(const struct remesa_shader_program *program,
 struct remesa_usse_fragment_color_extent *out)
{
 if(!program||!out||overlaps(program,sizeof(*program),out,sizeof(*out)))return EINVAL;
 int error=remesa_shader_program_validate(program);if(error)return error;
 if(program->status!=REMESA_PROGRAM_COMPILE_OFFLINE||
    remesa_shader_ir_fragment_color_input(&program->semantic))return ENOTSUP;
 unsigned char bytes[REMESA_USSE_CPU_MOVE_BYTES];
 const struct remesa_usse_fragment_color_request request={1,0};
 return remesa_usse_fragment_color_encode(&request,bytes,sizeof(bytes),out);
}
void remesa_shader_program_finish(struct remesa_shader_program *program)
{if(program)memset(program,0,sizeof(*program));}
int remesa_shader_program_prepare_launch(const struct remesa_shader_program *program,
 const struct remesa_pds_request *request,struct remesa_pds_plan *out)
{
 if(!out||!program||overlaps(program,sizeof(*program),out,sizeof(*out)))return EINVAL;
 int error=remesa_shader_program_validate(program);
 if(error)return error;
 if(program->status==REMESA_PROGRAM_COMPILE_OFFLINE)return ENOTSUP;
 return remesa_pds_prepare(&program->usse,request,out);
}
int remesa_shader_program_encode_launch(const struct remesa_shader_program *program,
 const struct remesa_pds_plan *plan,const struct remesa_pds_request *current,
 void *out,size_t capacity,size_t *written)
{
 if(!written||(!out&&capacity))return EINVAL;
 int error=remesa_shader_program_validate(program);
 if(error)return error;
 if(program->status==REMESA_PROGRAM_COMPILE_OFFLINE)return ENOTSUP;
 error=remesa_pds_validate_binding(&program->usse,plan,current);
 if(error)return error;
 return remesa_pds_encode(&program->usse,plan,out,capacity,written);
}
int remesa_shader_program_preview_pds(const struct remesa_shader_program *program,
 const struct remesa_pds_candidate_request *request,struct remesa_shader_pds_preview *out)
{
 if(!program||!request||!out||overlaps(out,sizeof(*out),program,sizeof(*program))||
    overlaps(out,sizeof(*out),request,sizeof(*request)))return EINVAL;
 int error=remesa_shader_program_validate(program);if(error)return error;
 if(program->status==REMESA_PROGRAM_COMPILE_OFFLINE)return ENOTSUP;
 struct remesa_shader_pds_preview preview={.stage=program->semantic.stage};
 error=remesa_usse_identity(&program->usse,&preview.semantic_identity);if(error)return error;
 error=remesa_pds_candidate_preview(request,&preview.candidate);if(error)return error;
 *out=preview;return 0;
}
int remesa_shader_pipeline_prepare(const struct remesa_shader_program *vertex,
 const struct remesa_shader_program *fragment,struct remesa_shader_pipeline *out)
{
 if(!vertex||!fragment||!out||overlaps(out,sizeof(*out),vertex,sizeof(*vertex))||
    overlaps(out,sizeof(*out),fragment,sizeof(*fragment)))return EINVAL;
 int error=remesa_shader_program_validate(vertex);if(error)return error;
 error=remesa_shader_program_validate(fragment);if(error)return error;
 if(vertex->status==REMESA_PROGRAM_COMPILE_OFFLINE||fragment->status==REMESA_PROGRAM_COMPILE_OFFLINE)
  return ENOTSUP;
 if(vertex->semantic.stage!=REMESA_IR_VERTEX||fragment->semantic.stage!=REMESA_IR_FRAGMENT)
  return ENOTSUP;
 struct remesa_shader_pipeline pair={.schema=REMESA_SHADER_PIPELINE_SCHEMA,
  .status=REMESA_PROGRAM_SEMANTIC_ONLY,.binary_error=ENOTSUP};
 error=remesa_shader_ir_vertex_input(&vertex->semantic,&pair.position_input_slot);
 if(error)return error;
 error=remesa_shader_ir_fragment_color(&fragment->semantic,&pair.fragment_argb);
 if(error)return error;
 error=remesa_usse_identity(&vertex->usse,&pair.identity[0]);if(error)return error;
 error=remesa_usse_identity(&fragment->usse,&pair.identity[1]);if(error)return error;
 pair.virtual_instructions[0]=vertex->usse.count;
 pair.virtual_instructions[1]=fragment->usse.count;
 *out=pair;return 0;
}
int remesa_shader_pipeline_revalidate(const struct remesa_shader_pipeline *pair,
 const struct remesa_shader_program *vertex,const struct remesa_shader_program *fragment)
{
 if(!pair)return EINVAL;
 struct remesa_shader_pipeline current={0};
 int error=remesa_shader_pipeline_prepare(vertex,fragment,&current);if(error)return error;
 /* Compare semantic fields explicitly: compiler padding is not state. The
  * identities do not authorize execution, and every call validates the actual
  * immutable programs before consulting their correlation keys. */
 if(pair->schema!=current.schema||pair->status!=current.status||
    pair->identity[0]!=current.identity[0]||pair->identity[1]!=current.identity[1]||
    pair->virtual_instructions[0]!=current.virtual_instructions[0]||
    pair->virtual_instructions[1]!=current.virtual_instructions[1]||
    pair->position_input_slot!=current.position_input_slot||
    pair->fragment_argb!=current.fragment_argb||pair->binary_error!=current.binary_error)
  return ESTALE;
 return 0;
}
int remesa_shader_draw_programs_prepare(const struct remesa_shader_program *vertex,
 const struct remesa_shader_program *fragment,struct remesa_shader_draw_programs *out)
{
 if(!vertex||!fragment||!out||overlaps(out,sizeof(*out),vertex,sizeof(*vertex))||
    overlaps(out,sizeof(*out),fragment,sizeof(*fragment)))return EINVAL;
 struct remesa_shader_draw_programs programs={0};
 int error=remesa_shader_pipeline_prepare(vertex,fragment,&programs.pipeline);
 if(error)return error;
 programs.vertex=*vertex;programs.fragment=*fragment;
 *out=programs;return 0;
}
int remesa_shader_draw_programs_validate(const struct remesa_shader_draw_programs *programs)
{
 if(!programs)return EINVAL;
 return remesa_shader_pipeline_revalidate(&programs->pipeline,&programs->vertex,&programs->fragment);
}
static int same_program(const struct remesa_shader_program *a,
                        const struct remesa_shader_program *b)
{
 /* Padding is not shader state. Compare the complete canonical semantic
  * representation, including unused fields already checked by validation.
  * The lowering is determined exactly by these fields, never by its hash. */
 if(a->schema!=b->schema||a->status!=b->status||
    a->semantic.schema!=b->semantic.schema||a->semantic.stage!=b->semantic.stage||
    a->semantic.count!=b->semantic.count)return 0;
 for(unsigned n=0;n<REMESA_SHADER_IR_INSTRUCTIONS;n++) {
  const struct remesa_ir_instruction *x=&a->semantic.instructions[n];
  const struct remesa_ir_instruction *y=&b->semantic.instructions[n];
  if(x->op!=y->op||x->dst!=y->dst||x->components!=y->components||
     x->type!=y->type||x->input_slot!=y->input_slot)return 0;
  for(unsigned c=0;c<4;c++)
   if(x->src[c].reg!=y->src[c].reg||x->src[c].channel!=y->src[c].channel||
      x->bits[c]!=y->bits[c])return 0;
 }
 return 1;
}
int remesa_shader_draw_programs_revalidate(const struct remesa_shader_draw_programs *programs,
 const struct remesa_shader_program *vertex,const struct remesa_shader_program *fragment)
{
 int error=remesa_shader_draw_programs_validate(programs);if(error)return error;
 error=remesa_shader_program_validate(vertex);if(error)return error;
 error=remesa_shader_program_validate(fragment);if(error)return error;
 return same_program(&programs->vertex,vertex)&&same_program(&programs->fragment,fragment) ? 0 : ESTALE;
}
struct remesa_shader_image {
 struct remesa_shader_draw_programs programs;
 struct remesa_shader_storage *fragment,*vertex;
 struct remesa_shader_storage *fragment_pds,*fragment_pds_mask;
 bool fragment_pds_requested;
 struct remesa_pds_structure_request fragment_pds_source;
 struct remesa_pds_block_request fragment_pds_block;
 struct remesa_shader_pds_links fragment_pds_links;
 struct remesa_usse_vertex_mapping vertex_mapping;
 enum remesa_shader_candidate_form fragment_form;
};
/* Regenerate CPU candidate bytes from retained typed inputs and checked links.
 * No GPU mapping is acquired; the original evidence mask is preserved. */
static int image_pds_expected(const struct remesa_shader_image *image,
 unsigned char *bytes,unsigned char *mask,size_t *length)
{
 if(!image->fragment_pds_requested)return ENODATA;
 /* Symbolic block alignment is uint64_t; host storage uses size_t. Never
  * narrow it and clamp a truncated zero into an apparently valid alignment. */
 if(image->fragment_pds_block.code_alignment>SIZE_MAX)return EOVERFLOW;
 if(image->fragment_pds_block.code_alignment>REMESA_SHADER_STORAGE_MAX_ALIGNMENT)return E2BIG;
 const struct remesa_shader_pds_links *links=&image->fragment_pds_links;
 int error=remesa_shader_pipeline_revalidate(&links->launch.semantic,
  &image->programs.vertex,&image->programs.fragment);if(error)return error;
 error=remesa_pds_validate(&image->programs.vertex.usse,&links->launch.stage[0]);if(error)return error;
 error=remesa_pds_validate(&image->programs.fragment.usse,&links->launch.stage[1]);if(error)return error;
 const struct remesa_pds_binding use[2]={links->use[0].program,links->use[1].program};
 error=remesa_pds_validate_use_links(links->launch.stage,use,links->use);if(error)return error;
 struct remesa_pds_request current;
 error=remesa_pds_request_from_block(&image->fragment_pds_block,&current);if(error)return error;
 error=remesa_pds_validate_binding(&image->programs.fragment.usse,&links->launch.stage[1],&current);
 if(error)return error;
 struct remesa_shader_storage_view vertex,fragment;
 if(!image->vertex)return EPROTO;
 error=remesa_shader_storage_inspect(image->vertex,&vertex);if(error)return error;
 error=remesa_shader_storage_inspect(image->fragment,&fragment);if(error)return error;
 if(use[0].bytes!=vertex.byte_length||use[1].bytes!=fragment.byte_length)return EPROTO;
 if(image->fragment_pds_source.code_policy!=REMESA_PDS_STRUCTURE_CODE_CPU_COMPLETE)return ENOTSUP;
 struct remesa_pds_structure shape;
 error=remesa_pds_structure_encode(&image->fragment_pds_source,&shape);if(error)return error;
 /* Texture entries still carry unresolved address relocations; host structural
  * analysis remains available, but they cannot enter this staging candidate. */
 if(shape.relocation_count!=1||shape.relocations[0].kind!=REMESA_PDS_STRUCTURE_USE||
    shape.relocations[0].data_cell||!use[1].address)return ENOTSUP;
 size_t data_bytes=(size_t)shape.data_dwords*4u,code_bytes=(size_t)shape.code_slot_count*4u;
 if(current.data.bytes!=data_bytes||current.code.bytes!=code_bytes||
    current.code.address+current.code.offset!=current.data.address+current.data.offset+data_bytes||
    links->use[1].data_byte_offset)return EPROTO;
 error=remesa_pds_structure_serialize(&shape,bytes,mask,REMESA_PDS_STRUCTURE_IMAGE_BYTES,length);
 if(error)return error;
 if(*length!=data_bytes+code_bytes)return EPROTO;
 for(size_t n=data_bytes;n<*length;n++)if(mask[n]!=255)return ENOTSUP;
 uint32_t entry=shape.use_background|links->use[1].address_bits;
 for(unsigned n=0;n<4;n++)bytes[n]=(unsigned char)(entry>>(n*8u));
 return 0;
}
static int image_pds_validate(const struct remesa_shader_image *image)
{
 unsigned char bytes[REMESA_PDS_STRUCTURE_IMAGE_BYTES],mask[sizeof(bytes)];size_t length;
 int error=image_pds_expected(image,bytes,mask,&length);
 if(error==ENODATA||error==ENOTSUP)
  return image->fragment_pds||image->fragment_pds_mask?EPROTO:0;
 if(error)return error;
 const struct remesa_shader_storage *stores[2]={image->fragment_pds,image->fragment_pds_mask};
 const unsigned char *expected[2]={bytes,mask};
 for(unsigned i=0;i<2;i++) {
  struct remesa_shader_storage_view view;
  error=remesa_shader_storage_inspect(stores[i],&view);if(error)return error;
  size_t alignment=i?4u:(size_t)image->fragment_pds_block.code_alignment;
  if(alignment<4)alignment=4;
  if(view.stage!=REMESA_IR_FRAGMENT||view.format!=REMESA_SHADER_STORAGE_HOST_PDS_STRUCTURE||
     view.status!=REMESA_SHADER_STORAGE_CANDIDATE||view.byte_length!=length||
     view.byte_alignment!=alignment||memcmp(view.bytes,expected[i],length))return EPROTO;
 }
 return 0;
}
static int image_records(const struct remesa_usse_program *program,
 unsigned char records[REMESA_FRAGMENT_BODY_BYTES],size_t *length,
 enum remesa_shader_candidate_form *form)
{
 size_t encoded;
 int error=remesa_usse_encode(program,records,REMESA_FRAGMENT_BODY_BYTES,&encoded);
 if(!error) {
  *length=encoded;*form=REMESA_SHADER_PACKED_CONSTANT;return 0;
 }
 if(error!=ENOTSUP)return error;
 /* Preserve the separately validated larger candidate for endpoint profiles
  * outside the two-color-correlated packed form. Neither form is executable. */
 struct remesa_fragment_body body;
 error=remesa_fragment_body_prepare(program,&body);if(error)return error;
 memcpy(records,body.records,sizeof(body.records));
 *length=sizeof(body.records);*form=REMESA_SHADER_FLOAT_PACK_BODY;return 0;
}
static int image_prepare(const struct remesa_shader_draw_programs *programs,
 const struct remesa_usse_vertex_mapping *mapping,
 struct remesa_shader_image **out)
{
 unsigned char records[REMESA_FRAGMENT_BODY_BYTES];size_t length;
 unsigned char vertex_records[REMESA_USSE_VERTEX_CANDIDATE_BYTES];size_t vertex_length=0;
 enum remesa_shader_candidate_form form;
 struct remesa_shader_image *image;
 if(!programs||!out||overlaps(out,sizeof(*out),programs,sizeof(*programs)))return EINVAL;
 if(mapping&&overlaps(out,sizeof(*out),mapping,sizeof(*mapping)))return EINVAL;
 if(*out)return EBUSY;
 int error=remesa_shader_draw_programs_validate(programs);if(error)return error;
 if(mapping) {
  error=remesa_usse_vertex_candidate(&programs->vertex.usse,mapping,vertex_records,
   sizeof(vertex_records),&vertex_length);if(error)return error;
 }
 error=image_records(&programs->fragment.usse,records,&length,&form);if(error)return error;
 image=calloc(1,sizeof(*image));if(!image)return ENOMEM;
 image->programs=*programs;
 image->fragment_form=form;
 const struct remesa_shader_storage_request request={
  .stage=REMESA_IR_FRAGMENT,.format=REMESA_SHADER_STORAGE_HOST_CPU_RECORDS,
  .bytes=records,.byte_length=length,.byte_alignment=8
 };
 error=remesa_shader_storage_create(&request,&image->fragment);
 if(error){free(image);return error;}
 if(mapping) {
  const struct remesa_shader_storage_request vertex_request={
   .stage=REMESA_IR_VERTEX,.format=REMESA_SHADER_STORAGE_HOST_CPU_RECORDS,
   .bytes=vertex_records,.byte_length=vertex_length,.byte_alignment=8
  };
  image->vertex_mapping=*mapping;
  error=remesa_shader_storage_create(&vertex_request,&image->vertex);
  if(error){remesa_shader_image_destroy(image);return error;}
 }
 *out=image;return 0;
}
int remesa_shader_image_prepare(const struct remesa_shader_draw_programs *programs,
 struct remesa_shader_image **out)
{return image_prepare(programs,NULL,out);}
int remesa_shader_image_prepare_vertex_candidate(const struct remesa_shader_draw_programs *programs,
 const struct remesa_usse_vertex_mapping *mapping,struct remesa_shader_image **out)
{return mapping?image_prepare(programs,mapping,out):EINVAL;}
int remesa_shader_image_clone(const struct remesa_shader_image *source,
 struct remesa_shader_image **out)
{
 if(!source||!out||sizeof(*out)>UINTPTR_MAX-(uintptr_t)out||
    overlaps(out,sizeof(*out),source,sizeof(*source)))return EINVAL;
 struct remesa_shader_image_view view;
 int error=remesa_shader_image_inspect(source,&view);if(error)return error;
 const struct remesa_shader_storage_view *payloads[4]={&view.vertex,&view.fragment,
  &view.fragment_pds,&view.fragment_pds_mask};
 for(unsigned i=0;i<4;i++)
  if(overlaps(out,sizeof(*out),payloads[i]->bytes,payloads[i]->byte_length))return EINVAL;
 if(*out)return EBUSY;
 struct remesa_shader_image *copy=calloc(1,sizeof(*copy));if(!copy)return ENOMEM;
 *copy=*source;
 copy->vertex=copy->fragment=copy->fragment_pds=copy->fragment_pds_mask=NULL;
 struct remesa_shader_storage **dest[4]={&copy->vertex,&copy->fragment,
  &copy->fragment_pds,&copy->fragment_pds_mask};
 struct remesa_shader_storage *const src[4]={source->vertex,source->fragment,
  source->fragment_pds,source->fragment_pds_mask};
 for(unsigned i=0;i<4;i++) {
  error=remesa_shader_storage_reference(dest[i],src[i]);
  if(error){remesa_shader_image_destroy(copy);return error;}
 }
 *out=copy;return 0;
}
int remesa_shader_image_validate(const struct remesa_shader_image *image)
{
 unsigned char expected[REMESA_FRAGMENT_BODY_BYTES];size_t length;
 enum remesa_shader_candidate_form form;
 struct remesa_shader_storage_view view;
 if(!image)return EINVAL;
 int error=remesa_shader_draw_programs_validate(&image->programs);if(error)return error;
 error=image_records(&image->programs.fragment.usse,expected,&length,&form);if(error)return error;
 error=remesa_shader_storage_inspect(image->fragment,&view);if(error)return error;
 if(view.stage!=REMESA_IR_FRAGMENT||view.format!=REMESA_SHADER_STORAGE_HOST_CPU_RECORDS||
    view.status!=REMESA_SHADER_STORAGE_CANDIDATE||view.byte_length!=length||
    image->fragment_form!=form||view.byte_alignment!=8||memcmp(view.bytes,expected,length))return EPROTO;
 if(image->vertex) {
  unsigned char vertex[REMESA_USSE_VERTEX_CANDIDATE_BYTES];size_t vertex_length;
  error=remesa_usse_vertex_candidate(&image->programs.vertex.usse,&image->vertex_mapping,
   vertex,sizeof(vertex),&vertex_length);if(error)return error;
  error=remesa_shader_storage_inspect(image->vertex,&view);if(error)return error;
  if(view.stage!=REMESA_IR_VERTEX||view.format!=REMESA_SHADER_STORAGE_HOST_CPU_RECORDS||
     view.status!=REMESA_SHADER_STORAGE_CANDIDATE||view.byte_alignment!=8||
     view.byte_length!=vertex_length||memcmp(view.bytes,vertex,vertex_length))return EPROTO;
 }
 return image_pds_validate(image);
}
int remesa_shader_image_inspect(const struct remesa_shader_image *image,
 struct remesa_shader_image_view *out)
{
 if(!out||!image||overlaps(out,sizeof(*out),image,sizeof(*image)))return EINVAL;
 int error=remesa_shader_image_validate(image);if(error)return error;
 struct remesa_shader_image_view view={.programs=&image->programs,
  .vertex_byte_length=0,.execution_error=ENOTSUP,.fragment_form=image->fragment_form,
  .fragment_pds_error=image->fragment_pds_requested?ENOTSUP:ENODATA};
 error=remesa_shader_storage_inspect(image->fragment,&view.fragment);if(error)return error;
 if(image->vertex) {
  error=remesa_shader_storage_inspect(image->vertex,&view.vertex);if(error)return error;
  view.vertex_byte_length=view.vertex.byte_length;
 }
 if(image->fragment_pds) {
  error=remesa_shader_storage_inspect(image->fragment_pds,&view.fragment_pds);if(error)return error;
  error=remesa_shader_storage_inspect(image->fragment_pds_mask,&view.fragment_pds_mask);if(error)return error;
  view.fragment_pds_error=0;
 }
 if(overlaps(out,sizeof(*out),view.programs,sizeof(*view.programs))||
    overlaps(out,sizeof(*out),view.fragment.bytes,view.fragment.byte_length)||
    overlaps(out,sizeof(*out),view.vertex.bytes,view.vertex.byte_length)||
    overlaps(out,sizeof(*out),view.fragment_pds.bytes,view.fragment_pds.byte_length)||
    overlaps(out,sizeof(*out),view.fragment_pds_mask.bytes,view.fragment_pds_mask.byte_length))return EINVAL;
 *out=view;return 0;
}
int remesa_shader_image_preview_use_entry(const struct remesa_shader_image *image,
 const struct remesa_use_entry_request *request,struct remesa_use_entry_plan *out)
{
 struct remesa_shader_image_view view;
 if(!image||!request||!out)return EINVAL;
 int error=remesa_shader_image_inspect(image,&view);if(error)return error;
 if(overlaps(out,sizeof(*out),image,sizeof(*image))||
    overlaps(out,sizeof(*out),view.programs,sizeof(*view.programs))||
    overlaps(out,sizeof(*out),view.fragment.bytes,view.fragment.byte_length)||
    overlaps(out,sizeof(*out),view.vertex.bytes,view.vertex.byte_length)||
    overlaps(out,sizeof(*out),view.fragment_pds.bytes,view.fragment_pds.byte_length)||
    overlaps(out,sizeof(*out),view.fragment_pds_mask.bytes,view.fragment_pds_mask.byte_length))return EINVAL;
 size_t bytes;
 if(request->data_master==REMESA_USE_PIXEL)bytes=view.fragment.byte_length;
 else if(request->data_master==REMESA_USE_VERTEX) {
  if(!view.vertex.byte_length)return ENOTSUP;
  bytes=view.vertex.byte_length;
 } else return ENOTSUP;
 if(request->program_bytes!=bytes)return EPROTO;
 struct remesa_use_entry_plan entry;
 uint32_t address_bits;
 error=remesa_use_entry_prepare(request,&entry);if(error)return error;
 /* Do not publish a native entry that the observed address representation
  * would truncate. This checks lossless software packing, not hardware
  * instruction alignment or a complete task-control word. */
 error=remesa_use_entry_preview_fixup(&entry,&address_bits);if(error)return error;
 *out=entry;return 0;
}
int remesa_shader_image_revalidate(const struct remesa_shader_image *image,
 const struct remesa_shader_draw_programs *programs)
{
 int error=remesa_shader_image_validate(image);if(error)return error;
 error=remesa_shader_draw_programs_validate(programs);if(error)return error;
 return remesa_shader_draw_programs_revalidate(&image->programs,&programs->vertex,&programs->fragment);
}
static int request_error(enum remesa_shader_request_status status)
{
 switch(status) {
 case REMESA_SHADER_REQUEST_OK:return 0;
 case REMESA_SHADER_REQUEST_INVALID:return EINVAL;
 case REMESA_SHADER_REQUEST_UNSUPPORTED:return ENOTSUP;
 case REMESA_SHADER_REQUEST_NOMEM:return ENOMEM;
 }
 return EPROTO;
}
int remesa_shader_image_build_request(const struct remesa_shader_image *image,
 uint64_t owner,uint64_t generation,struct remesa_shader_request *out)
{
 struct remesa_shader_image_view view;
 if(!out||sizeof(*out)>UINTPTR_MAX-(uintptr_t)out||
    overlaps(out,sizeof(*out),image,sizeof(*image)))return EINVAL;
 int error=remesa_shader_image_inspect(image,&view);if(error)return error;
 if(overlaps(out,sizeof(*out),view.programs,sizeof(*view.programs)))return EINVAL;
 /* Optional typed PDS data/code is CPU-candidate storage only. Unknown
  * relocations reject staging; this never admits or substitutes GPU code. */
 if(view.fragment_pds_error!=0&&view.fragment_pds_error!=ENODATA)return view.fragment_pds_error;
 if(overlaps(out,sizeof(*out),view.fragment_pds_mask.bytes,view.fragment_pds_mask.byte_length))return EINVAL;
 struct remesa_shader_blob blobs[REMESA_SHADER_RESOURCE_COUNT]={{0}};
 if(view.vertex.byte_length)blobs[REMESA_SHADER_VERTEX_USSE-1]=(struct remesa_shader_blob){
  .bytes=view.vertex.bytes,.length=(uint32_t)view.vertex.byte_length,
  .alignment=(uint32_t)view.vertex.byte_alignment,.state=REMESA_SHADER_CPU_CANDIDATE
 };
 blobs[REMESA_SHADER_FRAGMENT_USSE-1]=(struct remesa_shader_blob){
  .bytes=view.fragment.bytes,.length=(uint32_t)view.fragment.byte_length,
  .alignment=(uint32_t)view.fragment.byte_alignment,.state=REMESA_SHADER_CPU_CANDIDATE
 };
 if(view.fragment_pds.byte_length)blobs[REMESA_SHADER_FRAGMENT_PDS-1]=(struct remesa_shader_blob){
  .bytes=view.fragment_pds.bytes,.length=(uint32_t)view.fragment_pds.byte_length,
  .alignment=(uint32_t)view.fragment_pds.byte_alignment,.state=REMESA_SHADER_CPU_CANDIDATE
 };
 return request_error(remesa_shader_request_build(out,owner,generation,blobs));
}
int remesa_shader_image_validate_request(const struct remesa_shader_image *image,
 const struct remesa_shader_request *request)
{
 struct remesa_shader_image_view view;
 if(!request)return EINVAL;
 int error=remesa_shader_image_inspect(image,&view);if(error)return error;
 if(remesa_shader_request_validate(request,request->owner,request->generation)!=
    REMESA_SHADER_REQUEST_OK)return EPROTO;
 if(view.fragment_pds_error!=0&&view.fragment_pds_error!=ENODATA)return view.fragment_pds_error;
 const struct remesa_shader_storage_view *stages[REMESA_SHADER_RESOURCE_COUNT]={
  &view.vertex,&view.fragment,NULL,&view.fragment_pds};
 unsigned mask=0;size_t offset=0;
 for(unsigned n=0;n<REMESA_SHADER_RESOURCE_COUNT;n++) {
  const struct remesa_shader_storage_view *stage=stages[n];
  if(!stage||!stage->byte_length)continue;
  mask|=1u<<n;
  offset=(offset+stage->byte_alignment-1u)&~(stage->byte_alignment-1u);
  const struct remesa_shader_resource *resource=&request->resources[n];
  if(resource->offset!=offset||resource->length!=stage->byte_length||
     resource->alignment!=stage->byte_alignment||resource->state!=REMESA_SHADER_CPU_CANDIDATE||
     memcmp(request->data+offset,stage->bytes,stage->byte_length))return EPROTO;
  offset+=stage->byte_length;
 }
 if(request->resource_mask!=mask||request->byte_count!=offset)return EPROTO;
 return 0;
}
int remesa_shader_image_validate_execution(const struct remesa_shader_image *image)
{
 int error=remesa_shader_image_validate(image);return error?error:ENOTSUP;
}
void remesa_shader_image_destroy(struct remesa_shader_image *image)
{
 if(image){(void)remesa_shader_storage_reference(&image->fragment_pds,NULL);
  (void)remesa_shader_storage_reference(&image->fragment_pds_mask,NULL);
  (void)remesa_shader_storage_reference(&image->vertex,NULL);
  (void)remesa_shader_storage_reference(&image->fragment,NULL);free(image);}
}
/* Called only after each request's overflow/bounds checks have succeeded. */
static int stage_storage_disjoint(const struct remesa_pds_request *vertex,
                                 const struct remesa_pds_request *fragment)
{
 if(vertex->owner_cookie!=fragment->owner_cookie||vertex->operation!=fragment->operation||
    vertex->generation!=fragment->generation)return ESTALE;
 const struct remesa_pds_binding *a[2]={&vertex->code,&vertex->data};
 const struct remesa_pds_binding *b[2]={&fragment->code,&fragment->data};
 for(unsigned i=0;i<2;i++)for(unsigned j=0;j<2;j++) {
  if(a[i]->allocation_cookie==b[j]->allocation_cookie) {
   if(a[i]->address!=b[j]->address||a[i]->allocation_bytes!=b[j]->allocation_bytes||
      a[i]->mapping_generation!=b[j]->mapping_generation)return ESTALE;
  } else if(a[i]->address<b[j]->address+b[j]->allocation_bytes&&
            b[j]->address<a[i]->address+a[i]->allocation_bytes)return EINVAL;
  uint64_t x=a[i]->address+a[i]->offset,y=b[j]->address+b[j]->offset;
  if(x<y+b[j]->bytes&&y<x+a[i]->bytes)return EINVAL;
 }
 return 0;
}
int remesa_shader_pipeline_prepare_launch(const struct remesa_shader_program *vertex,
 const struct remesa_shader_program *fragment,const struct remesa_pds_request *vertex_request,
 const struct remesa_pds_request *fragment_request,struct remesa_shader_pipeline_launch *out)
{
 if(!out||!vertex||!fragment||!vertex_request||!fragment_request||
    overlaps(out,sizeof(*out),vertex,sizeof(*vertex))||
    overlaps(out,sizeof(*out),fragment,sizeof(*fragment))||
    overlaps(out,sizeof(*out),vertex_request,sizeof(*vertex_request))||
    overlaps(out,sizeof(*out),fragment_request,sizeof(*fragment_request)))return EINVAL;
 struct remesa_shader_pipeline_launch launch={0};
 int error=remesa_shader_pipeline_prepare(vertex,fragment,&launch.semantic);if(error)return error;
 error=remesa_shader_program_prepare_launch(vertex,vertex_request,&launch.stage[0]);if(error)return error;
 error=remesa_shader_program_prepare_launch(fragment,fragment_request,&launch.stage[1]);if(error)return error;
 error=stage_storage_disjoint(vertex_request,fragment_request);if(error)return error;
 *out=launch;return 0;
}
int remesa_shader_pipeline_revalidate_launch(const struct remesa_shader_pipeline_launch *launch,
 const struct remesa_shader_program *vertex,const struct remesa_shader_program *fragment,
 const struct remesa_pds_request *vertex_current,const struct remesa_pds_request *fragment_current)
{
 if(!launch)return EINVAL;
 int error=remesa_shader_pipeline_revalidate(&launch->semantic,vertex,fragment);if(error)return error;
 error=remesa_pds_validate_binding(&vertex->usse,&launch->stage[0],vertex_current);if(error)return error;
 error=remesa_pds_validate_binding(&fragment->usse,&launch->stage[1],fragment_current);if(error)return error;
 return stage_storage_disjoint(vertex_current,fragment_current);
}

struct remesa_shader_host_pipeline {
 struct remesa_shader_image *image;
 struct remesa_shader_host_pipeline_request request;
 struct remesa_shader_pds_links links;
 struct remesa_shader_storage *data[2];
 struct remesa_pds_data_cell *cells[2];
 struct remesa_pds_structure_request structure_request;
 struct remesa_pds_structure structure;
 struct remesa_shader_storage *structure_storage[3]; /* CODE, CODE mask, DATA mask */
};
static int structure_validate(const struct remesa_shader_host_pipeline *,
 const struct remesa_shader_host_pipeline_view *);
static int host_request_valid(const struct remesa_shader_host_pipeline_request *r)
{
 if(!r||sizeof(*r)>UINTPTR_MAX-(uintptr_t)r)return EINVAL;
 for(unsigned i=0;i<2;i++) {
  if(!r->data[i].cells||!r->data[i].cell_count)return EINVAL;
  if(r->data[i].cell_count>REMESA_SHADER_STORAGE_MAX_BYTES/4)return E2BIG;
  if(r->data[i].cell_count*sizeof(*r->data[i].cells)>
     UINTPTR_MAX-(uintptr_t)r->data[i].cells)return EINVAL;
  if(r->pds[i].data_bytes!=r->data[i].cell_count*4)return EINVAL;
 }
 return 0;
}
/* All byte counts come from the original emitters, not task descriptors. */
static int host_links(const struct remesa_shader_draw_programs *programs,
 const struct remesa_shader_host_pipeline_request *r,struct remesa_shader_pds_links *links)
{
 int error=host_request_valid(r);if(error)return error;
 if(!programs||sizeof(*programs)>UINTPTR_MAX-(uintptr_t)programs)return EINVAL;
 error=remesa_shader_draw_programs_validate(programs);if(error)return error;
 unsigned char vertex[REMESA_USSE_VERTEX_CANDIDATE_BYTES],fragment[REMESA_FRAGMENT_BODY_BYTES];
 size_t vertex_bytes,fragment_bytes;enum remesa_shader_candidate_form form;
 error=remesa_usse_vertex_candidate(&programs->vertex.usse,&r->vertex_mapping,
  vertex,sizeof(vertex),&vertex_bytes);if(error)return error;
 error=image_records(&programs->fragment.usse,fragment,&fragment_bytes,&form);if(error)return error;
 if(r->use[0].bytes!=vertex_bytes||r->use[1].bytes!=fragment_bytes)return EPROTO;
 struct remesa_pds_request pds[2];
 for(unsigned i=0;i<2;i++) {
  error=remesa_pds_request_from_block(&r->pds[i],&pds[i]);if(error)return error;
 }
 struct remesa_shader_pds_links result={0};
 error=remesa_shader_pipeline_prepare_launch(&programs->vertex,&programs->fragment,
  &pds[0],&pds[1],&result.launch);if(error)return error;
 error=remesa_pds_prepare_use_links(result.launch.stage,r->use,r->banks,
  r->data_byte_offsets,result.use);if(error)return error;
 *links=result;return 0;
}
int remesa_shader_host_pipeline_create(const struct remesa_shader_draw_programs *programs,
 const struct remesa_shader_host_pipeline_request *request,struct remesa_shader_host_pipeline **out)
{
 if(!out||sizeof(*out)>UINTPTR_MAX-(uintptr_t)out||!programs||!request||
    overlaps(out,sizeof(*out),programs,sizeof(*programs))||
    overlaps(out,sizeof(*out),request,sizeof(*request)))return EINVAL;
 int error=host_request_valid(request);if(error)return error;
 for(unsigned i=0;i<2;i++)
  if(overlaps(out,sizeof(*out),request->data[i].cells,
     request->data[i].cell_count*sizeof(*request->data[i].cells)))return EINVAL;
 if(*out)return EBUSY;
 struct remesa_shader_pds_links links;
 error=host_links(programs,request,&links);if(error)return error;
 unsigned char bytes[2][REMESA_SHADER_STORAGE_MAX_BYTES];
 struct remesa_pds_host_data data[2];
 for(unsigned i=0;i<2;i++)data[i]=(struct remesa_pds_host_data){
  .binding=links.launch.stage[i].request.data,.bytes=bytes[i],
  .byte_length=request->data[i].cell_count*4};
 error=remesa_pds_build_host_data_pair(links.launch.stage,request->use,links.use,request->data,data);
 if(error)return error;
 struct remesa_shader_host_pipeline *p=calloc(1,sizeof(*p));if(!p)return ENOMEM;
 p->request=*request;p->links=links;
 error=remesa_shader_image_prepare_vertex_candidate(programs,&request->vertex_mapping,&p->image);
 if(error)goto fail;
 for(unsigned i=0;i<2;i++) {
  size_t cell_bytes=request->data[i].cell_count*sizeof(*p->cells[i]);
  p->cells[i]=malloc(cell_bytes);if(!p->cells[i]){error=ENOMEM;goto fail;}
  memcpy(p->cells[i],request->data[i].cells,cell_bytes);
  p->request.data[i].cells=p->cells[i];
  const struct remesa_shader_storage_request storage={
   .stage=i?REMESA_IR_FRAGMENT:REMESA_IR_VERTEX,
   .format=REMESA_SHADER_STORAGE_HOST_PDS_DATA,.bytes=bytes[i],
   .byte_length=data[i].byte_length,.byte_alignment=4};
  error=remesa_shader_storage_create(&storage,&p->data[i]);if(error)goto fail;
 }
 *out=p;return 0;
fail:
 remesa_shader_host_pipeline_destroy(p);return error;
}
static int host_views(const struct remesa_shader_host_pipeline *p,
 struct remesa_shader_host_pipeline_view *out)
{
 struct remesa_shader_host_pipeline_view view={.image_owner=p->image,.links=&p->links,
  .structure_error=ENOTSUP,.pds_code_error=ENOTSUP,.execution_error=ENOTSUP};
 int error=remesa_shader_image_inspect(p->image,&view.image);if(error)return error;
 for(unsigned i=0;i<2;i++) {
  view.data[i].binding=p->links.launch.stage[i].request.data;
  error=remesa_shader_storage_inspect(p->data[i],&view.data[i].storage);if(error)return error;
 }
 if(p->structure_storage[0]) {
  struct remesa_shader_storage_view *views[3]={&view.structure_code,
   &view.structure_code_mask,&view.structure_data_mask};
  for(unsigned i=0;i<3;i++) {
   error=remesa_shader_storage_inspect(p->structure_storage[i],views[i]);if(error)return error;
  }
  view.structure=&p->structure;view.structure_error=0;
 } else if(p->structure_storage[1]||p->structure_storage[2])return EPROTO;
 *out=view;return 0;
}
int remesa_shader_host_pipeline_validate(const struct remesa_shader_host_pipeline *p)
{
 if(!p)return EINVAL;
 struct remesa_shader_host_pipeline_view view;
 int error=host_views(p,&view);if(error)return error;
 struct remesa_shader_pds_links current;
 error=host_links(view.image.programs,&p->request,&current);if(error)return error;
 unsigned char vertex[REMESA_USSE_VERTEX_CANDIDATE_BYTES];size_t vertex_bytes;
 error=remesa_usse_vertex_candidate(&view.image.programs->vertex.usse,&p->request.vertex_mapping,
  vertex,sizeof(vertex),&vertex_bytes);if(error)return error;
 if(vertex_bytes!=view.image.vertex.byte_length||
    memcmp(vertex,view.image.vertex.bytes,vertex_bytes))return EPROTO;
 error=remesa_shader_pipeline_revalidate_launch(&p->links.launch,
  &view.image.programs->vertex,&view.image.programs->fragment,
  &current.launch.stage[0].request,&current.launch.stage[1].request);if(error)return error;
 error=remesa_pds_validate_use_links(p->links.launch.stage,p->request.use,p->links.use);
 if(error)return error;
 struct remesa_pds_host_data data[2];
 for(unsigned i=0;i<2;i++) {
  const struct remesa_shader_storage_view *s=&view.data[i].storage;
  if(p->links.use[i].entry.bank!=p->request.banks[i]||
     p->links.use[i].data_byte_offset!=p->request.data_byte_offsets[i])return ESTALE;
  if(s->stage!=(i?REMESA_IR_FRAGMENT:REMESA_IR_VERTEX)||
     s->format!=REMESA_SHADER_STORAGE_HOST_PDS_DATA||
     s->status!=REMESA_SHADER_STORAGE_CANDIDATE||s->byte_alignment!=4||
     s->byte_length!=p->request.data[i].cell_count*4)return EPROTO;
  data[i]=(struct remesa_pds_host_data){.binding=view.data[i].binding,
   .bytes=(void *)s->bytes,.byte_length=s->byte_length};
 }
 error=remesa_pds_validate_host_data_pair(p->links.launch.stage,p->request.use,
  p->links.use,p->request.data,data);if(error)return error;
 return structure_validate(p,&view);
}
int remesa_shader_host_pipeline_inspect(const struct remesa_shader_host_pipeline *p,
 struct remesa_shader_host_pipeline_view *out)
{
 if(!p||!out||sizeof(*out)>UINTPTR_MAX-(uintptr_t)out||
    overlaps(out,sizeof(*out),p,sizeof(*p))||
    overlaps(out,sizeof(*out),p->image,sizeof(*p->image)))return EINVAL;
 int error=remesa_shader_host_pipeline_validate(p);if(error)return error;
 struct remesa_shader_host_pipeline_view view;
 error=host_views(p,&view);if(error)return error;
 if(overlaps(out,sizeof(*out),view.image.programs,sizeof(*view.image.programs))||
    overlaps(out,sizeof(*out),view.image.vertex.bytes,view.image.vertex.byte_length)||
    overlaps(out,sizeof(*out),view.image.fragment.bytes,view.image.fragment.byte_length)||
    overlaps(out,sizeof(*out),view.image.fragment_pds.bytes,view.image.fragment_pds.byte_length)||
    overlaps(out,sizeof(*out),view.image.fragment_pds_mask.bytes,view.image.fragment_pds_mask.byte_length))return EINVAL;
 for(unsigned i=0;i<2;i++) {
  if(overlaps(out,sizeof(*out),view.data[i].storage.bytes,view.data[i].storage.byte_length)||
     overlaps(out,sizeof(*out),p->cells[i],p->request.data[i].cell_count*sizeof(*p->cells[i])))return EINVAL;
 }
 const struct remesa_shader_storage_view *structure_views[3]={&view.structure_code,
  &view.structure_code_mask,&view.structure_data_mask};
 for(unsigned i=0;i<3;i++)
  if(overlaps(out,sizeof(*out),structure_views[i]->bytes,structure_views[i]->byte_length))return EINVAL;
 *out=view;return 0;
}
static int host_binding_equal(const struct remesa_pds_binding *a,const struct remesa_pds_binding *b)
{
 return a->allocation_cookie==b->allocation_cookie&&a->owner_cookie==b->owner_cookie&&
  a->mapping_generation==b->mapping_generation&&a->address==b->address&&
  a->offset==b->offset&&a->bytes==b->bytes&&a->allocation_bytes==b->allocation_bytes;
}
int remesa_shader_host_pipeline_revalidate(const struct remesa_shader_host_pipeline *p,
 const struct remesa_shader_draw_programs *programs,const struct remesa_shader_host_pipeline_request *r)
{
 int error=remesa_shader_host_pipeline_validate(p);if(error)return error;
 struct remesa_shader_pds_links current;
 error=host_links(programs,r,&current);if(error)return error;
 error=remesa_shader_image_revalidate(p->image,programs);if(error)return error;
 error=remesa_shader_pipeline_revalidate_launch(&p->links.launch,&programs->vertex,
  &programs->fragment,&current.launch.stage[0].request,&current.launch.stage[1].request);
 if(error)return error;
 for(unsigned c=0;c<4;c++) {
  const struct remesa_usse_vertex_mapping *a=&p->request.vertex_mapping,*b=&r->vertex_mapping;
  if(a->input[c].class_id!=b->input[c].class_id||a->input[c].number!=b->input[c].number||
     a->output[c].class_id!=b->output[c].class_id||a->output[c].number!=b->output[c].number)return ESTALE;
 }
 for(unsigned i=0;i<2;i++) {
  const struct remesa_shader_host_pipeline_request *a=&p->request;
  if(!host_binding_equal(&a->use[i],&r->use[i])||
     !host_binding_equal(&a->pds[i].allocation,&r->pds[i].allocation)||
     a->pds[i].code_alignment!=r->pds[i].code_alignment||
     a->banks[i]!=r->banks[i]||a->data_byte_offsets[i]!=r->data_byte_offsets[i]||
     a->data[i].cell_count!=r->data[i].cell_count)return ESTALE;
  for(size_t c=0;c<a->data[i].cell_count;c++)
   if(a->data[i].cells[c].kind!=r->data[i].cells[c].kind||
      a->data[i].cells[c].value!=r->data[i].cells[c].value)return ESTALE;
 }
 return 0;
}
void remesa_shader_host_pipeline_destroy(struct remesa_shader_host_pipeline *p)
{
 if(!p)return;
 for(unsigned i=0;i<2;i++) {
  (void)remesa_shader_storage_reference(&p->data[i],NULL);free(p->cells[i]);
 }
 for(unsigned i=0;i<3;i++)(void)remesa_shader_storage_reference(&p->structure_storage[i],NULL);
 remesa_shader_image_destroy(p->image);free(p);
}
static int pixel_task_request(const struct remesa_shader_draw_programs *programs,
 const struct remesa_shader_host_pipeline_request *request,uint32_t metadata6,
 struct remesa_pds_data_cell cells[REMESA_PDS_CPU_PIXEL_DATA_CELLS],
 struct remesa_shader_host_pipeline_request *out)
{
 if(!programs||sizeof(*programs)>UINTPTR_MAX-(uintptr_t)programs||
    !request||sizeof(*request)>UINTPTR_MAX-(uintptr_t)request)return EINVAL;
 int error=remesa_shader_draw_programs_validate(programs);if(error)return error;
 size_t bytes;
 error=remesa_usse_encoded_size(&programs->fragment.usse,&bytes);if(error)return error;
 if(bytes!=8)return ENOTSUP;
 if(request->pds[1].data_bytes!=4*REMESA_PDS_CPU_PIXEL_DATA_CELLS||
    request->data_byte_offsets[1])return EINVAL;
 error=remesa_pds_cpu_pixel_task_recipe(metadata6,cells);if(error)return error;
 *out=*request;
 /* The copy is consumed synchronously by the existing immutable constructor;
  * its recipe is owned there, never retained as this stack pointer. */
 out->data[1]=(struct remesa_pds_host_data_recipe){
  .cells=cells,.cell_count=REMESA_PDS_CPU_PIXEL_DATA_CELLS};
 return 0;
}
int remesa_shader_host_pipeline_create_pixel_task(const struct remesa_shader_draw_programs *programs,
 const struct remesa_shader_host_pipeline_request *request,uint32_t metadata6,
 struct remesa_shader_host_pipeline **out)
{
 if(!out||sizeof(*out)>UINTPTR_MAX-(uintptr_t)out||!request||!programs||
    overlaps(out,sizeof(*out),request,sizeof(*request))||
    overlaps(out,sizeof(*out),programs,sizeof(*programs)))return EINVAL;
 struct remesa_pds_data_cell cells[REMESA_PDS_CPU_PIXEL_DATA_CELLS];
 struct remesa_shader_host_pipeline_request current;
 int error=pixel_task_request(programs,request,metadata6,cells,&current);if(error)return error;
 return remesa_shader_host_pipeline_create(programs,&current,out);
}
int remesa_shader_host_pipeline_revalidate_pixel_task(const struct remesa_shader_host_pipeline *p,
 const struct remesa_shader_draw_programs *programs,
 const struct remesa_shader_host_pipeline_request *request,uint32_t metadata6)
{
 struct remesa_pds_data_cell cells[REMESA_PDS_CPU_PIXEL_DATA_CELLS];
 struct remesa_shader_host_pipeline_request current;
 int error=pixel_task_request(programs,request,metadata6,cells,&current);if(error)return error;
 return remesa_shader_host_pipeline_revalidate(p,programs,&current);
}
#define HOST_STRUCTURE_BYTES (4u*(REMESA_PDS_STRUCTURE_DATA_CELLS+REMESA_PDS_STRUCTURE_CODE_SLOTS))
static int structure_request(const struct remesa_shader_draw_programs *programs,
 const struct remesa_shader_host_pipeline_request *request,
 const struct remesa_pds_structure_request *source,
 struct remesa_pds_structure *shape,struct remesa_pds_data_cell *cells,
 struct remesa_shader_host_pipeline_request *current)
{
 if(!programs||sizeof(*programs)>UINTPTR_MAX-(uintptr_t)programs||
    !request||sizeof(*request)>UINTPTR_MAX-(uintptr_t)request||
    !source||sizeof(*source)>UINTPTR_MAX-(uintptr_t)source)return EINVAL;
 int error=remesa_shader_draw_programs_validate(programs);if(error)return error;
 size_t body_bytes;
 error=remesa_usse_encoded_size(&programs->fragment.usse,&body_bytes);if(error)return error;
 if(body_bytes!=8)return ENOTSUP;
 error=remesa_pds_structure_encode(source,shape);if(error)return error;
 uint32_t metadata6;
 error=remesa_pds_cpu_meta6_unpack(source->task_word1,&metadata6);if(error)return error;
 if(request->pds[1].data_bytes!=(uint64_t)shape->data_dwords*4||
    request->pds[1].code_bytes!=(uint64_t)shape->code_slot_count*4||
    request->data_byte_offsets[1])return EINVAL;
 struct remesa_pds_request layout;
 error=remesa_pds_request_from_block(&request->pds[1],&layout);if(error)return error;
 if(layout.code.address+layout.code.offset!=
    layout.data.address+layout.data.offset+layout.data.bytes)return EINVAL;
 for(unsigned i=0;i<shape->data_dwords;i++)
  cells[i]=(struct remesa_pds_data_cell){REMESA_PDS_DATA_LITERAL32,shape->data[i]};
 /* psb CPU descriptor grammar selects its own background bits. This is not
  * the separate Xpsb constant-pixel recipe's unconditional 0x100000 default. */
 cells[0]=(struct remesa_pds_data_cell){REMESA_PDS_DATA_USE_ENTRY_LOWER19,shape->use_background};
 cells[1]=(struct remesa_pds_data_cell){REMESA_PDS_DATA_CPU_META6,metadata6};
 *current=*request;
 current->data[1]=(struct remesa_pds_host_data_recipe){cells,shape->data_dwords};
 return host_request_valid(current);
}
static int structure_validate(const struct remesa_shader_host_pipeline *p,
 const struct remesa_shader_host_pipeline_view *view)
{
 if(!p->structure_storage[0])return 0;
 struct remesa_pds_structure shape;
 struct remesa_pds_data_cell cells[REMESA_PDS_STRUCTURE_DATA_CELLS];
 struct remesa_shader_host_pipeline_request current;
 int error=structure_request(view->image.programs,&p->request,&p->structure_request,
  &shape,cells,&current);if(error)return error;
 if(memcmp(&shape,&p->structure,sizeof(shape)))return EPROTO;
 for(unsigned i=0;i<shape.data_dwords;i++)
  if(cells[i].kind!=p->cells[1][i].kind||cells[i].value!=p->cells[1][i].value)return EPROTO;
 unsigned char bytes[HOST_STRUCTURE_BYTES],mask[HOST_STRUCTURE_BYTES];size_t written;
 error=remesa_pds_structure_serialize(&shape,bytes,mask,sizeof(bytes),&written);if(error)return error;
 size_t data_bytes=(size_t)shape.data_dwords*4,code_bytes=(size_t)shape.code_slot_count*4;
 if(written!=data_bytes+code_bytes)return EPROTO;
 const struct remesa_shader_storage_view *stores[3]={&view->structure_code,
  &view->structure_code_mask,&view->structure_data_mask};
 const unsigned char *expected[3]={bytes+data_bytes,mask+data_bytes,mask};
 const size_t lengths[3]={code_bytes,code_bytes,data_bytes};
 for(unsigned i=0;i<3;i++) {
  const struct remesa_shader_storage_view *s=stores[i];
  if(s->stage!=REMESA_IR_FRAGMENT||s->format!=REMESA_SHADER_STORAGE_HOST_PDS_STRUCTURE||
     s->status!=REMESA_SHADER_STORAGE_CANDIDATE||s->byte_alignment!=4||
     s->byte_length!=lengths[i]||memcmp(s->bytes,expected[i],lengths[i]))return EPROTO;
 }
 return 0;
}
int remesa_shader_host_pipeline_create_pds_structure(const struct remesa_shader_draw_programs *programs,
 const struct remesa_shader_host_pipeline_request *request,const struct remesa_pds_structure_request *source,
 struct remesa_shader_host_pipeline **out)
{
 if(!out||sizeof(*out)>UINTPTR_MAX-(uintptr_t)out||!programs||!request||!source||
    overlaps(out,sizeof(*out),programs,sizeof(*programs))||
    overlaps(out,sizeof(*out),request,sizeof(*request))||
    overlaps(out,sizeof(*out),source,sizeof(*source)))return EINVAL;
 struct remesa_pds_structure shape;
 struct remesa_pds_data_cell cells[REMESA_PDS_STRUCTURE_DATA_CELLS];
 struct remesa_shader_host_pipeline_request current;
 int error=structure_request(programs,request,source,&shape,cells,&current);if(error)return error;
 if(overlaps(out,sizeof(*out),request->data[0].cells,
    request->data[0].cell_count*sizeof(*request->data[0].cells)))return EINVAL;
 if(*out)return EBUSY;
 unsigned char bytes[HOST_STRUCTURE_BYTES],mask[HOST_STRUCTURE_BYTES];size_t written;
 error=remesa_pds_structure_serialize(&shape,bytes,mask,sizeof(bytes),&written);if(error)return error;
 size_t data_bytes=(size_t)shape.data_dwords*4,code_bytes=(size_t)shape.code_slot_count*4;
 if(written!=data_bytes+code_bytes)return EPROTO;
 struct remesa_shader_host_pipeline *p=NULL;
 error=remesa_shader_host_pipeline_create(programs,&current,&p);if(error)return error;
 p->structure_request=*source;p->structure=shape;
 const unsigned char *input[3]={bytes+data_bytes,mask+data_bytes,mask};
 const size_t length[3]={code_bytes,code_bytes,data_bytes};
 for(unsigned i=0;i<3;i++) {
  const struct remesa_shader_storage_request store={.stage=REMESA_IR_FRAGMENT,
   .format=REMESA_SHADER_STORAGE_HOST_PDS_STRUCTURE,.bytes=input[i],
   .byte_length=length[i],.byte_alignment=4};
  error=remesa_shader_storage_create(&store,&p->structure_storage[i]);if(error)goto fail;
 }
 if(source->code_policy==REMESA_PDS_STRUCTURE_CODE_CPU_COMPLETE) {
  p->image->fragment_pds_requested=true;
  p->image->fragment_pds_source=*source;
  p->image->fragment_pds_block=request->pds[1];
  p->image->fragment_pds_links=p->links;
  unsigned char combined[REMESA_PDS_STRUCTURE_IMAGE_BYTES],evidence[sizeof(combined)];
  size_t combined_bytes;
  error=image_pds_expected(p->image,combined,evidence,&combined_bytes);
  if(error&&error!=ENOTSUP)goto fail;
  if(!error) {
   const unsigned char *candidate[2]={combined,evidence};
   struct remesa_shader_storage **destination[2]={&p->image->fragment_pds,&p->image->fragment_pds_mask};
   for(unsigned i=0;i<2;i++) {
    size_t alignment=i?4u:(size_t)request->pds[1].code_alignment;
    if(alignment<4)alignment=4;
    const struct remesa_shader_storage_request store={.stage=REMESA_IR_FRAGMENT,
     .format=REMESA_SHADER_STORAGE_HOST_PDS_STRUCTURE,.bytes=candidate[i],
     .byte_length=combined_bytes,.byte_alignment=alignment};
    error=remesa_shader_storage_create(&store,destination[i]);if(error)goto fail;
   }
  }
 }
 error=remesa_shader_host_pipeline_validate(p);if(error)goto fail;
 *out=p;return 0;
fail:
 remesa_shader_host_pipeline_destroy(p);return error;
}
int remesa_shader_host_pipeline_revalidate_pds_structure(const struct remesa_shader_host_pipeline *p,
 const struct remesa_shader_draw_programs *programs,const struct remesa_shader_host_pipeline_request *request,
 const struct remesa_pds_structure_request *source)
{
 int error=remesa_shader_host_pipeline_validate(p);if(error)return error;
 if(!p->structure_storage[0])return ESTALE;
 struct remesa_pds_structure shape;
 struct remesa_pds_data_cell cells[REMESA_PDS_STRUCTURE_DATA_CELLS];
 struct remesa_shader_host_pipeline_request current;
 error=structure_request(programs,request,source,&shape,cells,&current);if(error)return error;
 if(memcmp(&shape,&p->structure,sizeof(shape)))return ESTALE;
 return remesa_shader_host_pipeline_revalidate(p,programs,&current);
}
static int contract_request_ranges(const struct remesa_pds_host_transfer_request *requests,
 const void *out,size_t bytes)
{
 if(!requests||2*sizeof(*requests)>UINTPTR_MAX-(uintptr_t)requests||
    (bytes&&(!out||bytes>UINTPTR_MAX-(uintptr_t)out))||
    overlaps(out,bytes,requests,2*sizeof(*requests)))return EINVAL;
 for(unsigned i=0;i<2;i++) {
  if(!requests[i].transfers||!requests[i].transfer_count)return EINVAL;
  if(requests[i].transfer_count>REMESA_PDS_HOST_TASK_SLOTS)return E2BIG;
  size_t length=requests[i].transfer_count*sizeof(*requests[i].transfers);
  if(length>UINTPTR_MAX-(uintptr_t)requests[i].transfers||
     overlaps(out,bytes,requests[i].transfers,length))return EINVAL;
 }
 return 0;
}
/* Prevent publication into any borrowed immutable source or owner metadata. */
static int contract_output_valid(const struct remesa_shader_host_pipeline *p,
 const struct remesa_shader_host_pipeline_view *view,const void *out,size_t bytes)
{
 if(!bytes)return 1;
 if(!out||bytes>UINTPTR_MAX-(uintptr_t)out||
    overlaps(out,bytes,p,sizeof(*p))||overlaps(out,bytes,p->image,sizeof(*p->image))||
    overlaps(out,bytes,view->image.programs,sizeof(*view->image.programs))||
    overlaps(out,bytes,view->image.vertex.bytes,view->image.vertex.byte_length)||
    overlaps(out,bytes,view->image.fragment.bytes,view->image.fragment.byte_length)||
    overlaps(out,bytes,view->image.fragment_pds.bytes,view->image.fragment_pds.byte_length)||
    overlaps(out,bytes,view->image.fragment_pds_mask.bytes,view->image.fragment_pds_mask.byte_length))return 0;
 for(unsigned i=0;i<2;i++)
  if(overlaps(out,bytes,view->data[i].storage.bytes,view->data[i].storage.byte_length)||
     overlaps(out,bytes,p->cells[i],p->request.data[i].cell_count*sizeof(*p->cells[i])))return 0;
 const struct remesa_shader_storage_view *structure_views[3]={&view->structure_code,
  &view->structure_code_mask,&view->structure_data_mask};
 for(unsigned i=0;i<3;i++)
  if(overlaps(out,bytes,structure_views[i]->bytes,structure_views[i]->byte_length))return 0;
 return 1;
}
static uint32_t execution_blockers(void)
{
 return REMESA_EXECUTION_DATA_TO_DS|REMESA_EXECUTION_TASK_CONTROL|
  REMESA_EXECUTION_PDS_INSTRUCTIONS|REMESA_EXECUTION_VS_INPUT_BANK|
  REMESA_EXECUTION_VS_EXPORT|REMESA_EXECUTION_USSE_TERMINATION|
  REMESA_EXECUTION_DEVICE_COMPLETION;
}
int remesa_shader_host_pipeline_prepare_contract(const struct remesa_shader_host_pipeline *p,
 const struct remesa_pds_host_transfer_request requests[2],struct remesa_shader_execution_contract *out)
{
 if(!p||!out)return EINVAL;
 int error=contract_request_ranges(requests,out,sizeof(*out));if(error)return error;
 error=remesa_shader_host_pipeline_validate(p);if(error)return error;
 struct remesa_shader_host_pipeline_view view;
 error=host_views(p,&view);if(error)return error;
 if(!contract_output_valid(p,&view,out,sizeof(*out)))return EINVAL;
 struct remesa_shader_execution_contract result={
  .schema=REMESA_SHADER_EXECUTION_CONTRACT_SCHEMA,.blockers=execution_blockers(),
  .operation=p->links.launch.stage[0].request.operation,
  .generation=p->links.launch.stage[0].request.generation,
  .owner_cookie=p->links.launch.stage[0].request.owner_cookie,
  .fragment_identity=view.image.programs->pipeline.identity[1],
  .fragment_bytes=(uint32_t)view.image.fragment.byte_length,.fragment_form=view.image.fragment_form,
  .pds_instruction_error=ENOTSUP,.fragment_termination_error=ENOTSUP,
  .completion_error=ENOTSUP,.execution_error=ENOTSUP
 };
 error=remesa_pds_prepare_task_pair(p->links.launch.stage,p->request.use,p->links.use,
  p->request.data,requests,result.task);if(error)return error;
 error=remesa_usse_vertex_export_plan(&view.image.programs->vertex.usse,
  &p->request.vertex_mapping,&result.vertex);if(error)return error;
 *out=result;return 0;
}
int remesa_shader_host_pipeline_validate_contract(const struct remesa_shader_host_pipeline *p,
 const struct remesa_pds_host_transfer_request requests[2],const struct remesa_shader_execution_contract *contract)
{
 if(!p||!contract)return EINVAL;
 int error=contract_request_ranges(requests,contract,sizeof(*contract));if(error)return error;
 error=remesa_shader_host_pipeline_validate(p);if(error)return error;
 struct remesa_shader_host_pipeline_view view;
 error=host_views(p,&view);if(error)return error;
 if(!contract_output_valid(p,&view,contract,sizeof(*contract)))return EINVAL;
 if(contract->schema!=REMESA_SHADER_EXECUTION_CONTRACT_SCHEMA||
    contract->blockers!=execution_blockers()||
    contract->operation!=p->links.launch.stage[0].request.operation||
    contract->generation!=p->links.launch.stage[0].request.generation||
    contract->owner_cookie!=p->links.launch.stage[0].request.owner_cookie||
    contract->fragment_identity!=view.image.programs->pipeline.identity[1]||
    contract->fragment_bytes!=view.image.fragment.byte_length||
    contract->fragment_form!=(uint32_t)view.image.fragment_form||
    contract->pds_instruction_error!=ENOTSUP||contract->fragment_termination_error!=ENOTSUP||
    contract->completion_error!=ENOTSUP||contract->execution_error!=ENOTSUP)return EPROTO;
 error=remesa_pds_validate_task_pair(p->links.launch.stage,p->request.use,p->links.use,
  p->request.data,requests,contract->task);if(error)return error;
 return remesa_usse_vertex_export_validate(&view.image.programs->vertex.usse,
  &p->request.vertex_mapping,&contract->vertex);
}
static int pixel_task_output_ranges(const struct remesa_shader_host_pipeline *p,
 const struct remesa_pds_host_transfer_request *requests,
 const struct remesa_pds_cpu_pixel_task_output *out)
{
 if(!p||!out)return EINVAL;
 int error=contract_request_ranges(requests,out,sizeof(*out));if(error)return error;
 error=remesa_shader_host_pipeline_validate(p);if(error)return error;
 struct remesa_shader_host_pipeline_view view;
 error=host_views(p,&view);if(error)return error;
 if(!contract_output_valid(p,&view,out,sizeof(*out)))return EINVAL;
 if(view.image.fragment_form!=REMESA_SHADER_PACKED_CONSTANT||
    view.image.fragment.byte_length!=8)return ENOTSUP;
 return 0;
}
int remesa_shader_host_pipeline_prepare_pixel_task_output(const struct remesa_shader_host_pipeline *p,
 const struct remesa_pds_host_transfer_request requests[2],struct remesa_pds_cpu_pixel_task_output *out)
{
 int error=pixel_task_output_ranges(p,requests,out);if(error)return error;
 struct remesa_shader_execution_contract contract;
 error=remesa_shader_host_pipeline_prepare_contract(p,requests,&contract);if(error)return error;
 /* The complete graph comes from the actual immutable VS/FS DATA. The CPU
  * consumer does not select a physical DS source, port or termination rule. */
 return remesa_pds_cpu_pixel_task_prepare(&contract.task[1],out);
}
int remesa_shader_host_pipeline_validate_pixel_task_output(const struct remesa_shader_host_pipeline *p,
 const struct remesa_pds_host_transfer_request requests[2],const struct remesa_pds_cpu_pixel_task_output *out)
{
 int error=pixel_task_output_ranges(p,requests,out);if(error)return error;
 struct remesa_pds_cpu_pixel_task_output current;
 error=remesa_shader_host_pipeline_prepare_pixel_task_output(p,requests,&current);if(error)return error;
 if(out->schema!=current.schema||out->cpu_word_count!=current.cpu_word_count||out->reserved||
    out->data_to_ds_error!=current.data_to_ds_error||out->dispatch_error!=current.dispatch_error||
    out->termination_error!=current.termination_error)return EPROTO;
 for(unsigned i=0;i<REMESA_PDS_CPU_PIXEL_TASK_WORDS;i++) {
  const struct remesa_pds_cpu_pixel_task_source *a=&out->sources[i],*b=&current.sources[i];
  if(out->task.words[i]!=current.task.words[i]||a->data_cell!=b->data_cell||
     a->logical_slot!=b->logical_slot||a->value!=b->value||
     a->data_address!=b->data_address)return EPROTO;
 }
 return 0;
}
int remesa_shader_host_pipeline_encode_contract(const struct remesa_shader_host_pipeline *p,
 const struct remesa_pds_host_transfer_request requests[2],const struct remesa_shader_execution_contract *contract,
 void *out,size_t cap,size_t *written)
{
 if(!written||sizeof(*written)>UINTPTR_MAX-(uintptr_t)written||(!out&&cap)||
    cap>UINTPTR_MAX-(uintptr_t)out||!contract||
    overlaps(out,cap,contract,sizeof(*contract))||
    overlaps(written,sizeof(*written),contract,sizeof(*contract))||
    overlaps(out,cap,written,sizeof(*written)))return EINVAL;
 int error=contract_request_ranges(requests,out,cap);if(error)return error;
 error=contract_request_ranges(requests,written,sizeof(*written));if(error)return error;
 error=remesa_shader_host_pipeline_validate_contract(p,requests,contract);if(error)return error;
 struct remesa_shader_host_pipeline_view view;
 error=host_views(p,&view);if(error)return error;
 if(!contract_output_valid(p,&view,out,cap)||
    !contract_output_valid(p,&view,written,sizeof(*written)))return EINVAL;
 /* Logical routing, exact entry fields and VS aliases do not supply hardware
  * DATA->DS, task output, physical export, termination or completion rules. */
 return ENOTSUP;
}

/* Selected producer path. Old scalar USSE/PDS/pair interfaces keep their
 * existing rejection boundary; no artificial scalar plan is manufactured. */
static void selected_hash_word(uint64_t *h,uint32_t word,unsigned *index)
{
 for(unsigned b=0;b<4;b++) {
  *h=(*h<<7)|(*h>>57);*h^=((word>>(8*b))&255u)+(uint64_t)(*index)++;
 }
}
int remesa_shader_program_selected_resources(const struct remesa_shader_program *program,
 struct remesa_shader_selected_resources *out)
{
 if(!program||!out||overlaps(program,sizeof(*program),out,sizeof(*out)))return EINVAL;
 int error=remesa_shader_program_validate(program);if(error)return error;
 struct remesa_shader_selected_resources r={.host_alignment=32};
 if(program->status==REMESA_PROGRAM_COMPILE_OFFLINE&&
    !remesa_shader_ir_white_lambert(&program->semantic)) {
  r.profile=REMESA_PDS_SELECTED_VS_WHITE_LAMBERT3;
  r.logical_use_bytes=64;r.reserved_use_bytes=144;
  error=remesa_shader_ir_vertex_layout(&program->semantic,&r.vertex);if(error)return error;
 } else if(program->status==REMESA_PROGRAM_COMPILE_OFFLINE&&
           !remesa_shader_ir_fragment_color_input(&program->semantic)) {
  struct remesa_usse_fragment_color_extent extent;
  error=remesa_shader_program_fragment_color_extent(program,&extent);if(error)return error;
  r.profile=REMESA_PDS_SELECTED_FS_INPUT_COL0;
  r.logical_use_bytes=(uint32_t)extent.logical_bytes;r.reserved_use_bytes=(uint32_t)extent.allocation_bytes;
  r.fragment_input_slot=1;r.fragment_components=4;
  r.r_id=extent.r_id;r.r_components=extent.r_components;r.r_mode=extent.r_mode;r.r_flag=extent.r_flag;
 } else {
  uint32_t argb;
  if(program->status!=REMESA_PROGRAM_SEMANTIC_ONLY||
     remesa_shader_ir_fragment_color(&program->semantic,&argb)||argb!=UINT32_C(0xff00ff00))return ENOTSUP;
  r.profile=REMESA_PDS_SELECTED_FS_CONSTANT_DRIVER_BG0;
  r.logical_use_bytes=8;r.reserved_use_bytes=8;r.constant_argb=argb;
 }
 /* Host correlation consumes complete canonical semantic fields and actual
  * emitted bytes in LE order; never treat the noncryptographic hash as proof
  * of mapping ownership or replace exact program comparisons with it. */
 unsigned char bytes[64];size_t length;
 error=remesa_shader_program_encode(program,bytes,sizeof(bytes),&length);if(error)return error;
 if(length!=r.logical_use_bytes)return EPROTO;
 uint64_t h=0;unsigned index=0;
 selected_hash_word(&h,r.profile,&index);selected_hash_word(&h,program->schema,&index);
 selected_hash_word(&h,program->status,&index);selected_hash_word(&h,program->semantic.schema,&index);
 selected_hash_word(&h,program->semantic.stage,&index);selected_hash_word(&h,program->semantic.count,&index);
 for(unsigned n=0;n<REMESA_SHADER_IR_INSTRUCTIONS;n++) {
  const struct remesa_ir_instruction *i=&program->semantic.instructions[n];
  selected_hash_word(&h,i->op,&index);selected_hash_word(&h,i->dst,&index);
  selected_hash_word(&h,i->components,&index);selected_hash_word(&h,i->type,&index);
  for(unsigned c=0;c<4;c++){selected_hash_word(&h,i->src[c].reg,&index);selected_hash_word(&h,i->src[c].channel,&index);}
  for(unsigned c=0;c<4;c++)selected_hash_word(&h,i->bits[c],&index);
  selected_hash_word(&h,i->input_slot,&index);
 }
 for(size_t n=0;n<length;n++){h=(h<<7)|(h>>57);h^=(uint64_t)bytes[n]+index++;}
 r.semantic_identity=h?h:1;*out=r;return 0;
}
int remesa_shader_program_prepare_selected_pds(const struct remesa_shader_program *program,
 const struct remesa_pds_selected_request *request,struct remesa_pds_selected_plan *out)
{
 if(!program||!request||!out||overlaps(program,sizeof(*program),out,sizeof(*out))||
    overlaps(request,sizeof(*request),out,sizeof(*out)))return EINVAL;
 struct remesa_shader_selected_resources r;
 int error=remesa_shader_program_selected_resources(program,&r);if(error)return error;
 if(request->profile!=r.profile)return ENOTSUP;
 if(request->semantic_identity!=r.semantic_identity)return ESTALE;
 struct remesa_pds_selected_plan p;
 error=remesa_pds_selected_prepare(request,&p);if(error)return error;
 if(p.stage!=program->semantic.stage||p.use_committed_bytes!=r.logical_use_bytes||
    p.use_reserved_bytes!=r.reserved_use_bytes)return EPROTO;
 *out=p;return 0;
}
struct remesa_shader_selected_pipeline {
 struct remesa_shader_program programs[2];
 struct remesa_shader_selected_resources resources[2];
 struct remesa_pds_selected_plan plans[2];
 struct remesa_shader_storage *stores[3][2]; /* USE, PDS DATA+CODE, known mask */
};
static int selected_buffers(const struct remesa_shader_program *program,
 const struct remesa_shader_selected_resources *resources,const struct remesa_pds_selected_plan *plan,
 unsigned char use[64],unsigned char pds[64],unsigned char mask[64],size_t *use_length,size_t *pds_length)
{
 int error=remesa_shader_program_encode(program,use,64,use_length);if(error)return error;
 if(*use_length!=resources->logical_use_bytes)return EPROTO;
 error=remesa_pds_selected_encode(plan,pds,mask,64,pds_length);if(error)return error;
 if(*pds_length!=plan->image_bytes||*pds_length!=(size_t)plan->data_bytes+plan->code_bytes)return EPROTO;
 for(size_t n=plan->data_bytes;n<*pds_length;n++)if(mask[n]!=255)return ENOTSUP;
 return 0;
}
static int selected_stage_valid(const struct remesa_shader_selected_resources r[2])
{
 if(r[0].profile!=REMESA_PDS_SELECTED_VS_WHITE_LAMBERT3||
    r[1].profile!=REMESA_PDS_SELECTED_FS_INPUT_COL0)return ENOTSUP;
 return 0;
}
void remesa_shader_selected_pipeline_destroy(struct remesa_shader_selected_pipeline *p)
{
 if(!p)return;
 for(unsigned k=0;k<3;k++)for(unsigned s=0;s<2;s++)
  (void)remesa_shader_storage_reference(&p->stores[k][s],NULL);
 free(p);
}
int remesa_shader_selected_pipeline_prepare(const struct remesa_shader_program programs[2],
 const struct remesa_pds_selected_request requests[2],struct remesa_shader_selected_pipeline **out)
{
 if(!programs||!requests||!out||
    overlaps(out,sizeof(*out),programs,2*sizeof(*programs))||
    overlaps(out,sizeof(*out),requests,2*sizeof(*requests)))return EINVAL;
 if(*out)return EBUSY;
 struct remesa_shader_selected_resources r[2];struct remesa_pds_selected_plan plans[2];
 for(unsigned s=0;s<2;s++) {
  int error=remesa_shader_program_selected_resources(&programs[s],&r[s]);if(error)return error;
  error=remesa_shader_program_prepare_selected_pds(&programs[s],&requests[s],&plans[s]);if(error)return error;
 }
 int error=selected_stage_valid(r);if(error)return error;
 error=remesa_pds_selected_pair_validate(plans);if(error)return error;
 struct remesa_shader_selected_pipeline *p=calloc(1,sizeof(*p));if(!p)return ENOMEM;
 memcpy(p->programs,programs,sizeof(p->programs));memcpy(p->resources,r,sizeof(r));memcpy(p->plans,plans,sizeof(plans));
 for(unsigned s=0;s<2;s++) {
  unsigned char use[64],pds[64],mask[64];size_t ul,pl;
  error=selected_buffers(&p->programs[s],&r[s],&plans[s],use,pds,mask,&ul,&pl);if(error)break;
  const unsigned char *bytes[3]={use,pds,mask};const size_t lengths[3]={ul,pl,pl};
  for(unsigned k=0;k<3;k++) {
   const struct remesa_shader_storage_request req={.stage=p->programs[s].semantic.stage,
    .format=k?REMESA_SHADER_STORAGE_HOST_PDS_STRUCTURE:REMESA_SHADER_STORAGE_HOST_CPU_RECORDS,
    .bytes=bytes[k],.byte_length=lengths[k],.byte_alignment=k==2?4u:32u};
   error=remesa_shader_storage_create(&req,&p->stores[k][s]);if(error)break;
  }
  if(error)break;
 }
 if(error){remesa_shader_selected_pipeline_destroy(p);return error;}
 *out=p;return 0;
}
int remesa_shader_selected_pipeline_validate(const struct remesa_shader_selected_pipeline *p)
{
 if(!p)return EINVAL;
 struct remesa_shader_selected_resources r[2];
 for(unsigned s=0;s<2;s++) {
  int error=remesa_shader_program_selected_resources(&p->programs[s],&r[s]);if(error)return error;
  if(memcmp(&r[s],&p->resources[s],sizeof(r[s])))return EPROTO;
  struct remesa_pds_selected_plan plan;
  error=remesa_shader_program_prepare_selected_pds(&p->programs[s],&p->plans[s].request,&plan);if(error)return error;
  error=remesa_pds_selected_validate(&p->plans[s],&plan.request);if(error)return error;
  unsigned char use[64],pds[64],mask[64];size_t ul,pl;
  error=selected_buffers(&p->programs[s],&r[s],&p->plans[s],use,pds,mask,&ul,&pl);if(error)return error;
  const unsigned char *bytes[3]={use,pds,mask};const size_t lengths[3]={ul,pl,pl};
  for(unsigned k=0;k<3;k++) {
   struct remesa_shader_storage_view view;error=remesa_shader_storage_inspect(p->stores[k][s],&view);if(error)return error;
   if(view.stage!=p->programs[s].semantic.stage||view.format!=(k?REMESA_SHADER_STORAGE_HOST_PDS_STRUCTURE:REMESA_SHADER_STORAGE_HOST_CPU_RECORDS)||
      view.status!=REMESA_SHADER_STORAGE_CANDIDATE||view.byte_length!=lengths[k]||view.byte_alignment!=(k==2?4u:32u)||
      memcmp(view.bytes,bytes[k],lengths[k]))return EPROTO;
  }
 }
 int error=selected_stage_valid(r);return error?error:remesa_pds_selected_pair_validate(p->plans);
}
static int selected_output_valid(const struct remesa_shader_selected_pipeline *p,const void *out,size_t bytes)
{
 if(!out||bytes>UINTPTR_MAX-(uintptr_t)out||overlaps(p,sizeof(*p),out,bytes))return 0;
 for(unsigned k=0;k<3;k++)for(unsigned s=0;s<2;s++) {
  struct remesa_shader_storage_view view;
  if(remesa_shader_storage_inspect(p->stores[k][s],&view)||overlaps(view.bytes,view.byte_length,out,bytes))return 0;
 }
 return 1;
}
int remesa_shader_selected_pipeline_inspect(const struct remesa_shader_selected_pipeline *p,
 struct remesa_shader_selected_pipeline_view *out)
{
 int error=remesa_shader_selected_pipeline_validate(p);if(error)return error;
 if(!selected_output_valid(p,out,sizeof(*out)))return EINVAL;
 struct remesa_shader_selected_pipeline_view v={.programs=p->programs,.resources=p->resources,.plans=p->plans,
  .blockers=REMESA_SELECTED_VERTEX_TRANSPORT|REMESA_SELECTED_DRAW_PACKET,
  .vertex_transport_error=ENOTSUP,.draw_packet_error=ENOTSUP,.execution_error=ENOTSUP};
 for(unsigned s=0;s<2;s++) {
  error=remesa_shader_storage_inspect(p->stores[0][s],&v.use[s]);if(error)return error;
  error=remesa_shader_storage_inspect(p->stores[1][s],&v.pds[s]);if(error)return error;
  error=remesa_shader_storage_inspect(p->stores[2][s],&v.known_mask[s]);if(error)return error;
 }
 *out=v;return 0;
}
int remesa_shader_selected_pipeline_clone(const struct remesa_shader_selected_pipeline *source,
 struct remesa_shader_selected_pipeline **out)
{
 int error=remesa_shader_selected_pipeline_validate(source);if(error)return error;
 if(!selected_output_valid(source,out,sizeof(*out)))return EINVAL;
 if(*out)return EBUSY;
 struct remesa_shader_selected_pipeline *p=calloc(1,sizeof(*p));if(!p)return ENOMEM;
 *p=*source;memset(p->stores,0,sizeof(p->stores));
 for(unsigned k=0;k<3;k++)for(unsigned s=0;s<2;s++) {
  error=remesa_shader_storage_reference(&p->stores[k][s],source->stores[k][s]);
  if(error){remesa_shader_selected_pipeline_destroy(p);return error;}
 }
 *out=p;return 0;
}
int remesa_shader_selected_pipeline_revalidate(const struct remesa_shader_selected_pipeline *p,
 const struct remesa_shader_program programs[2],const struct remesa_pds_selected_request current[2])
{
 if(!programs||!current)return EINVAL;
 int error=remesa_shader_selected_pipeline_validate(p);if(error)return error;
 for(unsigned s=0;s<2;s++) {
  error=remesa_shader_program_validate(&programs[s]);if(error)return error;
  if(!same_program(&p->programs[s],&programs[s]))return ESTALE;
  error=remesa_pds_selected_validate(&p->plans[s],&current[s]);if(error)return error;
 }
 return 0;
}
int remesa_shader_selected_pipeline_build_request(const struct remesa_shader_selected_pipeline *p,
 uint64_t owner,uint64_t generation,struct remesa_shader_request *out)
{
 struct remesa_shader_selected_pipeline_view v;
 int error=remesa_shader_selected_pipeline_inspect(p,&v);if(error)return error;
 if(!selected_output_valid(p,out,sizeof(*out)))return EINVAL;
 struct remesa_shader_blob blobs[REMESA_SHADER_RESOURCE_COUNT]={{0}};
 const struct remesa_shader_storage_view *stores[4]={&v.use[0],&v.use[1],&v.pds[0],&v.pds[1]};
 for(unsigned i=0;i<4;i++)blobs[i]=(struct remesa_shader_blob){.bytes=stores[i]->bytes,
  .length=(uint32_t)stores[i]->byte_length,.alignment=(uint32_t)stores[i]->byte_alignment,.state=REMESA_SHADER_CPU_CANDIDATE};
 return request_error(remesa_shader_request_build(out,owner,generation,blobs));
}
int remesa_shader_selected_pipeline_validate_request(const struct remesa_shader_selected_pipeline *p,
 const struct remesa_shader_request *request)
{
 if(!request||sizeof(*request)>UINTPTR_MAX-(uintptr_t)request)return EINVAL;
 struct remesa_shader_selected_pipeline_view v;
 int error=remesa_shader_selected_pipeline_inspect(p,&v);if(error)return error;
 if(!selected_output_valid(p,request,sizeof(*request)))return EINVAL;
 if(remesa_shader_request_validate(request,request->owner,request->generation)!=REMESA_SHADER_REQUEST_OK)return EPROTO;
 const struct remesa_shader_storage_view *stores[4]={&v.use[0],&v.use[1],&v.pds[0],&v.pds[1]};size_t offset=0;
 for(unsigned i=0;i<4;i++) {
  const struct remesa_shader_storage_view *s=stores[i];const struct remesa_shader_resource *r=&request->resources[i];
  offset=(offset+s->byte_alignment-1)&~(s->byte_alignment-1);
  if(r->offset!=offset||r->length!=s->byte_length||r->alignment!=s->byte_alignment||
     r->state!=REMESA_SHADER_CPU_CANDIDATE||memcmp(request->data+offset,s->bytes,s->byte_length))return EPROTO;
  offset+=s->byte_length;
 }
 return request->resource_mask==15&&request->byte_count==offset?0:EPROTO;
}
int remesa_shader_selected_pipeline_validate_execution(const struct remesa_shader_selected_pipeline *p)
{int error=remesa_shader_selected_pipeline_validate(p);return error?error:ENOTSUP;}
