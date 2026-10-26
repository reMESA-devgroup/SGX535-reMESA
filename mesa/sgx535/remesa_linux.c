/* SPDX-License-Identifier: MIT */
/* Synchronous, single-attempt Linux transport. Host staging is never a
 * rasterizer: target bytes become readable only from a validated GPU response.
 * Kernel ownership/quarantine protects actual SGX backing on uncertain failure.
 */
#include "remesa_linux.h"
#include "remesa/native_uapi.h"
#include "remesa/shader_request.h"
#include "remesa_shader_program.h"
#include "remesa_draw.h"
#include "remesa_persistent.h"
#include "remesa_linux_memory.h"
#include "remesa_fragment_body.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

_Static_assert(sizeof(struct remesa_native_draw)==4240,"native/i386 UAPI");
struct linux_bo { size_t size; unsigned char *bytes; bool readable, busy, failed; };
struct linux_fence { struct linux_fence *next; int error; bool published; };
struct linux_shader {
 struct linux_shader *next;
 void *token;
 struct remesa_shader_image *image;
 struct remesa_shader_request request;
 struct remesa_shader_bindings bindings;
 uint64_t generation;
 void *storage_token;
 bool supplied_cpu_image;
 /* Keep the source inventory through authenticated retained completion;
  * normal Gallium teardown must not release it after an uncertain attempt. */
 bool fragment_held;
 /* Immutable host plan, never an executable task or authenticated PDS map. */
 bool pds_links_retained;
 struct remesa_shader_pds_links pds_links;
 struct remesa_shader_storage *pds_host_data[2];
};
struct linux_device {
 int fd; bool attempted;
 struct remesa_linux_receipt receipt;
 struct remesa_native_draw response;
 struct linux_shader *shaders;
 struct linux_fence *fences;
 struct remesa_shader_cookie_pool shader_cookies;
 uint64_t shader_generation;
 bool shader_candidate_valid;
 struct remesa_shader_request shader_candidate;
 struct remesa_device shader_storage;
 struct remesa_usse_vertex_mapping vertex_candidate_mapping;
 bool vertex_candidate_configured;
 struct remesa_linux_memory *shader_memory;
 bool fragment_enabled,fragment_prepared,fragment_active;
 struct remesa_linux_fragment_carrier fragment_carrier;
 struct remesa_retained_request fragment_job;
 struct remesa_linux_fragment_receipt fragment_receipt;
};
static int endpoint_output_check(const struct linux_device *,const void *,size_t);
static uint32_t fragment_color(const struct linux_device *d)
{return d->fragment_carrier.fragment_argb?d->fragment_carrier.fragment_argb:UINT32_C(0xffff00ff);}
static int fragment_admit(const struct remesa_shader_image *image,uint32_t expected)
{
 struct remesa_shader_image_view view;
 struct remesa_fragment_constant record;
 int error=remesa_shader_image_inspect(image,&view);if(error)return error;
 if(view.fragment_form!=REMESA_SHADER_PACKED_CONSTANT||view.fragment.byte_length!=8)
  return ENOTSUP;
 error=remesa_fragment_constant_decode(view.fragment.bytes,8,&record);
 if(error)return error;
 return record.argb==expected?0:ENOTSUP;
}
static bool overlaps(const void *a,size_t an,const void *b,size_t bn)
{
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 if(an>UINTPTR_MAX-x || bn>UINTPTR_MAX-y)return true;
 return x<=y ? y-x<an : x-y<bn;
}
int remesa_linux_shader_request_build(const struct remesa_shader_image *image,
 uint64_t owner,uint64_t generation,struct remesa_shader_request *out)
{return remesa_shader_image_build_request(image,owner,generation,out);}
static int prepare_candidate(void *priv,const struct remesa_shader_draw_programs *programs,
 const struct remesa_usse_vertex_mapping *mapping,
 const struct remesa_shader_image *provided,void **out)
{
 struct linux_device *d=priv;
 struct linux_shader *shader;
 struct remesa_shader_request request;
 if(!out)return EINVAL;
 if(d) {
  int cookie_error=endpoint_output_check(d,out,sizeof(*out));
  if(cookie_error)return cookie_error;
 }
 *out=NULL;
 if(!d||!programs)return EINVAL;
 if(d->shader_cookies.count>=REMESA_SHADER_COOKIE_LIMIT)return ENOSPC;
 if(d->shader_generation==UINT64_MAX)return EOVERFLOW;
 shader=calloc(1,sizeof(*shader));if(!shader)return ENOMEM;
 int error=provided?remesa_shader_image_clone(provided,&shader->image):
  mapping?remesa_shader_image_prepare_vertex_candidate(programs,mapping,&shader->image):
  remesa_shader_image_prepare(programs,&shader->image);
 shader->supplied_cpu_image=provided!=NULL;
 if(error){free(shader);return error;}
 shader->generation=d->shader_generation+1;
 if(d->shader_storage.ops) {
  struct remesa_device *store=&d->shader_storage;
  error=provided?remesa_persistent_prepare_image(store,shader->image,&shader->storage_token):
   mapping?remesa_persistent_prepare_vertex_candidate(store,programs,mapping,
   &shader->storage_token):store->ops->prepare_shader_programs(store->priv,programs,&shader->storage_token);
  if(error==ENOTSUP && shader->storage_token) {
   error=remesa_persistent_shader_bindings(store,shader->storage_token,&request,&shader->bindings);
   if(!error && request.generation<=d->shader_generation)error=EPROTO;
   if(!error)shader->generation=request.generation;
  } else if(!error)error=EPROTO;
 } else error=remesa_linux_shader_request_build(shader->image,(uint64_t)(uintptr_t)d,
   shader->generation,&request);
 if(!error) {
  error=remesa_shader_image_validate_request(shader->image,&request);
 }
 if(!error)error=remesa_shader_cookie_create(&d->shader_cookies,&shader->token);
 if(error){
  if(shader->storage_token)d->shader_storage.ops->destroy_shader_programs(
   d->shader_storage.priv,shader->storage_token);
  remesa_shader_image_destroy(shader->image);free(shader);return error;
 }
 shader->request=request;
 shader->next=d->shaders;d->shaders=shader;d->shader_generation=shader->generation;
 d->shader_candidate=request;d->shader_candidate_valid=true;
 *out=shader->token;
 /* Returning an owned token on error lets Gallium exercise real failure
  * cleanup. CPU candidate storage is prepared, but success at this callback
  * boundary requires executable vertex/fragment/PDS programs and hardware
  * qualification. Optional GEM staging never gives this candidate an SGX VA.
  * No historical draw ioctl or GPU action occurs. */
 /* The explicit fragment-stage laboratory prepares only this original FS.
  * It does not promote the semantic VS or PDS to executable original code. */
 return !provided&&d->fragment_enabled?fragment_admit(shader->image,fragment_color(d)):
  remesa_shader_image_validate_execution(shader->image);
}
static int prepare_shader_programs(void *priv,const struct remesa_shader_draw_programs *programs,
 void **out)
{
 struct linux_device *d=priv;
 return prepare_candidate(priv,programs,d&&d->vertex_candidate_configured?
  &d->vertex_candidate_mapping:NULL,NULL,out);
}
static struct linux_shader **shader_link(struct linux_device *d,void *token)
{
 if(!d)return NULL;
 for(struct linux_shader **link=&d->shaders;*link;link=&(*link)->next)
  if((*link)->token==token)return link;
 return NULL;
}
/* Query outputs must not mutate any live program or raw draw snapshot owned
 * by this endpoint, including a different token's borrowed VB/IB. The nested
 * persistent endpoint owns the snapshots and supplies borrowed views here. */
static int endpoint_output_check(const struct linux_device *d,
 const void *out,size_t bytes)
{
 if(!d||!out||overlaps(out,bytes,d,sizeof(*d)))return EINVAL;
 int cookie_error=remesa_shader_cookie_output_check(&d->shader_cookies,out,bytes);
 if(cookie_error)return cookie_error;
 for(const struct linux_shader *shader=d->shaders;shader;shader=shader->next) {
  struct remesa_shader_image_view view;
  if(overlaps(out,bytes,shader,sizeof(*shader)))return EINVAL;
  int error=remesa_shader_image_inspect(shader->image,&view);if(error)return error;
  if(overlaps(out,bytes,view.programs,sizeof(*view.programs))||
     overlaps(out,bytes,view.fragment.bytes,view.fragment.byte_length)||
     overlaps(out,bytes,view.vertex.bytes,view.vertex.byte_length)||
     overlaps(out,bytes,view.fragment_pds.bytes,view.fragment_pds.byte_length)||
     overlaps(out,bytes,view.fragment_pds_mask.bytes,view.fragment_pds_mask.byte_length))return EINVAL;
  for(unsigned i=0;i<2;i++)if(shader->pds_host_data[i]) {
   struct remesa_shader_storage_view data;
   error=remesa_shader_storage_inspect(shader->pds_host_data[i],&data);if(error)return error;
   if(overlaps(out,bytes,data.bytes,data.byte_length))return EINVAL;
  }
 }
 if(d->shader_storage.ops)
  return remesa_persistent_shader_output_check(&d->shader_storage,out,bytes);
 return 0;
}
static int shader_output_check(const struct remesa_device *device,
 const void *out,size_t bytes)
{
 if(!device||!out||overlaps(out,bytes,device,sizeof(*device)))return EINVAL;
 return endpoint_output_check(device->priv,out,bytes);
}
static int fragment_geometry(const struct remesa_draw_packet *packet,uint32_t argb,bool quarter)
{
 struct remesa_native_draw actual;
 struct remesa_native_draw expected={.version=REMESA_NATIVE_ABI_VERSION,
  .width=32,.height=32,.vertex_count=3,.index_count=3,.argb=argb,
  .positions={{0x41000000,0x41000000,0x3f000000,0x3f800000},
   {0x41c00000,0x41000000,0x3f000000,0x3f800000},
   {0x41000000,0x41c00000,0x3f000000,0x3f800000}},.indices={0,1,2}};
 if(quarter) {
  /* IEEE7548.25/24.25, finite mathematical vertex data, not PDS fields. */
  const uint32_t xy[3][2]={{0x41040000,0x41040000},
   {0x41c20000,0x41040000},{0x41040000,0x41c20000}};
  for(unsigned v=0;v<3;v++)for(unsigned c=0;c<2;c++)expected.positions[v][c]=xy[v][c];
 }
 int error=remesa_linux_request_build(packet,&actual);if(error)return error;
 return memcmp(&actual,&expected,offsetof(struct remesa_native_draw,operation))?ENOTSUP:0;
}
static int fragment_stage(struct linux_device *d,struct linux_shader *shader)
{
 struct remesa_shader_image_view view;
 const struct remesa_retained_binding *use=&d->fragment_job.bindings[1];
 const struct remesa_persistent_ops *memory=remesa_linux_memory_ops();
 struct remesa_linux_fragment_receipt *receipt=&d->fragment_receipt;
 void *mapped=NULL;unsigned char *before=NULL;int error,cleanup;
 error=fragment_admit(shader->image,fragment_color(d));if(error)return error;
 if(shader->request.owner!=remesa_linux_memory_context(d->shader_memory))return ESTALE;
 for(unsigned i=0;i<4;i++)if(shader->bindings.resources[i].handle)
  for(unsigned j=0;j<10;j++)
   if(shader->bindings.resources[i].handle==d->fragment_job.bindings[j].id)return EINVAL;
 error=remesa_persistent_shader_verify(&d->shader_storage,shader->storage_token);
 if(error)return error;
 error=remesa_shader_image_inspect(shader->image,&view);if(error)return error;
 for(unsigned i=0;i<10;i++) {
  const struct remesa_retained_binding *b=&d->fragment_job.bindings[i];
  error=remesa_linux_memory_validate_backing(d->shader_memory,b->id,b->va,(size_t)b->bytes);
  if(error)return error;
 }
 before=malloc((size_t)use->bytes);if(!before)return ENOMEM;
 receipt->owner=shader->request.owner;receipt->generation=shader->generation;
 receipt->source_handle=shader->bindings.resources[1].handle;
 receipt->source_offset=shader->bindings.resources[1].offset;
 receipt->use_handle=use->id;receipt->use_va=use->va;
 memcpy(receipt->source,view.fragment.bytes,8);
 if(d->fragment_carrier.independent_vertex) {
  if(view.vertex.byte_length!=16||view.vertex.byte_alignment!=8||
     shader->bindings.resources[0].handle!=receipt->source_handle||
     shader->bindings.resources[0].length!=16||use->bytes<0x50){error=ENOTSUP;goto done;}
  receipt->vertex_source_handle=shader->bindings.resources[0].handle;
  receipt->vertex_source_offset=shader->bindings.resources[0].offset;
  receipt->vertex_use_offset=0x40;
  memcpy(receipt->vertex_source,view.vertex.bytes,16);
 }
 error=memory->cpu_map(d->shader_memory,use->id,false,&mapped);
 if(error){if(mapped)(void)memory->cpu_unmap(d->shader_memory,use->id,false);goto done;}
 if(!mapped){error=EOWNERDEAD;goto done;}
 memcpy(before,mapped,(size_t)use->bytes);memcpy(receipt->preimage,mapped,8);
 receipt->program_changed=memcmp(receipt->source,receipt->preimage,8)!=0;
 if(d->fragment_carrier.independent_vertex)
  memcpy(receipt->vertex_preimage,(const unsigned char *)mapped+0x40,16);
 error=memory->cpu_unmap(d->shader_memory,use->id,false);mapped=NULL;
 if(error){error=EOWNERDEAD;goto done;}
 if(d->fragment_carrier.independent_vertex&&
    memcmp(receipt->vertex_preimage,receipt->vertex_source,16)) {error=ESTALE;goto done;}
 /* The green experiment replaces the exact admitted magenta entry. Check its
  * preimage before any USE write or possible issue, rather than discovering a
  * foreign carrier only in the post-submission authentication callback. */
 if(fragment_color(d)==UINT32_C(0xff00ff00)) {
  struct remesa_fragment_constant previous;
  if(remesa_fragment_constant_decode(receipt->preimage,8,&previous)||
     previous.argb!=UINT32_C(0xffff00ff)) {error=ESTALE;goto done;}
 }
 if(d->fragment_carrier.require_program_change&&!receipt->program_changed) {
  error=ENOTSUP;goto done;
 }
 error=memory->cpu_map(d->shader_memory,use->id,true,&mapped);
 if(error){if(mapped)(void)memory->cpu_unmap(d->shader_memory,use->id,true);goto done;}
 if(!mapped){error=EOWNERDEAD;goto done;}
 if(memcmp(mapped,before,(size_t)use->bytes)) {
  (void)memory->cpu_unmap(d->shader_memory,use->id,true);mapped=NULL;
  error=EOWNERDEAD;goto done;
 }
 memcpy(mapped,view.fragment.bytes,8);
 if(d->fragment_carrier.independent_vertex)
  memcpy((unsigned char *)mapped+0x40,view.vertex.bytes,16);
 error=memory->cpu_unmap(d->shader_memory,use->id,true);mapped=NULL;
 if(error)goto done;
 error=memory->cpu_map(d->shader_memory,use->id,false,&mapped);
 if(error){if(mapped)(void)memory->cpu_unmap(d->shader_memory,use->id,false);goto done;}
 if(!mapped){error=EOWNERDEAD;goto done;}
 if(memcmp(mapped,view.fragment.bytes,8)||
    memcmp((unsigned char *)mapped+8,before+8,(size_t)use->bytes-8))error=EOWNERDEAD;
 cleanup=memory->cpu_unmap(d->shader_memory,use->id,false);mapped=NULL;
 if(cleanup)error=EOWNERDEAD;
 if(!error){receipt->staged=true;receipt->outside_fragment_unchanged=true;
  receipt->vertex_staged=d->fragment_carrier.independent_vertex;}
done:
 if(error==EOWNERDEAD)receipt->quarantined=true;
 free(before);return error;
}
static int fragment_capture(struct linux_device *d,struct linux_shader *shader,
 struct remesa_linux_fragment_capture *out)
{
 struct remesa_shader_request request;
 struct remesa_persistent_shader_backing program;
 struct remesa_persistent_draw_backing geometry;
 /* The bounded experiment accounts exactly one token and two CPU-only BOs.
  * Optional HOST DATA and additional tokens require their own inventory. */
 if(d->shaders!=shader||shader->next||shader->pds_host_data[0]||
    shader->pds_host_data[1])return ENOTSUP;
 int error=remesa_persistent_shader_snapshot(&d->shader_storage,
  shader->storage_token,&request,&program);if(error)return error;
 error=remesa_persistent_shader_draw_backing(&d->shader_storage,
  shader->storage_token,&geometry);if(error)return error;
 if(program.owner!=request.owner||program.generation!=shader->generation||
    geometry.owner!=program.owner||geometry.generation!=program.generation||
    !program.pinned||!program.staged||program.quarantined||
    !geometry.pinned||!geometry.staged||!program.handle||!geometry.handle||
    program.handle==geometry.handle||
    program.handle!=d->fragment_receipt.source_handle)return ESTALE;
 uint64_t total=program.allocation_bytes;
 if(geometry.allocation_bytes>UINT64_MAX-total)return EOVERFLOW;
 total+=geometry.allocation_bytes;
 uint64_t pages=0;
 for(unsigned i=0;i<10;i++) {
  const struct remesa_retained_binding *b=&d->fragment_job.bindings[i];
  if(b->id==program.handle||b->id==geometry.handle)return EINVAL;
  if(b->bytes>UINT64_MAX-total)return EOVERFLOW;
  total+=b->bytes;pages+=b->bytes/4096;
 }
 if(pages>UINT32_MAX)return EOVERFLOW;
 const uint32_t handles[2]={program.handle,geometry.handle};
 const uint64_t sizes[2]={program.allocation_bytes,geometry.allocation_bytes};
 for(unsigned i=0;i<2;i++) {
  struct remesa_memory_info info;
  error=remesa_linux_memory_info(d->shader_memory,handles[i],&info);if(error)return error;
  if(info.allocated_bytes!=total||info.objects!=12||info.pinned_objects!=12||
     info.mapped_pages!=pages||info.leases||info.held_jobs||info.quarantined||
     info.object_bytes!=sizes[i]||info.object_pinned!=1||info.gpu_va||
     info.object_mapped_pages||info.object_lease||info.verified_pages)return ESTALE;
 }
 struct remesa_linux_fragment_capture capture={.owner=program.owner,
  .generation=program.generation,.program_handle=program.handle,
  .geometry_handle=geometry.handle,.source_offset=d->fragment_receipt.source_offset,
  .program_bytes=program.allocation_bytes,.geometry_bytes=geometry.allocation_bytes,
  .program_changed=d->fragment_receipt.program_changed,.argb=fragment_color(d)};
 memcpy(capture.source,d->fragment_receipt.source,8);
 memcpy(capture.preimage,d->fragment_receipt.preimage,8);
 capture.vertex_staged=d->fragment_receipt.vertex_staged;
 capture.vertex_source_handle=d->fragment_receipt.vertex_source_handle;
 capture.vertex_source_offset=d->fragment_receipt.vertex_source_offset;
 capture.vertex_use_offset=d->fragment_receipt.vertex_use_offset;
 memcpy(capture.vertex_source,d->fragment_receipt.vertex_source,16);
 memcpy(capture.vertex_preimage,d->fragment_receipt.vertex_preimage,16);
 *out=capture;return 0;
}
static int fragment_capture_after(struct linux_device *d,struct linux_shader *shader,
 const struct remesa_linux_fragment_capture *before)
{
 struct remesa_linux_fragment_capture after;
 /* Only after submit_retained reports its bounded retired response. Read the
  * unmapped CPU program/geometry backing and INFO, never COLOR or extra MMIO.
  * The existing endpoint recorder captures the raw results before the trusted
  * authentication callback, which must not reenter this endpoint itself. */
 int error=remesa_persistent_shader_verify(&d->shader_storage,shader->storage_token);
 if(error)return error;
 for(unsigned i=0;i<10;i++) {
  const struct remesa_retained_binding *b=&d->fragment_job.bindings[i];
  error=remesa_linux_memory_validate_backing(d->shader_memory,b->id,b->va,(size_t)b->bytes);
  if(error)return error;
 }
 error=fragment_capture(d,shader,&after);if(error)return error;
 /* Preserve actual read-only carrier observations for the protected recorder.
  * This is after nominal retirement, before authentication/publication. Reads
  * do not establish physical origin; the adapter must corroborate them with
  * the independent observer and the same job. No expected pixels are used. */
 const struct remesa_persistent_ops *memory=remesa_linux_memory_ops();
 for(unsigned i=0;i<6;i++) {
  void *observed=NULL;
  error=memory->cpu_map(d->shader_memory,d->fragment_job.bindings[i].id,false,&observed);
  if(error) {
   if(observed)(void)memory->cpu_unmap(d->shader_memory,d->fragment_job.bindings[i].id,false);
   return error;
  }
  if(!observed)return EOWNERDEAD;
  error=memory->cpu_unmap(d->shader_memory,d->fragment_job.bindings[i].id,false);
  if(error)return error;
 }
 if(after.owner!=before->owner||after.generation!=before->generation||
    after.program_handle!=before->program_handle||
    after.geometry_handle!=before->geometry_handle||
    after.source_offset!=before->source_offset||after.argb!=before->argb||
    after.program_bytes!=before->program_bytes||
    after.geometry_bytes!=before->geometry_bytes||
    after.program_changed!=before->program_changed||
    memcmp(after.source,before->source,sizeof(after.source))||
    memcmp(after.preimage,before->preimage,sizeof(after.preimage))||
    after.vertex_staged!=before->vertex_staged||
    after.vertex_source_handle!=before->vertex_source_handle||
    after.vertex_source_offset!=before->vertex_source_offset||
    after.vertex_use_offset!=before->vertex_use_offset||
    memcmp(after.vertex_source,before->vertex_source,16)||
    memcmp(after.vertex_preimage,before->vertex_preimage,16))return ESTALE;
 return 0;
}
static int submit_fragment(struct linux_device *d,struct linux_bo *target,
 const struct remesa_draw_packet *packet,const struct remesa_shader_draw_inputs *inputs,
 struct linux_shader *shader,void **out)
{
 struct remesa_linux_fragment_evidence evidence={0};
 struct remesa_linux_fragment_capture capture;
 const struct remesa_persistent_ops *memory=remesa_linux_memory_ops();
 struct remesa_draw_plan geometry;
 struct remesa_shader_image_view view;
 struct linux_fence *f;void *color=NULL;
 int error=fragment_geometry(packet,fragment_color(d),d->fragment_carrier.gl_fbo_half_pixel_center);if(error)return error;
 if(d->fragment_prepared||d->attempted)return EBUSY;
 if(!shader->storage_token||target->size!=4096)return EINVAL;
 error=remesa_shader_draw_inputs_validate(inputs,&geometry);if(error)return error;
 error=remesa_shader_image_inspect(shader->image,&view);if(error)return error;
 const float positions[3][4]={{-.5f,-.5f,0,1},{.5f,-.5f,0,1},{-.5f,.5f,0,1}};
 const float quarter_positions[3][4]={{-.484375f,-.484375f,0,1},
  {.515625f,-.484375f,0,1},{-.484375f,.515625f,0,1}};
 const void *selected_positions=d->fragment_carrier.gl_fbo_half_pixel_center?
  (const void *)quarter_positions:(const void *)positions;
 const uint16_t indices[3]={0,1,2};
 if(!inputs->retained_fragment_carrier||inputs->preserve_color||inputs->position_input_slot!=view.programs->pipeline.position_input_slot||
    inputs->position_format!=PIPE_FORMAT_R32G32B32A32_FLOAT||
    inputs->element_offset||inputs->vertex_stride!=16||
    inputs->vertex_bytes<sizeof(positions)||
    inputs->count!=3||inputs->index_bias||
    inputs->color_width!=32||inputs->color_height!=32||inputs->color_stride!=128||
    inputs->color_bytes!=4096||inputs->cull_face||inputs->scissor_enabled)return ENOTSUP;
 /* Gallium uploaders suballocate VB/IB storage. Check the observed selected
  * ranges, not their padding or allocation origins. These are CPU byte/index
  * offsets; the retained GPU addresses, commands and geometry do not change.
  * Keep the full bounded prefixes in stage_draw for source authentication. */
 if(inputs->index_size==2) {
  if(inputs->index_bytes<sizeof(indices)||
     memcmp((const unsigned char *)inputs->index_data+(size_t)geometry.index_first_byte,
            indices,sizeof(indices)))return ENOTSUP;
 } else if(inputs->index_size!=0||inputs->start||inputs->index_data||inputs->index_bytes)
  return ENOTSUP;
 /* The non-indexed start0/count3 case selects exactly the same three
  * vertices as the admitted uint16 {0,1,2}. The packet above still has to
  * match the complete qualified geometry; no GPU packet or index storage
  * is synthesized from a historical program. stage_draw preserves the
  * actual absence of an index source rather than inventing an input BO. */
 if(inputs->vertex_offset>inputs->vertex_bytes||
    sizeof(positions)>inputs->vertex_bytes-inputs->vertex_offset)return EINVAL;
 if(memcmp((const unsigned char *)inputs->vertex_data+inputs->vertex_offset,
           selected_positions,sizeof(positions)))return ENOTSUP;
 for(unsigned c=0;c<3;c++)
  if(inputs->viewport.scale[c]!=(c==2?.5f:16.f)||
     inputs->viewport.translate[c]!=(c==2?.5f:16.f))return ENOTSUP;
 error=remesa_persistent_shader_stage_draw(&d->shader_storage,shader->storage_token,inputs);
 if(error)return error;
 f=calloc(1,sizeof(*f));if(!f)return ENOMEM;
 f->next=d->fences;d->fences=f;
 d->fragment_prepared=true;d->fragment_active=true;shader->fragment_held=true;
 target->readable=false;target->busy=true;
 error=fragment_stage(d,shader);
 if(error)goto done;
 if(d->fragment_carrier.authenticate_fragment) {
  error=fragment_capture(d,shader,&capture);if(error)goto done;
 }
 /* Only this endpoint's retained ioctl can issue. It borrows its existing
  * SGX owner; the diagnostic native ioctl/construct path is never called. */
 d->attempted=true;
 error=remesa_linux_memory_submit_retained(d->shader_memory,&d->fragment_job,
  &d->fragment_receipt.attempted,d->fragment_carrier.record,d->fragment_carrier.priv);
 if(error)goto done;
 if(d->fragment_carrier.authenticate_fragment) {
  error=fragment_capture_after(d,shader,&capture);if(error)goto done;
  error=d->fragment_carrier.authenticate_fragment(d->fragment_carrier.priv,
   &d->fragment_job,&capture,&evidence);
 }
 else error=d->fragment_carrier.authenticate(d->fragment_carrier.priv,&d->fragment_job,&evidence);
 if(error)goto done;
 const struct remesa_evidence_id *id=&evidence.expected.id;
 if(id->context!=d->fragment_job.context||id->operation!=d->fragment_job.operation||
    id->serial!=d->fragment_job.serial||id->slot!=d->fragment_job.slot||
    id->generation!=d->fragment_job.generation||evidence.expected.resources!=10||
    remesa_retained_completion_verify(&evidence.expected,&evidence.response,
      &evidence.observer,&evidence.ownership)!=REMESA_EVIDENCE_OK)
  {error=EPROTO;goto done;}
 /* Selected retained response is 44 bytes of lifecycle/ticket +4096 COLOR.
  * Reject another record format; never manufacture output from semantic color. */
 if(d->fragment_job.observer_response_bytes!=4140){error=EPROTO;goto done;}
 const struct remesa_retained_binding *output=&d->fragment_job.bindings[5];
 error=remesa_linux_memory_validate_backing(d->shader_memory,output->id,output->va,4096);
 if(error)goto done;
 error=memory->cpu_map(d->shader_memory,output->id,false,&color);
 if(error){if(color)(void)memory->cpu_unmap(d->shader_memory,output->id,false);color=NULL;goto done;}
 if(!color){error=EOWNERDEAD;goto done;}
 if(memcmp(color,d->fragment_job.observer_response+44,4096))error=EOWNERDEAD;
 if(!error)memcpy(target->bytes,color,4096);
 if(memory->cpu_unmap(d->shader_memory,output->id,false))error=EOWNERDEAD;
 color=NULL;
 if(!error){d->fragment_receipt.authenticated=true;target->readable=true;f->published=true;}
done:
 if(error && (d->fragment_receipt.attempted||error==EOWNERDEAD))
  d->fragment_receipt.quarantined=true;
 if(!d->fragment_receipt.quarantined)shader->fragment_held=false;
 d->fragment_active=false;target->busy=false;target->failed=error!=0;f->error=error;*out=f;return error;
}
static int submit_shader_programs(void *priv,void *bo,const struct remesa_draw_packet *packet,
 const struct remesa_shader_draw_inputs *inputs,void *token,void **out)
{
 struct linux_device *d=priv;
 if(!out)return EINVAL;
 if(d) {
  int error=endpoint_output_check(d,out,sizeof(*out));
  if(error)return error;
 }
 *out=NULL;
 if(!bo||!packet||!inputs)return EINVAL;
 struct linux_shader **link=shader_link(d,token);
 if(!link)return EINVAL;
 if((*link)->storage_token) {
  struct remesa_shader_request request;
  struct remesa_shader_bindings bindings;
  int error=remesa_persistent_shader_bindings(&d->shader_storage,
   (*link)->storage_token,&request,&bindings);
  if(error)return error;
  if(memcmp(&request,&(*link)->request,sizeof(request)) ||
     memcmp(&bindings,&(*link)->bindings,sizeof(bindings)))return ESTALE;
 }
 /* Supplied image tokens are an explicit CPU staging operation. Existing
  * laboratory carrier admission cannot promote this newly supplied profile. */
 if((*link)->supplied_cpu_image)return ENOTSUP;
 if(d->fragment_enabled)
  return submit_fragment(d,bo,packet,inputs,*link,out);
 /* Normal original-program execution stays gated before target memory or
  * syscalls. Only explicit FS-stage carrier mode uses the retained endpoint. */
 return remesa_shader_image_validate_execution((*link)->image);
}
static void destroy_shader_programs(void *priv,void *token)
{
 struct linux_device *d=priv;
 struct linux_shader **link=shader_link(d,token);
 /* The void Gallium cleanup callback can abandon its token, but an active or
  * uncertain retained operation still owns the complete source inventory.
  * Checked close already refuses this endpoint until retirement is known. */
 if(link && (*link)->fragment_held)return;
 if(link){struct linux_shader *shader=*link;*link=shader->next;
  if(shader->storage_token)d->shader_storage.ops->destroy_shader_programs(
   d->shader_storage.priv,shader->storage_token);
  for(unsigned i=0;i<2;i++)remesa_shader_storage_reference(&shader->pds_host_data[i],NULL);
  remesa_shader_image_destroy(shader->image);free(shader);}
}
int remesa_linux_request_build(const struct remesa_draw_packet *p,struct remesa_native_draw *r)
{
 struct remesa_native_draw v={0};
 if(!r || !p) return EINVAL;
 if(p->vertex_count>REMESA_NATIVE_MAX_VERTICES || p->index_count>REMESA_NATIVE_MAX_INDICES) return ENOTSUP;
 if(remesa_draw_packet_validate(p)!=REMESA_PACKET_OK) return EINVAL;
 v.version=REMESA_NATIVE_ABI_VERSION;v.width=p->width;v.height=p->height;
 v.vertex_count=p->vertex_count;v.index_count=p->index_count;v.argb=p->argb;
 memcpy(v.positions,p->positions,p->vertex_count*16);memcpy(v.indices,p->indices,p->index_count*2);
 *r=v;return 0;
}
static int allocate(void *priv,size_t size,void **out)
{
 struct linux_bo *b;
 struct linux_device *d=priv;
 if(!out)return EINVAL;
 if(d) {
  int error=endpoint_output_check(d,out,sizeof(*out));
  if(error)return error;
 }
 *out=NULL;
 if(!size || size>1024*1024) return EINVAL;
 b=calloc(1,sizeof(*b));if(!b) return ENOMEM;
 b->bytes=calloc(1,size);if(!b->bytes) { free(b);return ENOMEM; }
 b->size=size;*out=b;return 0;
}
static void destroy_bo(void *priv,void *bo)
{ struct linux_bo *b=bo;(void)priv;if(b) { free(b->bytes);free(b); } }
static int map(void *priv,void *bo,bool write,void **out)
{
 struct linux_bo *b=bo;struct linux_device *d=priv;
 if(!b || !out)return EINVAL;
 if(d) {
  int error=endpoint_output_check(d,out,sizeof(*out));
  if(error)return error;
 }
 *out=NULL;
 if(b->failed) return EIO;
 if(b->busy || (!write && !b->readable)) return EAGAIN;
 *out=b->bytes;return 0;
}
static void unmap(void *priv,void *bo,bool write)
{ struct linux_bo *b=bo;(void)priv;if(b && write) b->readable=true; }
static int submit(void *priv,void *bo,const struct remesa_draw_packet *p,void **out)
{
 struct linux_device *d=priv;struct linux_bo *b=bo;
 struct remesa_native_draw request,original;struct linux_fence *f;int error;
 if(!d || !b || !out) return EINVAL;
 error=endpoint_output_check(d,out,sizeof(*out));
 if(error)return error;
 *out=NULL;
 if(d->fragment_enabled)return ENOTSUP;
 if(d->attempted) return EBUSY;
 if(b->size!=REMESA_NATIVE_COLOR_BYTES) return EINVAL;
 error=remesa_linux_request_build(p,&request);if(error) return error;
 f=calloc(1,sizeof(*f));if(!f) return ENOMEM;
 f->next=d->fences;d->fences=f;
 original=request;b->readable=false;b->busy=true;
 /* Consume before syscall. EINTR, EFAULT and all other errors never retry. */
 d->attempted=true;d->receipt.attempted=1;
 error=ioctl(d->fd,DRM_IOCTL_REMESA_NATIVE_DRAW,&request);
 d->response=request;d->receipt.ioctl_errno=error<0 ? errno : 0;
 d->receipt.operation=request.operation;d->receipt.operation_errno=request.operation_errno;
 d->receipt.phase=request.phase;d->receipt.events=request.events;
 d->receipt.issued=request.issued;d->receipt.cpu_published=request.published;
 if(error<0) f->error=d->receipt.ioctl_errno;
 else if(error!=0) f->error=EPROTO;
 else if(memcmp(&request,&original,offsetof(struct remesa_native_draw,operation)) ||
         request.operation!=1 || request.operation_errno ||
         request.status!=REMESA_NATIVE_STATUS_COMPLETED || request.phase!=9 ||
         request.events!=7 || request.issued!=1 || request.published!=1)
  f->error=request.operation_errno ? EIO : EPROTO;
 else { memcpy(b->bytes,request.color,sizeof(request.color));b->readable=true;f->published=true; }
 b->failed=f->error!=0;b->busy=false;*out=f;return f->error;
}
/* Membership is checked before dereferencing a caller's opaque fence. A
 * foreign/stale handle cannot authenticate another endpoint or consume its
 * completion object through the void destructor. */
static struct linux_fence **fence_link(struct linux_device *d,const void *fence)
{
 if(!d||!fence)return NULL;
 for(struct linux_fence **link=&d->fences;*link;link=&(*link)->next)
  if(*link==fence)return link;
 return NULL;
}
static int wait(void *priv,void *fence,uint64_t ns,bool visible)
{
 struct linux_fence **link=fence_link(priv,fence);(void)ns;
 if(!link)return EINVAL;
 struct linux_fence *f=*link;
 return f->error?f->error:visible&&!f->published?EAGAIN:0;
}
static void destroy_fence(void *priv,void *fence)
{
 struct linux_fence **link=fence_link(priv,fence);
 if(link){struct linux_fence *f=*link;*link=f->next;free(f);}
}
static const struct remesa_device_ops ops={
 .allocate=allocate,.destroy_bo=destroy_bo,.map=map,.unmap=unmap,
 .submit=submit,.wait=wait,.destroy_fence=destroy_fence,
 .prepare_shader_programs=prepare_shader_programs,
 .submit_shader_programs=submit_shader_programs,
 .destroy_shader_programs=destroy_shader_programs,
};
int remesa_linux_prepare_vertex_candidate(struct remesa_device *device,
 const struct remesa_shader_draw_programs *programs,
 const struct remesa_usse_vertex_mapping *mapping,void **out)
{
 if(!device||device->ops!=&ops||!device->priv||!programs||!mapping||!out)return EINVAL;
 int error=shader_output_check(device,out,sizeof(*out));if(error)return error;
 if(overlaps(out,sizeof(*out),programs,sizeof(*programs))||
    overlaps(out,sizeof(*out),mapping,sizeof(*mapping)))return EINVAL;
 if(*out)return EBUSY;
 return prepare_candidate(device->priv,programs,mapping,NULL,out);
}
int remesa_linux_prepare_image(struct remesa_device *device,
 const struct remesa_shader_image *image,void **out)
{
 if(!device||device->ops!=&ops||!device->priv||!image||!out)return EINVAL;
 int error=shader_output_check(device,out,sizeof(*out));if(error)return error;
 struct linux_device *d=device->priv;
 if(!d->shader_storage.ops)return ENODATA;
 struct remesa_shader_image_view view;
 error=remesa_shader_image_inspect(image,&view);if(error)return error;
 if(overlaps(out,sizeof(*out),view.programs,sizeof(*view.programs)))return EINVAL;
 const struct remesa_shader_storage_view *payloads[4]={&view.vertex,&view.fragment,
  &view.fragment_pds,&view.fragment_pds_mask};
 for(unsigned i=0;i<4;i++)
  if(overlaps(out,sizeof(*out),payloads[i]->bytes,payloads[i]->byte_length))return EINVAL;
 if(*out)return EBUSY;
 return prepare_candidate(device->priv,view.programs,NULL,image,out);
}
int remesa_linux_set_vertex_candidate_mapping(struct remesa_device *device,
 const struct remesa_usse_vertex_mapping *mapping)
{
 if(!device||device->ops!=&ops||!device->priv||!mapping)return EINVAL;
 struct linux_device *d=device->priv;
 if(overlaps(mapping,sizeof(*mapping),device,sizeof(*device))||
    overlaps(mapping,sizeof(*mapping),d,sizeof(*d)))return EINVAL;
 /* Freeze the profile before its first retained candidate. Old requests and
  * live tokens must not acquire a different numeric assignment retroactively. */
 if(d->shader_generation||d->shaders)return EBUSY;
 int error=remesa_usse_vertex_mapping_validate(mapping);if(error)return error;
 d->vertex_candidate_mapping=*mapping;d->vertex_candidate_configured=true;return 0;
}
static int admit_fd(struct remesa_device *device,int fd)
{
 struct linux_device *d;char name[32]={0};struct drm_version v={0};int error;
 v.name_len=sizeof(name)-1;v.name=name;
 int result=ioctl(fd,DRM_IOCTL_VERSION,&v);
 if(result<0) error=errno;
 else if(result) error=EPROTO;
 else error=v.name_len==strlen("gma500") && !memcmp(name,"gma500",v.name_len) ? 0 : ENODEV;
 if(error) { close(fd);return error; }
 d=calloc(1,sizeof(*d));if(!d) { close(fd);return ENOMEM; }
 d->fd=fd;device->ops=&ops;device->priv=d;device->qualified_hardware_backend=true;return 0;
}
static bool empty_device(const struct remesa_device *d)
{ return !d->ops && !d->priv && !d->offline_test_backend && !d->qualified_hardware_backend && !d->resource_only_backend && !d->retained_fragment_carrier && !d->retained_gl_fbo_half_pixel_center && d->framebuffer_mode==REMESA_FRAMEBUFFER_REPLACE; }
int remesa_linux_open(struct remesa_device *device,const char *node)
{
 int fd;
 if(!device || !node) return EINVAL;
 if(!empty_device(device)) return EBUSY;
 fd=open(node,O_RDWR|O_CLOEXEC);if(fd<0) return errno;
 return admit_fd(device,fd);
}
int remesa_linux_open_fd(struct remesa_device *device,int borrowed_fd)
{
 int fd,flags;
 if(!device || borrowed_fd<0) return EINVAL;
 if(!empty_device(device)) return EBUSY;
 flags=fcntl(borrowed_fd,F_GETFL);
 if(flags<0) return errno;
 if((flags&O_ACCMODE)!=O_RDWR) return EACCES;
 fd=fcntl(borrowed_fd,F_DUPFD_CLOEXEC,0);if(fd<0) return errno;
 return admit_fd(device,fd);
}
int remesa_linux_enable_shader_staging(struct remesa_device *device,
 const struct remesa_persistent_config *cfg)
{
 if(!device || device->ops!=&ops || !device->priv || !cfg ||
    !cfg->resource_only_backend || cfg->offline_test_backend || cfg->qualified_hardware_backend)
  return EINVAL;
 struct linux_device *d=device->priv;
 if(d->attempted || d->shader_generation || d->shader_storage.ops)return EBUSY;
 int error=remesa_persistent_open(&d->shader_storage,cfg);
 if(!error && cfg->ops==remesa_linux_memory_ops())d->shader_memory=cfg->priv;
 return error;
}
int remesa_linux_enable_retained_fragment(struct remesa_device *device,
 const struct remesa_linux_fragment_carrier *config)
{
 if(!device||device->ops!=&ops||!device->priv||!config||!config->job||
    !config->memory||!config->record||
    (!!config->authenticate==!!config->authenticate_fragment))return EINVAL;
 struct linux_device *d=device->priv;
 if(d->fragment_enabled||d->attempted||d->shader_generation)return EBUSY;
 if(config->fragment_argb&&config->fragment_argb!=UINT32_C(0xffff00ff)&&
    config->fragment_argb!=UINT32_C(0xff00ff00))return ENOTSUP;
 if(config->fragment_argb==UINT32_C(0xff00ff00)&&
    (!config->authenticate_fragment||!config->require_program_change))return ENOTSUP;
 if(config->gl_fbo_half_pixel_center &&
    (!config->independent_vertex || config->fragment_argb!=UINT32_C(0xff00ff00) ||
     !config->authenticate_fragment || !config->require_program_change))return ENOTSUP;
 if(config->independent_vertex) {
  if(config->fragment_argb!=UINT32_C(0xff00ff00)||!config->authenticate_fragment||
     !d->vertex_candidate_configured)return ENOTSUP;
  for(unsigned c=0;c<4;c++)
   if(d->vertex_candidate_mapping.input[c].class_id!=2||
      d->vertex_candidate_mapping.input[c].number!=c||
      d->vertex_candidate_mapping.output[c].class_id!=1||
      d->vertex_candidate_mapping.output[c].number!=c)return ENOTSUP;
 }
 if(d->shader_memory!=config->memory||!d->shader_storage.ops)return EINVAL;
 struct remesa_retained_request job=*config->job;
 int error=remesa_retained_request_validate(&job);if(error)return error;
 if(job.version!=1||job.flags||job.context!=remesa_linux_memory_context(config->memory))return EINVAL;
 /* Allocation metadata of the existing exact profile, not instruction fields. */
 const uint64_t sizes[10]={0x20000,0x8000,0x1000,0x1000,0x1000,0x1000,
  0x2000,0x2000000,0x2000000,0x1000};
 for(unsigned i=0;i<10;i++)if(job.bindings[i].bytes!=sizes[i])return ENOTSUP;
 if(job.bindings[0].va%4096||job.bindings[1].va%32768)return EINVAL;
 d->fragment_carrier=*config;d->fragment_carrier.job=NULL;d->fragment_job=job;
 d->fragment_enabled=true;device->retained_fragment_carrier=true;
 device->retained_gl_fbo_half_pixel_center=config->gl_fbo_half_pixel_center;
 return 0;
}
int remesa_linux_last_retained_fragment(const struct remesa_device *device,
 struct remesa_linux_fragment_receipt *out)
{
 if(!device||device->ops!=&ops||!device->priv||!out)return EINVAL;
 int error=shader_output_check(device,out,sizeof(*out));if(error)return error;
 const struct linux_device *d=device->priv;
 if(!d->fragment_prepared)return EAGAIN;
 *out=d->fragment_receipt;return 0;
}
int remesa_linux_close_checked(struct remesa_device *device)
{
 if(!device || device->ops!=&ops || !device->priv)return EINVAL;
 struct linux_device *d=device->priv;
 if(d->fragment_active||d->fragment_receipt.quarantined||d->fences)return EBUSY;
 while(d->shaders)destroy_shader_programs(d,d->shaders->token);
 if(d->shader_storage.ops) {
  int error=remesa_persistent_close(&d->shader_storage);
  if(error)return error;
 }
 remesa_shader_cookie_pool_finish(&d->shader_cookies);
 close(d->fd);free(d);memset(device,0,sizeof(*device));return 0;
}
void remesa_linux_close(struct remesa_device *device)
{ if(device && device->ops==&ops)(void)remesa_linux_close_checked(device); }
int remesa_linux_last_shader_request(const struct remesa_device *device,struct remesa_shader_request *r)
{
 if(!device||device->ops!=&ops||!device->priv||!r)return EINVAL;
 struct linux_device *d=device->priv;
 if(!d->shader_candidate_valid)return EAGAIN;
 int error=shader_output_check(device,r,sizeof(*r));if(error)return error;
 *r=d->shader_candidate;return 0;
}
int remesa_linux_shader_bindings(const struct remesa_device *device,void *token,
 struct remesa_shader_request *request,struct remesa_shader_bindings *bindings)
{
 if(!device || device->ops!=&ops || !device->priv || !request || !bindings)return EINVAL;
 struct linux_device *d=device->priv;
 struct linux_shader **link=shader_link(d,token);
 if(!link)return EINVAL;
 struct linux_shader *shader=*link;
 if(!shader->storage_token)return ENOTSUP;
 if(overlaps(request,sizeof(*request),bindings,sizeof(*bindings)))return EINVAL;
 int error=shader_output_check(device,request,sizeof(*request));if(error)return error;
 error=shader_output_check(device,bindings,sizeof(*bindings));if(error)return error;
 struct remesa_shader_request r;
 struct remesa_shader_bindings b;
 error=remesa_persistent_shader_bindings(&d->shader_storage,shader->storage_token,&r,&b);
 if(error)return error;
 if(memcmp(&r,&shader->request,sizeof(r)) || memcmp(&b,&shader->bindings,sizeof(b)))return ESTALE;
 /* The second metadata snapshot checks caller outputs against the nested
  * owner's storage too. External serialization keeps both snapshots stable. */
 return remesa_persistent_shader_bindings(&d->shader_storage,shader->storage_token,request,bindings);
}
int remesa_linux_shader_verify(struct remesa_device *device,void *token)
{
 struct remesa_shader_request request;
 struct remesa_shader_bindings bindings;
 int error=remesa_linux_shader_bindings(device,token,&request,&bindings);if(error)return error;
 struct linux_device *d=device->priv;
 struct linux_shader *shader=*shader_link(d,token);
 if(shader->pds_host_data[0]||shader->pds_host_data[1]) {
  /* Verify the owned host relocation before touching provider backing. These
   * retained CPU bindings are not fresh hardware VA/MMU observations. */
  struct remesa_pds_request current[2]={shader->pds_links.launch.stage[0].request,
   shader->pds_links.launch.stage[1].request};
  struct remesa_pds_host_data_view data[2];
  error=remesa_linux_shader_host_pds_data(device,token,
   shader->pds_links.use[0].program.address,current,data);if(error)return error;
 }
 return remesa_persistent_shader_verify(&d->shader_storage,shader->storage_token);
}
int remesa_linux_shader_stage_draw(struct remesa_device *device,void *token,
 const struct remesa_shader_draw_inputs *inputs)
{
 struct remesa_shader_request request;
 struct remesa_shader_bindings bindings;
 int error=remesa_linux_shader_bindings(device,token,&request,&bindings);
 if(error)return error;
 struct linux_device *d=device->priv;
 struct linux_shader *shader=*shader_link(d,token);
 struct remesa_shader_image_view view;
 error=remesa_shader_image_inspect(shader->image,&view);if(error)return error;
 if(!inputs||inputs->position_input_slot!=view.programs->pipeline.position_input_slot)
  return EINVAL;
 if(inputs->preserve_color&&device->framebuffer_mode!=REMESA_FRAMEBUFFER_PRESERVE)
  return ENOTSUP;
 struct remesa_draw_plan validated;
 error=remesa_shader_draw_inputs_validate(inputs,&validated);if(error)return error;
 /* The resource owner keeps the single immutable raw snapshot. No duplicate
  * CPU copy and no independent replacement lifetime exists in this wrapper. */
 return remesa_persistent_shader_stage_draw(&d->shader_storage,
   shader->storage_token,inputs);
}
int remesa_linux_shader_draw_snapshot(const struct remesa_device *device,void *token,
 struct remesa_shader_draw_inputs *out,struct remesa_draw_plan *plan)
{
 struct remesa_shader_request request;
 struct remesa_shader_bindings bindings;
 int error=remesa_linux_shader_bindings(device,token,&request,&bindings);
 if(error)return error;
 struct linux_device *d=device->priv;
 struct linux_shader *shader=*shader_link(d,token);
 if(!out||!plan)return EINVAL;
 error=shader_output_check(device,out,sizeof(*out));if(error)return error;
 error=shader_output_check(device,plan,sizeof(*plan));if(error)return error;
 return remesa_persistent_shader_draw_snapshot(&d->shader_storage,
   shader->storage_token,out,plan);
}

int remesa_linux_shader_draw_backing(const struct remesa_device *device,void *token,
 struct remesa_persistent_draw_backing *out)
{
 struct remesa_shader_request request;
 struct remesa_shader_bindings bindings;
 int error=remesa_linux_shader_bindings(device,token,&request,&bindings);
 if(error)return error;
 if(!out)return EINVAL;
 struct linux_device *d=device->priv;
 struct linux_shader *shader=*shader_link(d,token);
 error=shader_output_check(device,out,sizeof(*out));if(error)return error;
 return remesa_persistent_shader_draw_backing(&d->shader_storage,
   shader->storage_token,out);
}
int remesa_linux_shader_preview_stage_entry(const struct remesa_device *device,void *token,
 uint32_t stage,uint64_t allocation_address,uint32_t bank,struct remesa_use_entry_plan *out)
{
 if(!device||device->ops!=&ops||!device->priv||!out)return EINVAL;
 struct linux_device *d=device->priv;
 struct linux_shader **link=shader_link(d,token);
 if(!link)return EINVAL;
 int error=shader_output_check(device,out,sizeof(*out));if(error)return error;
 struct remesa_shader_request request;
 struct remesa_shader_bindings bindings;
 error=remesa_linux_shader_bindings(device,token,&request,&bindings);
 if(error)return error;
 if(stage!=REMESA_IR_VERTEX&&stage!=REMESA_IR_FRAGMENT)return ENOTSUP;
 unsigned slot=stage==REMESA_IR_VERTEX?REMESA_SHADER_VERTEX_USSE-1:REMESA_SHADER_FRAGMENT_USSE-1;
 if(!(bindings.resource_mask&(1u<<slot)))return ENOTSUP;
 const struct remesa_shader_binding *program=&bindings.resources[slot];
 const struct remesa_use_entry_request entry={
  .allocation_address=allocation_address,.allocation_bytes=program->allocation_bytes,
  .entry_offset=program->offset,.program_bytes=program->length,
  .bank=bank,.data_master=stage==REMESA_IR_VERTEX?REMESA_USE_VERTEX:REMESA_USE_PIXEL
 };
 return remesa_shader_image_preview_use_entry((*link)->image,&entry,out);
}
int remesa_linux_shader_preview_use_entry(const struct remesa_device *device,void *token,
 uint64_t allocation_address,uint32_t bank,struct remesa_use_entry_plan *out)
{return remesa_linux_shader_preview_stage_entry(device,token,REMESA_IR_FRAGMENT,
 allocation_address,bank,out);}
int remesa_linux_shader_preview_entry_pair(const struct remesa_device *device,void *token,
 uint64_t allocation_address,const uint32_t banks[2],struct remesa_use_entry_plan *entries)
{
 if(!device||device->ops!=&ops||!device->priv||!banks||!entries||
    sizeof(*banks)*2>UINTPTR_MAX-(uintptr_t)banks)return EINVAL;
 struct linux_device *d=device->priv;
 struct linux_shader **link=shader_link(d,token);
 if(!link)return EINVAL;
 int error=shader_output_check(device,entries,sizeof(*entries)*2);if(error)return error;
 const uint32_t bank[2]={banks[0],banks[1]};
 struct remesa_shader_request request;
 struct remesa_shader_bindings bindings;
 error=remesa_linux_shader_bindings(device,token,&request,&bindings);
 if(error)return error;
 error=remesa_shader_image_validate_request((*link)->image,&request);if(error)return error;
 const unsigned slots[2]={REMESA_SHADER_VERTEX_USSE-1,REMESA_SHADER_FRAGMENT_USSE-1};
 if((bindings.resource_mask&3u)!=3u)return ENOTSUP;
 /* Current staging places both candidates in one allocation. A future
  * split-allocation path must supply separate authenticated mappings. */
 if(bindings.resources[slots[0]].handle!=bindings.resources[slots[1]].handle)
  return ENOTSUP;
 struct remesa_use_entry_request entry[2]={{0}};
 for(unsigned i=0;i<2;i++) {
  const struct remesa_shader_binding *program=&bindings.resources[slots[i]];
  entry[i]=(struct remesa_use_entry_request){
   .allocation_address=allocation_address,.allocation_bytes=program->allocation_bytes,
   .entry_offset=program->offset,.program_bytes=program->length,.bank=bank[i],
   .data_master=i==0?REMESA_USE_VERTEX:REMESA_USE_PIXEL
  };
 }
 struct remesa_use_entry_plan plan[2];
 error=remesa_use_entry_pair_prepare(entry,plan);if(error)return error;
 entries[0]=plan[0];entries[1]=plan[1];return 0;
}
int remesa_linux_shader_preview_pds_links(const struct remesa_device *device,void *token,
 uint64_t allocation_address,const uint32_t banks[2],const struct remesa_pds_request *requests,
 const uint64_t offsets[2],struct remesa_shader_pds_links *out)
{
 if(!device||device->ops!=&ops||!device->priv||!banks||!requests||!offsets||!out)return EINVAL;
 struct linux_device *d=device->priv;
 struct linux_shader **link=shader_link(d,token);if(!link)return EINVAL;
 int error=shader_output_check(device,out,sizeof(*out));if(error)return error;
 if(overlaps(out,sizeof(*out),banks,sizeof(*banks)*2)||
    overlaps(out,sizeof(*out),requests,sizeof(*requests)*2)||
    overlaps(out,sizeof(*out),offsets,sizeof(*offsets)*2))return EINVAL;
 struct remesa_shader_request request;
 struct remesa_shader_bindings bindings;
 error=remesa_linux_shader_bindings(device,token,&request,&bindings);if(error)return error;
 error=remesa_shader_image_validate_request((*link)->image,&request);if(error)return error;
 if((bindings.resource_mask&3u)!=3u)return ENOTSUP;
 struct remesa_shader_image_view view;
 error=remesa_shader_image_inspect((*link)->image,&view);if(error)return error;
 struct remesa_pds_binding programs[2];
 for(unsigned i=0;i<2;i++) {
  if(requests[i].owner_cookie!=request.owner||requests[i].generation!=request.generation)
   return ESTALE;
  const struct remesa_shader_binding *b=&bindings.resources[i];
  /* This generation names the immutable CPU backing, not an observed GTT
   * mapping epoch. allocation_address remains explicitly symbolic. */
  programs[i]=(struct remesa_pds_binding){.allocation_cookie=b->handle,
   .owner_cookie=bindings.owner,.mapping_generation=bindings.generation,
   .address=allocation_address,.offset=b->offset,.bytes=b->length,
   .allocation_bytes=b->allocation_bytes};
 }
 struct remesa_shader_pds_links result={0};
 error=remesa_shader_pipeline_prepare_launch(&view.programs->vertex,&view.programs->fragment,
  &requests[0],&requests[1],&result.launch);if(error)return error;
 error=remesa_pds_prepare_use_links(result.launch.stage,programs,banks,offsets,result.use);
 if(error)return error;
 *out=result;return 0;
}
int remesa_linux_shader_revalidate_pds_links(const struct remesa_device *device,void *token,
 uint64_t allocation_address,const struct remesa_pds_request *requests,
 const struct remesa_shader_pds_links *links)
{
 if(!device||device->ops!=&ops||!device->priv||!shader_link(device->priv,token))return EINVAL;
 if(!links||sizeof(*links)>UINTPTR_MAX-(uintptr_t)links)return EINVAL;
 const uint32_t banks[2]={links->use[0].entry.bank,links->use[1].entry.bank};
 const uint64_t offsets[2]={links->use[0].data_byte_offset,links->use[1].data_byte_offset};
 struct remesa_shader_pds_links current;
 int error=remesa_linux_shader_preview_pds_links(device,token,allocation_address,banks,
  requests,offsets,&current);if(error)return error;
 struct linux_device *d=device->priv;
 struct remesa_shader_image_view view;
 error=remesa_shader_image_inspect((*shader_link(d,token))->image,&view);if(error)return error;
 error=remesa_shader_pipeline_revalidate_launch(&links->launch,&view.programs->vertex,
  &view.programs->fragment,&requests[0],&requests[1]);if(error)return error;
 const struct remesa_pds_binding programs[2]={current.use[0].program,current.use[1].program};
 return remesa_pds_validate_use_links(current.launch.stage,programs,links->use);
}
int remesa_linux_shader_retain_pds_links(const struct remesa_device *device,void *token,
 uint64_t allocation_address,const uint32_t banks[2],const struct remesa_pds_request *requests,
 const uint64_t offsets[2])
{
 /* Build both stages before publishing anything. The original preview checks
  * the live backing, exact program lengths and every symbolic allocation. */
 struct remesa_shader_pds_links result;
 int error=remesa_linux_shader_preview_pds_links(device,token,allocation_address,
  banks,requests,offsets,&result);if(error)return error;
 struct linux_shader *shader=*shader_link(device->priv,token);
 if(shader->pds_links_retained)return EBUSY;
 shader->pds_links=result;shader->pds_links_retained=true;return 0;
}
int remesa_linux_shader_retained_pds_links(const struct remesa_device *device,void *token,
 uint64_t allocation_address,const struct remesa_pds_request *current,
 struct remesa_shader_pds_links *out)
{
 if(!device||device->ops!=&ops||!device->priv||!current||!out)return EINVAL;
 struct linux_shader **link=shader_link(device->priv,token);if(!link)return EINVAL;
 int error=shader_output_check(device,out,sizeof(*out));if(error)return error;
 if(overlaps(out,sizeof(*out),current,sizeof(*current)*2))return EINVAL;
 if(!(*link)->pds_links_retained)return EAGAIN;
 /* The retained plan is not its own freshness witness: callers supply fresh
  * metadata; the live-token path checks backing identity and program image.
  * These host checks still do not authenticate PDS CODE/DATA GPU mappings. */
 error=remesa_linux_shader_revalidate_pds_links(device,token,allocation_address,
  current,&(*link)->pds_links);if(error)return error;
 *out=(*link)->pds_links;return 0;
}
int remesa_linux_shader_preview_pds_records(const struct remesa_device *device,void *token,
 uint64_t allocation_address,const struct remesa_pds_request *current,
 struct remesa_pds_record_preview *out)
{
 if(!device||device->ops!=&ops||!device->priv||!current||!out)return EINVAL;
 if(overlaps(out,2*sizeof(*out),current,2*sizeof(*current)))return EINVAL;
 int error=shader_output_check(device,out,2*sizeof(*out));if(error)return error;
 struct remesa_shader_pds_links links;
 error=remesa_linux_shader_retained_pds_links(device,token,allocation_address,current,&links);
 if(error)return error;
 const struct remesa_pds_binding programs[2]={links.use[0].program,links.use[1].program};
 struct remesa_pds_use_link use[2]={links.use[0],links.use[1]};
 return remesa_pds_preview_record_pair(links.launch.stage,programs,use,out);
}
int remesa_linux_shader_apply_host_pds_fixups(const struct remesa_device *device,void *token,
 uint64_t allocation_address,const struct remesa_pds_request *current,
 const struct remesa_pds_host_data *data)
{
 if(!device||device->ops!=&ops||!device->priv||!current||!data||
    2*sizeof(*data)>UINTPTR_MAX-(uintptr_t)data)return EINVAL;
 struct remesa_shader_pds_links links;
 int error=remesa_linux_shader_retained_pds_links(device,token,allocation_address,current,&links);
 if(error)return error;
 for(unsigned i=0;i<2;i++) {
  error=shader_output_check(device,data[i].bytes,data[i].byte_length);if(error)return error;
  if(overlaps(data[i].bytes,data[i].byte_length,current,2*sizeof(*current)))return EINVAL;
 }
 const struct remesa_pds_binding programs[2]={links.use[0].program,links.use[1].program};
 const struct remesa_pds_use_link use[2]={links.use[0],links.use[1]};
 return remesa_pds_apply_host_entry_fixups(links.launch.stage,programs,use,data);
}
int remesa_linux_shader_retain_host_pds_data(const struct remesa_device *device,void *token,
 uint64_t allocation_address,const struct remesa_pds_request *current,
 const struct remesa_pds_host_data *data)
{
 if(!device||device->ops!=&ops||!device->priv||!current||!data||
    2*sizeof(*data)>UINTPTR_MAX-(uintptr_t)data)return EINVAL;
 struct remesa_shader_pds_links links;
 int error=remesa_linux_shader_retained_pds_links(device,token,allocation_address,current,&links);
 if(error)return error;
 const struct remesa_pds_binding programs[2]={links.use[0].program,links.use[1].program};
 const struct remesa_pds_use_link use[2]={links.use[0],links.use[1]};
 /* The retained live links already authenticate the host plan and USE
  * ranges. DATA ownership/relocation does not require contiguous T+8 CODE.
  * Execution and descriptor projection remain separately gated. */
 struct linux_shader *shader=*shader_link(device->priv,token);
 if(shader->pds_host_data[0]||shader->pds_host_data[1])return EBUSY;
 unsigned char bytes[2][1024];struct remesa_pds_host_data copy[2];
 for(unsigned i=0;i<2;i++) {
  if(!data[i].bytes||data[i].byte_length!=current[i].data.bytes||
     !data[i].byte_length||data[i].byte_length>sizeof(bytes[i]))return EINVAL;
  error=shader_output_check(device,data[i].bytes,data[i].byte_length);if(error)return error;
  memcpy(bytes[i],data[i].bytes,data[i].byte_length);
  copy[i]=data[i];copy[i].bytes=bytes[i];
 }
 error=remesa_pds_apply_host_entry_fixups(links.launch.stage,programs,use,copy);if(error)return error;
 struct remesa_shader_storage *storage[2]={NULL,NULL};
 for(unsigned i=0;i<2;i++) {
  const struct remesa_shader_storage_request request={.stage=i?REMESA_IR_FRAGMENT:REMESA_IR_VERTEX,
   .format=REMESA_SHADER_STORAGE_HOST_PDS_DATA,.bytes=bytes[i],
   .byte_length=copy[i].byte_length,.byte_alignment=16};
  error=remesa_shader_storage_create(&request,&storage[i]);
  if(error) {
   for(unsigned j=0;j<2;j++)remesa_shader_storage_reference(&storage[j],NULL);
   return error;
  }
 }
 shader->pds_host_data[0]=storage[0];shader->pds_host_data[1]=storage[1];return 0;
}
int remesa_linux_shader_host_pds_data(const struct remesa_device *device,void *token,
 uint64_t allocation_address,const struct remesa_pds_request *current,
 struct remesa_pds_host_data_view *out)
{
 if(!device||device->ops!=&ops||!device->priv||!current||!out)return EINVAL;
 int error=shader_output_check(device,out,2*sizeof(*out));if(error)return error;
 if(overlaps(out,2*sizeof(*out),current,2*sizeof(*current)))return EINVAL;
 struct remesa_shader_pds_links links;
 error=remesa_linux_shader_retained_pds_links(device,token,allocation_address,current,&links);
 if(error)return error;
 struct linux_shader *shader=*shader_link(device->priv,token);
 if(!shader->pds_host_data[0]||!shader->pds_host_data[1])return EAGAIN;
 struct remesa_pds_host_data_view result[2];memset(result,0,sizeof(result));
 for(unsigned i=0;i<2;i++) {
  result[i].binding=links.use[i].request.data;
  error=remesa_shader_storage_inspect(shader->pds_host_data[i],&result[i].storage);if(error)return error;
 }
 /* Token/metadata liveness alone cannot establish the current DATA entry.
  * Check the actual retained bytes before any staging/provider operation. */
 struct remesa_pds_host_data data[2];
 for(unsigned i=0;i<2;i++)data[i]=(struct remesa_pds_host_data){
  .binding=result[i].binding,.bytes=(void *)result[i].storage.bytes,
  .byte_length=result[i].storage.byte_length};
 const struct remesa_pds_binding programs[2]={links.use[0].program,links.use[1].program};
 error=remesa_pds_validate_host_entry_fixups(links.launch.stage,programs,links.use,data);
 if(error)return error;
 out[0]=result[0];out[1]=result[1];return 0;
}
int remesa_linux_shader_stage_pds_data(struct remesa_device *device,void *token,
 uint64_t allocation_address,const struct remesa_pds_request *current)
{
 struct remesa_pds_host_data_view data[2];
 int e=remesa_linux_shader_host_pds_data(device,token,allocation_address,current,data);
 if(e)return e;
 struct linux_device *d=device->priv;
 struct linux_shader *s=*shader_link(d,token);
 if(!s->storage_token)return ENOTSUP;
 return remesa_persistent_shader_stage_pds_data(&d->shader_storage,s->storage_token,data);
}
int remesa_linux_shader_pds_backing(const struct remesa_device *device,void *token,
 uint64_t allocation_address,const struct remesa_pds_request *current,
 struct remesa_persistent_pds_backing *out)
{
 if(!out)return EINVAL;
 int e=shader_output_check(device,out,sizeof(*out));if(e)return e;
 if(current&&overlaps(out,sizeof(*out),current,2*sizeof(*current)))return EINVAL;
 struct remesa_pds_host_data_view data[2];
 e=remesa_linux_shader_host_pds_data(device,token,allocation_address,current,data);
 if(e)return e;
 struct linux_device *d=device->priv;
 struct linux_shader *s=*shader_link(d,token);
 if(!s->storage_token)return ENOTSUP;
 return remesa_persistent_shader_pds_backing(&d->shader_storage,s->storage_token,out);
}
int remesa_linux_last_receipt(const struct remesa_device *device,struct remesa_linux_receipt *r)
{
 if(!device || device->ops!=&ops || !device->priv || !r)return EINVAL;
 int error=shader_output_check(device,r,sizeof(*r));if(error)return error;
 *r=((struct linux_device *)device->priv)->receipt;return 0;
}
int remesa_linux_response(const struct remesa_device *device,struct remesa_native_draw *r)
{
 if(!device || device->ops!=&ops || !device->priv || !r)return EINVAL;
 struct linux_device *d=device->priv;
 if(!d->attempted)return EAGAIN;
 int error=shader_output_check(device,r,sizeof(*r));if(error)return error;
 *r=d->response;return 0;
}
