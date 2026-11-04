#include "remesa/scene_initial.h"
#include "remesa/matrix_preserve_ta.h"
/* SPDX-License-Identifier: MIT */
#include "remesa_persistent.h"
#include "remesa_shader_program.h"
#include "remesa_fragment_body.h"
#include "remesa_draw.h"
#include "remesa_pds.h"
#include "remesa_pds_structure.h"
#include "remesa_ta_index.h"
#include "remesa_ta_zclear.h"
#include "remesa_depth_scene_image.h"
#include "remesa_resource_layout.h"
#include "remesa/shader_request.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#define PAGE_BYTES 4096u
_Static_assert(REMESA_SCENE_USE_ENTRIES<=REMESA_SCENE_USE_MAX,"native USE reference capacity");
_Static_assert(REMESA_PROVIDER_MAX_USES>=REMESA_PERSISTENT_BUFFERS,"provider must hold the complete captured allocation graph");
_Static_assert(REMESA_LINKED_BODY_MAX_DRAWS==REMESA_TA_REFERENCE_MAX_DRAWS,"TA reference capacity");
struct linked_program {
 struct remesa_shader_linkage linkage;
 size_t offset[2],bytes[2],reserved[2];
 struct remesa_use_entry_plan entry[2];
 struct remesa_shader_selected_resources resources[2];
};
struct linked_geometry {
 uint32_t vertex_count,index_count,min_vertex,max_vertex;
 size_t vertex_bytes,index_offset,index_bytes;
};
struct memory;
struct linked_body {
 struct memory *dependencies[REMESA_LINKED_BODY_MAX_DRAWS+1];
 unsigned dependency_count,draw_count;bool parameters,homogeneous;
 size_t bytes;
 uint8_t commands[REMESA_LINKED_BODY_MAX_DRAWS*36+12];
};
struct linked_ta {
 struct memory *prefix,*body;size_t bytes;bool preserve_depth;
 uint8_t commands[68+36*REMESA_LINKED_BODY_MAX_DRAWS];
};
struct linked_dpm {struct memory *table,*parameter;};
struct linked_targets {struct memory *ta,*color,*depth;uint32_t pitch;bool depth_finalize;};
struct linked_background {
 struct memory *program,*targets;
 struct remesa_use_entry_plan entry;
 uint8_t known[64];
};
struct linked_pds {
 struct memory *program,*geometry,*parameters;
 struct remesa_pds_selected_plan plans[2];
 uint8_t known[288];
};
struct memory {
 struct remesa_token token;uint32_t handle;uint64_t va;
 size_t bytes,allocated;unsigned refs,scene_holds;
 bool live,owner,pinned,gpu_mapped,reserved,cpu_mapped,readable,quarantined;
 bool depth_backing,scene_hw,scene_color_read;
 struct remesa_scene_layout scene_layout;
 struct remesa_depth_scene_image *scene_image;
 uint8_t *control_image;
 bool isp_update,scene_close,raw_geometry,shader_program,shader_pds;
 struct linked_dpm *linked_dpm;
 bool linked_event_auxiliary,linked_event_profile;
 bool linked_region;
 uint32_t event_pixel_bank;
 struct remesa_use_entry_plan state_use_entry;
 unsigned state_kind,state_parameter_dwords;
 size_t state_bytes,state_use_offset,state_pds_offset,state_ta_offset,state_ta_bytes;
 struct shader_upload *shader_dependency;
 struct linked_program *linked_program;
 struct linked_geometry *linked_geometry;
 struct linked_pds *linked_pds;
 struct memory *linked_dependency;
 struct linked_body *linked_body;
 struct remesa_persistent_zclear_prefix_backing *zclear_prefix;
 struct linked_ta *linked_ta;
 struct linked_targets *linked_targets;
 struct linked_background *linked_background;
 uint8_t state_pds_known[REMESA_PDS_STRUCTURE_STATE_BYTES];
 uint32_t depth_width,depth_height,depth_pitch,depth_cpp;
};
struct submission {
 struct remesa_fence fence;struct memory *target;
 struct remesa_draw_packet packet;uint64_t operation;
 bool live,detached,released,scene,scene_verified;int error;
 struct remesa_scene_packet scene_packet;
};
struct shader_upload {
 struct shader_upload *next;
 void *token;
 struct remesa_shader_image *image;
 struct remesa_shader_request request;
 uint32_t handle;
 size_t allocated;
 bool pinned,leased,staged,quarantined,owner;
 struct memory *gpu;
 unsigned state_refs;
 struct remesa_use_entry_plan gpu_entry[2];
 struct {
  struct memory *gpu;
  uint8_t image[REMESA_PERSISTENT_SHADER_PDS_BYTES];
  uint8_t known[REMESA_PERSISTENT_SHADER_PDS_BYTES];
  uint32_t use_lower19[2];
 } pds_gpu;
 /* Original CPU staging is preserved; optional immutable GPU promotions are
  * separately owned and do not admit physical fetch or submission. */
 struct {
  struct remesa_shader_draw_snapshot *snapshot;
  uint32_t handle;
  struct memory *gpu;
  size_t allocated,vertex_bytes,index_offset,index_bytes;
  bool pinned,leased,staged;
 } geometry;
 /* Original HOST DATA only. The single CPU backing contains both snapshots,
  * never CODE, an SGX mapping or an executable task-control contract. */
 struct {
  struct remesa_shader_storage *data[2];
  uint32_t handle;
  size_t allocated,offset[2],length[2];
  bool pinned,leased,staged;
 } pds;
};
struct scene_capture {
 struct remesa_persistent_scene_footprint footprint;
 struct memory *held[REMESA_PERSISTENT_BUFFERS];
 struct memory *parents[4]; /* background,event,region,DPM; held above */
 unsigned count;uint64_t serial,published_operation,parameter_sequence;bool live,submitted;
 struct memory *read_color;
};
struct persistent {
 struct remesa_persistent_config cfg;size_t allocated;
 struct remesa_slot slots[REMESA_PERSISTENT_BUFFERS];
 struct remesa_registry registry;struct remesa_provider provider;
 struct memory buffers[REMESA_PERSISTENT_BUFFERS];
 struct submission jobs[REMESA_PROVIDER_MAX_JOBS];
 struct submission *dispatching;bool failed;
 struct shader_upload *shaders;
 struct remesa_shader_cookie_pool shader_cookies;
 unsigned shader_count;
 uint64_t shader_generation;
 uint64_t linked_pds_serial,scene_serial;
 struct scene_capture scenes[REMESA_PROVIDER_MAX_JOBS];
 remesa_gpu_copy_fn native_copy;
 remesa_gpu_fill_fn native_fill;
 remesa_gpu_scene_fn native_scene;
 bool native_draw_scene;
 remesa_gpu_scene_parameters_fn native_scene_parameters;
 int (*release_provider)(void *);void *release_priv;int release_error;
};
static struct shader_upload **shader_link(struct persistent *d,const void *token)
{
 if(!d || !token)return NULL;
 for(struct shader_upload **p=&d->shaders;*p;p=&(*p)->next)
  if((*p)->token==token)return p;
 return NULL;
}
/* Never free uncertain GEM backing, even when no pointer was returned by a
 * failed CPU-map call. The provider may have acquired a lease before failing. */
static int shader_quarantine(struct persistent *d,struct shader_upload *s)
{s->quarantined=true;d->failed=true;return EOWNERDEAD;}
/* Check provider object identity even when CREATE reports an ordinary error.
 * A repeated live ID cannot be reclaimed as a newly created allocation. */
static bool handle_aliases(struct persistent *d,uint32_t handle,const uint32_t *field)
{
 bool alias=false;
 if(!handle)return false;
 for(struct shader_upload *s=d->shaders;s;s=s->next) {
  if((&s->handle!=field && s->handle==handle) ||
     (&s->geometry.handle!=field && s->geometry.handle==handle)) {
   s->quarantined=true;alias=true;
  }
  if(&s->pds.handle!=field && s->pds.handle==handle) {
   s->quarantined=true;alias=true;
  }
 }
 for(unsigned i=0;i<REMESA_PERSISTENT_BUFFERS;i++) {
  struct memory *b=&d->buffers[i];
  if(d->buffers[i].live && &d->buffers[i].handle!=field &&
     d->buffers[i].handle==handle) {
   b->quarantined=true;b->readable=false;alias=true;
   if(b->token.generation)remesa_resource_quarantine(&d->slots[b->token.slot].resource);
  }
 }
 if(alias)d->failed=true;
 return alias;
}
static void destroy_bo(void *,void *);
static int geometry_dispose(struct persistent *d,struct shader_upload *s)
{
 const struct remesa_persistent_ops *o=d->cfg.ops;
 if(s->quarantined || s->geometry.leased)return shader_quarantine(d,s);
 if(s->geometry.gpu) {
  struct memory *b=s->geometry.gpu;
  destroy_bo(d,b);
  if(b->live)return shader_quarantine(d,s);
  s->geometry.gpu=NULL;
 }
 if(s->geometry.pinned) {
  int e=o->unpin(d->cfg.priv,s->geometry.handle);
  if(e)return shader_quarantine(d,s);
  s->geometry.pinned=false;
 }
 if(s->geometry.handle) {
  int e=o->destroy(d->cfg.priv,s->geometry.handle);
  if(e)return shader_quarantine(d,s);
 }
 d->allocated-=s->geometry.allocated;
 remesa_shader_draw_snapshot_destroy(s->geometry.snapshot);
 memset(&s->geometry,0,sizeof(s->geometry));return 0;
}
static int pds_dispose(struct persistent *d,struct shader_upload *s)
{
 const struct remesa_persistent_ops *o=d->cfg.ops;
 if(s->quarantined || s->pds.leased)return shader_quarantine(d,s);
 if(s->pds.pinned) {
  int e=o->unpin(d->cfg.priv,s->pds.handle);
  if(e)return shader_quarantine(d,s);
  s->pds.pinned=false;
 }
 if(s->pds.handle) {
  int e=o->destroy(d->cfg.priv,s->pds.handle);
  if(e)return shader_quarantine(d,s);
  s->pds.handle=0;
 }
 d->allocated-=s->pds.allocated;s->pds.allocated=0;
 for(unsigned i=0;i<2;i++)remesa_shader_storage_reference(&s->pds.data[i],NULL);
 memset(&s->pds,0,sizeof(s->pds));return 0;
}
static int shader_dispose(struct persistent *d,struct shader_upload **link)
{
 struct shader_upload *s=*link;
 const struct remesa_persistent_ops *o=d->cfg.ops;
 if(s->state_refs)return EBUSY;
 if(s->quarantined || s->leased)return shader_quarantine(d,s);
 if(s->pds_gpu.gpu) {
  destroy_bo(d,s->pds_gpu.gpu);
  if(s->pds_gpu.gpu->live)return shader_quarantine(d,s);
  s->pds_gpu.gpu=NULL;
 }
 if(s->gpu) {
  destroy_bo(d,s->gpu);
  if(s->gpu->live)return shader_quarantine(d,s);
  s->gpu=NULL;
 }
 int pds_error=pds_dispose(d,s);if(pds_error)return pds_error;
 int geometry_error=geometry_dispose(d,s);if(geometry_error)return geometry_error;
 if(s->pinned) {
  int e=o->unpin(d->cfg.priv,s->handle);
  if(e)return shader_quarantine(d,s);
  s->pinned=false;
 }
 if(s->handle) {
  int e=o->destroy(d->cfg.priv,s->handle);
  if(e)return shader_quarantine(d,s);
 }
 *link=s->next;d->allocated-=s->allocated;--d->shader_count;
 remesa_shader_image_destroy(s->image);free(s);return 0;
}
static int stage_lease(struct persistent *d,struct shader_upload *s,bool write)
{
 void *cpu=NULL;
 const struct remesa_persistent_ops *o=d->cfg.ops;
 int e=o->cpu_map(d->cfg.priv,s->handle,write,&cpu);
 if(e) {
  /* An error plus a pointer violates the ordinary atomic-error contract. */
  if(e==EOWNERDEAD || cpu)return shader_quarantine(d,s);
  return e;
 }
 s->leased=true;
 if(cpu) {
  if(write) {
   memset(cpu,0,s->allocated);
   memcpy(cpu,s->request.data,s->request.byte_count);
  } else {
   if(memcmp(cpu,s->request.data,s->request.byte_count))e=EPROTO;
   for(size_t i=s->request.byte_count;i<s->allocated;i++)
    if(((const unsigned char *)cpu)[i]) {e=EPROTO;break;}
  }
 } else e=EPROTO;
 /* A success-with-NULL still acquired a lease. Release once, without touching
  * memory, and only reclaim backing if that release demonstrably succeeds. */
 int release_error=o->cpu_unmap(d->cfg.priv,s->handle,write);
 if(release_error)return shader_quarantine(d,s);
 s->leased=false;return e;
}
static int pds_lease(struct persistent *d,struct shader_upload *s,bool write)
{
 struct remesa_shader_storage_view view[2];
 for(unsigned i=0;i<2;i++) {
  int e=remesa_shader_storage_inspect(s->pds.data[i],&view[i]);if(e)return e;
 }
 void *cpu=NULL;
 const struct remesa_persistent_ops *o=d->cfg.ops;
 int e=o->cpu_map(d->cfg.priv,s->pds.handle,write,&cpu);
 if(e) {
  if(e==EOWNERDEAD||cpu)return shader_quarantine(d,s);
  return e;
 }
 s->pds.leased=true;
 if(cpu) {
  if(write) {
   memset(cpu,0,s->pds.allocated);
   for(unsigned i=0;i<2;i++)
    memcpy((unsigned char *)cpu+s->pds.offset[i],view[i].bytes,s->pds.length[i]);
  } else {
   for(unsigned i=0;i<2;i++)
    if(memcmp((const unsigned char *)cpu+s->pds.offset[i],view[i].bytes,s->pds.length[i]))e=EPROTO;
   for(size_t i=s->pds.length[0];i<s->pds.allocated;i++)
    if((i<s->pds.offset[1]||i>=s->pds.offset[1]+s->pds.length[1])&&
       ((const unsigned char *)cpu)[i]){e=EPROTO;break;}
  }
 } else e=EPROTO;
 int release_error=o->cpu_unmap(d->cfg.priv,s->pds.handle,write);
 if(release_error)return shader_quarantine(d,s);
 s->pds.leased=false;return e;
}
static int geometry_lease(struct persistent *d,struct shader_upload *s,bool write)
{
 struct remesa_shader_draw_inputs inputs;
 struct remesa_draw_plan plan;
 int e=remesa_shader_draw_snapshot_inspect(s->geometry.snapshot,&inputs,&plan);
 if(e)return e;
 void *cpu=NULL;
 const struct remesa_persistent_ops *o=d->cfg.ops;
 e=o->cpu_map(d->cfg.priv,s->geometry.handle,write,&cpu);
 if(e) {
  if(e==EOWNERDEAD || cpu)return shader_quarantine(d,s);
  return e;
 }
 s->geometry.leased=true;
 if(cpu) {
  if(write) {
   memset(cpu,0,s->geometry.allocated);
   memcpy(cpu,inputs.vertex_data,s->geometry.vertex_bytes);
   if(s->geometry.index_bytes)
    memcpy((unsigned char *)cpu+s->geometry.index_offset,
           inputs.index_data,s->geometry.index_bytes);
  } else {
   if(memcmp(cpu,inputs.vertex_data,s->geometry.vertex_bytes) ||
      (s->geometry.index_bytes &&
       memcmp((const unsigned char *)cpu+s->geometry.index_offset,
              inputs.index_data,s->geometry.index_bytes)))e=EPROTO;
   for(size_t i=s->geometry.vertex_bytes;i<s->geometry.allocated;i++)
    if((i<s->geometry.index_offset ||
        i>=s->geometry.index_offset+s->geometry.index_bytes) &&
       ((const unsigned char *)cpu)[i]) {e=EPROTO;break;}
  }
 } else e=EPROTO;
 int release_error=o->cpu_unmap(d->cfg.priv,s->geometry.handle,write);
 if(release_error)return shader_quarantine(d,s);
 s->geometry.leased=false;return e;
}
static int prepare_candidate(void *data,
 const struct remesa_shader_draw_programs *programs,
 const struct remesa_usse_vertex_mapping *mapping,
 const struct remesa_shader_image *provided,void **out)
{
 struct persistent *d=data;
 struct shader_upload *s;
 struct remesa_shader_image_view view;
 int e;
 if(!out)return EINVAL;
 if(d) {
  int cookie_error=remesa_shader_cookie_output_check(&d->shader_cookies,out,sizeof(*out));
  if(cookie_error)return cookie_error;
 }
 *out=NULL;
 if(!d || !programs)return EINVAL;
 if(d->failed)return EIO;
 if(d->shader_cookies.count>=REMESA_SHADER_COOKIE_LIMIT)return ENOSPC;
 if(d->shader_generation==UINT64_MAX)return EOVERFLOW;
 if(d->shader_count==REMESA_PERSISTENT_BUFFERS ||
    PAGE_BYTES>d->cfg.byte_budget-d->allocated)return ENOSPC;
 /* CPU research records must not enter the ordinary allocation path, which
  * reserves and installs an SGX VA. This path creates/pins only CPU backing. */
 s=calloc(1,sizeof(*s));if(!s)return ENOMEM;
 e=provided?remesa_shader_image_clone(provided,&s->image):
  mapping?remesa_shader_image_prepare_vertex_candidate(programs,mapping,&s->image):
  remesa_shader_image_prepare(programs,&s->image);
 if(e){free(s);return e;}
 e=remesa_shader_image_inspect(s->image,&view);
 if(e){remesa_shader_image_destroy(s->image);free(s);return e;}
 if(!view.fragment.byte_length || view.fragment.byte_length>PAGE_BYTES ||
    view.fragment.byte_alignment>PAGE_BYTES || view.vertex.byte_length>PAGE_BYTES ||
    view.vertex.byte_alignment>PAGE_BYTES) {
  remesa_shader_image_destroy(s->image);free(s);return ENOTSUP;
 }
 e=remesa_shader_image_build_request(s->image,d->cfg.context,d->shader_generation+1,&s->request);
 if(e){remesa_shader_image_destroy(s->image);free(s);return e;}
 if(s->request.byte_count>PAGE_BYTES) {
  remesa_shader_image_destroy(s->image);free(s);return E2BIG;
 }
 s->allocated=PAGE_BYTES;s->next=d->shaders;d->shaders=s;
 ++d->shader_count;d->allocated+=s->allocated;
 e=d->cfg.ops->create(d->cfg.priv,s->allocated,&s->handle);
 /* A recycled handle would alias existing owned storage. Never attempt to
  * unpin/destroy it, since the provider has violated object identity. */
 if(handle_aliases(d,s->handle,&s->handle))return shader_quarantine(d,s);
 if(e==EOWNERDEAD){shader_quarantine(d,s);return e;}
 if(e)goto fail;
 if(!s->handle){e=EPROTO;goto fail;}
 e=d->cfg.ops->pin(d->cfg.priv,s->handle);
 if(e==EOWNERDEAD){shader_quarantine(d,s);return e;}
 if(e)goto fail;
 s->pinned=true;
 e=stage_lease(d,s,true);if(e)goto fail;
 e=stage_lease(d,s,false);if(e)goto fail;
 e=remesa_shader_cookie_create(&d->shader_cookies,&s->token);if(e)goto fail;
 s->staged=s->owner=true;d->shader_generation=s->request.generation;
 *out=s->token;
 /* Owned, byte-for-byte verified GEM staging; still no executable VS/PDS,
  * physical register mapping, qualified termination or execution address. */
 return remesa_shader_image_validate_execution(s->image);
fail:
 if(s->quarantined)return EOWNERDEAD;
 return shader_dispose(d,&d->shaders)?EOWNERDEAD:e;
}
static int prepare_shader_programs(void *data,
 const struct remesa_shader_draw_programs *programs,void **out)
{return prepare_candidate(data,programs,NULL,NULL,out);}
static int submit_shader_programs(void *data,void *target,
 const struct remesa_draw_packet *packet,const struct remesa_shader_draw_inputs *inputs,
 void *token,void **out)
{
 struct persistent *d=data;
 if(!out)return EINVAL;
 if(d) {
  int error=remesa_shader_cookie_output_check(&d->shader_cookies,out,sizeof(*out));
  if(error)return error;
 }
 *out=NULL;
 if(!d || !target || !packet || !inputs)return EINVAL;
 struct shader_upload **link=shader_link(d,token);
 if(!link || !(*link)->owner)return EINVAL;
 if(d->failed || (*link)->quarantined)return EIO;
 /* CPU staging and opt-in code ownership do not qualify a physical draw.
  * Reject before historical dispatch, target access, job hold or device action. */
 return remesa_shader_image_validate_execution((*link)->image);
}
static void destroy_shader_programs(void *data,void *token)
{
 struct persistent *d=data;
 struct shader_upload **link=shader_link(d,token);
 if(!link || !(*link)->owner)return;
 (*link)->owner=false;(void)shader_dispose(d,link);
}
static bool owns_memory(struct persistent *d,const void *object)
{ for(unsigned i=0;i<REMESA_PERSISTENT_BUFFERS;i++) if(object==&d->buffers[i])return true;return false; }
static bool owns_job(struct persistent *d,const void *object)
{ for(unsigned i=0;i<REMESA_PROVIDER_MAX_JOBS;i++) if(object==&d->jobs[i])return true;return false; }
static int status(int s)
{ return s==REMESA_OK?0:s==REMESA_BUSY?EBUSY:s==REMESA_UNSUPPORTED?EOPNOTSUPP:s==REMESA_AMBIGUOUS?EIO:EINVAL; }
static bool token_equal(struct remesa_token a,struct remesa_token b)
{ return a.slot==b.slot && a.generation==b.generation; }
static void quarantine(struct persistent *d,struct memory *b)
{
 b->quarantined=true;b->readable=false;
 if(b->token.generation) remesa_resource_quarantine(&d->slots[b->token.slot].resource);
}
static void memory_error(struct persistent *d,struct memory *b,int error)
{ quarantine(d,b);if(error==EOWNERDEAD)d->failed=true; }
static void drop_memory(struct persistent *,struct memory *);
static void dispose(struct persistent *d,struct memory *b)
{
 const struct remesa_persistent_ops *o=d->cfg.ops;int e;
 if(!b->live || b->refs || b->quarantined) return;
 if(b->token.generation) {
  struct remesa_slot *slot=&d->slots[b->token.slot];
  if(slot->holds || (slot->resource.state!=REMESA_NEW && slot->resource.state!=REMESA_MAPPED)) {
   quarantine(d,b);return;
  }
 }
 if(b->gpu_mapped) {
  e=o->gpu_unmap(d->cfg.priv,b->handle,b->va,b->allocated);
  if(e) {memory_error(d,b,e);return;} b->gpu_mapped=false;
 }
 if(b->pinned) {
  e=o->unpin(d->cfg.priv,b->handle);if(e) {memory_error(d,b,e);return;} b->pinned=false;
 }
 if(b->handle) {
  e=o->destroy(d->cfg.priv,b->handle);if(e) {memory_error(d,b,e);return;} b->handle=0;
 }
 if(b->token.generation && remesa_registry_remove(&d->registry,b->token)!=REMESA_OK) {
  quarantine(d,b);return;
 }
 struct shader_upload *dependency=b->shader_dependency;
 struct memory *linked_dependencies[REMESA_LINKED_BODY_MAX_DRAWS+14]={NULL,NULL,b->linked_dependency};
 if(b->linked_pds){linked_dependencies[0]=b->linked_pds->program;linked_dependencies[1]=b->linked_pds->geometry;}
 if(b->linked_pds)linked_dependencies[REMESA_LINKED_BODY_MAX_DRAWS+13]=b->linked_pds->parameters;
 if(b->linked_body)for(unsigned i=0;i<b->linked_body->dependency_count;i++)
  linked_dependencies[3+i]=b->linked_body->dependencies[i];
 if(b->linked_ta){linked_dependencies[REMESA_LINKED_BODY_MAX_DRAWS+4]=b->linked_ta->prefix;
  linked_dependencies[REMESA_LINKED_BODY_MAX_DRAWS+5]=b->linked_ta->body;}
 if(b->linked_targets){linked_dependencies[REMESA_LINKED_BODY_MAX_DRAWS+6]=b->linked_targets->ta;
  linked_dependencies[REMESA_LINKED_BODY_MAX_DRAWS+7]=b->linked_targets->color;
  linked_dependencies[REMESA_LINKED_BODY_MAX_DRAWS+8]=b->linked_targets->depth;}
 if(b->linked_background){linked_dependencies[REMESA_LINKED_BODY_MAX_DRAWS+9]=b->linked_background->program;
  linked_dependencies[REMESA_LINKED_BODY_MAX_DRAWS+10]=b->linked_background->targets;}
 if(b->linked_dpm){linked_dependencies[REMESA_LINKED_BODY_MAX_DRAWS+11]=b->linked_dpm->table;
  linked_dependencies[REMESA_LINKED_BODY_MAX_DRAWS+12]=b->linked_dpm->parameter;}
 d->allocated-=b->allocated;free(b->scene_image);free(b->control_image);free(b->linked_program);
 free(b->linked_geometry);free(b->linked_pds);free(b->linked_body);free(b->zclear_prefix);free(b->linked_ta);
 free(b->linked_targets);free(b->linked_background);free(b->linked_dpm);memset(b,0,sizeof(*b));
 for(unsigned i=0;i<REMESA_LINKED_BODY_MAX_DRAWS+14;i++)if(linked_dependencies[i]){
  struct memory *held=linked_dependencies[i];
  if(!held->live||!held->refs){quarantine(d,held);d->failed=true;continue;}
  drop_memory(d,held);
 }
 if(dependency) {
  if(!dependency->state_refs){shader_quarantine(d,dependency);return;}
  --dependency->state_refs;
  if(!dependency->state_refs&&!dependency->owner) {
   struct shader_upload **link=shader_link(d,dependency->token);
   if(!link||shader_dispose(d,link))d->failed=true;
  }
 }
}
static void drop_memory(struct persistent *d,struct memory *b)
{ if(b->refs) --b->refs;dispose(d,b); }
static struct remesa_persistent_va_range placement_range(const struct persistent *d,
 enum remesa_persistent_va_class kind)
{
 struct remesa_persistent_va_range r=d->cfg.placement[kind];
 if(!r.start&&!r.end) {r.start=d->cfg.va_start;r.end=d->cfg.va_end;}
 if(!r.alignment)r.alignment=PAGE_BYTES;
 return r;
}
static int reserve_va(struct persistent *d,struct memory *b,
 enum remesa_persistent_va_class kind)
{
 const struct remesa_persistent_va_range r=placement_range(d,kind);
 uint64_t va=r.start;
 for(unsigned attempt=0;attempt<=REMESA_PERSISTENT_BUFFERS+d->cfg.reserved_count;attempt++) {
  bool overlap=false;
  /* Validated root <=2^32 and alignment <=2^32 keep this sum in uint64_t. */
  va=(va+r.alignment-1)&~(r.alignment-1);
  if(va>r.end || b->allocated>r.end-va) return ENOSPC;
  for(unsigned i=0;i<d->cfg.reserved_count;i++) {
   if(va<d->cfg.reserved[i].end && d->cfg.reserved[i].start<va+b->allocated) {
    va=d->cfg.reserved[i].end;overlap=true;break;
   }
  }
  if(overlap)continue;
  for(unsigned i=0;i<REMESA_PERSISTENT_BUFFERS;i++) {
   struct memory *other=&d->buffers[i];
   if(other==b || !other->live || !other->reserved) continue;
   if(va<other->va+other->allocated && other->va<va+b->allocated) {
    va=other->va+other->allocated;overlap=true;break;
   }
  }
  if(!overlap) {b->va=va;b->reserved=true;return 0;}
 }
 return ENOSPC;
}
static int allocate_class(void *data,size_t bytes,void **out,
 enum remesa_persistent_va_class kind)
{
 struct persistent *d=data;struct memory *b=NULL;int e;
 if(!out) return EINVAL;
 e=remesa_shader_cookie_output_check(&d->shader_cookies,out,sizeof(*out));
 if(e)return e;
 *out=NULL;
 if(d->failed) return EIO;
 if(!bytes || bytes>SIZE_MAX-(PAGE_BYTES-1)) return EINVAL;
 size_t n=(bytes+PAGE_BYTES-1)&~(size_t)(PAGE_BYTES-1);
 if(n>d->cfg.byte_budget-d->allocated) return ENOSPC;
 for(unsigned i=0;i<REMESA_PERSISTENT_BUFFERS;i++) if(!d->buffers[i].live) {b=&d->buffers[i];break;}
 if(!b) return ENOSPC;
 *b=(struct memory){.live=true,.bytes=bytes,.allocated=n};d->allocated+=n;
 e=status(remesa_registry_allocate(&d->registry,n,&b->token));if(e) goto fail;
 e=reserve_va(d,b,kind);if(e) goto fail;
 e=d->cfg.ops->create(d->cfg.priv,n,&b->handle);
 /* The shared provider must not recycle an identity that is still owned by
  * a CPU-only shader candidate or another resource. Check even on an error:
  * cleanup of a duplicated handle could destroy someone else's backing. */
 if(handle_aliases(d,b->handle,&b->handle)) {e=EOWNERDEAD;goto fail;}
 if(e)goto fail;
 if(!b->handle) {e=EPROTO;goto fail;}
 e=d->cfg.ops->pin(d->cfg.priv,b->handle);if(e) goto fail;b->pinned=true;
 e=d->cfg.ops->gpu_map(d->cfg.priv,b->handle,b->va,n);if(e) goto fail;b->gpu_mapped=true;
 e=status(remesa_registry_map(&d->registry,b->token,b->va,PAGE_BYTES));if(e) goto fail;
 b->refs=1;b->owner=true;*out=b;return 0;
fail:
 if(e==EOWNERDEAD) {quarantine(d,b);d->failed=true;} else dispose(d,b);
 return e;
}
static int allocate(void *data,size_t bytes,void **out)
{ return allocate_class(data,bytes,out,REMESA_VA_DEFAULT); }
static void destroy_bo(void *data,void *object)
{
 struct persistent *d=data;struct memory *b=object;
 if(!owns_memory(d,b) || !b->live || !b->owner) return;
 b->owner=false;drop_memory(d,b);
}
static int map_bo(void *data,void *object,bool write,void **out)
{
 struct persistent *d=data;struct memory *b=object;struct remesa_resource r;int e;
 if(!out) return EINVAL;
 e=remesa_shader_cookie_output_check(&d->shader_cookies,out,sizeof(*out));
 if(e)return e;
 *out=NULL;
 if(!owns_memory(d,b)) return EINVAL;
 if(!b->live || b->quarantined) return EIO;
 if(!b->owner) return EINVAL;
 if(write&&(b->linked_dpm||b->linked_background||b->linked_targets||b->linked_ta||b->zclear_prefix||b->linked_body||b->scene_hw||b->isp_update||b->raw_geometry||b->shader_program||b->shader_pds||b->scene_image||b->control_image))return ENOTSUP;
 if(b->cpu_mapped || b->scene_holds || b->refs==UINT32_MAX) return EBUSY;
 if(remesa_registry_snapshot(&d->registry,b->token,&r)!=REMESA_OK) return EINVAL;
 if(r.state!=REMESA_MAPPED) return EBUSY;
 if(!write && !b->readable) return EAGAIN;
 e=d->cfg.ops->cpu_map(d->cfg.priv,b->handle,write,out);
 if(e==EOWNERDEAD) {quarantine(d,b);d->failed=true;}
 if(e) {*out=NULL;return e;}
 if(!*out) {quarantine(d,b);return EPROTO;}
 b->cpu_mapped=true;++b->refs;return 0;
}
static void unmap_bo(void *data,void *object,bool write)
{
 struct persistent *d=data;struct memory *b=object;int e;
 if(!owns_memory(d,b) || !b->live || !b->cpu_mapped) return;
 if(b->scene_color_read)return; /* explicit capture READ lease, never WRITE */
 e=d->cfg.ops->cpu_unmap(d->cfg.priv,b->handle,write);
 b->cpu_mapped=false;
 if(!e && write) e=d->cfg.ops->sync_for_device(d->cfg.priv,b->handle);
 if(e) {quarantine(d,b);if(e==EOWNERDEAD)d->failed=true;}
 else if(write) b->readable=true;
 drop_memory(d,b);
}
static enum remesa_dispatch dispatch(void *data,const struct remesa_job *job)
{
 struct persistent *d=data;struct submission *j=d->dispatching;
 for(unsigned i=0;i<REMESA_PERSISTENT_BUFFERS;i++)
  if(d->buffers[i].scene_color_read)return REMESA_NOT_ISSUED;
 if(j&&j->scene){
  if(!d->native_scene||job->count!=j->scene_packet.binding_count)return REMESA_NOT_ISSUED;
  j->operation=job->operation;
  const struct remesa_gpu_scene_submit scene={job->context,job->operation,&j->scene_packet};
  struct remesa_gpu_scene_receipt r={0};
  enum remesa_dispatch result=d->native_scene(d->cfg.priv,&scene,&r);
  if(result==REMESA_NOT_ISSUED)return result;
  if(result!=REMESA_ISSUED||r.context!=job->context||r.operation!=job->operation||
     !r.retired||!r.published||!r.acknowledged||r.failed)return REMESA_ISSUANCE_AMBIGUOUS;
  j->scene_verified=true;return REMESA_ISSUED;
 }
 if(!j || job->count!=1 || !token_equal(job->uses[0].token,j->target->token)) return REMESA_NOT_ISSUED;
 j->operation=job->operation;
 struct remesa_gpu_submit s={job->context,job->operation,j->target->va,j->target->bytes,j->target->handle,&j->packet};
 return d->cfg.ops->dispatch(d->cfg.priv,&s);
}
static int release(void *data,const struct remesa_job *job)
{
 struct persistent *d=data;
 /* Visibility is separate from retirement, and the mapping remains pinned. */
 if(job->state!=REMESA_FENCE_PUBLISHED) return REMESA_BUSY;
 for(unsigned i=0;i<REMESA_PROVIDER_MAX_JOBS;i++){
  const struct submission *j=&d->jobs[i];
  if(j->live&&j->scene&&j->operation==job->operation)
   return j->scene_verified&&job->context==d->cfg.context?REMESA_OK:REMESA_BUSY;
 }
 int e=d->cfg.ops->release(d->cfg.priv,job->context,job->operation);
 return e==EAGAIN?REMESA_RETRY:e?REMESA_BUSY:REMESA_OK;
}
static int submit(void *data,void *object,const struct remesa_draw_packet *packet,void **out)
{
 struct persistent *d=data;
 if(out) {
  int error=remesa_shader_cookie_output_check(&d->shader_cookies,out,sizeof(*out));
  if(error)return error;
 }
 if(d->cfg.resource_only_backend) {if(out)*out=NULL;return EOPNOTSUPP;}
 struct memory *b=object;struct submission *j=NULL;int s;
 if(!out) return EINVAL;
 *out=NULL;
 if(!owns_memory(d,b)) return EINVAL;
 if(d->failed || !b->live || b->quarantined) return EIO;
 if(!b->owner) return EINVAL;
 if(b->cpu_mapped || b->scene_holds || b->refs==UINT32_MAX) return EBUSY;
 if(b->bytes!=4096 || remesa_draw_packet_validate(packet)!=REMESA_PACKET_OK) return EINVAL;
 if(b->linked_dpm||b->linked_background||b->linked_targets||b->linked_ta||b->zclear_prefix||b->linked_body||b->scene_hw||b->depth_backing||b->isp_update||b->raw_geometry||b->shader_program||b->shader_pds||b->scene_image||b->control_image) return ENOTSUP; /* No typed data through color ABI. */
 for(unsigned i=0;i<REMESA_PROVIDER_MAX_JOBS;i++) if(!d->jobs[i].live) {j=&d->jobs[i];break;}
 if(!j) return EBUSY;
 *j=(struct submission){.target=b,.live=true};j->packet=*packet;++b->refs;
 struct remesa_use use={b->token,0,b->bytes,REMESA_ACCESS_WRITE};
 d->dispatching=j;s=remesa_provider_submit(&d->provider,&use,1,&j->fence);d->dispatching=NULL;
 if(s!=REMESA_OK && s!=REMESA_AMBIGUOUS) {memset(j,0,sizeof(*j));drop_memory(d,b);return status(s);}
 b->readable=false;*out=j;
 if(s==REMESA_AMBIGUOUS) {j->error=EIO;quarantine(d,b);d->failed=true;}
 return status(s);
}
static int fault(struct persistent *d,struct submission *j)
{
 remesa_provider_fault(&d->provider,j->fence,d->cfg.context,j->operation);
 quarantine(d,j->target);j->error=EIO;d->failed=true;return EIO;
}
static int wait_job(void *data,void *object,uint64_t timeout,bool visible)
{
 struct persistent *d=data;struct submission *j=object;struct remesa_gpu_receipt r={0};struct remesa_job job;int e;
 (void)visible; /* Always require publication before permitting backing reuse. */
 if(!owns_job(d,j) || !j->live) return EINVAL;
 if(j->error) return j->error;
 if(j->released) return 0;
 e=d->cfg.ops->poll(d->cfg.priv,d->cfg.context,j->operation,timeout,&r);
 if(e==EAGAIN || e==ETIMEDOUT || e==EBUSY) return e;
 if(e || r.context!=d->cfg.context || r.operation!=j->operation || r.failed || (r.published && !r.retired)) return fault(d,j);
 if(remesa_provider_query(&d->provider,j->fence,&job)!=REMESA_OK) return fault(d,j);
 if((job.state==REMESA_FENCE_RETIRED || job.state==REMESA_FENCE_PUBLISHED) && !r.retired) return fault(d,j);
 if(job.state==REMESA_FENCE_PUBLISHED && !r.published) return fault(d,j);
 if(r.retired && job.state==REMESA_FENCE_PENDING &&
    remesa_provider_complete(&d->provider,j->fence,r.context,r.operation,1)!=REMESA_OK) return fault(d,j);
 if(!r.published) return EAGAIN;
 /* Completion may be reported out of order. Publication/acknowledgement must
  * stay in endpoint operation order, including jobs detached by a destroyed
  * context. Do not release a newer GEM writer while an older receipt remains
  * unacknowledged. Drain older dependencies with nonblocking polls; a caller
  * waiting on the newest Gallium fence need not keep every earlier handle.
  * Recursion strictly decreases operation IDs and is bounded by job slots. */
 for(unsigned i=0;i<REMESA_PROVIDER_MAX_JOBS;i++) {
  struct submission *earlier=&d->jobs[i];
  if(earlier->live && !earlier->released && earlier->operation<j->operation) {
   int older=wait_job(d,earlier,0,true);
   if(older==EAGAIN || older==ETIMEDOUT || older==EBUSY)return EAGAIN;
   if(older)return fault(d,j);
  }
 }
 if(job.state!=REMESA_FENCE_PUBLISHED &&
    remesa_provider_publish(&d->provider,j->fence,r.context,r.operation)!=REMESA_OK) return fault(d,j);
 int released=remesa_provider_release(&d->provider,j->fence);
 if(released==REMESA_RETRY) return EAGAIN;
 if(released!=REMESA_OK) return fault(d,j);
 j->target->readable=true;drop_memory(d,j->target);j->target=NULL;j->released=true;
 return 0;
}
static void destroy_fence(void *data,void *object)
{
 struct persistent *d=data;struct submission *j=object;
 if(!owns_job(d,j) || !j->live) return;
 j->detached=true;
 if(j->released) memset(j,0,sizeof(*j));
 else {int e=wait_job(d,j,0,true);if(!e)memset(j,0,sizeof(*j));}
}
static int map_retired_color(struct remesa_device *,void *,void **);
static int unmap_retired_color(struct remesa_device *,void *);
static const struct remesa_device_ops device_ops={
 .allocate=allocate,.destroy_bo=destroy_bo,.map=map_bo,.unmap=unmap_bo,
 .submit=submit,.wait=wait_job,.destroy_fence=destroy_fence,
 .prepare_shader_programs=prepare_shader_programs,
 .submit_shader_programs=submit_shader_programs,
 .destroy_shader_programs=destroy_shader_programs,
 .validate_attachment_storage=remesa_persistent_validate_attachment_storage,
 .allocate_color_attachment=remesa_persistent_color_attachment_allocate,
 .allocate_depth_attachment=remesa_persistent_depth_allocate,
 .prepare_linked_scene=remesa_persistent_linked_scene_prepare,
 .submit_linked_scene=remesa_persistent_scene_capture_submit,
 .release_linked_scene=remesa_persistent_scene_capture_release,
 .map_retired_color=map_retired_color,.unmap_retired_color=unmap_retired_color,
};
int remesa_persistent_open(struct remesa_device *device,const struct remesa_persistent_config *cfg)
{
 struct persistent *d;
 if(device && (device->ops || device->priv)) return EBUSY;
 if(!device || !cfg || !cfg->ops || !cfg->context ||
    ((unsigned)cfg->offline_test_backend+(unsigned)cfg->qualified_hardware_backend+
     (unsigned)cfg->resource_only_backend)!=1 || !cfg->byte_budget ||
    !cfg->va_start || cfg->va_start%PAGE_BYTES || cfg->va_end%PAGE_BYTES || cfg->va_end<=cfg->va_start ||
    cfg->va_end>UINT64_C(0x100000000) || cfg->reserved_count>REMESA_PERSISTENT_BUFFERS) return EINVAL;
 for(unsigned i=0;i<REMESA_VA_CLASS_COUNT;i++) {
  const struct remesa_persistent_va_range *r=&cfg->placement[i];
  if(!r->start&&!r->end&&!r->alignment)continue;
  if(r->start<cfg->va_start||r->end>cfg->va_end||r->start>=r->end||
     r->start%PAGE_BYTES||r->end%PAGE_BYTES||
     (r->alignment&&(r->alignment<PAGE_BYTES||r->alignment>UINT64_C(0x100000000)||
                      (r->alignment&(r->alignment-1)))))return EINVAL;
 }
 for(unsigned i=0;i<cfg->reserved_count;i++) {
  uint64_t start=cfg->reserved[i].start,end=cfg->reserved[i].end;
  if(start<cfg->va_start || end>cfg->va_end || start>=end ||
     start%PAGE_BYTES || end%PAGE_BYTES)return EINVAL;
  for(unsigned j=0;j<i;j++)
   if(start<cfg->reserved[j].end && cfg->reserved[j].start<end)return EINVAL;
 }
 const struct remesa_persistent_ops *o=cfg->ops;
 if(!o->create||!o->pin||!o->gpu_map||!o->gpu_unmap||!o->unpin||!o->destroy||!o->cpu_map||!o->cpu_unmap||!o->sync_for_device||!o->dispatch||!o->poll||!o->release) return EOPNOTSUPP;
 d=calloc(1,sizeof(*d));if(!d)return ENOMEM;d->cfg=*cfg;
 struct remesa_backend b={dispatch,release,d};
 if(remesa_registry_init(&d->registry,d->slots,REMESA_PERSISTENT_BUFFERS)!=REMESA_OK ||
    remesa_provider_init(&d->provider,&d->registry,cfg->context,&b)!=REMESA_OK) {free(d);return EINVAL;}
 struct remesa_resource_limits limits=cfg->resource_limits;
 uint64_t capacity=cfg->byte_budget;
 if(capacity>cfg->va_end-cfg->va_start)capacity=cfg->va_end-cfg->va_start;
 if(!limits.max_resource_bytes || limits.max_resource_bytes>capacity)limits.max_resource_bytes=capacity;
 *device=(struct remesa_device){.ops=&device_ops,.priv=d,.offline_test_backend=cfg->offline_test_backend,.qualified_hardware_backend=cfg->qualified_hardware_backend,.resource_only_backend=cfg->resource_only_backend,.framebuffer_mode=REMESA_FRAMEBUFFER_REPLACE,.resource_limits=limits};return 0;
}
int remesa_persistent_validate_attachment_storage(const struct remesa_device *device)
{
 if(!device||device->ops!=&device_ops||!device->priv)return EINVAL;
 const struct persistent *d=device->priv;
 if(d->failed)return EOWNERDEAD;
 const struct remesa_persistent_va_range range=placement_range(d,REMESA_VA_SURFACE);
 const uint32_t alignment=device->resource_limits.row_alignment;
 if(alignment<128||alignment>8192||(alignment&(alignment-1)))return ENOTSUP;
 if(!device->resource_only_backend||!d->cfg.resource_only_backend||
    range.start<UINT64_C(0x40000000)||range.end>UINT64_C(0x100000000))return ENOTSUP;
 return 0;
}
int remesa_persistent_color_attachment_allocate(struct remesa_device *device,
 uint32_t width,uint32_t height,void **out)
{
 int e=remesa_persistent_validate_attachment_storage(device);if(e)return e;
 struct remesa_resource_layout layout;
 e=remesa_resource_layout(&device->resource_limits,width,height,4,&layout);if(e)return e;
 return remesa_persistent_allocate_placed(device,layout.bytes,REMESA_VA_SURFACE,out);
}
int remesa_persistent_collect(struct remesa_device *device)
{
 if(!device || device->ops!=&device_ops || !device->priv)return EINVAL;
 struct persistent *d=device->priv;
 for(unsigned attempt=0;attempt<REMESA_PROVIDER_MAX_JOBS;attempt++) {
  struct submission *oldest=NULL;
  for(unsigned i=0;i<REMESA_PROVIDER_MAX_JOBS;i++) {
   struct submission *j=&d->jobs[i];
   if(j->live && j->detached && (!oldest || j->operation<oldest->operation))oldest=j;
  }
  if(!oldest)return 0;
  int e=wait_job(d,oldest,0,true);if(e)return e;
  memset(oldest,0,sizeof(*oldest));
 }
 return 0;
}
int remesa_persistent_close(struct remesa_device *device)
{
 if(!device || device->ops!=&device_ops || !device->priv)return EINVAL;
 struct persistent *d=device->priv;remesa_persistent_collect(device);
 for(unsigned i=0;i<REMESA_PROVIDER_MAX_JOBS;i++) if(d->jobs[i].live||d->scenes[i].live)return EBUSY;
 for(unsigned i=0;i<REMESA_PERSISTENT_BUFFERS;i++) if(d->buffers[i].live)return EBUSY;
 if(d->shaders)return EBUSY;
 if(d->release_provider){
  if(d->release_error)return d->release_error;
  if(d->failed)return EOWNERDEAD;
  int e=d->release_provider(d->release_priv);
  if(e){d->failed=true;d->release_error=e>0?e:EOWNERDEAD;return d->release_error;}
 }
 remesa_shader_cookie_pool_finish(&d->shader_cookies);
 free(d);memset(device,0,sizeof(*device));return 0;
}
static bool overlaps(const void *a,size_t an,const void *b,size_t bn)
{
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 if(an>UINTPTR_MAX-x || bn>UINTPTR_MAX-y)return true;
 return x<=y?y-x<an:x-y<bn;
}
static bool geometry_output_overlap(struct shader_upload *s,const void *out,size_t bytes)
{
 if(!s->geometry.snapshot)return false;
 struct remesa_shader_draw_inputs inputs;
 struct remesa_draw_plan plan;
 if(remesa_shader_draw_snapshot_inspect(s->geometry.snapshot,&inputs,&plan))return true;
 return overlaps(out,bytes,inputs.vertex_data,inputs.vertex_bytes) ||
  (inputs.index_bytes && overlaps(out,bytes,inputs.index_data,inputs.index_bytes));
}
/* Protect every retained identity and immutable payload, not only the queried
 * shader. A metadata query must not mutate another token's program/VB/IB. */
int remesa_persistent_shader_output_check(const struct remesa_device *device,
                                         const void *out,size_t bytes)
{
 if(!device||device->ops!=&device_ops||!device->priv||!out||!bytes)return EINVAL;
 struct persistent *d=device->priv;
 if(overlaps(out,bytes,device,sizeof(*device))||
    overlaps(out,bytes,d,sizeof(*d)))return EINVAL;
 int error=remesa_shader_cookie_output_check(&d->shader_cookies,out,bytes);
 if(error)return error;
 for(unsigned i=0;i<REMESA_PERSISTENT_BUFFERS;i++)
  if(d->buffers[i].scene_image&&overlaps(out,bytes,d->buffers[i].scene_image,
     sizeof(*d->buffers[i].scene_image)))return EINVAL;
 for(unsigned i=0;i<REMESA_PERSISTENT_BUFFERS;i++)
  if(d->buffers[i].control_image&&overlaps(out,bytes,d->buffers[i].control_image,
     REMESA_CONTROL_IMAGE_BYTES))return EINVAL;
 for(struct shader_upload *s=d->shaders;s;s=s->next) {
  struct remesa_shader_image_view view;
  if(overlaps(out,bytes,s,sizeof(*s)))return EINVAL;
  error=remesa_shader_image_inspect(s->image,&view);if(error)return error;
  if(overlaps(out,bytes,view.programs,sizeof(*view.programs))||
     overlaps(out,bytes,view.fragment.bytes,view.fragment.byte_length)||
     overlaps(out,bytes,view.vertex.bytes,view.vertex.byte_length)||
     overlaps(out,bytes,view.fragment_pds.bytes,view.fragment_pds.byte_length)||
     overlaps(out,bytes,view.fragment_pds_mask.bytes,view.fragment_pds_mask.byte_length)||
     geometry_output_overlap(s,out,bytes))return EINVAL;
  for(unsigned i=0;i<2;i++)if(s->pds.data[i]) {
   struct remesa_shader_storage_view data;
   error=remesa_shader_storage_inspect(s->pds.data[i],&data);if(error)return error;
   if(overlaps(out,bytes,data.bytes,data.byte_length))return EINVAL;
  }
 }
 return 0;
}
int remesa_persistent_shader_snapshot(const struct remesa_device *device,void *token,
 struct remesa_shader_request *request,struct remesa_persistent_shader_backing *backing)
{
 if(!device || device->ops!=&device_ops || !device->priv || !request || !backing)
  return EINVAL;
 struct persistent *d=device->priv;
 struct shader_upload **link=shader_link(d,token);
 if(!link || !(*link)->owner)return EINVAL;
 struct shader_upload *s=*link;
 if(d->failed || s->quarantined)return EOWNERDEAD;
 if(!s->staged || !s->pinned || !s->handle || s->leased)return EPROTO;
 struct remesa_shader_image_view view;
 int e=remesa_shader_image_inspect(s->image,&view);if(e)return e;
 if(overlaps(request,sizeof(*request),backing,sizeof(*backing)))return EINVAL;
 e=remesa_persistent_shader_output_check(device,request,sizeof(*request));if(e)return e;
 e=remesa_persistent_shader_output_check(device,backing,sizeof(*backing));if(e)return e;
 if(remesa_shader_request_validate(&s->request,d->cfg.context,s->request.generation)
    !=REMESA_SHADER_REQUEST_OK)return EPROTO;
 e=remesa_shader_image_validate_request(s->image,&s->request);if(e)return e;
 struct remesa_persistent_shader_backing v={
  .owner=d->cfg.context,.generation=s->request.generation,.handle=s->handle,
  .allocation_bytes=s->allocated,.pinned=s->pinned,.staged=s->staged,
  .quarantined=s->quarantined
 };
 *request=s->request;*backing=v;return 0;
}

int remesa_persistent_shader_bindings(const struct remesa_device *device,void *token,
 struct remesa_shader_request *request,struct remesa_shader_bindings *bindings)
{
 struct remesa_shader_request r;
 struct remesa_persistent_shader_backing backing;
 struct remesa_shader_bindings b;
 if(!request || !bindings)return EINVAL;
 int e=remesa_persistent_shader_snapshot(device,token,&r,&backing);if(e)return e;
 struct persistent *d=device->priv;
 struct shader_upload *s=*shader_link(d,token);
 struct remesa_shader_image_view view;
 e=remesa_shader_image_inspect(s->image,&view);if(e)return e;
 if(overlaps(request,sizeof(*request),bindings,sizeof(*bindings)))return EINVAL;
 e=remesa_persistent_shader_output_check(device,request,sizeof(*request));if(e)return e;
 e=remesa_persistent_shader_output_check(device,bindings,sizeof(*bindings));if(e)return e;
 if(backing.allocation_bytes>UINT32_MAX)return EOVERFLOW;
 if(remesa_shader_bindings_build(&b,&r,backing.owner,backing.generation,
    backing.handle,(uint32_t)backing.allocation_bytes)!=REMESA_SHADER_REQUEST_OK)return EPROTO;
 *request=r;*bindings=b;return 0;
}

int remesa_persistent_shader_stage_draw(struct remesa_device *device,void *token,
 const struct remesa_shader_draw_inputs *inputs)
{
 struct remesa_shader_request request;
 struct remesa_persistent_shader_backing backing;
 int e=remesa_persistent_shader_snapshot(device,token,&request,&backing);if(e)return e;
 struct persistent *d=device->priv;
 struct shader_upload *s=*shader_link(d,token);
 if(s->geometry.snapshot)return EBUSY;
 if(!inputs)return EINVAL;
 struct remesa_shader_image_view image;
 e=remesa_shader_image_inspect(s->image,&image);if(e)return e;
 if(inputs->position_input_slot!=image.programs->pipeline.position_input_slot)return EINVAL;
 if(inputs->preserve_color && device->framebuffer_mode!=REMESA_FRAMEBUFFER_PRESERVE)
  return ENOTSUP;
 struct remesa_shader_draw_snapshot *snapshot=NULL;
 e=remesa_shader_draw_snapshot_create(inputs,&snapshot);if(e)return e;
 struct remesa_shader_draw_inputs owned;
 struct remesa_draw_plan plan;
 e=remesa_shader_draw_snapshot_inspect(snapshot,&owned,&plan);
 if(e){remesa_shader_draw_snapshot_destroy(snapshot);return e;}
 /* Four-byte padding is deterministic host serialization only. Neither an
  * instruction boundary nor a physical vertex/index-fetch unit is inferred. */
 if(owned.index_bytes && owned.vertex_bytes>SIZE_MAX-3) {
  remesa_shader_draw_snapshot_destroy(snapshot);return EOVERFLOW;
 }
 size_t index_offset=owned.index_bytes?(owned.vertex_bytes+3)&~(size_t)3:0;
 if(index_offset>REMESA_SHADER_DRAW_SNAPSHOT_MAX_BYTES ||
    owned.index_bytes>REMESA_SHADER_DRAW_SNAPSHOT_MAX_BYTES-index_offset ||
    owned.vertex_bytes>REMESA_SHADER_DRAW_SNAPSHOT_MAX_BYTES) {
  remesa_shader_draw_snapshot_destroy(snapshot);return E2BIG;
 }
 size_t length=owned.index_bytes?index_offset+owned.index_bytes:owned.vertex_bytes;
 if(length>SIZE_MAX-(PAGE_BYTES-1)) {
  remesa_shader_draw_snapshot_destroy(snapshot);return EOVERFLOW;
 }
 size_t allocated=(length+PAGE_BYTES-1)&~(size_t)(PAGE_BYTES-1);
 if(allocated>d->cfg.byte_budget-d->allocated) {
  remesa_shader_draw_snapshot_destroy(snapshot);return ENOSPC;
 }
 s->geometry.snapshot=snapshot;s->geometry.allocated=allocated;
 s->geometry.vertex_bytes=owned.vertex_bytes;s->geometry.index_offset=index_offset;
 s->geometry.index_bytes=owned.index_bytes;d->allocated+=allocated;
 e=d->cfg.ops->create(d->cfg.priv,allocated,&s->geometry.handle);
 if(handle_aliases(d,s->geometry.handle,&s->geometry.handle))return shader_quarantine(d,s);
 if(e==EOWNERDEAD)return shader_quarantine(d,s);
 if(e)goto fail;
 if(!s->geometry.handle){e=EPROTO;goto fail;}
 e=d->cfg.ops->pin(d->cfg.priv,s->geometry.handle);
 if(e==EOWNERDEAD)return shader_quarantine(d,s);
 if(e)goto fail;
 s->geometry.pinned=true;
 e=geometry_lease(d,s,true);if(e)goto fail;
 e=geometry_lease(d,s,false);if(e)goto fail;
 s->geometry.staged=true;return 0;
fail:
 if(s->quarantined)return EOWNERDEAD;
 return geometry_dispose(d,s)?EOWNERDEAD:e;
}

int remesa_persistent_shader_draw_snapshot(const struct remesa_device *device,void *token,
 struct remesa_shader_draw_inputs *out,struct remesa_draw_plan *plan)
{
 struct remesa_shader_request request;
 struct remesa_persistent_shader_backing backing;
 int error=remesa_persistent_shader_snapshot(device,token,&request,&backing);
 if(error)return error;
 if(!out||!plan||overlaps(out,sizeof(*out),plan,sizeof(*plan)))return EINVAL;
 error=remesa_persistent_shader_output_check(device,out,sizeof(*out));if(error)return error;
 error=remesa_persistent_shader_output_check(device,plan,sizeof(*plan));if(error)return error;
 struct shader_upload *s=*shader_link(device->priv,token);
 if(!s->geometry.snapshot)return EAGAIN;
 if(!s->geometry.staged||!s->geometry.pinned||!s->geometry.handle||
    s->geometry.leased)return EPROTO;
 return remesa_shader_draw_snapshot_inspect(s->geometry.snapshot,out,plan);
}
int remesa_persistent_shader_draw_backing(const struct remesa_device *device,void *token,
 struct remesa_persistent_draw_backing *out)
{
 if(!out)return EINVAL;
 struct remesa_shader_request request;
 struct remesa_persistent_shader_backing program;
 int e=remesa_persistent_shader_snapshot(device,token,&request,&program);if(e)return e;
 struct persistent *d=device->priv;
 struct shader_upload *s=*shader_link(d,token);
 if(!s->geometry.snapshot)return EAGAIN;
 if(!s->geometry.staged || !s->geometry.pinned || !s->geometry.handle ||
    s->geometry.leased)return EPROTO;
 struct remesa_shader_image_view image;
 e=remesa_shader_image_inspect(s->image,&image);if(e)return e;
 e=remesa_persistent_shader_output_check(device,out,sizeof(*out));if(e)return e;
 struct remesa_persistent_draw_backing value={
  .owner=program.owner,.generation=program.generation,.handle=s->geometry.handle,
  .allocation_bytes=s->geometry.allocated,.vertex_bytes=s->geometry.vertex_bytes,
  .index_offset=s->geometry.index_offset,.index_bytes=s->geometry.index_bytes,
  .pinned=s->geometry.pinned,.staged=s->geometry.staged
 };
 *out=value;return 0;
}

/* The token retains this owned allocation; no caller can substitute a VA.
 * Physical fetch format and requester qualification are separate contracts. */
int remesa_persistent_shader_prepare_draw_gpu(struct remesa_device *device,void *token)
{
 struct remesa_shader_draw_inputs inputs;struct remesa_draw_plan plan;
 int e=remesa_persistent_shader_draw_snapshot(device,token,&inputs,&plan);if(e)return e;
 struct persistent *d=device->priv;struct shader_upload *s=*shader_link(d,token);
 if(s->geometry.gpu)return EBUSY;
 for(unsigned i=0;i<REMESA_PERSISTENT_BUFFERS;i++)
  if(d->buffers[i].cpu_mapped)return EBUSY;
 /* Revalidate the committed immutable extent, not its page-rounded capacity. */
 e=remesa_shader_draw_inputs_validate(&inputs,&plan);if(e)return e;
 e=geometry_lease(d,s,false);if(e)return e;
 void *object=NULL;e=allocate_class(d,s->geometry.allocated,&object,REMESA_VA_PROGRAM);
 if(e)return e==EOWNERDEAD?shader_quarantine(d,s):e;
 struct memory *b=object;b->raw_geometry=true;
 const struct remesa_persistent_ops *o=d->cfg.ops;
 for(unsigned pass=0;pass<2;pass++) {
  void *cpu=NULL;bool write=pass==0;
  e=o->cpu_map(d->cfg.priv,b->handle,write,&cpu);
  if(e) {
   if(e==EOWNERDEAD||cpu) {b->cpu_mapped=true;memory_error(d,b,EOWNERDEAD);e=EOWNERDEAD;}
   goto fail;
  }
  b->cpu_mapped=true;
  if(!cpu)e=EPROTO;
  else if(write) {
   memset(cpu,0,b->allocated);memcpy(cpu,inputs.vertex_data,s->geometry.vertex_bytes);
   if(s->geometry.index_bytes)memcpy((uint8_t *)cpu+s->geometry.index_offset,
                                    inputs.index_data,s->geometry.index_bytes);
  } else {
   if(memcmp(cpu,inputs.vertex_data,s->geometry.vertex_bytes)||
      (s->geometry.index_bytes&&memcmp((uint8_t *)cpu+s->geometry.index_offset,
       inputs.index_data,s->geometry.index_bytes)))e=EPROTO;
   for(size_t i=s->geometry.vertex_bytes;i<b->allocated;i++)
    if((i<s->geometry.index_offset||i>=s->geometry.index_offset+s->geometry.index_bytes)&&
       ((uint8_t *)cpu)[i]){e=EPROTO;break;}
  }
  if(o->cpu_unmap(d->cfg.priv,b->handle,write)) {
   memory_error(d,b,EOWNERDEAD);e=EOWNERDEAD;goto fail;
  }
  b->cpu_mapped=false;if(e)goto fail;
 }
 e=o->sync_for_device(d->cfg.priv,b->handle);
 if(e){memory_error(d,b,EOWNERDEAD);e=EOWNERDEAD;goto fail;}
 b->readable=true;s->geometry.gpu=b;return 0;
fail:
 if(b->quarantined){s->geometry.gpu=b;return shader_quarantine(d,s);}
 destroy_bo(d,b);
 if(b->live){s->geometry.gpu=b;return shader_quarantine(d,s);}
 return e;
}
int remesa_persistent_shader_draw_gpu_backing(const struct remesa_device *device,void *token,
 struct remesa_persistent_draw_gpu_backing *out)
{
 if(!out)return EINVAL;
 struct remesa_shader_draw_inputs inputs;struct remesa_draw_plan plan;
 int e=remesa_persistent_shader_draw_snapshot(device,token,&inputs,&plan);if(e)return e;
 e=remesa_persistent_shader_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct shader_upload *s=*shader_link(d,token);
 struct memory *b=s->geometry.gpu;if(!b)return EAGAIN;
 for(unsigned i=0;i<REMESA_PERSISTENT_BUFFERS;i++)
  if(d->buffers[i].cpu_mapped)return EBUSY;
 if(!b->live||!b->owner||b->quarantined||!b->raw_geometry||!b->pinned||
    !b->gpu_mapped||!b->readable||b->cpu_mapped)return EPROTO;
 struct remesa_persistent_draw_gpu_backing value={
  .owner=d->cfg.context,.generation=s->request.generation,.gpu_va=b->va,
  .handle=b->handle,.allocation_bytes=b->allocated,.vertex_bytes=s->geometry.vertex_bytes,
  .index_offset=s->geometry.index_offset,.index_bytes=s->geometry.index_bytes,
  .min_vertex=plan.min_vertex,.max_vertex=plan.max_vertex};
 *out=value;return 0;
}

/* Own a code allocation using the same provider, registry and VA allocator as
 * other typed resources. CPU image state remains CANDIDATE; no execution gate
 * or request ABI is promoted by this opt-in ownership operation. */
static int owned_image_lease(struct persistent *d,struct memory *b,
 const uint8_t *image,size_t bytes,bool write)
{
 const struct remesa_persistent_ops *o=d->cfg.ops;void *cpu=NULL;
 int e=o->cpu_map(d->cfg.priv,b->handle,write,&cpu);
 if(e) {
  if(e==EOWNERDEAD||cpu) {
   b->cpu_mapped=true;memory_error(d,b,EOWNERDEAD);return EOWNERDEAD;
  }
  return e;
 }
 b->cpu_mapped=true;
 if(!cpu)e=EPROTO;
 else if(write) {
  memset(cpu,0,b->allocated);memcpy(cpu,image,bytes);
 } else {
  if(memcmp(cpu,image,bytes))e=EPROTO;
  for(size_t i=bytes;i<b->allocated;i++)
   if(((const uint8_t *)cpu)[i]){e=EPROTO;break;}
 }
 if(o->cpu_unmap(d->cfg.priv,b->handle,write)) {
  memory_error(d,b,EOWNERDEAD);return EOWNERDEAD;
 }
 b->cpu_mapped=false;return e;
}
int remesa_persistent_shader_prepare_gpu(struct remesa_device *device,void *token,
 const uint32_t banks[2])
{
 if(!banks||(uintptr_t)banks%_Alignof(uint32_t)||
    sizeof(*banks)*2>UINTPTR_MAX-(uintptr_t)banks)return EINVAL;
 const uint32_t bank[2]={banks[0],banks[1]};
 struct remesa_shader_request request;struct remesa_persistent_shader_backing backing;
 int e=remesa_persistent_shader_snapshot(device,token,&request,&backing);if(e)return e;
 struct persistent *d=device->priv;struct shader_upload *s=*shader_link(d,token);
 if(s->gpu)return EBUSY;
 struct remesa_shader_image_view image;
 e=remesa_shader_image_inspect(s->image,&image);if(e)return e;
 /* Only the generated POSITION4 VS16 and restricted packed constant FS8.
  * Optional PDS/masks and other host grammars are not promoted by this API. */
 if(request.resource_mask!=3u||image.vertex.byte_length!=16||
    image.fragment.byte_length!=8||image.fragment_form!=REMESA_SHADER_PACKED_CONSTANT)
  return ENOTSUP;
 uint32_t register_offset[2];
 for(unsigned i=0;i<2;i++) {
  e=remesa_use_base_offset(bank[i],&register_offset[i]);if(e)return e;
 }
 if(register_offset[0]==register_offset[1])return EBUSY;
 for(unsigned i=0;i<REMESA_PERSISTENT_BUFFERS;i++)
  if(d->buffers[i].cpu_mapped)return EBUSY;
 e=stage_lease(d,s,false);
 if(e==EPROTO)return shader_quarantine(d,s);
 if(e)return e;
 void *object=NULL;e=allocate_class(d,s->allocated,&object,REMESA_VA_PROGRAM);
 if(e)return e==EOWNERDEAD?shader_quarantine(d,s):e;
 struct memory *b=object;b->shader_program=true;
 struct remesa_use_entry_request entry[2];struct remesa_use_entry_plan plan[2];
 for(unsigned i=0;i<2;i++)entry[i]=(struct remesa_use_entry_request){
  .allocation_address=b->va,.allocation_bytes=b->allocated,
  .entry_offset=request.resources[i].offset,.program_bytes=request.resources[i].length,
  .bank=bank[i],.data_master=i==0?REMESA_USE_VERTEX:REMESA_USE_PIXEL};
 e=remesa_use_entry_pair_prepare(entry,plan);if(e)goto fail;
 e=owned_image_lease(d,b,s->request.data,s->request.byte_count,true);if(e)goto fail;
 e=owned_image_lease(d,b,s->request.data,s->request.byte_count,false);if(e)goto fail;
 e=d->cfg.ops->sync_for_device(d->cfg.priv,b->handle);
 if(e){memory_error(d,b,EOWNERDEAD);e=EOWNERDEAD;goto fail;}
 b->readable=true;s->gpu=b;s->gpu_entry[0]=plan[0];s->gpu_entry[1]=plan[1];return 0;
fail:
 if(b->quarantined){s->gpu=b;return shader_quarantine(d,s);}
 destroy_bo(d,b);
 if(b->live){s->gpu=b;return shader_quarantine(d,s);}
 return e;
}
int remesa_persistent_shader_gpu_backing(const struct remesa_device *device,void *token,
 struct remesa_persistent_shader_gpu_backing *out)
{
 if(!out)return EINVAL;
 struct remesa_shader_request request;struct remesa_persistent_shader_backing backing;
 int e=remesa_persistent_shader_snapshot(device,token,&request,&backing);if(e)return e;
 e=remesa_persistent_shader_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct shader_upload *s=*shader_link(d,token);
 struct memory *b=s->gpu;if(!b)return EAGAIN;
 for(unsigned i=0;i<REMESA_PERSISTENT_BUFFERS;i++)
  if(d->buffers[i].cpu_mapped)return EBUSY;
 if(!b->live||!b->owner||b->quarantined||!b->shader_program||!b->pinned||
    !b->gpu_mapped||!b->readable||b->cpu_mapped)return EPROTO;
 struct remesa_persistent_shader_gpu_backing result={
  .owner=d->cfg.context,.generation=s->request.generation,.gpu_va=b->va,
  .handle=b->handle,.allocation_bytes=b->allocated};
 for(unsigned i=0;i<2;i++) {
  result.program_offset[i]=request.resources[i].offset;
  result.program_bytes[i]=request.resources[i].length;result.entry[i]=s->gpu_entry[i];
 }
 *out=result;return 0;
}

/* One owned image holds primary VS/FS and the empty-secondary CPU profile.
 *32-byte object alignment matches the existing fragment-reference encoder;
 * deterministic unknown DATA/padding remains masked rather than qualified. */
static const size_t shader_pds_offsets[3]={0,64,128};
static const size_t shader_pds_bytes[3]={64,56,4};
static const size_t shader_pds_data_bytes[3]={48,48,0};
static const size_t shader_pds_code_bytes[3]={16,8,4};
int remesa_persistent_shader_prepare_pds_gpu(struct remesa_device *device,void *token)
{
 struct remesa_persistent_shader_gpu_backing code;
 int e=remesa_persistent_shader_gpu_backing(device,token,&code);if(e)return e;
 struct persistent *d=device->priv;struct shader_upload *s=*shader_link(d,token);
 if(s->pds_gpu.gpu)return EBUSY;
 struct remesa_persistent_draw_gpu_backing raw;
 e=remesa_persistent_shader_draw_gpu_backing(device,token,&raw);if(e)return e;
 struct remesa_shader_draw_inputs inputs;struct remesa_draw_plan plan;
 e=remesa_persistent_shader_draw_snapshot(device,token,&inputs,&plan);if(e)return e;
 if(inputs.position_format!=PIPE_FORMAT_R32G32B32A32_FLOAT||
    inputs.position_input_slot||inputs.vertex_stride!=16||inputs.index_bias)return ENOTSUP;
 if(raw.owner!=code.owner||raw.generation!=code.generation)return ESTALE;
 const uint64_t base=(uint64_t)inputs.vertex_offset+inputs.element_offset;
 if((base&3u)||base>raw.vertex_bytes||plan.vertex_end_byte>raw.vertex_bytes||
    plan.min_vertex!=raw.min_vertex||plan.max_vertex!=raw.max_vertex||
    (uint64_t)plan.max_vertex*inputs.vertex_stride+16>raw.vertex_bytes-base)return EINVAL;
 e=remesa_persistent_shader_verify(device,token);if(e)return e;
 uint8_t image[REMESA_PERSISTENT_SHADER_PDS_BYTES]={0};
 uint8_t known[REMESA_PERSISTENT_SHADER_PDS_BYTES]={0};uint32_t fixup[2];
 for(unsigned i=0;i<2;i++) {
  e=remesa_use_entry_preview_fixup(&code.entry[i],&fixup[i]);if(e)return e;
 }
 const struct remesa_pds_vertex_image_request vertex={
  .source_allocation_address=raw.gpu_va,.source_allocation_bytes=raw.vertex_bytes,
  .source_offset=base,.vertex_stride_dwords=inputs.vertex_stride/4,.use_lower19=fixup[0]};
 size_t written=0;
 e=remesa_pds_structure_encode_vertex_image(&vertex,image,known,64,&written);if(e)return e;
 if(written!=64)return EPROTO;
 const struct remesa_pds_structure_request fs={.code_policy=REMESA_PDS_STRUCTURE_CODE_CPU_COMPLETE};
 struct remesa_pds_structure shape;
 e=remesa_pds_structure_encode(&fs,&shape);if(e)return e;
 e=remesa_pds_structure_serialize(&shape,image+64,known+64,56,&written);if(e)return e;
 if(written!=56||shape.data_dwords!=12||shape.relocation_count!=1||
    shape.relocations[0].kind!=REMESA_PDS_STRUCTURE_USE||shape.relocations[0].data_cell||
    shape.relocations[0].background)
  return EPROTO;
 /* This is the empty psb primary CPU grammar with background0, matching the
  * physically preserved reference. The other pixel_task_pack profile's
  *0x100000 contribution must not be silently mixed into this constructor.
  * Original DATA0 evidence mask stays0; the derived fixup is separate metadata. */
 uint32_t task=shape.relocations[0].background|fixup[1];
 for(unsigned i=0;i<4;i++)image[64+i]=(uint8_t)(task>>(8*i));
 const struct remesa_pds_code_instruction secondary={REMESA_PDS_STRUCTURE_CPU_EMPTY_SECONDARY_AF,
  REMESA_PDS_STRUCTURE_NO_CELL,REMESA_PDS_STRUCTURE_NO_CELL};
 e=remesa_pds_structure_encode_code(&secondary,1,image+128,4,&written);if(e)return e;
 if(written!=4)return EPROTO;
 memset(known+128,255,4);
 void *object=NULL;e=allocate_class(d,sizeof(image),&object,REMESA_VA_PROGRAM);
 if(e)return e==EOWNERDEAD?shader_quarantine(d,s):e;
 struct memory *b=object;b->shader_pds=true;
 e=owned_image_lease(d,b,image,sizeof(image),true);if(e)goto fail;
 e=owned_image_lease(d,b,image,sizeof(image),false);if(e)goto fail;
 e=d->cfg.ops->sync_for_device(d->cfg.priv,b->handle);
 if(e){memory_error(d,b,EOWNERDEAD);e=EOWNERDEAD;goto fail;}
 memcpy(s->pds_gpu.image,image,sizeof(image));memcpy(s->pds_gpu.known,known,sizeof(known));
 s->pds_gpu.use_lower19[0]=fixup[0];s->pds_gpu.use_lower19[1]=fixup[1];
 b->readable=true;s->pds_gpu.gpu=b;return 0;
fail:
 if(b->quarantined){s->pds_gpu.gpu=b;return shader_quarantine(d,s);}
 destroy_bo(d,b);
 if(b->live){s->pds_gpu.gpu=b;return shader_quarantine(d,s);}
 return e;
}
int remesa_persistent_shader_pds_gpu_backing(const struct remesa_device *device,void *token,
 struct remesa_persistent_shader_pds_gpu_backing *out)
{
 if(!out||(uintptr_t)out%_Alignof(struct remesa_persistent_shader_pds_gpu_backing))return EINVAL;
 struct remesa_persistent_shader_gpu_backing code;
 int e=remesa_persistent_shader_gpu_backing(device,token,&code);if(e)return e;
 e=remesa_persistent_shader_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct shader_upload *s=*shader_link(d,token);
 struct memory *b=s->pds_gpu.gpu;if(!b)return EAGAIN;
 if(!b->live||!b->owner||b->quarantined||!b->shader_pds||!b->pinned||
    !b->gpu_mapped||!b->readable||b->cpu_mapped)return EPROTO;
 struct remesa_shader_draw_inputs inputs;struct remesa_draw_plan plan;
 e=remesa_persistent_shader_draw_snapshot(device,token,&inputs,&plan);if(e)return e;
 struct remesa_persistent_shader_pds_gpu_backing result={
  .owner=code.owner,.generation=code.generation,.gpu_va=b->va,
  .handle=b->handle,.allocation_bytes=b->allocated,.vertex_stride=inputs.vertex_stride,
  .min_vertex=plan.min_vertex,.max_vertex=plan.max_vertex};
 for(unsigned i=0;i<3;i++) {
  result.object_offset[i]=shader_pds_offsets[i];result.object_bytes[i]=shader_pds_bytes[i];
  result.data_bytes[i]=shader_pds_data_bytes[i];result.code_bytes[i]=shader_pds_code_bytes[i];
 }
 memcpy(result.known_mask,s->pds_gpu.known,sizeof(result.known_mask));
 memcpy(result.use_lower19,s->pds_gpu.use_lower19,sizeof(result.use_lower19));
 *out=result;return 0;
}

int remesa_persistent_shader_stage_pds_data(struct remesa_device *device,void *token,
 const struct remesa_pds_host_data_view *data)
{
 struct remesa_shader_request request;
 struct remesa_persistent_shader_backing program;
 int e=remesa_persistent_shader_snapshot(device,token,&request,&program);if(e)return e;
 struct persistent *d=device->priv;
 struct shader_upload *s=*shader_link(d,token);
 if(s->pds.data[0]||s->pds.data[1]||s->pds.handle)return EBUSY;
 if(!data||2*sizeof(*data)>UINTPTR_MAX-(uintptr_t)data)return EINVAL;
 for(unsigned i=0;i<2;i++) {
  const struct remesa_shader_storage_view *v=&data[i].storage;
  if(v->stage!=(i?REMESA_IR_FRAGMENT:REMESA_IR_VERTEX)||
     v->format!=REMESA_SHADER_STORAGE_HOST_PDS_DATA||!v->bytes||
     !v->byte_length||v->byte_length>1024||v->byte_length%4||
     v->byte_alignment!=16||data[i].binding.owner_cookie!=program.owner||
     data[i].binding.bytes!=v->byte_length)return EINVAL;
  e=remesa_persistent_shader_output_check(device,v->bytes,v->byte_length);if(e)return e;
 }
 size_t offset=(data[0].storage.byte_length+15)&~(size_t)15;
 size_t used=offset+data[1].storage.byte_length;
 size_t allocated=(used+PAGE_BYTES-1)&~(size_t)(PAGE_BYTES-1);
 if(allocated>d->cfg.byte_budget-d->allocated)return ENOSPC;
 for(unsigned i=0;i<2;i++) {
  const struct remesa_shader_storage_view *v=&data[i].storage;
  const struct remesa_shader_storage_request r={.stage=v->stage,.format=v->format,
   .bytes=v->bytes,.byte_length=v->byte_length,.byte_alignment=v->byte_alignment};
  e=remesa_shader_storage_create(&r,&s->pds.data[i]);if(e)goto fail;
  s->pds.length[i]=v->byte_length;
 }
 s->pds.offset[1]=offset;s->pds.allocated=allocated;d->allocated+=allocated;
 e=d->cfg.ops->create(d->cfg.priv,allocated,&s->pds.handle);
 if(handle_aliases(d,s->pds.handle,&s->pds.handle))return shader_quarantine(d,s);
 if(e==EOWNERDEAD)return shader_quarantine(d,s);
 if(e)goto fail;
 if(!s->pds.handle){e=EPROTO;goto fail;}
 e=d->cfg.ops->pin(d->cfg.priv,s->pds.handle);
 if(e==EOWNERDEAD)return shader_quarantine(d,s);
 if(e)goto fail;
 s->pds.pinned=true;
 e=pds_lease(d,s,true);if(e)goto fail;
 e=pds_lease(d,s,false);if(e)goto fail;
 s->pds.staged=true;return 0;
fail:
 if(s->quarantined)return EOWNERDEAD;
 return pds_dispose(d,s)?EOWNERDEAD:e;
}
int remesa_persistent_shader_pds_backing(const struct remesa_device *device,void *token,
 struct remesa_persistent_pds_backing *out)
{
 if(!out)return EINVAL;
 struct remesa_shader_request request;
 struct remesa_persistent_shader_backing program;
 int e=remesa_persistent_shader_snapshot(device,token,&request,&program);if(e)return e;
 struct shader_upload *s=*shader_link(device->priv,token);
 if(!s->pds.data[0]||!s->pds.data[1])return EAGAIN;
 if(!s->pds.staged||!s->pds.pinned||!s->pds.handle||s->pds.leased)return EPROTO;
 e=remesa_persistent_shader_output_check(device,out,sizeof(*out));if(e)return e;
 struct remesa_persistent_pds_backing result={.owner=program.owner,
  .generation=program.generation,.handle=s->pds.handle,.allocation_bytes=s->pds.allocated,
  .data_offset={s->pds.offset[0],s->pds.offset[1]},
  .data_bytes={s->pds.length[0],s->pds.length[1]},.pinned=true,.staged=true};
 *out=result;return 0;
}
int remesa_persistent_shader_verify(struct remesa_device *device,void *token)
{
 struct remesa_shader_request r;
 struct remesa_persistent_shader_backing backing;
 int e=remesa_persistent_shader_snapshot(device,token,&r,&backing);if(e)return e;
 struct persistent *d=device->priv;
 struct shader_upload *s=*shader_link(d,token);
 struct remesa_shader_image_view view;
 e=remesa_shader_image_inspect(s->image,&view);if(e)return e;
 e=stage_lease(d,s,false);
 if(e==EPROTO)return shader_quarantine(d,s);
 if(!e && s->gpu) {
  if(!s->gpu->live||s->gpu->quarantined||!s->gpu->shader_program||
     !s->gpu->pinned||!s->gpu->gpu_mapped||s->gpu->cpu_mapped)return EPROTO;
  e=owned_image_lease(d,s->gpu,s->request.data,s->request.byte_count,false);
  if(e==EPROTO)return shader_quarantine(d,s);
 }
 if(!e && s->pds_gpu.gpu) {
  struct memory *b=s->pds_gpu.gpu;
  if(!b->live||b->quarantined||!b->shader_pds||!b->pinned||!b->gpu_mapped||b->cpu_mapped)
   return EPROTO;
  e=owned_image_lease(d,b,s->pds_gpu.image,sizeof(s->pds_gpu.image),false);
  if(e==EPROTO)return shader_quarantine(d,s);
 }
 if(!e && s->geometry.snapshot) {
  if(!s->geometry.staged || !s->geometry.pinned || !s->geometry.handle ||
     s->geometry.leased)return EPROTO;
  e=geometry_lease(d,s,false);
  if(e==EPROTO)return shader_quarantine(d,s);
 }
 if(!e && s->pds.data[0]) {
  if(!s->pds.staged||!s->pds.pinned||!s->pds.handle||s->pds.leased)return EPROTO;
  e=pds_lease(d,s,false);
  if(e==EPROTO)return shader_quarantine(d,s);
 }
 return e;
}
int remesa_persistent_prepare_vertex_candidate(struct remesa_device *device,
 const struct remesa_shader_draw_programs *programs,
 const struct remesa_usse_vertex_mapping *mapping,void **out)
{
 if(!device||device->ops!=&device_ops||!device->priv||!programs||!mapping||!out)return EINVAL;
 if(overlaps(out,sizeof(*out),programs,sizeof(*programs))||
    overlaps(out,sizeof(*out),mapping,sizeof(*mapping)))return EINVAL;
 int e=remesa_persistent_shader_output_check(device,out,sizeof(*out));if(e)return e;
 if(*out)return EBUSY;
 return prepare_candidate(device->priv,programs,mapping,NULL,out);
}

int remesa_persistent_prepare_image(struct remesa_device *device,
 const struct remesa_shader_image *image,void **out)
{
 if(!device||device->ops!=&device_ops||!device->priv||!image||!out)return EINVAL;
 int error=remesa_persistent_shader_output_check(device,out,sizeof(*out));if(error)return error;
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

/* These APIs reuse the existing BO owner and VA reservation ledger. They do
 * not establish a Z surface's hardware visibility or add it to a job. */
static int depth_output_check(const struct remesa_device *device,void *out,size_t bytes)
{
 int e=remesa_persistent_shader_output_check(device,out,bytes);if(e)return e;
 struct persistent *d=device->priv;
 if(d->failed)return EIO;
 for(unsigned i=0;i<REMESA_PERSISTENT_BUFFERS;i++)
  if(d->buffers[i].live&&d->buffers[i].cpu_mapped)return EBUSY;
 return 0;
}
int remesa_persistent_allocate_placed(struct remesa_device *device,size_t bytes,
 enum remesa_persistent_va_class kind,void **out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(*out)return EBUSY;
 if((unsigned)kind>=REMESA_VA_CLASS_COUNT)return EINVAL;
 return allocate_class(device->priv,bytes,out,kind);
}
int remesa_persistent_memory_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_memory_backing *out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct memory *b=object;
 if(!owns_memory(d,b)||!b->live||!b->owner)return EINVAL;
 if(b->quarantined||!b->pinned||!b->gpu_mapped)return EIO;
 struct remesa_resource resource;
 if(remesa_registry_snapshot(&d->registry,b->token,&resource)!=REMESA_OK)return ESTALE;
 if(resource.state!=REMESA_MAPPED||b->refs!=1)return EBUSY;
 const struct remesa_persistent_memory_backing value={
  .owner=d->cfg.context,.generation=b->token.generation,.gpu_va=b->va,
  .handle=b->handle,.logical_bytes=b->bytes,.allocation_bytes=b->allocated};
 *out=value;return 0;
}
int remesa_persistent_render_target_snapshot(const struct remesa_device *device,
 void *object,uint32_t width,uint32_t height,uint32_t pitch,
 struct remesa_persistent_render_target_backing *out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct memory *b=object;
 if(!owns_memory(d,b)||!b->live||!b->owner)return EINVAL;
 if(b->linked_dpm||b->linked_background||b->linked_targets||b->linked_ta||b->zclear_prefix||b->linked_body||b->scene_hw||b->depth_backing||b->isp_update||b->raw_geometry||b->shader_program||
    b->shader_pds||b->scene_image||b->control_image)return ENOTSUP;
 if(b->quarantined||!b->pinned||!b->gpu_mapped)return EIO;
 struct remesa_resource resource;
 if(remesa_registry_snapshot(&d->registry,b->token,&resource)!=REMESA_OK)return ESTALE;
 if(resource.state!=REMESA_MAPPED||b->refs!=1)return EBUSY;
 struct remesa_persistent_render_target_backing result={
  .owner=d->cfg.context,.generation=b->token.generation,.gpu_va=b->va,
  .handle=b->handle,.allocation_bytes=b->allocated};
 const struct remesa_render_target_request request={width,height,pitch,b->va,b->bytes};
 e=remesa_render_target_encode_linear_bgra8(&request,&result.state);if(e)return e;
 *out=result;return 0;
}
int remesa_persistent_scene_allocate(struct remesa_device *device,uint32_t width,
 uint32_t height,void **out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(*out)return EBUSY;
 struct remesa_scene_layout layout;
 e=remesa_scene_layout_encode(width,height,0,&layout);if(e)return e;
 struct persistent *d=device->priv;void *object=NULL;
 e=allocate_class(d,layout.required_bytes,&object,REMESA_VA_SURFACE);if(e)return e;
 struct memory *b=object;const uint8_t zero=0;
 b->scene_hw=true;b->scene_layout=layout;
 b->depth_width=width;b->depth_height=height;
 /* owned_image_lease initializes and checks the entire rounded allocation,
  * including padding; using one explicit zero avoids NULL zero-length copies. */
 e=owned_image_lease(d,b,&zero,1,true);if(e)goto fail;
 e=owned_image_lease(d,b,&zero,1,false);if(e)goto fail;
 e=d->cfg.ops->sync_for_device(d->cfg.priv,b->handle);
 if(e){memory_error(d,b,EOWNERDEAD);e=EOWNERDEAD;goto fail;}
 b->readable=true;*out=b;return 0;
fail:
 destroy_bo(d,b);return b->live?EOWNERDEAD:e;
}
int remesa_persistent_scene_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_scene_backing *out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct memory *b=object;
 if(!owns_memory(d,b)||!b->live||!b->owner||!b->scene_hw)return EINVAL;
 if(b->quarantined||!b->pinned||!b->gpu_mapped||!b->readable)return EIO;
 struct remesa_resource resource;
 if(remesa_registry_snapshot(&d->registry,b->token,&resource)!=REMESA_OK)return ESTALE;
 if(resource.state!=REMESA_MAPPED||b->refs!=1)return EBUSY;
 struct remesa_persistent_scene_backing result={.owner=d->cfg.context,
  .generation=b->token.generation,.gpu_va=b->va,
  .region_gpu_va=b->va+b->scene_layout.cookie[7],.handle=b->handle,
  .width=b->depth_width,.height=b->depth_height,.allocation_bytes=b->allocated,
  .layout=b->scene_layout};
 *out=result;return 0;
}
int remesa_persistent_depth_allocate(struct remesa_device *device,uint32_t width,
 uint32_t height,uint32_t cpp,void **out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(*out)return EBUSY;
 if(!width||!height||(cpp!=2&&cpp!=4))return EINVAL;
 if(width>2048||height>2048)return ENOTSUP;
 struct persistent *d=device->priv;
 /* Check the selected allocation range, not the endpoint root: programs
  * and Z may now belong to distinct ranges of the same owner. */
 if(placement_range(d,REMESA_VA_SURFACE).start<UINT64_C(0x40000000))return ENOTSUP;
 struct remesa_resource_layout layout;
 e=remesa_resource_layout(&device->resource_limits,(width+31u)&~31u,
                          (height+31u)&~31u,cpp,&layout);if(e)return e;
 uint32_t pitch=layout.stride/cpp;
 if(layout.stride%cpp||pitch>2048||(pitch&31u))return ENOTSUP;
 void *object=NULL;e=allocate_class(d,layout.bytes,&object,REMESA_VA_SURFACE);if(e)return e;
 struct memory *b=object;
 const struct remesa_depth_surface_request request={width,height,pitch,cpp,
  REMESA_DEPTH_SURFACE_UNDEFINED,b->va,b->allocated};
 struct remesa_depth_surface_state state;
 e=remesa_depth_surface_encode(&request,&state);
 if(e) {
  destroy_bo(d,b);
  return b->live&&b->quarantined?EOWNERDEAD:e;
 }
 b->depth_backing=true;b->depth_width=width;b->depth_height=height;
 b->depth_pitch=pitch;b->depth_cpp=cpp;
 *out=b;return 0;
}
int remesa_persistent_depth_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_depth_backing *out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct memory *b=object;
 if(!owns_memory(d,b))return EINVAL;
 if(!b->live||!b->owner||!b->depth_backing)return EINVAL;
 if(b->quarantined||!b->pinned||!b->gpu_mapped)return EIO;
 struct remesa_resource resource;
 if(remesa_registry_snapshot(&d->registry,b->token,&resource)!=REMESA_OK)return ESTALE;
 if(resource.state!=REMESA_MAPPED||b->refs!=1)return EBUSY;
 struct remesa_persistent_depth_backing result={
  .owner=d->cfg.context,.generation=b->token.generation,.gpu_va=b->va,
  .handle=b->handle,.width=b->depth_width,.height=b->depth_height,
  .pitch_pixels=b->depth_pitch,.cpp=b->depth_cpp,.allocation_bytes=b->allocated};
 const struct remesa_depth_surface_request request={b->depth_width,b->depth_height,
  b->depth_pitch,b->depth_cpp,REMESA_DEPTH_SURFACE_UNDEFINED,b->va,b->allocated};
 e=remesa_depth_surface_encode(&request,&result.initial_surface);if(e)return e;
 *out=result;return 0;
}

enum { ISP_UPDATE_STATE=0,ISP_UPDATE_USE=16,ISP_UPDATE_PDS=32,
       ISP_UPDATE_TA=96,ISP_UPDATE_BYTES=108 };
/* This lease is private to preparation; published objects are never writable.
 * An error with an acquired/uncertain lease is not an ordinary rollback. */
static int isp_update_lease(struct persistent *d,struct memory *b,
 const uint8_t *expected,size_t expected_bytes,bool write)
{
 void *cpu=NULL;const struct remesa_persistent_ops *o=d->cfg.ops;
 int e=o->cpu_map(d->cfg.priv,b->handle,write,&cpu);
 if(e) {
  if(e==EOWNERDEAD||cpu) {
   b->cpu_mapped=true;memory_error(d,b,EOWNERDEAD);return EOWNERDEAD;
  }
  return e;
 }
 b->cpu_mapped=true;
 if(!cpu)e=EPROTO;
 else if(write) {
  memset(cpu,0,b->allocated);memcpy(cpu,expected,expected_bytes);
 } else {
  if(memcmp(cpu,expected,expected_bytes))e=EPROTO;
  for(size_t i=expected_bytes;i<b->allocated;i++)
   if(((const unsigned char *)cpu)[i]){e=EPROTO;break;}
 }
 int released=o->cpu_unmap(d->cfg.priv,b->handle,write);
 if(released) {memory_error(d,b,EOWNERDEAD);return EOWNERDEAD;}
 b->cpu_mapped=false;
 if(!e&&write) {
  int synced=o->sync_for_device(d->cfg.priv,b->handle);
  if(synced){memory_error(d,b,synced);return EOWNERDEAD;}
 }
 return e;
}
static int state_update_prepare(struct remesa_device *device,
 const uint8_t *source_state,size_t source_bytes,unsigned kind,uint32_t bank,
 struct shader_upload *dependency,struct memory *linked_dependency,
 uint32_t coefficient_dwords,void **out)
{
 int e;
 const bool parameters=kind==REMESA_PERSISTENT_STATE_VERTEX_PARAMETERS;
 if(!source_bytes||source_bytes%4||
    (parameters?(!coefficient_dwords||coefficient_dwords>127||source_bytes>512):source_bytes/4>16))return ENOTSUP;
 const bool scene_close=kind==REMESA_PERSISTENT_STATE_SCENE_CLOSE;
 const size_t use_offset=(source_bytes+15u)&~(size_t)15u;
 const size_t pds_offset=(use_offset+16u+31u)&~(size_t)31u;
 const size_t ta_offset=pds_offset+64u;
 const size_t ta_bytes=scene_close?12u:8u;
 /* Preserve all108 legacy bytes, including the final unused zero word. */
 const size_t image_bytes=source_bytes==8?ISP_UPDATE_BYTES:ta_offset+ta_bytes;
 uint8_t image[640]={0};if(image_bytes>sizeof(image))return E2BIG;
 /* For PARAM, source_state holds only coefficient_dwords*4 initialized bytes;
  * source_bytes names the transfer span. Prefix VA derives from the owner. */
 if(!parameters)memcpy(image+ISP_UPDATE_STATE,source_state,source_bytes);
 /* Reuse the independently verified original n=1..16 state auxiliary operands.
  * Numeric classes are not asserted as architectural register-bank names. */
 const struct remesa_usse_cpu_move move={.source={2,0},.destination={1,0},
  .control12=(uint32_t)(source_bytes/4)-1u,.control21=1,.control23=1};
 const struct remesa_usse_cpu_three_source tail={.source={{0,0},{2,(uint32_t)(source_bytes/4)},{2,0}},
  .source_extension={0,1,1},.word0_control21=1,.word1_control18=1};
 size_t written=0;
 if(parameters){
  const struct remesa_usse_cpu_control_ee ee={.record_last=1};
  e=remesa_usse_cpu_control_ee_encode(&ee,image+use_offset,8,&written);
 }else e=remesa_usse_cpu_move_encode(&move,image+use_offset,8,&written);
 if(e)return e;
 if(written!=8)return EPROTO;
 if(!parameters)e=remesa_usse_cpu_three_source_encode(&tail,image+use_offset+8,8,&written);
 if(e)return e;
 if(written!=8)return EPROTO;
 /* Reject invalid banks before allocating backing. Actual bank ownership is
  * not acquired/programmed by a successful numeric entry-plan operation. */
 uint32_t ignored;e=remesa_use_base_offset(bank,&ignored);if(e)return e;
 struct persistent *d=device->priv;void *object=NULL;
 if(linked_dependency&&linked_dependency->refs==UINT32_MAX)return EBUSY;
 e=allocate_class(d,PAGE_BYTES,&object,REMESA_VA_PROGRAM);if(e)return e;
 struct memory *b=object;b->isp_update=true;b->scene_close=scene_close;
 b->state_kind=kind;b->state_bytes=source_bytes;b->state_use_offset=use_offset;
 b->state_parameter_dwords=coefficient_dwords;
 b->state_pds_offset=pds_offset;b->state_ta_offset=ta_offset;b->state_ta_bytes=ta_bytes;
 const struct remesa_use_entry_request use={.allocation_address=b->va,
  .allocation_bytes=b->allocated,.entry_offset=use_offset,.program_bytes=parameters?8:16,
  .bank=bank,.data_master=REMESA_USE_VERTEX};
 struct remesa_use_entry_plan entry;uint32_t fixup;
 e=remesa_use_entry_prepare(&use,&entry);if(e)goto fail;
 e=remesa_use_entry_preview_fixup(&entry,&fixup);if(e)goto fail;
 if(dependency)for(unsigned i=0;i<2;i++)
  if(entry.register_offset==dependency->gpu_entry[i].register_offset&&
     entry.register_word!=dependency->gpu_entry[i].register_word){e=EBUSY;goto fail;}
 if(linked_dependency)for(unsigned i=0;i<2;i++){
  const struct remesa_use_entry_plan *other=&linked_dependency->linked_pds->program->linked_program->entry[i];
  if(entry.register_offset==other->register_offset&&entry.register_word!=other->register_word){e=EBUSY;goto fail;}
 }
 if(parameters){
  const struct remesa_pds_parameter_source_request source={b->va,b->allocated,0,
   coefficient_dwords,(uint32_t)(source_bytes/4)};
  e=remesa_pds_structure_encode_parameter_source(&source,source_state,
   coefficient_dwords*4u,image,source_bytes,&written);
  if(e)goto fail;
  if(written!=source_bytes){e=EPROTO;goto fail;}
 }
 const struct remesa_pds_state_image_request pds={
  .source_allocation_address=b->va,.source_allocation_bytes=b->allocated,
  .source_offset=ISP_UPDATE_STATE,.state_dwords=(uint32_t)(source_bytes/4),.use_lower19=fixup,
  .task_profile=parameters?REMESA_PDS_STATE_TASK_PARAMETERS:REMESA_PDS_STATE_TASK_ISP};
 uint8_t mask[REMESA_PDS_STRUCTURE_STATE_BYTES];
 e=remesa_pds_structure_encode_state_image(&pds,image+pds_offset,mask,
                                         sizeof(mask),&written);if(e)goto fail;
 if(written!=sizeof(mask)){e=EPROTO;goto fail;}
 /* Distinct source headers and command contracts; never turn a word0 ISP
  * update into a final scene state by appending a literal to its reference. */
 if(scene_close) {
  const struct remesa_ta_scene_close_request close={b->va,b->allocated,
   pds_offset,REMESA_PDS_STRUCTURE_STATE_BYTES};
  e=remesa_ta_scene_close_encode(&close,image+ta_offset);
 } else if(parameters)e=remesa_ta_parameter_reference_encode(b->va+pds_offset,
  (uint32_t)(source_bytes/4),image+ta_offset);
 else e=remesa_ta_state_reference_encode(b->va+pds_offset,(uint32_t)(source_bytes/4),false,image+ta_offset);
 if(e)goto fail;
 if(dependency){b->shader_dependency=dependency;++dependency->state_refs;}
 if(linked_dependency){b->linked_dependency=linked_dependency;++linked_dependency->refs;}
 e=isp_update_lease(d,b,image,image_bytes,true);if(e)goto fail;
 e=isp_update_lease(d,b,image,image_bytes,false);if(e)goto fail;
 b->state_use_entry=entry;memcpy(b->state_pds_known,mask,sizeof(mask));
 b->readable=true;*out=b;return 0;
fail:
 destroy_bo(d,b);
 if((linked_dependency||parameters)&&b->live){*out=b;return EOWNERDEAD;}
 if(b->live&&b->quarantined)return dependency?shader_quarantine(d,dependency):EOWNERDEAD;
 return e;
}
int remesa_persistent_vertex_parameters_prepare(struct remesa_device *device,
 const struct remesa_vertex_parameter_values *request,uint32_t bank,void **out)
{
 return remesa_persistent_vertex_parameter_block_prepare(device,
  (const uint32_t *)(const void *)request,16,20,bank,out);
}
int remesa_persistent_vertex_parameter_block_prepare(struct remesa_device *device,
 const uint32_t *coefficient_bits,uint32_t coefficient_dwords,
 uint32_t transfer_dwords,uint32_t bank,void **out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(!coefficient_dwords||coefficient_dwords>127||!transfer_dwords||transfer_dwords>128)return ENOTSUP;
 const size_t bytes_count=coefficient_dwords*4u;
 if(!coefficient_bits||(uintptr_t)coefficient_bits%_Alignof(uint32_t)||
    bytes_count>UINTPTR_MAX-(uintptr_t)coefficient_bits||
    overlaps(coefficient_bits,bytes_count,out,sizeof(*out)))return EINVAL;
 if(*out)return EBUSY;
 uint8_t bytes[508],preview[512];size_t written;
 for(unsigned i=0;i<coefficient_dwords;i++)for(unsigned j=0;j<4;j++)
  bytes[4*i+j]=(uint8_t)(coefficient_bits[i]>>(8*j));
 /* Validate the existing source/DMA policy before allocating. This address is
  * private to the dry encode; the actual source is rebuilt from its live VA. */
 const struct remesa_pds_parameter_source_request check={0x20000000,4096,0,
  coefficient_dwords,transfer_dwords};
 e=remesa_pds_structure_encode_parameter_source(&check,bytes,bytes_count,
  preview,sizeof(preview),&written);if(e)return e;
 return state_update_prepare(device,bytes,transfer_dwords*4u,
  REMESA_PERSISTENT_STATE_VERTEX_PARAMETERS,bank,NULL,NULL,coefficient_dwords,out);
}
int remesa_persistent_vertex_parameters_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_vertex_parameters_backing *out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct remesa_persistent_memory_backing memory;
 e=remesa_persistent_memory_snapshot(device,object,&memory);if(e)return e;
 const struct memory *b=object;
 if(!b->isp_update||b->state_kind!=REMESA_PERSISTENT_STATE_VERTEX_PARAMETERS||
    !b->state_parameter_dwords)return EINVAL;
 if(!b->readable)return EAGAIN;
 struct remesa_persistent_vertex_parameters_backing value={
  .image={.owner=memory.owner,.generation=memory.generation,.gpu_va=memory.gpu_va,
   .handle=memory.handle,.allocation_bytes=memory.allocation_bytes,
   .state_offset=0,.state_bytes=b->state_bytes,.use_offset=b->state_use_offset,.use_bytes=8,
   .pds_offset=b->state_pds_offset,.pds_bytes=60,.ta_offset=b->state_ta_offset,.ta_bytes=8,
   .use_entry=b->state_use_entry},
  .coefficient_dwords=b->state_parameter_dwords,
  .effective_source_dwords=1u+b->state_parameter_dwords,
  .transfer_dwords=(uint32_t)(b->state_bytes/4)};
 memcpy(value.image.pds_known_mask,b->state_pds_known,sizeof(value.image.pds_known_mask));
 *out=value;return 0;
}
int remesa_persistent_isp_update_prepare(struct remesa_device *device,
 const struct remesa_isp_opaque_triangle_request *request,uint32_t bank,void **out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(!request||sizeof(*request)>UINTPTR_MAX-(uintptr_t)request||
    overlaps(out,sizeof(*out),request,sizeof(*request)))return EINVAL;
 if(*out)return EBUSY;
 struct remesa_isp_opaque_triangle_request r;memcpy(&r,request,sizeof(r));
 uint8_t state[8];e=remesa_isp_opaque_triangle_data_encode(&r,state);if(e)return e;
 return state_update_prepare(device,state,8,REMESA_PERSISTENT_STATE_ISP_WORD0,bank,NULL,NULL,0,out);
}
int remesa_persistent_scene_close_prepare(struct remesa_device *device,uint32_t width,
 uint32_t height,uint32_t bank,void **out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(*out)return EBUSY;
 uint8_t state[8];e=remesa_ta_scene_close_data_encode(width,height,state);if(e)return e;
 e=state_update_prepare(device,state,8,REMESA_PERSISTENT_STATE_SCENE_CLOSE,bank,NULL,NULL,0,out);
 if(!e){struct memory *b=*out;b->depth_width=width;b->depth_height=height;}
 return e;
}
static int state_update_snapshot(const struct remesa_device *device,void *object,
 bool scene_close,struct remesa_persistent_isp_update_backing *out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct memory *b=object;
 if(!owns_memory(d,b)||!b->live||!b->owner||!b->isp_update||b->scene_close!=scene_close||b->state_kind!=(scene_close?
    REMESA_PERSISTENT_STATE_SCENE_CLOSE:REMESA_PERSISTENT_STATE_ISP_WORD0))return EINVAL;
 if(b->quarantined||!b->pinned||!b->gpu_mapped||!b->readable)return EIO;
 struct remesa_resource resource;
 if(remesa_registry_snapshot(&d->registry,b->token,&resource)!=REMESA_OK)return ESTALE;
 if(resource.state!=REMESA_MAPPED||b->refs!=1)return EBUSY;
 struct remesa_persistent_isp_update_backing result={.owner=d->cfg.context,
  .generation=b->token.generation,.gpu_va=b->va,.handle=b->handle,
  .allocation_bytes=b->allocated,.state_offset=ISP_UPDATE_STATE,.state_bytes=8,
  .use_offset=ISP_UPDATE_USE,.use_bytes=16,.pds_offset=ISP_UPDATE_PDS,.pds_bytes=60,
  .ta_offset=ISP_UPDATE_TA,.ta_bytes=scene_close?12:8,.use_entry=b->state_use_entry};
 memcpy(result.pds_known_mask,b->state_pds_known,sizeof(result.pds_known_mask));
 *out=result;return 0;
}
int remesa_persistent_isp_update_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_isp_update_backing *out)
{return state_update_snapshot(device,object,false,out);}
int remesa_persistent_scene_close_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_scene_close_backing *out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct remesa_persistent_scene_close_backing result;
 e=state_update_snapshot(device,object,true,&result.image);if(e)return e;
 *out=result;return 0;
}

static int owned_fragment_reference(const struct remesa_device *device,void *token,
 bool clear,struct remesa_ta_fragment_binding_request *out,struct shader_upload **owner)
{
 struct remesa_persistent_shader_pds_gpu_backing pds;
 int e=remesa_persistent_shader_pds_gpu_backing(device,token,&pds);if(e)return e;
 struct persistent *d=device->priv;struct shader_upload *s=*shader_link(d,token);
 if(s->state_refs==UINT32_MAX)return EOVERFLOW;
 e=remesa_persistent_shader_verify((struct remesa_device *)device,token);if(e)return e;
 if(clear) {
  struct remesa_shader_image_view image;struct remesa_fragment_constant color;
  e=remesa_shader_image_inspect(s->image,&image);if(e)return e;
  e=remesa_fragment_constant_prepare(&image.programs->fragment.usse,&color);if(e)return e;
  if(color.argb!=UINT32_C(0xff00ff00))return ENOTSUP;
 }
 struct remesa_ta_fragment_binding_request result={
  .address_window_base=pds.gpu_va&~UINT64_C(0x0fffffff),
  .profile=clear?REMESA_TA_FRAGMENT_SCENE_ZCLEAR:REMESA_TA_FRAGMENT_AUTHORED_CONSTANT};
 result.primary=(struct remesa_ta_fragment_pds_reference){pds.gpu_va,pds.allocation_bytes,
  pds.object_offset[1],pds.object_bytes[1],12,0,clear?0u:1u};
 result.secondary=(struct remesa_ta_fragment_pds_reference){pds.gpu_va,pds.allocation_bytes,
  pds.object_offset[2],pds.object_bytes[2],0,0,0};
 *out=result;*owner=s;return 0;
}
static int constant_state_prepare(struct remesa_device *device,void *token,
 const struct remesa_persistent_constant_state_request *request,uint32_t bank,
 bool after_zclear,void **out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(!request||sizeof(*request)>UINTPTR_MAX-(uintptr_t)request||
    overlaps(request,sizeof(*request),out,sizeof(*out)))return EINVAL;
 if(*out)return EBUSY;
 struct remesa_persistent_constant_state_request input;memcpy(&input,request,sizeof(input));
 struct remesa_ta_constant_draw_state_request state={.isp=input.isp,
  .viewport=input.viewport,.scalar_profile=input.scalar_profile};
 struct shader_upload *dependency;
 e=owned_fragment_reference(device,token,false,&state.fragment,&dependency);if(e)return e;
 uint8_t bytes[56];size_t length=sizeof(bytes);
 if(after_zclear)e=remesa_ta_constant_draw_after_zclear_encode(&state,bytes,sizeof(bytes),&length);
 else e=remesa_ta_constant_draw_state_encode(&state,bytes);
 if(e)return e;
 return state_update_prepare(device,bytes,length,after_zclear?REMESA_PERSISTENT_STATE_AFTER_ZCLEAR:
  REMESA_PERSISTENT_STATE_CONSTANT_DRAW,bank,dependency,NULL,0,out);
}
int remesa_persistent_constant_state_prepare(struct remesa_device *device,void *token,
 const struct remesa_persistent_constant_state_request *request,uint32_t bank,void **out)
{return constant_state_prepare(device,token,request,bank,false,out);}
int remesa_persistent_after_zclear_state_prepare(struct remesa_device *device,void *token,
 const struct remesa_persistent_constant_state_request *request,uint32_t bank,void **out)
{return constant_state_prepare(device,token,request,bank,true,out);}
int remesa_persistent_fragment_state_prepare(struct remesa_device *device,void *token,
 uint32_t bank,void **out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(*out)return EBUSY;
 struct remesa_ta_fragment_binding_request binding;struct shader_upload *dependency;
 e=owned_fragment_reference(device,token,false,&binding,&dependency);if(e)return e;
 uint8_t bytes[16];e=remesa_ta_fragment_binding_encode(&binding,bytes);if(e)return e;
 return state_update_prepare(device,bytes,sizeof(bytes),REMESA_PERSISTENT_STATE_FRAGMENT_UPDATE,
  bank,dependency,NULL,0,out);
}
int remesa_persistent_zclear_state_prepare(struct remesa_device *device,void *token,
 uint32_t bank,void **out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(*out)return EBUSY;
 struct remesa_ta_fragment_binding_request binding;struct shader_upload *dependency;
 e=owned_fragment_reference(device,token,true,&binding,&dependency);if(e)return e;
 uint8_t bytes[44];e=remesa_ta_scene_zclear_state_encode(&binding,bytes);if(e)return e;
 return state_update_prepare(device,bytes,sizeof(bytes),REMESA_PERSISTENT_STATE_SCENE_ZCLEAR,
  bank,dependency,NULL,0,out);
}
int remesa_persistent_scene_state_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_scene_state_backing *out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct memory *b=object;
 if(!owns_memory(d,b)||!b->live||!b->owner||!b->isp_update||
    (b->state_kind!=REMESA_PERSISTENT_STATE_CONSTANT_DRAW&&
     b->state_kind!=REMESA_PERSISTENT_STATE_SCENE_ZCLEAR&&
     b->state_kind!=REMESA_PERSISTENT_STATE_AFTER_ZCLEAR&&
     b->state_kind!=REMESA_PERSISTENT_STATE_FRAGMENT_UPDATE))return EINVAL;
 if(b->quarantined||!b->pinned||!b->gpu_mapped||!b->readable)return EIO;
 struct shader_upload *s=b->shader_dependency;
 /* A deleted public shader token may still have a valid internal state hold. */
 if(!s||!s->state_refs||s->quarantined||!s->gpu||!s->pds_gpu.gpu||
    !s->gpu->live||!s->pds_gpu.gpu->live)return ESTALE;
 struct remesa_resource resource;
 if(remesa_registry_snapshot(&d->registry,b->token,&resource)!=REMESA_OK)return ESTALE;
 if(resource.state!=REMESA_MAPPED||b->refs!=1)return EBUSY;
 struct remesa_persistent_scene_state_backing result={.kind=b->state_kind,
  .shader_owner=d->cfg.context,.shader_generation=s->request.generation,
  .shader_code_handle=s->gpu->handle,.shader_pds_handle=s->pds_gpu.gpu->handle};
 result.image=(struct remesa_persistent_isp_update_backing){.owner=d->cfg.context,
  .generation=b->token.generation,.gpu_va=b->va,.handle=b->handle,
  .allocation_bytes=b->allocated,.state_offset=0,.state_bytes=b->state_bytes,
  .use_offset=b->state_use_offset,.use_bytes=16,.pds_offset=b->state_pds_offset,.pds_bytes=60,
  .ta_offset=b->state_ta_offset,.ta_bytes=b->state_ta_bytes,.use_entry=b->state_use_entry};
 memcpy(result.image.pds_known_mask,b->state_pds_known,sizeof(result.image.pds_known_mask));
 *out=result;return 0;
}

int remesa_persistent_depth_scene_prepare(struct remesa_device *device,
 const struct remesa_depth_scene_image_request *request,void **out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(!request||sizeof(*request)>UINTPTR_MAX-(uintptr_t)request||
    overlaps(request,sizeof(*request),out,sizeof(*out)))return EINVAL;
 struct remesa_depth_scene_image_request r;memcpy(&r,request,sizeof(r));
 if(r.gpu_va||r.allocation_bytes)return EINVAL;
 for(unsigned i=0;i<3;i++) {
  struct remesa_shader_image_view v;
  e=remesa_shader_image_inspect(r.programs[i],&v);if(e)return e;
  const struct remesa_shader_storage_view *payloads[4]={&v.vertex,&v.fragment,&v.fragment_pds,&v.fragment_pds_mask};
  if(overlaps(out,sizeof(*out),v.programs,sizeof(*v.programs)))return EINVAL;
  for(unsigned j=0;j<4;j++)if(overlaps(out,sizeof(*out),payloads[j]->bytes,payloads[j]->byte_length))return EINVAL;
 }
 if(*out)return EBUSY;
 /* The provider supplies this address after allocation; external numeric
  * mappings cannot be promoted through this owner API. */
 struct persistent *d=device->priv;void *object=NULL;
 e=allocate_class(d,REMESA_DEPTH_SCENE_IMAGE_BYTES,&object,REMESA_VA_PROGRAM);if(e)return e;
 struct memory *b=object;
 b->scene_image=calloc(1,sizeof(*b->scene_image));
 if(!b->scene_image){e=ENOMEM;goto fail;}
 r.gpu_va=b->va;r.allocation_bytes=b->allocated;
 e=remesa_depth_scene_image_encode(&r,b->scene_image);if(e)goto fail;
 e=owned_image_lease(d,b,b->scene_image->bytes,REMESA_DEPTH_SCENE_IMAGE_BYTES,true);if(e)goto fail;
 e=owned_image_lease(d,b,b->scene_image->bytes,REMESA_DEPTH_SCENE_IMAGE_BYTES,false);if(e)goto fail;
 e=d->cfg.ops->sync_for_device(d->cfg.priv,b->handle);
 if(e){memory_error(d,b,EOWNERDEAD);e=EOWNERDEAD;goto fail;}
 b->depth_width=r.width;b->depth_height=r.height;b->readable=true;*out=b;return 0;
fail:
 destroy_bo(d,b);return b->live?EOWNERDEAD:e;
}
int remesa_persistent_depth_scene_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_depth_scene_backing *out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct memory *b=object;
 if(!owns_memory(d,b)||!b->live||!b->owner||!b->scene_image)return EINVAL;
 if(b->quarantined||!b->pinned||!b->gpu_mapped||!b->readable)return EIO;
 struct remesa_resource resource;
 if(remesa_registry_snapshot(&d->registry,b->token,&resource)!=REMESA_OK)return ESTALE;
 if(resource.state!=REMESA_MAPPED||b->refs!=1)return EBUSY;
 struct remesa_persistent_depth_scene_backing result={.owner=d->cfg.context,
  .generation=b->token.generation,.gpu_va=b->va,.handle=b->handle,
  .width=b->depth_width,.height=b->depth_height,.allocation_bytes=b->allocated,
  .vertex_profile=(uint32_t)b->scene_image->vertex_profile,
  .used_bytes=b->scene_image->used_bytes,.ta_offset=b->scene_image->ta_offset,
  .ta_bytes=b->scene_image->ta_bytes};
 memcpy(result.use_entries,b->scene_image->use_entries,sizeof(result.use_entries));
 *out=result;return 0;
}
int remesa_persistent_depth_scene_verify(struct remesa_device *device,void *object)
{
 struct remesa_persistent_depth_scene_backing snapshot;
 int e=remesa_persistent_depth_scene_snapshot(device,object,&snapshot);if(e)return e;
 struct persistent *d=device->priv;struct memory *b=object;
 e=owned_image_lease(d,b,b->scene_image->bytes,REMESA_DEPTH_SCENE_IMAGE_BYTES,false);
 if(e)memory_error(d,b,e);
 return e;
}

/* Typed immutable control DATA. Numeric references are checked by the encoders;
 * the composing LAB owner retains and validates the referenced BOs separately.
 * This operation grants neither job holds nor permission to execute. */
int remesa_persistent_control_prepare(struct remesa_device *device,
 const struct remesa_raster_control_request *r,
 const struct remesa_raster_object_reference *ta,void **out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(!r||!ta||overlaps(r,sizeof(*r),out,sizeof(*out))||
    overlaps(ta,sizeof(*ta),out,sizeof(*out)))return EINVAL;
 if(*out)return EBUSY;
 uint8_t bytes[REMESA_CONTROL_IMAGE_BYTES];
 e=remesa_raster_control_encode(r,bytes);if(e)return e;
 e=remesa_ta_control_encode(ta,bytes+REMESA_RASTER_CONTROL_BYTES);if(e)return e;
 struct persistent *d=device->priv;void *object=NULL;
 e=allocate_class(d,sizeof(bytes),&object,REMESA_VA_DEFAULT);if(e)return e;
 struct memory *b=object;
 b->control_image=malloc(sizeof(bytes));if(!b->control_image){e=ENOMEM;goto fail;}
 memcpy(b->control_image,bytes,sizeof(bytes));
 e=owned_image_lease(d,b,b->control_image,sizeof(bytes),true);if(e)goto fail;
 e=owned_image_lease(d,b,b->control_image,sizeof(bytes),false);if(e)goto fail;
 e=d->cfg.ops->sync_for_device(d->cfg.priv,b->handle);
 if(e){memory_error(d,b,EOWNERDEAD);e=EOWNERDEAD;goto fail;}
 b->readable=true;*out=b;return 0;
fail:destroy_bo(d,b);return b->live?EOWNERDEAD:e;
}
int remesa_persistent_control_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_memory_backing *out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct memory *b=object;
 if(!owns_memory(d,b)||!b->live||!b->control_image)return EINVAL;
 if(!b->readable)return EIO;
 return remesa_persistent_memory_snapshot(device,object,out);
}
int remesa_persistent_control_verify(struct remesa_device *device,void *object)
{
 struct remesa_persistent_memory_backing snapshot;
 int e=remesa_persistent_control_snapshot(device,object,&snapshot);if(e)return e;
 struct persistent *d=device->priv;struct memory *b=object;
 e=owned_image_lease(d,b,b->control_image,REMESA_CONTROL_IMAGE_BYTES,false);
 if(e)memory_error(d,b,e);
 return e;
}
static bool gears_attribute_profile(uint32_t profile)
{
 return profile==REMESA_PDS_SELECTED_VS_GEARS_ATTRIBUTE_PADDED3||
  profile==REMESA_PDS_SELECTED_VS_GEARS_ATTRIBUTE_CLAMPED_PADDED3;
}
static int linked_program_prepare(struct remesa_device *device,
 const struct remesa_shader_linkage *link,const uint32_t banks[2],bool gears,void **out)
{
 if(!link||!banks||!out||(uintptr_t)banks%_Alignof(uint32_t)||
    overlaps(out,sizeof(*out),link,sizeof(*link))||
    overlaps(out,sizeof(*out),banks,2*sizeof(*banks)))return EINVAL;
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(*out)return EBUSY;
 e=remesa_shader_linkage_validate(link);if(e)return e;
 if(gears&&remesa_shader_ir_fragment_color_input(&link->fragment.semantic))return ENOTSUP;
 const uint32_t bank[2]={banks[0],banks[1]};uint32_t registers[2];
 for(unsigned i=0;i<2;i++){e=remesa_use_base_offset(bank[i],&registers[i]);if(e)return e;}
 if(registers[0]==registers[1])return EBUSY;
 const struct remesa_shader_program *programs[2]={&link->vertex,&link->fragment};
 struct remesa_shader_selected_resources resources[2];
 /* This is a software staging bound, not an SGX instruction/resource limit. */
 uint8_t image[512]={0};size_t offset[2],length[2],used=0;
 for(unsigned i=0;i<2;i++){
  e=gears&&!i?remesa_shader_program_gears_attribute_resources(programs[i],&resources[i]):
   remesa_shader_program_selected_resources(programs[i],&resources[i]);if(e)return e;
  if(resources[i].host_alignment!=32||!resources[i].logical_use_bytes||!resources[i].reserved_use_bytes||
     resources[i].logical_use_bytes>resources[i].reserved_use_bytes)return EPROTO;
  used=(used+31u)&~(size_t)31u;offset[i]=used;
  if(used>sizeof(image)||resources[i].reserved_use_bytes>sizeof(image)-used)return E2BIG;
  e=remesa_shader_program_encode(programs[i],image+used,resources[i].reserved_use_bytes,&length[i]);
  if(e)return e;
  if(length[i]!=resources[i].logical_use_bytes)return EPROTO;
  used+=resources[i].reserved_use_bytes;
 }
 struct linked_program *owned=calloc(1,sizeof(*owned));if(!owned)return ENOMEM;
 owned->linkage=*link;
 for(unsigned i=0;i<2;i++){
  owned->offset[i]=offset[i];owned->bytes[i]=length[i];owned->reserved[i]=resources[i].reserved_use_bytes;
  owned->resources[i]=resources[i];
 }
 struct persistent *d=device->priv;void *object=NULL;const size_t before=d->allocated;
 e=allocate_class(d,used,&object,REMESA_VA_PROGRAM);
 if(e){free(owned);return d->failed||d->allocated!=before?EOWNERDEAD:e;}
 struct memory *b=object;b->shader_program=true;b->linked_program=owned;
 struct remesa_use_entry_request requests[2];
 for(unsigned i=0;i<2;i++)requests[i]=(struct remesa_use_entry_request){
  .allocation_address=b->va,.allocation_bytes=b->allocated,.entry_offset=offset[i],
  .program_bytes=length[i],.bank=bank[i],.data_master=i?REMESA_USE_PIXEL:REMESA_USE_VERTEX};
 e=remesa_use_entry_pair_prepare(requests,owned->entry);if(e)goto fail;
 e=owned_image_lease(d,b,image,used,true);if(e)goto fail;
 e=owned_image_lease(d,b,image,used,false);if(e)goto fail;
 e=d->cfg.ops->sync_for_device(d->cfg.priv,b->handle);
 if(e){memory_error(d,b,EOWNERDEAD);e=EOWNERDEAD;goto fail;}
 b->readable=true;*out=b;return 0;
fail:
 if(b->quarantined){*out=b;return EOWNERDEAD;}
 destroy_bo(d,b);
 if(b->live){*out=b;return EOWNERDEAD;}
 return e;
}
int remesa_persistent_linked_program_prepare(struct remesa_device *device,
 const struct remesa_shader_linkage *link,const uint32_t banks[2],void **out)
{return linked_program_prepare(device,link,banks,false,out);}
int remesa_persistent_gears_attribute_program_prepare(struct remesa_device *device,
 const struct remesa_shader_linkage *link,const uint32_t banks[2],void **out)
{return linked_program_prepare(device,link,banks,true,out);}
int remesa_persistent_linked_program_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_linked_program_backing *out)
{
 struct remesa_persistent_memory_backing memory;
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 e=remesa_persistent_memory_snapshot(device,object,&memory);if(e)return e;
 struct memory *b=object;
 if(!b->shader_program||!b->linked_program)return EINVAL;
 if(!b->readable)return EAGAIN;
 const struct linked_program *owned=b->linked_program;
 struct remesa_persistent_linked_program_backing value={
  .owner=memory.owner,.generation=memory.generation,.gpu_va=memory.gpu_va,
  .handle=memory.handle,.allocation_bytes=memory.allocation_bytes};
 for(unsigned i=0;i<2;i++){
  value.program_offset[i]=owned->offset[i];value.program_bytes[i]=owned->bytes[i];
  value.reserved_bytes[i]=owned->reserved[i];value.entry[i]=owned->entry[i];
 }
 *out=value;return 0;
}

static int linked_live(struct persistent *d,struct memory *b,bool owner)
{
 if(!owns_memory(d,b)||!b->live||!b->refs||(owner&&!b->owner))return EINVAL;
 if(b->quarantined)return EIO;
 if(!b->pinned||!b->gpu_mapped||!b->readable||b->cpu_mapped)return EBUSY;
 struct remesa_resource r;
 if(remesa_registry_snapshot(&d->registry,b->token,&r)!=REMESA_OK)return ESTALE;
 return r.state==REMESA_MAPPED?0:EBUSY;
}
static void linked_program_metadata(const struct persistent *d,const struct memory *b,
 struct remesa_persistent_linked_program_backing *out)
{
 const struct linked_program *p=b->linked_program;
 *out=(struct remesa_persistent_linked_program_backing){.owner=d->cfg.context,
  .generation=b->token.generation,.gpu_va=b->va,.handle=b->handle,.allocation_bytes=b->allocated};
 for(unsigned i=0;i<2;i++){
  out->program_offset[i]=p->offset[i];out->program_bytes[i]=p->bytes[i];
  out->reserved_bytes[i]=p->reserved[i];out->entry[i]=p->entry[i];
 }
}
static void linked_geometry_metadata(const struct persistent *d,const struct memory *b,
 struct remesa_persistent_linked_geometry_backing *out)
{
 const struct linked_geometry *p=b->linked_geometry;
 *out=(struct remesa_persistent_linked_geometry_backing){.owner=d->cfg.context,
  .generation=b->token.generation,.gpu_va=b->va,.handle=b->handle,
  .vertex_count=p->vertex_count,.index_count=p->index_count,
  .min_vertex=p->min_vertex,.max_vertex=p->max_vertex,.stride_bytes=48,
  .allocation_bytes=b->allocated,.vertex_bytes=p->vertex_bytes,
  .index_offset=p->index_offset,.index_bytes=p->index_bytes};
}
int remesa_persistent_linked_geometry_prepare(struct remesa_device *device,
 const struct remesa_linked_geometry_input *input,void **out)
{
 if(!input||!out||overlaps(input,sizeof(*input),out,sizeof(*out)))return EINVAL;
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(*out)return EBUSY;
 const struct remesa_linked_geometry_input r=*input;
 if(!r.vertices||!r.vertex_count||r.vertex_count>REMESA_LINKED_MAX_VERTICES)return EINVAL;
 const size_t vertex_bytes=(size_t)r.vertex_count*48;
 if(r.vertex_bytes<vertex_bytes)return ENOSPC;
 if(r.vertex_bytes>UINTPTR_MAX-(uintptr_t)r.vertices||
    overlaps(r.vertices,r.vertex_bytes,out,sizeof(*out)))return EINVAL;
 uint32_t count=r.indices?r.index_count:r.vertex_count;
 if(!count||count>65535||count%3)return ENOTSUP;
 if(!r.indices&&(r.index_count||r.index_bytes))return EINVAL;
 const size_t index_bytes=(size_t)count*2;
 if(r.indices&&(r.index_bytes<index_bytes||r.index_bytes>UINTPTR_MAX-(uintptr_t)r.indices||
    overlaps(r.indices,r.index_bytes,out,sizeof(*out))))return EINVAL;
 uint32_t min=UINT32_MAX,max=0;
 /* Validate all index ranges before allocating or touching a provider. */
 for(uint32_t i=0;i<count;i++){
  uint16_t value=(uint16_t)i;
  if(r.indices)memcpy(&value,(const unsigned char *)r.indices+2*i,2);
  if(value>=r.vertex_count)return ERANGE;
  if(value<min)min=value;if(value>max)max=value;
 }
 size_t used=vertex_bytes+index_bytes;
 uint8_t *image=calloc(1,used);if(!image)return ENOMEM;
 struct linked_geometry *owned=calloc(1,sizeof(*owned));if(!owned){free(image);return ENOMEM;}
 *owned=(struct linked_geometry){r.vertex_count,count,min,max,vertex_bytes,vertex_bytes,index_bytes};
 memcpy(image,r.vertices,vertex_bytes);
 for(uint32_t i=0;i<count;i++){
  uint16_t value=(uint16_t)i;
  if(r.indices)memcpy(&value,(const unsigned char *)r.indices+2*i,2);
  image[vertex_bytes+2*i]=(uint8_t)value;image[vertex_bytes+2*i+1]=(uint8_t)(value>>8);
 }
 struct persistent *d=device->priv;void *object=NULL;const size_t before=d->allocated;
 e=allocate_class(d,used,&object,REMESA_VA_DEFAULT);
 if(e){free(image);free(owned);return d->failed||d->allocated!=before?EOWNERDEAD:e;}
 struct memory *b=object;b->raw_geometry=true;b->linked_geometry=owned;
 e=owned_image_lease(d,b,image,used,true);if(e)goto fail;
 e=owned_image_lease(d,b,image,used,false);if(e)goto fail;
 e=d->cfg.ops->sync_for_device(d->cfg.priv,b->handle);
 if(e){memory_error(d,b,EOWNERDEAD);e=EOWNERDEAD;goto fail;}
 free(image);b->readable=true;*out=b;return 0;
fail:
 free(image);
 if(b->quarantined){*out=b;return EOWNERDEAD;}
 destroy_bo(d,b);if(b->live){*out=b;return EOWNERDEAD;}return e;
}
int remesa_persistent_linked_geometry_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_linked_geometry_backing *out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct memory *b=object;
 e=linked_live(d,b,true);if(e)return e;
 if(!b->raw_geometry||!b->linked_geometry)return EINVAL;
 linked_geometry_metadata(d,b,out);return 0;
}
static int linked_pds_prepare(struct remesa_device *device,void *program,
 void *geometry,void *parameters,void **out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(*out)return EBUSY;
 struct persistent *d=device->priv;struct memory *code=program,*raw=geometry;
 e=linked_live(d,code,true);if(e)return e;
 e=linked_live(d,raw,true);if(e)return e;
 if(code==raw||!code->linked_program||!code->shader_program||
    !raw->linked_geometry||!raw->raw_geometry)return EINVAL;
 const struct linked_program *cp=code->linked_program;
 const struct linked_geometry *gp=raw->linked_geometry;
 const bool gears=parameters!=NULL;
 if((gears?!gears_attribute_profile(cp->resources[0].profile):cp->resources[0].profile!=REMESA_PDS_SELECTED_VS_WHITE_LAMBERT3)||
    cp->resources[1].profile!=REMESA_PDS_SELECTED_FS_INPUT_COL0)return ENOTSUP;
 struct memory *params=parameters;
 if(gears){
  struct remesa_persistent_vertex_parameters_backing pb;
  e=remesa_persistent_vertex_parameters_snapshot(device,params,&pb);if(e)return e;
  if(params==code||params==raw||pb.coefficient_dwords!=36||pb.effective_source_dwords!=37||pb.transfer_dwords!=40)return ENOTSUP;
  for(unsigned i=0;i<2;i++)if(pb.image.use_entry.register_offset==cp->entry[i].register_offset&&
   pb.image.use_entry.register_word!=cp->entry[i].register_word)return EBUSY;
  if(params->refs==UINT32_MAX)return EBUSY;
 }
 if(code->refs==UINT32_MAX||raw->refs==UINT32_MAX)return EBUSY;
 if(d->linked_pds_serial==UINT64_MAX)return EOVERFLOW;
 struct linked_pds *owned=calloc(1,sizeof(*owned));if(!owned)return ENOMEM;
 void *object=NULL;const size_t before=d->allocated;
 e=allocate_class(d,288,&object,REMESA_VA_PROGRAM);
 if(e){free(owned);return d->failed||d->allocated!=before?EOWNERDEAD:e;}
 struct memory *b=object;b->shader_pds=true;b->linked_pds=owned;
 owned->program=code;owned->geometry=raw;code->refs++;raw->refs++;
 if(params){owned->parameters=params;params->refs++;}
 const uint64_t serial=++d->linked_pds_serial;
 uint8_t image[288]={0};
 for(unsigned i=0;i<2;i++){
  struct remesa_pds_selected_request request={.profile=cp->resources[i].profile,
   .use_bank=cp->entry[i].bank,.semantic_identity=cp->resources[i].semantic_identity,
   .use={.allocation_cookie=code->handle,.owner_cookie=d->cfg.context,
    .mapping_generation=code->token.generation,.address=code->va,.offset=cp->offset[i],
    .bytes=cp->reserved[i],.allocation_bytes=code->allocated}};
  const struct remesa_pds_block_request block={.operation=serial,.generation=b->token.generation,
   .allocation={.allocation_cookie=b->handle,.owner_cookie=d->cfg.context,
    .mapping_generation=b->token.generation,.address=b->va,.offset=i?192:0,
    .bytes=i?64:192,.allocation_bytes=b->allocated},
   .data_bytes=48,.code_bytes=i?12:16,.code_alignment=16};
  e=remesa_pds_request_from_block(&block,&request.storage);if(e)goto fail;
  if(!i){
   request.vertex_source=(struct remesa_pds_binding){.allocation_cookie=raw->handle,
    .owner_cookie=d->cfg.context,.mapping_generation=raw->token.generation,
    .address=raw->va,.bytes=gp->vertex_bytes,.allocation_bytes=raw->allocated};
   request.source_records=gp->vertex_count;
  }
  e=remesa_pds_selected_prepare(&request,&owned->plans[i]);if(e)goto fail;
  if(owned->plans[i].use_committed_bytes!=cp->bytes[i]||
     owned->plans[i].use_reserved_bytes!=cp->reserved[i]||
     owned->plans[i].use_entry.entry_address!=cp->entry[i].entry_address||
     owned->plans[i].use_entry.register_word!=cp->entry[i].register_word){e=EPROTO;goto fail;}
  size_t n=0;const size_t at=i?192:0,capacity=i?64:192;
  e=remesa_pds_selected_encode(&owned->plans[i],image+at,owned->known+at,capacity,&n);if(e)goto fail;
  if(n!=(i?60u:64u)){e=EPROTO;goto fail;}
 }
 e=remesa_pds_selected_pair_validate(owned->plans);if(e)goto fail;
 const struct remesa_pds_code_instruction secondary={
  .kind=REMESA_PDS_STRUCTURE_CPU_EMPTY_SECONDARY_AF,
  .ds0_index=REMESA_PDS_STRUCTURE_NO_CELL,.ds1_index=REMESA_PDS_STRUCTURE_NO_CELL};
 size_t secondary_bytes=0;
 e=remesa_pds_structure_encode_code(&secondary,1,image+256,4,&secondary_bytes);if(e)goto fail;
 if(secondary_bytes!=4){e=EPROTO;goto fail;}
 memset(owned->known+256,0xff,4);
 e=owned_image_lease(d,b,image,sizeof(image),true);if(e)goto fail;
 e=owned_image_lease(d,b,image,sizeof(image),false);if(e)goto fail;
 e=d->cfg.ops->sync_for_device(d->cfg.priv,b->handle);
 if(e){memory_error(d,b,EOWNERDEAD);e=EOWNERDEAD;goto fail;}
 b->readable=true;*out=b;return 0;
fail:
 if(b->quarantined){*out=b;return EOWNERDEAD;}
 destroy_bo(d,b);if(b->live){*out=b;return EOWNERDEAD;}return e;
}
int remesa_persistent_linked_pds_prepare(struct remesa_device *device,void *program,
 void *geometry,void **out)
{return linked_pds_prepare(device,program,geometry,NULL,out);}
int remesa_persistent_gears_attribute_pds_prepare(struct remesa_device *device,
 void *program,void *geometry,void *parameters,void **out)
{if(!parameters)return EINVAL;return linked_pds_prepare(device,program,geometry,parameters,out);}
static int linked_pds_metadata(struct persistent *d,struct memory *b,bool owner,
 struct remesa_persistent_linked_pds_backing *out)
{
 int e=linked_live(d,b,owner);if(e)return e;
 if(!b->linked_pds||!b->shader_pds)return EINVAL;
 const struct linked_pds *p=b->linked_pds;
 e=linked_live(d,p->program,false);if(e)return e;
 e=linked_live(d,p->geometry,false);if(e)return e;
 if(p->parameters){e=linked_live(d,p->parameters,false);if(e)return e;}
 struct remesa_persistent_linked_pds_backing value={.owner=d->cfg.context,
  .generation=b->token.generation,.gpu_va=b->va,.handle=b->handle,.allocation_bytes=b->allocated};
 for(unsigned i=0;i<2;i++){
  value.object_offset[i]=i?192:0;value.image_bytes[i]=i?60:64;
  value.reserved_bytes[i]=i?64:192;value.plans[i]=p->plans[i];
 }
 value.object_offset[2]=256;value.image_bytes[2]=4;value.reserved_bytes[2]=32;
 memcpy(value.known_mask,p->known,sizeof(value.known_mask));
 linked_program_metadata(d,p->program,&value.program);
 linked_geometry_metadata(d,p->geometry,&value.geometry);
 *out=value;return 0;
}
int remesa_persistent_linked_pds_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_linked_pds_backing *out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 return linked_pds_metadata(device->priv,object,true,out);
}
int remesa_persistent_gears_attribute_pds_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_gears_attribute_pds_backing *out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct remesa_persistent_gears_attribute_pds_backing value={0};
 e=linked_pds_metadata(device->priv,object,true,&value.pds);if(e)return e;
 const struct memory *b=object,*p=b->linked_pds->parameters;
 if(!p||!gears_attribute_profile(value.pds.plans[0].request.profile)||
    p->state_kind!=REMESA_PERSISTENT_STATE_VERTEX_PARAMETERS||p->state_parameter_dwords!=36||p->state_bytes!=160)return ENOTSUP;
 struct persistent *d=device->priv;
 value.parameters=(struct remesa_persistent_vertex_parameters_backing){
  .image={.owner=d->cfg.context,.generation=p->token.generation,.gpu_va=p->va,
   .handle=p->handle,.allocation_bytes=p->allocated,.state_bytes=p->state_bytes,
   .use_offset=p->state_use_offset,.use_bytes=8,.pds_offset=p->state_pds_offset,.pds_bytes=60,
   .ta_offset=p->state_ta_offset,.ta_bytes=8,.use_entry=p->state_use_entry},
  .coefficient_dwords=36,.effective_source_dwords=37,.transfer_dwords=40};
 memcpy(value.parameters.image.pds_known_mask,p->state_pds_known,60);
 value.input_dwords=8;value.temporary_dwords=11;value.output_dwords=8;
 *out=value;return 0;
}

int remesa_persistent_linked_index_encode(const struct remesa_device *device,void *pds,
 uint32_t first_index,uint32_t index_count,uint8_t out[20])
{
 int e=depth_output_check(device,out,20);if(e)return e;
 struct remesa_persistent_linked_pds_backing backing;
 e=remesa_persistent_linked_pds_snapshot(device,pds,&backing);if(e)return e;
 const struct remesa_persistent_linked_geometry_backing *g=&backing.geometry;
 if(index_count<3||index_count%3)return ENOTSUP;
 if(first_index>g->index_count||index_count>g->index_count-first_index)return ERANGE;
 /* This closure currently admits only the three independent float4 sources
  * and POSITION4+COLOR0 outputs, already checked at program/PDS preparation. */
 if(g->stride_bytes!=48||backing.plans[0].data_bytes!=48||
    backing.plans[0].request.profile!=REMESA_PDS_SELECTED_VS_WHITE_LAMBERT3||
    backing.plans[1].request.profile!=REMESA_PDS_SELECTED_FS_INPUT_COL0)return ENOTSUP;
 const struct remesa_ta_index_request request={
  .index_bo_gpu_va=g->gpu_va,.index_bo_bytes=g->index_offset+g->index_bytes,
  .index_byte_offset=g->index_offset,.first_index=first_index,.index_count=index_count,
  .pds_bo_gpu_va=backing.gpu_va,.pds_bo_bytes=backing.allocation_bytes,
  .pds_byte_offset=backing.object_offset[0],.pds_object_bytes=backing.image_bytes[0],
  .pds_data_dwords=backing.plans[0].data_bytes/4,
  .descriptor_a_field10=g->stride_bytes/4,.descriptor_b_field38=32};
 return remesa_ta_index_compiled_position_color_triangles_encode(&request,out);
}

static int linked_state_prepare(struct remesa_device *device,void *pds,
 const struct remesa_persistent_linked_state_request *request,uint32_t bank,bool gears,
 bool homogeneous,void **out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(!request||overlaps(request,sizeof(*request),out,sizeof(*out)))return EINVAL;
 if(*out)return EBUSY;
 const struct remesa_persistent_linked_state_request r=*request;
 if(homogeneous&&(!gears||r.viewport.path!=REMESA_TA_VIEWPORT_MATRIX_WINDOW))return ENOTSUP;
 struct remesa_persistent_linked_pds_backing backing;
 e=remesa_persistent_linked_pds_snapshot(device,pds,&backing);if(e)return e;
 if((gears?
     (homogeneous?!gears_attribute_profile(backing.plans[0].request.profile):backing.plans[0].request.profile!=REMESA_PDS_SELECTED_VS_GEARS_ATTRIBUTE_PADDED3):
     backing.plans[0].request.profile!=REMESA_PDS_SELECTED_VS_WHITE_LAMBERT3)||
    backing.plans[1].request.profile!=REMESA_PDS_SELECTED_FS_INPUT_COL0)return ENOTSUP;
 const struct remesa_ta_fragment_binding_request fragment={
  .address_window_base=backing.gpu_va&UINT64_C(0xf0000000),
  .profile=REMESA_TA_FRAGMENT_INPUT_COLOR_CPU,
  .primary={backing.gpu_va,backing.allocation_bytes,backing.object_offset[1],
   backing.image_bytes[1],12,1,0},
  .secondary={backing.gpu_va,backing.allocation_bytes,backing.object_offset[2],
   backing.image_bytes[2],0,0,0}};
 uint8_t isp[8],fs[16],viewport[28],scalar[16],state[60]={0};size_t n=0;
 e=remesa_isp_opaque_triangle_data_encode(&r.isp,isp);if(e)return e;
 e=remesa_ta_fragment_binding_encode(&fragment,fs);if(e)return e;
 e=remesa_ta_window_viewport_encode(&r.viewport,viewport);if(e)return e;
 e=remesa_ta_scalar_groups_encode(REMESA_TA_SCALAR_POSITION4_PRIMARY_COLOR4,
  scalar,sizeof(scalar),&n);if(e)return e;
 if(n!=sizeof(scalar))return EPROTO;
 /* The existing child encoders select disjoint groups; merge their headers
  * once and copy only committed DATA, in the original serializer's order. */
 for(unsigned i=0;i<4;i++)state[i]=isp[i]|fs[i]|viewport[i]|scalar[i];
 memcpy(state+4,isp+4,4);memcpy(state+8,fs+4,12);
 memcpy(state+20,viewport+4,24);memcpy(state+44,scalar+4,12);
 /* Original serializer group1000 follows group800. The preserved physical
  * W=2 witness consumes this final zero word: 15 DWORDs, not 16 padded words.
  * The existing owner derives MOVE14/tail15, DMA15 and TA ceil(60/16)=4.
  */
 if(homogeneous)state[1]|=0x10;
 e=state_update_prepare(device,state,homogeneous?60:56,
  homogeneous?REMESA_PERSISTENT_STATE_GEARS_HOMOGENEOUS_DRAW:
  gears?REMESA_PERSISTENT_STATE_GEARS_DRAW:REMESA_PERSISTENT_STATE_LINKED_DRAW,
  bank,NULL,pds,0,out);
 if(!e){struct memory *b=*out;b->depth_width=r.viewport.framebuffer_width;
  b->depth_height=r.viewport.framebuffer_height;}
 return e;
}
int remesa_persistent_linked_state_prepare(struct remesa_device *device,void *pds,
 const struct remesa_persistent_linked_state_request *request,uint32_t bank,void **out)
{return linked_state_prepare(device,pds,request,bank,false,false,out);}
int remesa_persistent_gears_attribute_state_prepare(struct remesa_device *device,void *pds,
 const struct remesa_persistent_linked_state_request *request,uint32_t bank,void **out)
{return linked_state_prepare(device,pds,request,bank,true,false,out);}
int remesa_persistent_gears_homogeneous_state_prepare(struct remesa_device *device,void *pds,
 const struct remesa_persistent_linked_state_request *request,uint32_t bank,void **out)
{return linked_state_prepare(device,pds,request,bank,true,true,out);}
static int linked_state_snapshot(const struct remesa_device *device,void *object,
 bool gears,bool homogeneous,struct remesa_persistent_linked_state_backing *out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct memory *b=object;
 e=linked_live(d,b,true);if(e)return e;
 unsigned kind=homogeneous?REMESA_PERSISTENT_STATE_GEARS_HOMOGENEOUS_DRAW:
  gears?REMESA_PERSISTENT_STATE_GEARS_DRAW:REMESA_PERSISTENT_STATE_LINKED_DRAW;
 if(!b->isp_update||b->state_kind!=kind||
    b->state_bytes!=(homogeneous?60u:56u)||!b->linked_dependency)return EINVAL;
 struct remesa_persistent_linked_state_backing value={0};
 e=linked_pds_metadata(d,b->linked_dependency,false,&value.dependencies);if(e)return e;
 value.image=(struct remesa_persistent_isp_update_backing){.owner=d->cfg.context,
  .generation=b->token.generation,.gpu_va=b->va,.handle=b->handle,
  .allocation_bytes=b->allocated,.state_offset=0,.state_bytes=b->state_bytes,
  .use_offset=b->state_use_offset,.use_bytes=16,.pds_offset=b->state_pds_offset,.pds_bytes=60,
  .ta_offset=b->state_ta_offset,.ta_bytes=b->state_ta_bytes,.use_entry=b->state_use_entry};
 memcpy(value.image.pds_known_mask,b->state_pds_known,sizeof(value.image.pds_known_mask));
 *out=value;return 0;
}
int remesa_persistent_linked_state_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_linked_state_backing *out)
{return linked_state_snapshot(device,object,false,false,out);}
int remesa_persistent_gears_attribute_state_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_linked_state_backing *out)
{return linked_state_snapshot(device,object,true,false,out);}
int remesa_persistent_gears_homogeneous_state_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_linked_state_backing *out)
{return linked_state_snapshot(device,object,true,true,out);}

static int body_bank_add(const struct remesa_use_entry_plan *entry,
 struct remesa_use_entry_plan banks[16],bool present[16])
{
 if(entry->bank>=16)return ERANGE;
 unsigned b=entry->bank;
 if(present[b]&&(banks[b].register_offset!=entry->register_offset||
    banks[b].register_word!=entry->register_word))return EBUSY;
 banks[b]=*entry;present[b]=true;return 0;
}
static int linked_body_prepare(struct remesa_device *device,
 const struct remesa_linked_body_draw *draws,size_t count,void *close,bool gears,
 bool homogeneous,void **out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(!draws||!count||count>REMESA_LINKED_BODY_MAX_DRAWS||
    overlaps(draws,count*sizeof(*draws),out,sizeof(*out)))return EINVAL;
 if(*out)return EBUSY;
 if(homogeneous&&!gears)return ENOTSUP;
 struct remesa_linked_body_draw request[REMESA_LINKED_BODY_MAX_DRAWS];
 memcpy(request,draws,count*sizeof(*draws));
 struct persistent *d=device->priv;struct memory *ending=close;
 e=linked_live(d,ending,true);if(e)return e;
 if(!ending->scene_close||ending->state_kind!=REMESA_PERSISTENT_STATE_SCENE_CLOSE||
    !ending->depth_width||!ending->depth_height)return EINVAL;
 const size_t stride=gears?36:28;
 struct linked_body metadata={.draw_count=(unsigned)count,.bytes=count*stride+12,
  .parameters=gears,.homogeneous=homogeneous};
 metadata.dependencies[metadata.dependency_count++]=ending;
 struct remesa_use_entry_plan banks[16]={0};bool present[16]={0};
 e=body_bank_add(&ending->state_use_entry,banks,present);if(e)return e;
 uint8_t image[REMESA_LINKED_BODY_MAX_DRAWS*36+12]={0};
 for(size_t i=0;i<count;i++){
  struct memory *state=request[i].state;
  struct remesa_persistent_linked_state_backing sb;
  e=linked_state_snapshot(device,state,gears,homogeneous,&sb);if(e)return e;
  if(state->depth_width!=ending->depth_width||state->depth_height!=ending->depth_height)return EINVAL;
  for(unsigned p=0;p<2;p++){
   e=body_bank_add(&sb.dependencies.program.entry[p],banks,present);if(e)return e;
  }
  e=body_bank_add(&sb.image.use_entry,banks,present);if(e)return e;
  e=remesa_ta_state_reference_encode(sb.image.gpu_va+sb.image.pds_offset,
   (uint32_t)(sb.image.state_bytes/4),false,image+i*stride);if(e)return e;
  if(gears){
   struct memory *params=state->linked_dependency->linked_pds->parameters;
   e=linked_live(d,params,false);if(e)return e;
   if(params->state_kind!=REMESA_PERSISTENT_STATE_VERTEX_PARAMETERS||
      params->state_parameter_dwords!=36||params->state_bytes!=160)return ENOTSUP;
   e=body_bank_add(&params->state_use_entry,banks,present);if(e)return e;
   e=remesa_ta_parameter_reference_encode(params->va+params->state_pds_offset,40,image+i*stride+8);if(e)return e;
  }
  /* Public PDS ownership may have been released; state still owns this exact
   * internal PDS closure. Derive the same command without reacquiring a token. */
  const struct remesa_persistent_linked_geometry_backing *g=&sb.dependencies.geometry;
  if(request[i].index_count<3||request[i].index_count%3)return ENOTSUP;
  if(request[i].first_index>g->index_count||
     request[i].index_count>g->index_count-request[i].first_index)return ERANGE;
  const struct remesa_ta_index_request index={.index_bo_gpu_va=g->gpu_va,
   .index_bo_bytes=g->index_offset+g->index_bytes,.index_byte_offset=g->index_offset,
   .first_index=request[i].first_index,.index_count=request[i].index_count,
   .pds_bo_gpu_va=sb.dependencies.gpu_va,.pds_bo_bytes=sb.dependencies.allocation_bytes,
   .pds_byte_offset=sb.dependencies.object_offset[0],.pds_object_bytes=sb.dependencies.image_bytes[0],
   .pds_data_dwords=12,.descriptor_a_field10=12,.descriptor_b_field38=32};
  e=remesa_ta_index_compiled_position_color_triangles_encode(&index,image+i*stride+(gears?16:8));if(e)return e;
  unsigned j;for(j=0;j<metadata.dependency_count&&metadata.dependencies[j]!=state;j++);
  if(j==metadata.dependency_count)metadata.dependencies[metadata.dependency_count++]=state;
 }
 const struct remesa_ta_scene_close_request end={ending->va,ending->allocated,
  ending->state_pds_offset,REMESA_PDS_STRUCTURE_STATE_BYTES};
 e=remesa_ta_scene_close_encode(&end,image+count*stride);if(e)return e;
 memcpy(metadata.commands,image,metadata.bytes);
 for(unsigned j=0;j<metadata.dependency_count;j++)if(metadata.dependencies[j]->refs==UINT32_MAX)return EBUSY;
 struct linked_body *owned=malloc(sizeof(*owned));if(!owned)return ENOMEM;*owned=metadata;
 void *object=NULL;size_t before=d->allocated;
 e=allocate_class(d,metadata.bytes,&object,REMESA_VA_DEFAULT);
 if(e){free(owned);return d->failed||d->allocated!=before?EOWNERDEAD:e;}
 struct memory *b=object;b->linked_body=owned;
 for(unsigned j=0;j<owned->dependency_count;j++)owned->dependencies[j]->refs++;
 e=owned_image_lease(d,b,image,metadata.bytes,true);if(e)goto fail;
 e=owned_image_lease(d,b,image,metadata.bytes,false);if(e)goto fail;
 e=d->cfg.ops->sync_for_device(d->cfg.priv,b->handle);
 if(e){memory_error(d,b,EOWNERDEAD);e=EOWNERDEAD;goto fail;}
 b->readable=true;*out=b;return 0;
fail:
 if(b->quarantined){*out=b;return EOWNERDEAD;}
 destroy_bo(d,b);if(b->live){*out=b;return EOWNERDEAD;}return e;
}
int remesa_persistent_linked_body_prepare(struct remesa_device *device,
 const struct remesa_linked_body_draw *draws,size_t count,void *close,void **out)
{return linked_body_prepare(device,draws,count,close,false,false,out);}
int remesa_persistent_gears_attribute_body_prepare(struct remesa_device *device,
 const struct remesa_linked_body_draw *draws,size_t count,void *close,void **out)
{return linked_body_prepare(device,draws,count,close,true,false,out);}
int remesa_persistent_gears_homogeneous_body_prepare(struct remesa_device *device,
 const struct remesa_linked_body_draw *draws,size_t count,void *close,void **out)
{return linked_body_prepare(device,draws,count,close,true,true,out);}
static unsigned linked_body_state_kind(const struct linked_body *body)
{return body->homogeneous?REMESA_PERSISTENT_STATE_GEARS_HOMOGENEOUS_DRAW:
 body->parameters?REMESA_PERSISTENT_STATE_GEARS_DRAW:REMESA_PERSISTENT_STATE_LINKED_DRAW;}
int remesa_persistent_linked_body_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_linked_body_backing *out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct memory *b=object;
 e=linked_live(d,b,true);if(e)return e;
 if(!b->linked_body)return EINVAL;
 const struct linked_body *body=b->linked_body;
 for(unsigned j=0;j<body->dependency_count;j++){
  e=linked_live(d,body->dependencies[j],false);if(e)return e;
  if(j){struct remesa_persistent_linked_pds_backing pd;
   e=linked_pds_metadata(d,body->dependencies[j]->linked_dependency,false,&pd);if(e)return e;
  }
 }
 *out=(struct remesa_persistent_linked_body_backing){.owner=d->cfg.context,.generation=b->token.generation,
  .gpu_va=b->va,.handle=b->handle,.draw_count=body->draw_count,.dependency_count=body->dependency_count,
  .allocation_bytes=b->allocated,.command_bytes=body->bytes,.close_offset=body->bytes-12};
 return 0;
}

static int zclear_entry(const struct memory *b,size_t offset,size_t length,uint32_t bank,
 enum remesa_use_data_master master,struct remesa_use_entry_plan *plan,uint32_t *fixup)
{
 const struct remesa_use_entry_request q={.allocation_address=b->va,.allocation_bytes=b->allocated,
  .entry_offset=offset,.program_bytes=length,.bank=bank,.data_master=master};
 int e=remesa_use_entry_prepare(&q,plan);if(e)return e;
 return remesa_use_entry_preview_fixup(plan,fixup);
}
static void zclear_word(uint8_t *out,uint32_t v)
{for(unsigned i=0;i<4;i++)out[i]=(uint8_t)(v>>(i*8));}
static int zclear_prefix_prepare(struct remesa_device *device,
 uint32_t width,uint32_t height,float depth,uint32_t vertex_bank,
 uint32_t pixel_bank,uint32_t state_bank,void **out,bool initial)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(*out)return EBUSY;
 const struct remesa_ta_zclear_request clear={width,height,width,height,depth};
 struct remesa_ta_zclear_geometry geometry;
 e=remesa_ta_zclear_single_geometry_encode(&clear,&geometry);if(e)return e;
 uint32_t ignored;
 const uint32_t banks[3]={vertex_bank,pixel_bank,state_bank};
 for(unsigned i=0;i<3;i++){e=remesa_use_base_offset(banks[i],&ignored);if(e)return e;}
 if(vertex_bank==pixel_bank||pixel_bank==state_bank)return EBUSY;
 uint8_t image[556]={0};size_t n=0;
 struct remesa_shader_ir ir={.schema=REMESA_SHADER_IR_SCHEMA,.stage=REMESA_IR_VERTEX,.count=2};
 ir.instructions[0]=(struct remesa_ir_instruction){.op=REMESA_IR_INPUT,.components=4,.type=REMESA_IR_FLOAT32};
 ir.instructions[1]=(struct remesa_ir_instruction){.op=REMESA_IR_POSITION,
  .components=4,.type=REMESA_IR_FLOAT32,.dst=REMESA_SHADER_IR_NO_REGISTER};
 struct remesa_shader_program vs,fs;
 e=remesa_shader_program_prepare(&ir,&vs);if(e)return e;
 e=remesa_shader_program_prepare_color(UINT32_C(0xff00ff00),&fs);if(e)return e;
 struct remesa_usse_vertex_mapping mapping={0};
 for(unsigned i=0;i<4;i++){
  mapping.input[i]=(struct remesa_usse_cpu_register){2,i};mapping.output[i]=(struct remesa_usse_cpu_register){1,i};
 }
 e=remesa_usse_vertex_candidate(&vs.usse,&mapping,image,16,&n);if(e)return e;
 if(n!=16)return EPROTO;
 e=remesa_shader_program_encode(&fs,image+16,8,&n);if(e)return e;
 if(n!=8)return EPROTO;
 const struct remesa_usse_cpu_move move={.source={2,0},.destination={1,0},.control12=10,.control21=1,.control23=1};
 const struct remesa_usse_cpu_three_source tail={.source={{0,0},{2,11},{2,0}},
  .source_extension={0,1,1},.word0_control21=1,.word1_control18=1};
 e=remesa_usse_cpu_move_encode(&move,image+32,8,&n);if(e)return e;
 e=remesa_usse_cpu_three_source_encode(&tail,image+40,8,&n);if(e)return e;
 memcpy(image+64,geometry.positions,64);memcpy(image+128,geometry.indices,12);
 struct remesa_persistent_zclear_prefix_backing *owned=calloc(1,sizeof(*owned));if(!owned)return ENOMEM;
 struct persistent *d=device->priv;void *object=NULL;size_t before=d->allocated;
 e=allocate_class(d,initial?556:444,&object,REMESA_VA_PROGRAM);
 if(e){free(owned);return d->failed||d->allocated!=before?EOWNERDEAD:e;}
 struct memory *b=object;b->zclear_prefix=owned;
 uint32_t fixup[3];
 for(unsigned i=0;i<3;i++){
  size_t at=i==0?0:i==1?16:32,length=i==1?8:16;
  e=zclear_entry(b,at,length,banks[i],i==1?REMESA_USE_PIXEL:REMESA_USE_VERTEX,&owned->entries[i],&fixup[i]);if(e)goto fail;
 }
 if(vertex_bank==state_bank&&owned->entries[0].register_word!=owned->entries[2].register_word){e=EBUSY;goto fail;}
 const struct remesa_pds_vertex_image_request vertex={b->va,b->allocated,64,4,fixup[0]};
 uint8_t mask[64];
 e=remesa_pds_structure_encode_vertex_image(&vertex,image+160,mask,64,&n);if(e)goto fail;
 if(n!=64){e=EPROTO;goto fail;}
 const struct remesa_pds_structure_request fragment={.code_policy=REMESA_PDS_STRUCTURE_CODE_CPU_COMPLETE};
 struct remesa_pds_structure fp;
 e=remesa_pds_structure_encode(&fragment,&fp);if(e)goto fail;
 e=remesa_pds_structure_serialize(&fp,image+224,mask,56,&n);if(e)goto fail;
 if(n!=56||fp.relocation_count!=1||fp.relocations[0].data_cell||fp.relocations[0].background){e=EPROTO;goto fail;}
 zclear_word(image+224,fixup[1]);
 const struct remesa_pds_code_instruction secondary={REMESA_PDS_STRUCTURE_CPU_EMPTY_SECONDARY_AF,
  REMESA_PDS_STRUCTURE_NO_CELL,REMESA_PDS_STRUCTURE_NO_CELL};
 e=remesa_pds_structure_encode_code(&secondary,1,image+288,4,&n);if(e)goto fail;
 const struct remesa_ta_fragment_binding_request binding={.address_window_base=b->va&UINT64_C(0xf0000000),
  .primary={b->va,b->allocated,224,56,12,0,0},.secondary={b->va,b->allocated,288,4,0,0,0},
  .profile=REMESA_TA_FRAGMENT_SCENE_ZCLEAR};
 e=remesa_ta_scene_zclear_state_encode(&binding,image+304);if(e)goto fail;
 const struct remesa_pds_state_image_request sp={.source_allocation_address=b->va,
  .source_allocation_bytes=b->allocated,.source_offset=304,.state_dwords=11,.use_lower19=fixup[2]};
 e=remesa_pds_structure_encode_state_image(&sp,image+352,mask,60,&n);if(e)goto fail;
 e=remesa_ta_state_reference_encode(b->va+352,11,false,image+416);if(e)goto fail;
 const struct remesa_ta_index_request index={.index_bo_gpu_va=b->va,.index_bo_bytes=134,
  .index_byte_offset=128,.index_count=3,.pds_bo_gpu_va=b->va,.pds_bo_bytes=b->allocated,
  .pds_byte_offset=160,.pds_object_bytes=64,.pds_data_dwords=12,
  .descriptor_a_field10=4,.descriptor_b_field38=4};
 e=remesa_ta_index_triangles_encode(&index,image+424);if(e)goto fail;
 if(initial){
  /* Reuse the preserved initialization DATA and the already validated11-word
   * state-copy program. Added PDS CODE is a real upload task, not padding. */
  memcpy(image+448,remesa_scene_initial_data,44);
  const struct remesa_pds_state_image_request init={.source_allocation_address=b->va,
   .source_allocation_bytes=b->allocated,.source_offset=448,.state_dwords=11,.use_lower19=fixup[2]};
  e=remesa_pds_structure_encode_state_image(&init,image+496,mask,60,&n);if(e)goto fail;
 }
 *owned=(struct remesa_persistent_zclear_prefix_backing){.owner=d->cfg.context,.generation=b->token.generation,
  .gpu_va=b->va,.handle=b->handle,.width=width,.height=height,.index_count=3,.allocation_bytes=b->allocated,
  .used_bytes=initial?556:444,.vertex_offset=64,.index_offset=128,
  .vertex_capacity_bytes=64,.vertex_referenced_bytes=48,.index_capacity_bytes=12,.index_referenced_bytes=6,
  .use_offset={0,16,32},.use_bytes={16,8,16},.pds_offset={160,224,288,352},.pds_bytes={64,56,4,60},
  .state_offset=304,.state_bytes=44,.ta_offset=416,.ta_bytes=28,
  .initial_state_offset=initial?448:0,.initial_pds_offset=initial?496:0,.clear_depth=depth,
  .entries={owned->entries[0],owned->entries[1],owned->entries[2]}};
 e=owned_image_lease(d,b,image,owned->used_bytes,true);if(e)goto fail;
 e=owned_image_lease(d,b,image,owned->used_bytes,false);if(e)goto fail;
 e=d->cfg.ops->sync_for_device(d->cfg.priv,b->handle);
 if(e){memory_error(d,b,EOWNERDEAD);e=EOWNERDEAD;goto fail;}
 b->readable=true;*out=b;return 0;
fail:
 if(b->quarantined){*out=b;return EOWNERDEAD;}
 destroy_bo(d,b);if(b->live){*out=b;return EOWNERDEAD;}return e;
}
int remesa_persistent_zclear_prefix_prepare(struct remesa_device *d,
 uint32_t w,uint32_t h,float z,uint32_t vb,uint32_t pb,uint32_t sb,void **out)
{return zclear_prefix_prepare(d,w,h,z,vb,pb,sb,out,false);}
int remesa_persistent_initialized_prefix_prepare(struct remesa_device *d,
 uint32_t w,uint32_t h,float z,uint32_t vb,uint32_t pb,uint32_t sb,void **out)
{return zclear_prefix_prepare(d,w,h,z,vb,pb,sb,out,true);}
int remesa_persistent_zclear_prefix_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_zclear_prefix_backing *out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct memory *b=object;
 e=linked_live(d,b,true);if(e)return e;
 if(!b->zclear_prefix)return EINVAL;
 *out=*b->zclear_prefix;return 0;
}

/* Unique, current read allocations reachable by commands; lifetime comes
 * from retained owner edges, not from this metadata list. */
static int linked_read_add(struct persistent *d,struct memory *b,
 struct memory *reads[REMESA_PERSISTENT_BUFFERS],unsigned *count)
{
 int e=linked_live(d,b,false);if(e)return e;
 for(unsigned i=0;i<*count;i++)if(reads[i]==b)return 0;
 if(*count==REMESA_PERSISTENT_BUFFERS)return E2BIG;
 reads[(*count)++]=b;return 0;
}
static int linked_ta_reads(struct persistent *d,struct memory *prefix,struct memory *body,
 struct memory *reads[REMESA_PERSISTENT_BUFFERS],unsigned *count,
 struct remesa_use_entry_plan banks[16],bool present[16])
{
 int e=linked_read_add(d,prefix,reads,count);if(e)return e;
 e=linked_live(d,body,false);if(e)return e;
 if(!prefix->zclear_prefix||!body->linked_body)return EINVAL;
 const struct remesa_persistent_zclear_prefix_backing *p=prefix->zclear_prefix;
 for(unsigned i=0;i<3;i++){e=body_bank_add(&p->entries[i],banks,present);if(e)return e;}
 const struct linked_body *b=body->linked_body;
 for(unsigned i=0;i<b->dependency_count;i++){
  struct memory *state=b->dependencies[i];
  e=linked_read_add(d,state,reads,count);if(e)return e;
  if(state->depth_width!=p->width||state->depth_height!=p->height)return EINVAL;
  e=body_bank_add(&state->state_use_entry,banks,present);if(e)return e;
  if(!i){
   if(!state->scene_close||state->state_kind!=REMESA_PERSISTENT_STATE_SCENE_CLOSE)return EPROTO;
   continue;
  }
  if(state->state_kind!=linked_body_state_kind(b)||!state->linked_dependency)return EPROTO;
  struct memory *pds=state->linked_dependency;
  e=linked_read_add(d,pds,reads,count);if(e)return e;
  if(!pds->linked_pds)return EPROTO;
  struct memory *code=pds->linked_pds->program,*raw=pds->linked_pds->geometry;
  e=linked_read_add(d,code,reads,count);if(e)return e;
  e=linked_read_add(d,raw,reads,count);if(e)return e;
  if(body->linked_body->parameters){
   struct memory *params=pds->linked_pds->parameters;
   e=linked_read_add(d,params,reads,count);if(e)return e;
   e=body_bank_add(&params->state_use_entry,banks,present);if(e)return e;
  }
  if(!code->linked_program||!raw->linked_geometry)return EPROTO;
  for(unsigned j=0;j<2;j++){
   e=body_bank_add(&code->linked_program->entry[j],banks,present);if(e)return e;
  }
 }
 return 0;
}
static int linked_ta_prepare(struct remesa_device *device,void *prefix_object,
 void *body_object,bool preserve_depth,void **out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(*out)return EBUSY;
 struct persistent *d=device->priv;struct memory *prefix=prefix_object,*body=body_object;
 e=linked_live(d,prefix,true);if(e)return e;
 e=linked_live(d,body,true);if(e)return e;
 if(prefix==body||!prefix->zclear_prefix||!body->linked_body)return EINVAL;
 struct memory *reads[REMESA_PERSISTENT_BUFFERS]={0};unsigned count=0;
 struct remesa_use_entry_plan banks[16]={0};bool present[16]={0};
 e=linked_ta_reads(d,prefix,body,reads,&count,banks,present);if(e)return e;
 if(count>=REMESA_PERSISTENT_BUFFERS)return E2BIG; /* include the new TA owner */
 if(prefix->refs==UINT32_MAX||body->refs==UINT32_MAX)return EBUSY;
 uint8_t image[56+REMESA_LINKED_BODY_MAX_DRAWS*36+12]={0};
 const struct remesa_persistent_zclear_prefix_backing *p=prefix->zclear_prefix;
 e=remesa_ta_state_reference_encode(p->gpu_va+p->pds_offset[3],11,false,image);if(e)return e;
 const struct remesa_ta_index_request index={.index_bo_gpu_va=p->gpu_va,
  .index_bo_bytes=p->index_offset+p->index_referenced_bytes,.index_byte_offset=p->index_offset,
  .index_count=p->index_count,.pds_bo_gpu_va=p->gpu_va,.pds_bo_bytes=p->allocation_bytes,
  .pds_byte_offset=p->pds_offset[0],.pds_object_bytes=p->pds_bytes[0],.pds_data_dwords=12,
  .descriptor_a_field10=4,.descriptor_b_field38=4};
 e=remesa_ta_index_triangles_encode(&index,image+8);if(e)return e;
 size_t prefix_bytes=p->initial_pds_offset?56:28;
 if(p->initial_pds_offset){
  /* Preserve the exact old clear group as the second primitive. Both INDEX
   * commands refer to the SAME immutable vertex/index fetch window. */
  memcpy(image+28,image,28);
  e=remesa_ta_state_reference_encode(p->gpu_va+p->initial_pds_offset,11,false,image);if(e)return e;
 }
 if(preserve_depth){
  if(!p->initial_pds_offset||!body->linked_body->homogeneous||
     !body->linked_body->parameters||body->linked_body->draw_count!=1)return ENOTSUP;
  /* Retain INITIAL STATE8/INDEX20, whose physical predecessor fixed
   * empty output. Omit only the separate Z-clear STATE8/INDEX20. */
  prefix_bytes=28;
 }
 size_t bytes=prefix_bytes+body->linked_body->bytes;
 memcpy(image+prefix_bytes,body->linked_body->commands,body->linked_body->bytes);
 struct linked_ta *owned=calloc(1,sizeof(*owned));if(!owned)return ENOMEM;
 *owned=(struct linked_ta){.prefix=prefix,.body=body,.bytes=bytes,.preserve_depth=preserve_depth};
 memcpy(owned->commands,image,bytes);
 void *object=NULL;size_t before=d->allocated;
 e=allocate_class(d,bytes,&object,REMESA_VA_DEFAULT);
 if(e){free(owned);return d->failed||d->allocated!=before?EOWNERDEAD:e;}
 struct memory *b=object;b->linked_ta=owned;prefix->refs++;body->refs++;
 e=owned_image_lease(d,b,image,bytes,true);if(e)goto fail;
 e=owned_image_lease(d,b,image,bytes,false);if(e)goto fail;
 e=d->cfg.ops->sync_for_device(d->cfg.priv,b->handle);
 if(e){memory_error(d,b,EOWNERDEAD);e=EOWNERDEAD;goto fail;}
 b->readable=true;*out=b;return 0;
fail:
 if(b->quarantined){*out=b;return EOWNERDEAD;}
 destroy_bo(d,b);if(b->live){*out=b;return EOWNERDEAD;}return e;
}
int remesa_persistent_linked_ta_prepare(struct remesa_device *device,void *prefix,
 void *body,void **out)
{return linked_ta_prepare(device,prefix,body,false,out);}
int remesa_persistent_linked_ta_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_linked_ta_backing *out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct memory *b=object;
 e=linked_live(d,b,true);if(e)return e;
 if(!b->linked_ta)return EINVAL;
 const struct linked_ta *t=b->linked_ta;
 struct memory *reads[REMESA_PERSISTENT_BUFFERS]={0};unsigned count=0;
 e=linked_read_add(d,b,reads,&count);if(e)return e;
 struct remesa_use_entry_plan banks[16]={0};bool present[16]={0};
 e=linked_ta_reads(d,t->prefix,t->body,reads,&count,banks,present);if(e)return e;
 const struct remesa_persistent_zclear_prefix_backing *p=t->prefix->zclear_prefix;
 struct remesa_persistent_linked_ta_backing value={.owner=d->cfg.context,.generation=b->token.generation,
  .gpu_va=b->va,.handle=b->handle,.width=p->width,.height=p->height,
  .draw_count=t->body->linked_body->draw_count,.read_allocation_count=count,
  .allocation_bytes=b->allocated,.command_bytes=t->bytes,.draw_offset=t->preserve_depth?28:t->prefix->zclear_prefix->initial_pds_offset?56:28,.close_offset=t->bytes-12};
 for(unsigned i=0;i<count;i++){
  value.reads[i].generation=reads[i]->token.generation;value.reads[i].gpu_va=reads[i]->va;
  value.reads[i].handle=reads[i]->handle;value.reads[i].access=REMESA_ACCESS_READ;
  value.reads[i].allocation_bytes=reads[i]->allocated;
 }
 *out=value;return 0;
}

/* Attachments may be undefined/unreadable. Only storage ownership and current
 * mappings are checked here; linked_live's shader publication check is not a
 * substitute for an explicit attachment initialization/load contract. */
static int linked_attachment_live(struct persistent *d,struct memory *b,bool owner)
{
 if(!owns_memory(d,b)||!b->live||!b->refs||(owner&&!b->owner))return EINVAL;
 if(b->quarantined)return EIO;
 if(!b->pinned||!b->gpu_mapped||b->cpu_mapped)return EBUSY;
 struct remesa_resource r;
 if(remesa_registry_snapshot(&d->registry,b->token,&r)!=REMESA_OK)return ESTALE;
 return r.state==REMESA_MAPPED?0:EBUSY;
}
static int linked_targets_metadata(struct persistent *d,struct memory *ta,
 struct memory *color,struct memory *depth,uint32_t pitch,bool owner,
 struct remesa_persistent_linked_targets_backing *out)
{
 int e=linked_live(d,ta,owner);if(e)return e;
 if(!ta->linked_ta)return EINVAL;
 e=linked_attachment_live(d,color,owner);if(e)return e;
 e=linked_attachment_live(d,depth,owner);if(e)return e;
 if(color==depth||color==ta||depth==ta)return EINVAL;
 if(color->linked_background||color->linked_targets||color->linked_ta||color->linked_body||color->zclear_prefix||color->scene_hw||
    color->depth_backing||color->isp_update||color->raw_geometry||color->shader_program||
    color->shader_pds||color->scene_image||color->control_image)return ENOTSUP;
 const struct remesa_persistent_zclear_prefix_backing *prefix=ta->linked_ta->prefix->zclear_prefix;
 if(!depth->depth_backing||depth->depth_cpp!=4||depth->depth_width!=prefix->width||
    depth->depth_height!=prefix->height)return EINVAL;
 struct remesa_persistent_linked_targets_backing value={0};
 /* Snapshot the TA closure internally, including after the caller has released
 * its public reference. Never fabricate a public token or temporary owner. */
 struct memory *reads[REMESA_PERSISTENT_BUFFERS]={0};unsigned count=0;
 e=linked_read_add(d,ta,reads,&count);if(e)return e;
 struct remesa_use_entry_plan banks[16]={0};bool present[16]={0};
 e=linked_ta_reads(d,ta->linked_ta->prefix,ta->linked_ta->body,reads,&count,banks,present);if(e)return e;
 value.commands=(struct remesa_persistent_linked_ta_backing){.owner=d->cfg.context,
  .generation=ta->token.generation,.gpu_va=ta->va,.handle=ta->handle,
  .width=prefix->width,.height=prefix->height,.draw_count=ta->linked_ta->body->linked_body->draw_count,
  .read_allocation_count=count,.allocation_bytes=ta->allocated,.command_bytes=ta->linked_ta->bytes,
  .draw_offset=ta->linked_ta->preserve_depth?28:prefix->initial_pds_offset?56:28,.close_offset=ta->linked_ta->bytes-12};
 for(unsigned i=0;i<count;i++){
  value.commands.reads[i].generation=reads[i]->token.generation;
  value.commands.reads[i].gpu_va=reads[i]->va;value.commands.reads[i].handle=reads[i]->handle;
  value.commands.reads[i].access=REMESA_ACCESS_READ;value.commands.reads[i].allocation_bytes=reads[i]->allocated;
 }
 value.color=(struct remesa_persistent_render_target_backing){.owner=d->cfg.context,
  .generation=color->token.generation,.gpu_va=color->va,.handle=color->handle,.allocation_bytes=color->allocated};
 const struct remesa_render_target_request cr={prefix->width,prefix->height,pitch,color->va,color->bytes};
 e=remesa_render_target_encode_linear_bgra8(&cr,&value.color.state);if(e)return e;
 value.depth=(struct remesa_persistent_depth_backing){.owner=d->cfg.context,.generation=depth->token.generation,
  .gpu_va=depth->va,.handle=depth->handle,.width=prefix->width,.height=prefix->height,
  .pitch_pixels=depth->depth_pitch,.cpp=4,.allocation_bytes=depth->allocated};
 const struct remesa_depth_surface_request zr={prefix->width,prefix->height,depth->depth_pitch,4,
  ta->linked_ta->preserve_depth?REMESA_DEPTH_SURFACE_DEFINED:REMESA_DEPTH_SURFACE_UNDEFINED,depth->va,depth->allocated};
 e=remesa_depth_surface_encode(&zr,&value.depth.initial_surface);if(e)return e;
 /* Conservative permissions: depth testing reads and writes Z. COLOR may
  * require a load/preserve path. This declaration does not implement either. */
 value.color_access=REMESA_ACCESS_READ|REMESA_ACCESS_WRITE;
 value.depth_access=REMESA_ACCESS_READ|REMESA_ACCESS_WRITE;
 value.used_bytes=72;value.color_data_offset=0;value.ta_control_offset=16;
 *out=value;return 0;
}
static int linked_targets_prepare(struct remesa_device *device,void *ta_object,
 void *color_object,void *depth_object,uint32_t pitch,bool finalize,void **out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(*out)return EBUSY;
 struct persistent *d=device->priv;struct memory *ta=ta_object,*color=color_object,*depth=depth_object;
 struct remesa_persistent_linked_targets_backing m;
 e=linked_targets_metadata(d,ta,color,depth,pitch,true,&m);if(e)return e;
 if(ta->refs==UINT32_MAX||color->refs==UINT32_MAX||depth->refs==UINT32_MAX)return EBUSY;
 uint8_t image[72]={0};memcpy(image,m.color.state.target_data,16);
 const struct remesa_raster_object_reference commands={ta->va,ta->allocated,0,m.commands.command_bytes};
 e=remesa_ta_control_encode(&commands,image+16);if(e)return e;
 struct linked_targets *owned=malloc(sizeof(*owned));if(!owned)return ENOMEM;
 *owned=(struct linked_targets){ta,color,depth,pitch,finalize};void *object=NULL;size_t before=d->allocated;
 e=allocate_class(d,sizeof(image),&object,REMESA_VA_DEFAULT);
 if(e){free(owned);return d->failed||d->allocated!=before?EOWNERDEAD:e;}
 struct memory *b=object;b->linked_targets=owned;ta->refs++;color->refs++;depth->refs++;
 e=owned_image_lease(d,b,image,sizeof(image),true);if(e)goto fail;
 e=owned_image_lease(d,b,image,sizeof(image),false);if(e)goto fail;
 e=d->cfg.ops->sync_for_device(d->cfg.priv,b->handle);
 if(e){memory_error(d,b,EOWNERDEAD);e=EOWNERDEAD;goto fail;}
 b->readable=true;*out=b;return 0;
fail:
 if(b->quarantined){*out=b;return EOWNERDEAD;}
 destroy_bo(d,b);if(b->live){*out=b;return EOWNERDEAD;}return e;
}
int remesa_persistent_linked_targets_prepare(struct remesa_device *device,void *ta,
 void *color,void *depth,uint32_t pitch,void **out)
{return linked_targets_prepare(device,ta,color,depth,pitch,false,out);}
int remesa_persistent_finalized_targets_prepare(struct remesa_device *device,void *ta,
 void *color,void *depth,uint32_t pitch,void **out)
{return linked_targets_prepare(device,ta,color,depth,pitch,true,out);}
int remesa_persistent_linked_targets_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_linked_targets_backing *out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct memory *b=object;
 e=linked_live(d,b,true);if(e)return e;
 if(!b->linked_targets)return EINVAL;
 const struct linked_targets *t=b->linked_targets;
 struct remesa_persistent_linked_targets_backing value;
 e=linked_targets_metadata(d,t->ta,t->color,t->depth,t->pitch,false,&value);if(e)return e;
 value.owner=d->cfg.context;value.generation=b->token.generation;value.gpu_va=b->va;
 value.handle=b->handle;value.allocation_bytes=b->allocated;value.depth_finalize=t->depth_finalize;
 *out=value;return 0;
}

static int background_targets_metadata(struct persistent *d,struct memory *targets,bool owner,
 struct remesa_persistent_linked_targets_backing *out)
{
 int e=linked_live(d,targets,owner);if(e)return e;
 if(!targets->linked_targets)return EINVAL;
 const struct linked_targets *t=targets->linked_targets;
 e=linked_targets_metadata(d,t->ta,t->color,t->depth,t->pitch,false,out);if(e)return e;
 out->owner=d->cfg.context;out->generation=targets->token.generation;out->gpu_va=targets->va;
 out->handle=targets->handle;out->allocation_bytes=targets->allocated;out->depth_finalize=t->depth_finalize;return 0;
}
int remesa_persistent_linked_background_prepare(struct remesa_device *device,void *targets_object,
 uint32_t bank,void **out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(*out)return EBUSY;
 struct persistent *d=device->priv;struct memory *targets=targets_object;
 struct remesa_persistent_linked_targets_backing tm;
 e=background_targets_metadata(d,targets,true,&tm);if(e)return e;
 uint32_t ignored;e=remesa_use_base_offset(bank,&ignored);if(e)return e;
 if(targets->refs>=UINT32_MAX-1)return EBUSY;
 struct linked_background *owned=calloc(1,sizeof(*owned));if(!owned)return ENOMEM;
 void *object=NULL;size_t before=d->allocated;
 e=allocate_class(d,68,&object,REMESA_VA_RASTER);
 if(e){free(owned);return d->failed||d->allocated!=before?EOWNERDEAD:e;}
 struct memory *b=object;b->linked_background=owned;owned->targets=targets;targets->refs++;
 if(b->va<UINT64_C(0x30000000)||b->va+b->allocated>UINT64_C(0x40000000)){e=ENOTSUP;goto fail;}
 void *program=NULL;
 e=allocate_class(d,100,&program,REMESA_VA_PROGRAM);
 if(e){if(d->failed){memory_error(d,b,EOWNERDEAD);e=EOWNERDEAD;}goto fail;}
 struct memory *code=program;code->shader_pds=true;code->owner=false;owned->program=code;
 /* The program may survive its parent if its own disposal becomes uncertain.
  * Retain the COLOR/TA closure from that surviving allocation as well. */
 code->linked_dependency=targets;targets->refs++;
 if(code->va<UINT64_C(0x20000000)||code->va+code->allocated>UINT64_C(0x30000000)){e=ENOTSUP;goto fail;}
 uint32_t fixup;
 e=zclear_entry(code,0,8,bank,REMESA_USE_PIXEL,&owned->entry,&fixup);if(e)goto fail;
 struct memory *reads[REMESA_PERSISTENT_BUFFERS]={0};unsigned count=0;
 struct remesa_use_entry_plan banks[16]={0};bool present[16]={0};
 struct memory *ta=targets->linked_targets->ta;
 e=linked_ta_reads(d,ta->linked_ta->prefix,ta->linked_ta->body,reads,&count,banks,present);if(e)goto fail;
 e=body_bank_add(&owned->entry,banks,present);if(e)goto fail;
 uint8_t image[100]={0};size_t n=0;
 /* Existing original39c5c ->30e22(builder,1,2) ->30fb7, same typed operands
  * as tools/remesa/auxiliary_usse_image.c --background. No extra tail. */
 const struct remesa_usse_cpu_move use={.source={2,0},.destination={1,0},
  .control12=1,.control23=1,.control18=1};
 e=remesa_usse_cpu_move_encode(&use,image,8,&n);if(e)goto fail;
 if(n!=8){e=EPROTO;goto fail;}
 const struct remesa_pds_background_request primary={tm.color.gpu_va,fixup,4,
  tm.commands.width,tm.commands.height};
 e=remesa_pds_structure_encode_background(&primary,image+32,owned->known,64,&n);if(e)goto fail;
 if(n!=64){e=EPROTO;goto fail;}
 const struct remesa_pds_code_instruction secondary={REMESA_PDS_STRUCTURE_CPU_EMPTY_SECONDARY_AF,
  REMESA_PDS_STRUCTURE_NO_CELL,REMESA_PDS_STRUCTURE_NO_CELL};
 e=remesa_pds_structure_encode_code(&secondary,1,image+96,4,&n);if(e)goto fail;
 if(n!=4){e=EPROTO;goto fail;}
 uint8_t bg[68];
 const struct remesa_background_object_request background={
  .address_window_base=code->va&UINT64_C(0xf0000000),
  .primary={code->va,code->allocated,32,64,12,1,0},
  .secondary={code->va,code->allocated,96,4,0,0,0},
  .width=tm.commands.width,.height=tm.commands.height,.depth=1};
 e=remesa_background_object_encode(&background,bg);if(e)goto fail;
 e=owned_image_lease(d,code,image,sizeof(image),true);if(e)goto fail;
 e=owned_image_lease(d,code,image,sizeof(image),false);if(e)goto fail;
 e=d->cfg.ops->sync_for_device(d->cfg.priv,code->handle);
 if(e){memory_error(d,code,EOWNERDEAD);e=EOWNERDEAD;goto fail;}
 code->readable=true;
 e=owned_image_lease(d,b,bg,sizeof(bg),true);if(e)goto fail;
 e=owned_image_lease(d,b,bg,sizeof(bg),false);if(e)goto fail;
 e=d->cfg.ops->sync_for_device(d->cfg.priv,b->handle);
 if(e){memory_error(d,b,EOWNERDEAD);e=EOWNERDEAD;goto fail;}
 b->readable=true;*out=b;return 0;
fail:
 if(e==EOWNERDEAD||(owned->program&&owned->program->quarantined))memory_error(d,b,EOWNERDEAD);
 if(b->quarantined){*out=b;return EOWNERDEAD;}
 destroy_bo(d,b);if(b->live){*out=b;return EOWNERDEAD;}
 return d->failed?EOWNERDEAD:e;
}
static int background_metadata(struct persistent *d,struct memory *b,bool owner,
 struct remesa_persistent_linked_background_backing *out)
{
 int e=linked_live(d,b,owner);if(e)return e;
 if(!b->linked_background||!b->linked_background->program)return EINVAL;
 const struct linked_background *bg=b->linked_background;struct memory *p=bg->program;
 e=linked_live(d,p,false);if(e)return e;
 struct remesa_persistent_linked_background_backing value={.owner=d->cfg.context,
  .generation=b->token.generation,.gpu_va=b->va,.handle=b->handle,.allocation_bytes=b->allocated,
  .object_bytes=68,.program_generation=p->token.generation,.program_gpu_va=p->va,.program_handle=p->handle,
  .program_allocation_bytes=p->allocated,.program_used_bytes=100,.use_offset=0,.use_bytes=8,
  .primary_offset=32,.primary_bytes=64,.secondary_offset=96,.secondary_bytes=4,.entry=bg->entry};
 memcpy(value.primary_known_mask,bg->known,sizeof(value.primary_known_mask));
 e=background_targets_metadata(d,bg->targets,false,&value.targets);if(e)return e;
 *out=value;return 0;
}
int remesa_persistent_linked_background_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_linked_background_backing *out)
{
 if((uintptr_t)out%_Alignof(struct remesa_persistent_linked_background_backing))return EINVAL;
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 return background_metadata(device->priv,object,true,out);
}
int remesa_persistent_linked_event_auxiliary_prepare(struct remesa_device *device,
 void *targets_object,uint32_t bank,void **out)
{
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(*out)return EBUSY;
 struct persistent *d=device->priv;struct memory *targets=targets_object;
 struct remesa_persistent_linked_targets_backing tm;
 e=background_targets_metadata(d,targets,true,&tm);if(e)return e;
 uint32_t ignored;e=remesa_use_base_offset(bank,&ignored);if(e)return e;
 if(targets->refs==UINT32_MAX)return EBUSY;
 size_t before=d->allocated;void *object=NULL;
 e=allocate_class(d,40,&object,REMESA_VA_PROGRAM);
 if(e)return d->failed||d->allocated!=before?EOWNERDEAD:e;
 struct memory *b=object;b->shader_pds=true;b->linked_event_auxiliary=true;
 b->event_pixel_bank=bank;b->linked_dependency=targets;targets->refs++;
 if(b->va<UINT64_C(0x20000000)||b->va+b->allocated>UINT64_C(0x30000000)){e=ENOTSUP;goto fail;}
 struct remesa_use_entry_plan entries[2];uint32_t fixup;
 e=zclear_entry(b,0,16,bank,REMESA_USE_PIXEL,&entries[0],&fixup);if(e)goto fail;
 e=zclear_entry(b,32,8,bank,REMESA_USE_PIXEL,&entries[1],&fixup);if(e)goto fail;
 struct memory *reads[REMESA_PERSISTENT_BUFFERS]={0};unsigned count=0;
 struct remesa_use_entry_plan banks[16]={0};bool present[16]={0};
 struct memory *ta=targets->linked_targets->ta;
 e=linked_ta_reads(d,ta->linked_ta->prefix,ta->linked_ta->body,reads,&count,banks,present);if(e)goto fail;
 for(unsigned i=0;i<2;i++){e=body_bank_add(&entries[i],banks,present);if(e)goto fail;}
 uint8_t image[40]={0};size_t n=0;
 /* The existing target owner accepts only BGRA8, original surface tag4:
  * caller392ab selects transfer mode0. Both entries start on32-byte boundaries.
  * Bytes16..31 are reserved zero padding, never additional instructions. */
 e=remesa_usse_psb_event_transfer_encode(0,image,16,&n);if(e)goto fail;
 if(n!=16){e=EPROTO;goto fail;}
 e=remesa_usse_psb_event_end_encode(image+32,8,&n);if(e)goto fail;
 if(n!=8){e=EPROTO;goto fail;}
 e=owned_image_lease(d,b,image,sizeof(image),true);if(e)goto fail;
 e=owned_image_lease(d,b,image,sizeof(image),false);if(e)goto fail;
 e=d->cfg.ops->sync_for_device(d->cfg.priv,b->handle);
 if(e){memory_error(d,b,EOWNERDEAD);e=EOWNERDEAD;goto fail;}
 b->readable=true;*out=b;return 0;
fail:
 if(b->quarantined){*out=b;return EOWNERDEAD;}
 destroy_bo(d,b);if(b->live){*out=b;return EOWNERDEAD;}
 return d->failed?EOWNERDEAD:e;
}
static int event_auxiliary_metadata(struct persistent *d,struct memory *b,bool owner,
 struct remesa_persistent_linked_event_auxiliary_backing *out)
{
 int e=linked_live(d,b,owner);if(e)return e;
 if(!b->linked_event_auxiliary||!b->linked_dependency)return EINVAL;
 struct remesa_persistent_linked_event_auxiliary_backing value={.owner=d->cfg.context,
  .generation=b->token.generation,.gpu_va=b->va,.handle=b->handle,
  .allocation_bytes=b->allocated,.used_bytes=40,.program_offset={0,32},.program_bytes={16,8}};
 e=background_targets_metadata(d,b->linked_dependency,false,&value.targets);if(e)return e;
 for(unsigned i=0;i<2;i++){
  e=zclear_entry(b,value.program_offset[i],value.program_bytes[i],b->event_pixel_bank,
   REMESA_USE_PIXEL,&value.entries[i],&value.use_lower19[i]);if(e)return e;
 }
 /* Resolve all three event DATA references from the same live owned closure.
  * Bound the target source by committed DATA, never spare page capacity.
  * No caller-provided numeric snapshot is trusted as an ownership token. */
 const struct remesa_pds_psb_event_data_request request={
  .source_allocation_address=value.targets.gpu_va,
  .source_allocation_bytes=value.targets.used_bytes,
  .source_offset=value.targets.color_data_offset,
  .end_use_lower19=value.use_lower19[1],.transfer_use_lower19=value.use_lower19[0]};
 e=remesa_pds_structure_encode_psb_event_data(&request,value.event_data,
  value.event_data_known_mask,sizeof(value.event_data),&value.event_data_bytes);if(e)return e;
 if(value.event_data_bytes!=sizeof(value.event_data))return EPROTO;
 *out=value;return 0;
}
int remesa_persistent_linked_event_auxiliary_snapshot(const struct remesa_device *device,
 void *object,struct remesa_persistent_linked_event_auxiliary_backing *out)
{
 if((uintptr_t)out%_Alignof(struct remesa_persistent_linked_event_auxiliary_backing))return EINVAL;
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 return event_auxiliary_metadata(device->priv,object,true,out);
}
int remesa_persistent_linked_event_profile_prepare(struct remesa_device *device,
 void *auxiliary_object,void **out)
{
 if((uintptr_t)out%_Alignof(void *))return EINVAL;
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(*out)return EBUSY;
 struct persistent *d=device->priv;struct memory *aux=auxiliary_object;
 struct remesa_persistent_linked_event_auxiliary_backing metadata;
 e=event_auxiliary_metadata(d,aux,true,&metadata);if(e)return e;
 if(aux->refs==UINT32_MAX)return EBUSY;
 const struct remesa_pds_psb_event_data_request request={
  .source_allocation_address=metadata.targets.gpu_va,
  .source_allocation_bytes=metadata.targets.used_bytes,
  .source_offset=metadata.targets.color_data_offset,
  .end_use_lower19=metadata.use_lower19[1],.transfer_use_lower19=metadata.use_lower19[0]};
 uint8_t image[REMESA_PDS_PSB_EVENT_PROFILE_BYTES],known[REMESA_PDS_PSB_EVENT_PROFILE_BYTES];size_t n=0;
 e=remesa_pds_structure_encode_psb_event_profile(&request,image,known,sizeof(image),&n);if(e)return e;
 if(n!=sizeof(image))return EPROTO;
 size_t before=d->allocated;void *object=NULL;
 e=allocate_class(d,REMESA_PDS_PSB_EVENT_PROFILE_RESERVATION_BYTES,&object,REMESA_VA_PROGRAM);
 if(e)return d->failed||d->allocated!=before?EOWNERDEAD:e;
 struct memory *b=object;b->shader_pds=true;b->linked_event_profile=true;
 b->linked_dependency=aux;aux->refs++;
 if(b->va%32||b->va<UINT64_C(0x20000000)||b->va+b->allocated>UINT64_C(0x30000000)){e=ENOTSUP;goto fail;}
 /* Existing immutable image publication/verification, once at preparation.
  * Reserved400 and rounded backing are not additional CODE or fetch claims. */
 e=owned_image_lease(d,b,image,sizeof(image),true);if(e)goto fail;
 e=owned_image_lease(d,b,image,sizeof(image),false);if(e)goto fail;
 e=d->cfg.ops->sync_for_device(d->cfg.priv,b->handle);
 if(e){memory_error(d,b,EOWNERDEAD);e=EOWNERDEAD;goto fail;}
 b->readable=true;*out=b;return 0;
fail:
 if(b->quarantined){*out=b;return EOWNERDEAD;}
 destroy_bo(d,b);if(b->live){*out=b;return EOWNERDEAD;}
 return d->failed?EOWNERDEAD:e;
}
static int event_profile_metadata(struct persistent *d,struct memory *b,bool owner,
 struct remesa_persistent_linked_event_profile_backing *out)
{
 int e=linked_live(d,b,owner);if(e)return e;
 if(!b->linked_event_profile||!b->linked_dependency)return EINVAL;
 if(b->bytes!=REMESA_PDS_PSB_EVENT_PROFILE_RESERVATION_BYTES||b->allocated<b->bytes||
    b->va%32||b->va<UINT64_C(0x20000000)||b->va+b->allocated>UINT64_C(0x30000000))return EPROTO;
 struct remesa_persistent_linked_event_profile_backing value={.owner=d->cfg.context,
  .generation=b->token.generation,.gpu_va=b->va,.handle=b->handle,
  .allocation_bytes=b->allocated,.reserved_bytes=b->bytes,
  .used_bytes=REMESA_PDS_PSB_EVENT_PROFILE_BYTES,.data_bytes=64,.code_offset=64,.code_bytes=84,
  .descriptor_field0c=16};
 e=event_auxiliary_metadata(d,b->linked_dependency,false,&value.auxiliary);if(e)return e;
 *out=value;return 0;
}
int remesa_persistent_linked_event_profile_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_linked_event_profile_backing *out)
{
 if((uintptr_t)out%_Alignof(struct remesa_persistent_linked_event_profile_backing))return EINVAL;
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 return event_profile_metadata(device->priv,object,true,out);
}
static int dpm_storage_metadata(struct persistent *d,struct memory *b,bool owner,
 struct remesa_persistent_memory_backing *out)
{
 if(!owns_memory(d,b)||!b->live||!b->refs||(owner&&!b->owner))return EINVAL;
 if(b->quarantined)return EIO;
 if(!b->pinned||!b->gpu_mapped||b->cpu_mapped)return EBUSY;
 struct remesa_resource r;
 if(remesa_registry_snapshot(&d->registry,b->token,&r)!=REMESA_OK)return ESTALE;
 if(r.state!=REMESA_MAPPED)return EBUSY;
 if(b->bytes!=0x2000000u||b->allocated!=0x2000000u)return ENOTSUP;
 *out=(struct remesa_persistent_memory_backing){.owner=d->cfg.context,
  .generation=b->token.generation,.gpu_va=b->va,.handle=b->handle,
  .logical_bytes=b->bytes,.allocation_bytes=b->allocated};return 0;
}
static int dpm_plan_metadata(struct persistent *d,struct memory *table,
 struct memory *parameter,bool owner,struct remesa_persistent_linked_dpm_backing *out)
{
 if(table==parameter)return EINVAL;
 int e=dpm_storage_metadata(d,table,owner,&out->table);if(e)return e;
 e=dpm_storage_metadata(d,parameter,owner,&out->parameter);if(e)return e;
 e=remesa_scene_dpm_build(table->va,table->allocated,parameter->va,parameter->allocated,&out->plan);
 if(e)return e;
 out->table_access=out->parameter_access=REMESA_ACCESS_READ|REMESA_ACCESS_WRITE;
 return 0;
}
int remesa_persistent_linked_dpm_prepare(struct remesa_device *device,void *table_object,
 void *parameter_object,void **out)
{
 if((uintptr_t)out%_Alignof(void *))return EINVAL;
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(*out)return EBUSY;
 struct persistent *d=device->priv;struct memory *table=table_object,*parameter=parameter_object;
 struct remesa_persistent_linked_dpm_backing metadata={0};
 e=dpm_plan_metadata(d,table,parameter,true,&metadata);if(e)return e;
 if(table->refs==UINT32_MAX||parameter->refs==UINT32_MAX)return EBUSY;
 struct linked_dpm *owned=calloc(1,sizeof(*owned));if(!owned)return ENOMEM;
 void *object=NULL;size_t before=d->allocated;
 e=allocate_class(d,240,&object,REMESA_VA_RASTER);
 if(e){free(owned);return d->failed||d->allocated!=before?EOWNERDEAD:e;}
 struct memory *b=object;b->linked_dpm=owned;owned->table=table;owned->parameter=parameter;
 table->refs++;parameter->refs++;
 if(b->va<UINT64_C(0x30000000)||b->va+b->allocated>UINT64_C(0x40000000)){e=ENOTSUP;goto fail;}
 /* Only this small plan is uploaded. The64MiB of DPM storage is untouched.
  * Original wait7 is recorded separately from the required completion15. */
 uint8_t image[240];const struct remesa_lab_dpm_control *p=&metadata.plan;
 for(unsigned i=0;i<13;i++)zclear_word(image+i*4,p->cookie[i]);
 zclear_word(image+52,p->info_bytes);
 for(unsigned i=0;i<44;i++)zclear_word(image+56+i*4,p->load_pairs[i]);
 zclear_word(image+232,p->original_wait_mask);zclear_word(image+236,p->retained_completion_mask);
 e=owned_image_lease(d,b,image,sizeof(image),true);if(e)goto fail;
 e=owned_image_lease(d,b,image,sizeof(image),false);if(e)goto fail;
 e=d->cfg.ops->sync_for_device(d->cfg.priv,b->handle);
 if(e){memory_error(d,b,EOWNERDEAD);e=EOWNERDEAD;goto fail;}
 b->readable=true;*out=b;return 0;
fail:
 if(b->quarantined){*out=b;return EOWNERDEAD;}
 destroy_bo(d,b);if(b->live){*out=b;return EOWNERDEAD;}
 return d->failed?EOWNERDEAD:e;
}
int remesa_persistent_linked_dpm_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_linked_dpm_backing *out)
{
 if((uintptr_t)out%_Alignof(struct remesa_persistent_linked_dpm_backing))return EINVAL;
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct memory *b=object;
 e=linked_live(d,b,true);if(e)return e;
 if(!b->linked_dpm)return EINVAL;
 struct remesa_persistent_linked_dpm_backing value={.owner=d->cfg.context,
  .generation=b->token.generation,.gpu_va=b->va,.handle=b->handle,
  .allocation_bytes=b->allocated,.data_bytes=240};
 e=dpm_plan_metadata(d,b->linked_dpm->table,b->linked_dpm->parameter,false,&value);if(e)return e;
 *out=value;return 0;
}
static int linked_region_prepare(struct remesa_device *device,void *targets_object,
 bool cpu_zero,void **out)
{
 if((uintptr_t)out%_Alignof(void *))return EINVAL;
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 if(*out)return EBUSY;
 struct persistent *d=device->priv;struct memory *targets=targets_object;
 struct remesa_persistent_linked_targets_backing tm;
 e=background_targets_metadata(d,targets,true,&tm);if(e)return e;
 struct remesa_scene_layout layout;
 e=remesa_scene_layout_encode(tm.commands.width,tm.commands.height,0,&layout);if(e)return e;
 if(targets->refs==UINT32_MAX)return EBUSY;
 void *object=NULL;size_t before=d->allocated;
 e=allocate_class(d,layout.required_bytes,&object,REMESA_VA_SURFACE);
 if(e)return d->failed||d->allocated!=before?EOWNERDEAD:e;
 struct memory *b=object;b->scene_hw=true;b->linked_region=true;b->scene_layout=layout;
 b->depth_width=tm.commands.width;b->depth_height=tm.commands.height;
 b->linked_dependency=targets;targets->refs++;
 if(b->va<UINT64_C(0x40000000)||b->va+b->allocated>UINT64_C(0x100000000)){e=ENOTSUP;goto fail;}
 /* The legacy prepare API retains its one-time zero/verify/sync policy.
  * Raw allocation leaves contents undefined for kernel-origin validation. */
 if(cpu_zero){
  const uint8_t zero=0;
  e=owned_image_lease(d,b,&zero,1,true);if(e)goto fail;
  e=owned_image_lease(d,b,&zero,1,false);if(e)goto fail;
  e=d->cfg.ops->sync_for_device(d->cfg.priv,b->handle);
  if(e){memory_error(d,b,EOWNERDEAD);e=EOWNERDEAD;goto fail;}
  b->readable=true;
 }
 *out=b;return 0;
fail:
 if(b->quarantined){*out=b;return EOWNERDEAD;}
 destroy_bo(d,b);if(b->live){*out=b;return EOWNERDEAD;}
 return d->failed?EOWNERDEAD:e;
}
int remesa_persistent_linked_region_prepare(struct remesa_device *device,void *targets,void **out)
{return linked_region_prepare(device,targets,true,out);}
int remesa_persistent_linked_region_allocate(struct remesa_device *device,void *targets,void **out)
{return linked_region_prepare(device,targets,false,out);}
static int region_metadata(struct persistent *d,struct memory *b,bool owner,
 struct remesa_persistent_linked_region_backing *out)
{
 int e=linked_attachment_live(d,b,owner);if(e)return e;
 if(!b->linked_region||!b->scene_hw||!b->linked_dependency)return EINVAL;
 struct remesa_persistent_linked_region_backing value={0};
 e=background_targets_metadata(d,b->linked_dependency,false,&value.targets);if(e)return e;
 if(b->depth_width!=value.targets.commands.width||b->depth_height!=value.targets.commands.height)return ESTALE;
 const struct remesa_scene_layout *layout=&b->scene_layout;
 if(b->va<UINT64_C(0x40000000)||b->va+b->allocated>UINT64_C(0x100000000)||
    layout->required_bytes>b->allocated||layout->cookie[7]>=b->allocated||layout->cookie[8]>=b->allocated)return EPROTO;
 value.region=(struct remesa_persistent_scene_backing){.owner=d->cfg.context,
  .generation=b->token.generation,.gpu_va=b->va,.region_gpu_va=b->va+layout->cookie[7],
  .handle=b->handle,.width=b->depth_width,.height=b->depth_height,
  .allocation_bytes=b->allocated,.layout=*layout};
 const uint32_t base=(uint32_t)b->va;
 const uint32_t ta[14]={0x220,base+layout->cookie[8],0x208,layout->cookie[2],
  0x20c,layout->cookie[3],0x214,layout->cookie[4],0x210,layout->cookie[5],
  0x248,layout->cookie[6],0x21c,base+layout->cookie[7]};
 memcpy(value.ta_scene,ta,sizeof(ta));value.raster_scene[0]=0x408;
 value.raster_scene[1]=base+layout->cookie[7];
 value.access=REMESA_ACCESS_READ|REMESA_ACCESS_WRITE;*out=value;return 0;
}
int remesa_persistent_linked_region_snapshot(const struct remesa_device *device,void *object,
 struct remesa_persistent_linked_region_backing *out)
{
 if((uintptr_t)out%_Alignof(struct remesa_persistent_linked_region_backing))return EINVAL;
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 return region_metadata(device->priv,object,true,out);
}
static int scene_footprint_add(struct persistent *d,struct memory *b,uint32_t access,
 uint32_t roles,struct remesa_persistent_scene_footprint *out)
{
 int e=linked_attachment_live(d,b,false);if(e)return e;
 if(!access||(access&~(REMESA_ACCESS_READ|REMESA_ACCESS_WRITE))||!roles||
    !b->bytes||b->bytes>b->allocated||b->va%PAGE_BYTES||b->allocated%PAGE_BYTES||
    b->va<d->cfg.va_start||b->va>d->cfg.va_end||b->allocated>d->cfg.va_end-b->va)return EPROTO;
 for(unsigned i=0;i<out->allocation_count;i++){
  if(out->allocations[i].handle==b->handle){
   if(out->allocations[i].generation!=b->token.generation||out->allocations[i].gpu_va!=b->va||
      out->allocations[i].logical_bytes!=b->bytes||out->allocations[i].allocation_bytes!=b->allocated)return ESTALE;
   out->allocations[i].access|=access;out->allocations[i].roles|=roles;return 0;
  }
  if(b->va<out->allocations[i].gpu_va+out->allocations[i].allocation_bytes&&
     out->allocations[i].gpu_va<b->va+b->allocated)return EINVAL;
 }
 if(out->allocation_count==REMESA_PERSISTENT_BUFFERS)return E2BIG;
 unsigned n=out->allocation_count++;
 out->allocations[n].generation=b->token.generation;out->allocations[n].gpu_va=b->va;
 out->allocations[n].handle=b->handle;out->allocations[n].access=access;out->allocations[n].roles=roles;
 out->allocations[n].logical_bytes=b->bytes;out->allocations[n].allocation_bytes=b->allocated;return 0;
}
int remesa_persistent_scene_footprint(const struct remesa_device *device,void *background_object,
 void *aux_object,void *region_object,void *dpm_object,struct remesa_persistent_scene_footprint *out)
{
 if((uintptr_t)out%_Alignof(struct remesa_persistent_scene_footprint))return EINVAL;
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct memory *bg=background_object,*aux=aux_object,
  *region=region_object,*dpm=dpm_object;
 struct memory *parents[4]={bg,aux,region,dpm};
 for(unsigned i=0;i<4;i++){
  e=i==2?linked_attachment_live(d,parents[i],true):linked_live(d,parents[i],true);if(e)return e;
  for(unsigned j=0;j<i;j++)if(parents[i]==parents[j])return EINVAL;
 }
 struct memory *event=NULL;
 if(aux->linked_event_profile){
  struct remesa_persistent_linked_event_profile_backing metadata;
  e=remesa_persistent_linked_event_profile_snapshot(device,aux,&metadata);if(e)return e;
  event=aux;aux=event->linked_dependency;
 }
 if(!bg->linked_background||!aux->linked_event_auxiliary||!region->linked_region||
    !region->scene_hw||!dpm->linked_dpm)return EINVAL;
 struct memory *targets=bg->linked_background->targets;
 if(aux->linked_dependency!=targets||region->linked_dependency!=targets)return EINVAL;
 struct remesa_persistent_linked_targets_backing tm;
 e=background_targets_metadata(d,targets,false,&tm);if(e)return e;
 if(region->depth_width!=tm.commands.width||region->depth_height!=tm.commands.height||
    region->scene_layout.required_bytes>region->allocated||region->scene_layout.cookie[7]>=region->allocated||
    region->scene_layout.cookie[8]>=region->allocated)return ESTALE;
 struct remesa_persistent_linked_dpm_backing dm={0};
 e=dpm_plan_metadata(d,dpm->linked_dpm->table,dpm->linked_dpm->parameter,false,&dm);if(e)return e;
 struct memory *ta=targets->linked_targets->ta,*reads[REMESA_PERSISTENT_BUFFERS]={0};unsigned count=0;
 struct remesa_use_entry_plan banks[16]={0};bool present[16]={0};
 e=linked_read_add(d,ta,reads,&count);if(e)return e;
 e=linked_ta_reads(d,ta->linked_ta->prefix,ta->linked_ta->body,reads,&count,banks,present);if(e)return e;
 e=linked_live(d,bg->linked_background->program,false);if(e)return e;
 e=body_bank_add(&bg->linked_background->entry,banks,present);if(e)return e;
 for(unsigned i=0;i<2;i++){
  struct remesa_use_entry_plan entry;uint32_t fixup;
  e=zclear_entry(aux,i?32:0,i?8:16,aux->event_pixel_bank,REMESA_USE_PIXEL,&entry,&fixup);if(e)return e;
  e=body_bank_add(&entry,banks,present);if(e)return e;
 }
 struct remesa_persistent_scene_footprint value={.owner=d->cfg.context,.width=tm.commands.width,
  .height=tm.commands.height,.draw_count=tm.commands.draw_count,
  .incomplete=REMESA_SCENE_NEEDS_EVENT_PDS|REMESA_SCENE_NEEDS_COLOR_LOAD_INIT|
   REMESA_SCENE_NEEDS_DPM_INIT|REMESA_SCENE_NEEDS_RASTER_CONTROL|REMESA_SCENE_NEEDS_NATIVE_ADMISSION};
 for(unsigned i=0;i<count;i++){
  e=scene_footprint_add(d,reads[i],REMESA_ACCESS_READ,REMESA_SCENE_REF_TA,&value);if(e)return e;
 }
 struct memory *extra[]={targets,targets->linked_targets->color,targets->linked_targets->depth,
  bg,bg->linked_background->program,aux,region,dpm->linked_dpm->table,dpm->linked_dpm->parameter};
 const uint32_t roles[]={REMESA_SCENE_REF_TARGET_DATA,REMESA_SCENE_REF_COLOR,REMESA_SCENE_REF_DEPTH,
  REMESA_SCENE_REF_BACKGROUND,REMESA_SCENE_REF_BACKGROUND_PROGRAM,REMESA_SCENE_REF_EVENT_AUXILIARY,
  REMESA_SCENE_REF_REGION,REMESA_SCENE_REF_DPM_TABLE,REMESA_SCENE_REF_DPM_PARAMETER};
 const uint32_t rw=REMESA_ACCESS_READ|REMESA_ACCESS_WRITE;
 const uint32_t access[]={REMESA_ACCESS_READ,rw,rw,REMESA_ACCESS_READ,REMESA_ACCESS_READ,
  REMESA_ACCESS_READ,rw,rw,rw};
 for(unsigned i=0;i<sizeof(extra)/sizeof(extra[0]);i++){
  e=scene_footprint_add(d,extra[i],access[i],roles[i],&value);if(e)return e;
 }
 if(event){
  e=scene_footprint_add(d,event,REMESA_ACCESS_READ,REMESA_SCENE_REF_EVENT_PDS,&value);if(e)return e;
 }
 *out=value;return 0;
}

/* Captures reserve no GPU operation ID and never enter provider dispatch. */
static int scene_capture_add(struct persistent *d,struct scene_capture *c,struct memory *b)
{
 int e=linked_attachment_live(d,b,false);if(e)return e;
 for(unsigned i=0;i<c->count;i++)if(c->held[i]==b)return 0;
 if(c->count==REMESA_PERSISTENT_BUFFERS)return E2BIG;
 if(b->refs==UINT32_MAX||b->scene_holds==UINT32_MAX)return EOVERFLOW;
 c->held[c->count++]=b;return 0;
}
int remesa_persistent_scene_capture(struct remesa_device *device,void *bg,void *aux,
 void *region,void *dpm,struct remesa_scene_capture *out)
{
 if((uintptr_t)out%_Alignof(struct remesa_scene_capture))return EINVAL;
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct scene_capture value={0};
 e=remesa_persistent_scene_footprint(device,bg,aux,region,dpm,&value.footprint);if(e)return e;
 if(d->scene_serial==UINT64_MAX)return EOVERFLOW;
 unsigned slot;
 for(slot=0;slot<REMESA_PROVIDER_MAX_JOBS;slot++)if(!d->scenes[slot].live)break;
 if(slot==REMESA_PROVIDER_MAX_JOBS)return EBUSY;
 for(unsigned i=0;i<value.footprint.allocation_count;i++){
  struct memory *b=NULL;
  for(unsigned k=0;k<REMESA_PERSISTENT_BUFFERS;k++)
   if(d->buffers[k].live&&d->buffers[k].handle==value.footprint.allocations[i].handle){b=&d->buffers[k];break;}
  if(!b||b->token.generation!=value.footprint.allocations[i].generation)return ESTALE;
  e=scene_capture_add(d,&value,b);if(e)return e;
 }
 struct memory *parents[]={bg,aux,region,dpm};
 for(unsigned i=0;i<4;i++){e=scene_capture_add(d,&value,parents[i]);if(e)return e;value.parents[i]=parents[i];}
 /* Validation is complete before any count changes; failure is transactional. */
 value.serial=++d->scene_serial;value.live=true;
 for(unsigned i=0;i<value.count;i++){value.held[i]->refs++;value.held[i]->scene_holds++;}
 d->scenes[slot]=value;
 *out=(struct remesa_scene_capture){.owner=d->cfg.context,.serial=value.serial,.slot=slot};return 0;
}
static int scene_capture_lookup(struct persistent *d,struct remesa_scene_capture token,
 struct scene_capture **out)
{
 if(token.reserved||token.owner!=d->cfg.context||!token.serial||token.slot>=REMESA_PROVIDER_MAX_JOBS)return EINVAL;
 struct scene_capture *c=&d->scenes[token.slot];
 if(!c->live||c->serial!=token.serial)return ESTALE;
 if(d->failed)return EOWNERDEAD;
 if(c->count>REMESA_PERSISTENT_BUFFERS||c->footprint.allocation_count>REMESA_PERSISTENT_BUFFERS)return EPROTO;
 for(unsigned i=0;i<c->count;i++){
  struct memory *b=c->held[i];
  if(!owns_memory(d,b)||!b->live||!b->refs||!b->scene_holds)return ESTALE;
  if(b->quarantined)return EOWNERDEAD;
  int e=linked_attachment_live(d,b,false);if(e)return e;
 }
 *out=c;return 0;
}
int remesa_persistent_scene_capture_snapshot(const struct remesa_device *device,
 struct remesa_scene_capture token,struct remesa_persistent_scene_footprint *out)
{
 if((uintptr_t)out%_Alignof(struct remesa_persistent_scene_footprint))return EINVAL;
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct scene_capture *c=NULL;
 e=scene_capture_lookup(device->priv,token,&c);if(e)return e;
 *out=c->footprint;return 0;
}
int remesa_persistent_scene_controls_snapshot(const struct remesa_device *device,
 struct remesa_scene_capture token,struct remesa_persistent_scene_controls *out)
{
 if((uintptr_t)out%_Alignof(struct remesa_persistent_scene_controls))return EINVAL;
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct scene_capture *capture=NULL;
 e=scene_capture_lookup(d,token,&capture);if(e)return e;
 struct memory *bg=capture->parents[0],*event=capture->parents[1],*region=capture->parents[2];
 if(!event->linked_event_profile)return ENOTSUP;
 struct remesa_persistent_linked_background_backing bm;
 struct remesa_persistent_linked_event_profile_backing em;
 struct remesa_persistent_linked_region_backing rm;
 e=background_metadata(d,bg,false,&bm);if(e)return e;
 e=event_profile_metadata(d,event,false,&em);if(e)return e;
 e=region_metadata(d,region,false,&rm);if(e)return e;
 struct memory *targets=bg->linked_background->targets;
 if(event->linked_dependency->linked_dependency!=targets||region->linked_dependency!=targets)return ESTALE;
 const struct remesa_persistent_linked_targets_backing *tm=&bm.targets;
 struct memory *prefix=targets->linked_targets->ta->linked_ta->prefix;
 e=linked_live(d,prefix,false);if(e)return e;
 if(!prefix->zclear_prefix||tm->commands.width!=capture->footprint.width||
    tm->commands.height!=capture->footprint.height)return ESTALE;
 struct remesa_persistent_scene_controls value={.owner=token.owner,.serial=token.serial,
  .width=tm->commands.width,.height=tm->commands.height,.incomplete=capture->footprint.incomplete,
  .raster_bytes=REMESA_RASTER_CONTROL_BYTES,.ta_bytes=REMESA_TA_CONTROL_BYTES};
 const struct remesa_raster_control_request request={
  .color={value.width,value.height,tm->color.state.stride_bytes/4,tm->color.gpu_va,tm->color.state.image_bytes},
  .has_depth=true,.depth={.depth={tm->depth.width,tm->depth.height,tm->depth.pitch_pixels,tm->depth.cpp,
   targets->linked_targets->ta->linked_ta->preserve_depth?REMESA_DEPTH_SURFACE_DEFINED:REMESA_DEPTH_SURFACE_UNDEFINED,tm->depth.gpu_va,tm->depth.allocation_bytes},
   .color_width=value.width,.color_height=value.height,.clear_depth=prefix->zclear_prefix->clear_depth,
   .caller_finalize_flag=tm->depth_finalize},
  .background={bm.gpu_va,bm.allocation_bytes,0,bm.object_bytes},
  .event={em.gpu_va,em.allocation_bytes,0,em.used_bytes},.event_field0c=em.descriptor_field0c};
 e=remesa_raster_control_encode(&request,value.raster);if(e)return e;
 const struct remesa_raster_object_reference stream={tm->commands.gpu_va,tm->commands.allocation_bytes,
  0,tm->commands.command_bytes};
 e=remesa_ta_control_encode(&stream,value.ta);if(e)return e;
 memcpy(value.ta_scene,rm.ta_scene,sizeof(value.ta_scene));
 memcpy(value.raster_scene,rm.raster_scene,sizeof(value.raster_scene));
 *out=value;return 0;
}
static int scene_use_add(struct persistent *d,const struct scene_capture *capture,
 struct remesa_persistent_scene_use *out,struct memory *b,size_t offset,size_t bytes,
 const struct remesa_use_entry_plan *entry)
{
 int e=linked_live(d,b,false);if(e)return e;
 unsigned i;
 for(i=0;i<capture->count&&capture->held[i]!=b;i++);
 if(i==capture->count)return ESTALE;
 for(i=0;i<capture->footprint.allocation_count;i++)if(capture->footprint.allocations[i].handle==b->handle)break;
 if(i==capture->footprint.allocation_count||capture->footprint.allocations[i].generation!=b->token.generation||
    capture->footprint.allocations[i].access!=REMESA_ACCESS_READ)return EACCES;
 uint64_t base;enum remesa_use_data_master master;
 e=remesa_use_base_unpack_address(entry->register_word,&base,&master);if(e)return e;
 struct remesa_use_entry_request request={b->va,b->allocated,offset,bytes,entry->bank,master};
 struct remesa_use_entry_plan checked;
 e=remesa_use_entry_prepare(&request,&checked);if(e)return e;
 uint32_t fixup;e=remesa_use_entry_preview_fixup(&checked,&fixup);if(e)return e;
 if(checked.base_address!=entry->base_address||checked.entry_address!=entry->entry_address||
    checked.program_end!=entry->program_end||checked.entry_byte_offset!=entry->entry_byte_offset||
    checked.register_offset!=entry->register_offset||checked.register_word!=entry->register_word)return ESTALE;
 if((out->bank_mask&(1u<<entry->bank))&&
    (out->banks[entry->bank].offset!=entry->register_offset||out->banks[entry->bank].word!=entry->register_word))return EBUSY;
 if(out->entry_count==REMESA_SCENE_USE_ENTRIES)return E2BIG;
 unsigned n=out->entry_count++;
 out->entries[n].generation=b->token.generation;out->entries[n].handle=b->handle;
 out->entries[n].bank=entry->bank;out->entries[n].data_master=master;out->entries[n].lower19=fixup;
 out->entries[n].byte_offset=offset;out->entries[n].program_bytes=bytes;
 out->bank_mask|=1u<<entry->bank;
 out->banks[entry->bank].offset=entry->register_offset;out->banks[entry->bank].word=entry->register_word;
 return 0;
}
int remesa_persistent_scene_use_snapshot(const struct remesa_device *device,
 struct remesa_scene_capture token,struct remesa_persistent_scene_use *out)
{
 if((uintptr_t)out%_Alignof(struct remesa_persistent_scene_use))return EINVAL;
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct scene_capture *c=NULL;
 e=scene_capture_lookup(d,token,&c);if(e)return e;
 if(!c->parents[1]->linked_event_profile)return ENOTSUP;
 struct remesa_persistent_scene_use value={.owner=token.owner,.serial=token.serial,
  .incomplete=c->footprint.incomplete};
 for(unsigned i=0;i<c->count;i++){
  struct memory *b=c->held[i];
  if(b->zclear_prefix)for(unsigned k=0;k<3;k++){
   if(k==1&&c->parents[0]->linked_background->targets->linked_targets->ta->linked_ta->preserve_depth)continue;
   /* INITIAL consumes the existing vertex and state-copy USE entries.
    * Its DATA has zero fragment references; clear pixel CODE is not consumed. */
   const struct remesa_persistent_zclear_prefix_backing *p=b->zclear_prefix;
   e=scene_use_add(d,c,&value,b,p->use_offset[k],p->use_bytes[k],&p->entries[k]);if(e)return e;
  }
  if(b->scene_close||b->state_kind==REMESA_PERSISTENT_STATE_LINKED_DRAW||
     b->state_kind==REMESA_PERSISTENT_STATE_GEARS_DRAW||b->state_kind==REMESA_PERSISTENT_STATE_GEARS_HOMOGENEOUS_DRAW){
   e=scene_use_add(d,c,&value,b,b->state_use_offset,16,&b->state_use_entry);if(e)return e;
  }
  if(b->state_kind==REMESA_PERSISTENT_STATE_VERTEX_PARAMETERS){
   e=scene_use_add(d,c,&value,b,b->state_use_offset,8,&b->state_use_entry);if(e)return e;
  }
  if(b->linked_program)for(unsigned k=0;k<2;k++){
   const struct linked_program *p=b->linked_program;
   e=scene_use_add(d,c,&value,b,p->offset[k],p->bytes[k],&p->entry[k]);if(e)return e;
  }
  if(b->linked_background){
   e=scene_use_add(d,c,&value,b->linked_background->program,0,8,&b->linked_background->entry);if(e)return e;
  }
  if(b->linked_event_auxiliary)for(unsigned k=0;k<2;k++){
   struct remesa_use_entry_plan entry;uint32_t fixup;
   e=zclear_entry(b,k?32:0,k?8:16,b->event_pixel_bank,REMESA_USE_PIXEL,&entry,&fixup);if(e)return e;
   e=scene_use_add(d,c,&value,b,k?32:0,k?8:16,&entry);if(e)return e;
  }
 }
 if(!value.entry_count||!value.bank_mask)return ENODATA;
 *out=value;return 0;
}
struct scene_ta_context {
 struct persistent *device;struct scene_capture *capture;
 struct remesa_persistent_scene_ta *result;struct memory *state;
};
static int scene_ta_reference(void *context,const struct remesa_ta_reference *ref)
{
 struct scene_ta_context *ctx=context;struct persistent *d=ctx->device;
 struct scene_capture *c=ctx->capture;struct remesa_persistent_scene_ta *out=ctx->result;
 struct memory *b=NULL,*ta=c->parents[0]->linked_background->targets->linked_targets->ta;
 struct linked_ta *t=ta->linked_ta;size_t offset=0,object_bytes=ref->minimum_bytes;unsigned i;
 for(i=0;i<c->footprint.allocation_count;i++){
  const uint64_t va=c->footprint.allocations[i].gpu_va;
  if(ref->address<va||(uint64_t)ref->address-va>c->footprint.allocations[i].logical_bytes||
     ref->minimum_bytes>c->footprint.allocations[i].logical_bytes-((uint64_t)ref->address-va))continue;
  if(c->footprint.allocations[i].access!=REMESA_ACCESS_READ)return EACCES;
  for(unsigned k=0;k<c->count;k++)if(c->held[k]->handle==c->footprint.allocations[i].handle){b=c->held[k];break;}
  if(b&&(b->token.generation!=c->footprint.allocations[i].generation||b->va!=va))return ESTALE;
  offset=(uint64_t)ref->address-va;break;
 }
 if(!b)return ENOENT;
 int e=linked_live(d,b,false);if(e)return e;
 switch(ref->kind){
 case REMESA_TA_REF_INITIAL_STATE:{
  const struct remesa_persistent_zclear_prefix_backing *p=t->prefix->zclear_prefix;
  if(b!=t->prefix||!p->initial_pds_offset||offset!=p->initial_pds_offset)return EINVAL;
  object_bytes=60;break;
 }
 case REMESA_TA_REF_PREFIX_STATE:
 case REMESA_TA_REF_PREFIX_VERTEX:
 case REMESA_TA_REF_PREFIX_INDEX:{
  const struct remesa_persistent_zclear_prefix_backing *p=t->prefix->zclear_prefix;
  unsigned which=ref->kind==REMESA_TA_REF_PREFIX_STATE?3:0;
  if(b!=t->prefix)return EINVAL;
  if(ref->kind==REMESA_TA_REF_PREFIX_INDEX){if(offset!=p->index_offset||ref->index_count!=p->index_count)return EINVAL;}
  else{if(offset!=p->pds_offset[which])return EINVAL;object_bytes=p->pds_bytes[which];}
  break;
 }
 case REMESA_TA_REF_DRAW_STATE:{
  const struct linked_body *body=t->body->linked_body;bool found=false;
  for(unsigned k=1;k<body->dependency_count;k++)if(body->dependencies[k]==b)found=true;
  if(!found||b->state_kind!=linked_body_state_kind(body)||offset!=b->state_pds_offset)return EINVAL;
  object_bytes=60;ctx->state=b;break;
 }
 case REMESA_TA_REF_DRAW_PARAMETERS:{
  if(!t->body->linked_body->parameters||!ctx->state||!ctx->state->linked_dependency||
     b!=ctx->state->linked_dependency->linked_pds->parameters||
     b->state_kind!=REMESA_PERSISTENT_STATE_VERTEX_PARAMETERS||b->state_parameter_dwords!=36||
     b->state_bytes!=160||offset!=b->state_pds_offset)return EINVAL;
  object_bytes=60;break;
 }
 case REMESA_TA_REF_DRAW_INDEX:{
  if(!ctx->state||!ctx->state->linked_dependency||!ctx->state->linked_dependency->linked_pds)return EINVAL;
  struct memory *geometry=ctx->state->linked_dependency->linked_pds->geometry;
  if(b!=geometry||!b->linked_geometry)return EINVAL;
  const struct linked_geometry *g=b->linked_geometry;
  if(offset<g->index_offset||((offset-g->index_offset)&1)||offset-g->index_offset>g->index_bytes||
     ref->minimum_bytes>g->index_bytes-(offset-g->index_offset))return ERANGE;
  break;
 }
 case REMESA_TA_REF_DRAW_VERTEX:{
  if(!ctx->state||b!=ctx->state->linked_dependency||!b->linked_pds)return EINVAL;
  struct remesa_persistent_linked_pds_backing p;
  e=linked_pds_metadata(d,b,false,&p);if(e)return e;
  if(offset!=p.object_offset[0])return EINVAL;
  object_bytes=p.image_bytes[0];break;
 }
 case REMESA_TA_REF_CLOSE_STATE:
  if(b!=t->body->linked_body->dependencies[0]||!b->scene_close||offset!=b->state_pds_offset)return EINVAL;
  object_bytes=60;break;
 default:return EINVAL;
 }
 if(object_bytes<ref->minimum_bytes||offset>b->bytes||object_bytes>b->bytes-offset||
    out->reference_count==REMESA_TA_REFERENCE_MAX)return ERANGE;
 unsigned n=out->reference_count++;
 out->references[n].generation=b->token.generation;out->references[n].handle=b->handle;
 out->references[n].byte_offset=offset;out->references[n].minimum_bytes=ref->minimum_bytes;
 out->references[n].object_bytes=object_bytes;out->references[n].kind=ref->kind;
 out->references[n].index_count=ref->index_count;out->references[n].draw=ref->draw;
 return 0;
}
int remesa_persistent_scene_ta_snapshot(const struct remesa_device *device,
 struct remesa_scene_capture token,struct remesa_persistent_scene_ta *out)
{
 if((uintptr_t)out%_Alignof(struct remesa_persistent_scene_ta))return EINVAL;
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct scene_capture *c=NULL;
 e=scene_capture_lookup(d,token,&c);if(e)return e;
 struct memory *ta=c->parents[0]->linked_background->targets->linked_targets->ta;
 e=linked_live(d,ta,false);if(e)return e;
 if(!ta->linked_ta)return EINVAL;
 struct remesa_persistent_scene_ta value={.owner=token.owner,.serial=token.serial,.incomplete=c->footprint.incomplete};
 struct scene_ta_context ctx={d,c,&value,NULL};struct remesa_ta_reference_counts counts;
 e=ta->linked_ta->preserve_depth?
  remesa_matrix_preserve_ta_walk(ta->linked_ta->commands,ta->linked_ta->bytes,scene_ta_reference,&ctx,&counts):
  ta->linked_ta->body->linked_body->parameters?
  remesa_ta_gears_references_walk(ta->linked_ta->commands,ta->linked_ta->bytes,scene_ta_reference,&ctx,&counts):
  remesa_ta_references_walk(ta->linked_ta->commands,ta->linked_ta->bytes,scene_ta_reference,&ctx,&counts);if(e)return e;
 if(counts.references!=value.reference_count||counts.draws!=c->footprint.draw_count)return ESTALE;
 value.draw_count=counts.draws;*out=value;return 0;
}
int remesa_persistent_scene_parameters_snapshot(const struct remesa_device *device,
 struct remesa_scene_capture token,struct remesa_persistent_scene_parameters *out)
{
 if((uintptr_t)out%_Alignof(struct remesa_persistent_scene_parameters))return EINVAL;
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct scene_capture *c=NULL;
 e=scene_capture_lookup(d,token,&c);if(e)return e;
 struct remesa_persistent_scene_ta ta;
 e=remesa_persistent_scene_ta_snapshot(device,token,&ta);if(e)return e;
 struct remesa_persistent_scene_parameters value={.owner=token.owner,.serial=token.serial,.draw_count=ta.draw_count};
 for(unsigned r=0;r<ta.reference_count;r++)if(ta.references[r].kind==REMESA_TA_REF_DRAW_PARAMETERS){
  if(value.window_count==REMESA_TA_REFERENCE_MAX_DRAWS)return E2BIG;
  struct memory *b=NULL;
  for(unsigned i=0;i<c->count;i++)if(c->held[i]->handle==ta.references[r].handle){b=c->held[i];break;}
  if(!b||!b->isp_update||!b->readable||b->token.generation!=ta.references[r].generation||
     b->state_kind!=REMESA_PERSISTENT_STATE_VERTEX_PARAMETERS)return ESTALE;
  if(b->state_bytes>UINT32_MAX||(b->state_bytes&3))return ERANGE;
  struct remesa_parameter_window w={.owner=token.owner,.generation=b->token.generation,
   .gpu_va=b->va,.id=b->handle,.allocation_bytes=(uint32_t)b->allocated,
   .coefficient_dwords=b->state_parameter_dwords,.effective_source_dwords=1u+b->state_parameter_dwords,
   .transfer_dwords=(uint32_t)(b->state_bytes/4),.use_offset=(uint32_t)b->state_use_offset,.use_bytes=8,
   .pds_offset=(uint32_t)b->state_pds_offset,.pds_bytes=60,.ta_offset=(uint32_t)b->state_ta_offset,.ta_bytes=8};
  if(b->allocated>UINT32_MAX||b->state_use_offset>UINT32_MAX||b->state_pds_offset>UINT32_MAX||
     b->state_ta_offset>UINT32_MAX||!remesa_parameter_window_valid(&w))return ERANGE;
  unsigned i;
  for(i=0;i<c->footprint.allocation_count;i++)if(c->footprint.allocations[i].handle==b->handle)break;
  if(i==c->footprint.allocation_count||c->footprint.allocations[i].access!=REMESA_ACCESS_READ||
     c->footprint.allocations[i].generation!=w.generation||c->footprint.allocations[i].gpu_va!=w.gpu_va)
   return EACCES;
  unsigned n=value.window_count++;
  value.windows[n].draw=ta.references[r].draw;value.windows[n].window=w;
 }
 *out=value;return 0;
}
int remesa_persistent_scene_vertices_snapshot(const struct remesa_device *device,
 struct remesa_scene_capture token,struct remesa_persistent_scene_vertices *out)
{
 if((uintptr_t)out%_Alignof(struct remesa_persistent_scene_vertices))return EINVAL;
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct scene_capture *c=NULL;
 e=scene_capture_lookup(d,token,&c);if(e)return e;
 struct remesa_persistent_scene_ta ta;
 e=remesa_persistent_scene_ta_snapshot(device,token,&ta);if(e)return e;
 struct memory *commands=c->parents[0]->linked_background->targets->linked_targets->ta;
 struct remesa_persistent_scene_vertices value={.owner=token.owner,.serial=token.serial,
  .draw_count=ta.draw_count,.window_count=ta.draw_count+1,.incomplete=c->footprint.incomplete};
 for(unsigned slot=0;slot<value.window_count;slot++){
  unsigned draw=slot;
  unsigned ir=ta.reference_count,pr=ir;struct memory *vertex=NULL;size_t offset,bytes;uint32_t records;
  for(unsigned r=0;r<ta.reference_count;r++)if(ta.references[r].draw==draw){
   if(ta.references[r].kind==(draw?REMESA_TA_REF_DRAW_INDEX:REMESA_TA_REF_PREFIX_INDEX))ir=r;
   if(ta.references[r].kind==(draw?REMESA_TA_REF_DRAW_VERTEX:REMESA_TA_REF_PREFIX_VERTEX))pr=r;
  }
  if(ir==ta.reference_count||pr==ta.reference_count)return EPROTO;
  if(!draw){
   vertex=commands->linked_ta->prefix;
   offset=vertex->zclear_prefix->vertex_offset;bytes=vertex->zclear_prefix->vertex_referenced_bytes;
   if(bytes%16||bytes/16>UINT32_MAX)return ERANGE;
   records=bytes/16;
  }else{
   struct memory *pds=NULL;
   for(unsigned i=0;i<c->count;i++)if(c->held[i]->handle==ta.references[pr].handle){pds=c->held[i];break;}
   if(!pds||!pds->linked_pds)return ESTALE;
   vertex=pds->linked_pds->geometry;
   if(!vertex->linked_geometry)return ESTALE;
   offset=0;bytes=vertex->linked_geometry->vertex_bytes;records=vertex->linked_geometry->vertex_count;
  }
  e=linked_live(d,vertex,false);if(e)return e;
  if(offset>UINT32_MAX||bytes>UINT32_MAX||!records||offset>vertex->bytes||bytes>vertex->bytes-offset)return ERANGE;
  value.windows[slot].index_handle=ta.references[ir].handle;value.windows[slot].index_generation=ta.references[ir].generation;
  value.windows[slot].index_offset=ta.references[ir].byte_offset;value.windows[slot].index_bytes=ta.references[ir].object_bytes;
  value.windows[slot].vertex_handle=vertex->handle;value.windows[slot].vertex_generation=vertex->token.generation;
  value.windows[slot].vertex_offset=offset;value.windows[slot].vertex_bytes=bytes;value.windows[slot].vertex_count=records;
 }
 *out=value;return 0;
}
int remesa_persistent_scene_packet_snapshot(const struct remesa_device *device,
 struct remesa_scene_capture token,struct remesa_scene_packet *out)
{
 if((uintptr_t)out%_Alignof(struct remesa_scene_packet))return EINVAL;
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 struct persistent *d=device->priv;struct scene_capture *c=NULL;
 e=scene_capture_lookup(d,token,&c);if(e)return e;
 struct remesa_persistent_scene_controls controls;
 struct remesa_persistent_scene_use use;
 struct remesa_persistent_scene_vertices vertices;
 e=remesa_persistent_scene_controls_snapshot(device,token,&controls);if(e)return e;
 e=remesa_persistent_scene_use_snapshot(device,token,&use);if(e)return e;
 e=remesa_persistent_scene_vertices_snapshot(device,token,&vertices);if(e)return e;
 if(c->footprint.allocation_count>32||use.entry_count>55||vertices.window_count>17)return E2BIG;
 struct memory *bg=c->parents[0],*event=c->parents[1],*region=c->parents[2],*dpm=c->parents[3];
 struct remesa_persistent_linked_background_backing bm;
 struct remesa_persistent_linked_event_profile_backing em;
 struct remesa_persistent_linked_region_backing rm;
 e=background_metadata(d,bg,false,&bm);if(e)return e;
 e=event_profile_metadata(d,event,false,&em);if(e)return e;
 e=region_metadata(d,region,false,&rm);if(e)return e;
 struct memory *targets=bg->linked_background->targets;
 struct memory *prefix=targets->linked_targets->ta->linked_ta->prefix;
 struct remesa_scene_packet p={.owner=token.owner,.serial=token.serial,.version=1,
  .binding_count=c->footprint.allocation_count,.use_count=use.entry_count,.window_count=vertices.window_count};
 for(unsigned i=0;i<p.binding_count;i++){
  p.bindings[i].id=c->footprint.allocations[i].handle;p.bindings[i].access=c->footprint.allocations[i].access;
 }
 p.actions=(struct remesa_scene_packet_actions){1,32,controls.width,controls.height,rm.region.handle,
  dpm->linked_dpm->table->handle,dpm->linked_dpm->parameter->handle,0};
 const struct remesa_persistent_linked_targets_backing *tm=&bm.targets;
 uint32_t depth_bits;memcpy(&depth_bits,&prefix->zclear_prefix->clear_depth,4);
 p.controls=(struct remesa_scene_packet_controls){targets->linked_targets->ta->linked_ta->preserve_depth?3:tm->depth_finalize?2:1,80,controls.width,controls.height,
  tm->color.state.stride_bytes/4,tm->depth.pitch_pixels,tm->depth.cpp,depth_bits,
  tm->color.handle,tm->depth.handle,tm->commands.handle,0,tm->commands.command_bytes,
  bm.handle,0,bm.object_bytes,em.handle,0,em.used_bytes,tm->depth_finalize?1:0};
 for(unsigned i=0;i<p.use_count;i++){
  if(use.entries[i].byte_offset>UINT32_MAX||use.entries[i].program_bytes>UINT32_MAX)return ERANGE;
  p.uses[i]=(struct remesa_scene_packet_use){use.entries[i].handle,use.entries[i].byte_offset,
   use.entries[i].program_bytes,use.entries[i].bank,use.entries[i].data_master,0};
 }
 for(unsigned i=0;i<p.window_count;i++)p.windows[i]=(struct remesa_scene_packet_window){
  vertices.windows[i].index_handle,vertices.windows[i].index_offset,vertices.windows[i].index_bytes,
  vertices.windows[i].vertex_handle,vertices.windows[i].vertex_offset,vertices.windows[i].vertex_bytes,
  vertices.windows[i].vertex_count,0};
 if(!remesa_scene_packet_valid(&p))return EPROTO;
 *out=p;return 0;
}
int remesa_persistent_scene_capture_release(struct remesa_device *device,
 struct remesa_scene_capture token)
{
 if(!device||device->ops!=&device_ops||!device->priv)return EINVAL;
 struct persistent *d=device->priv;struct scene_capture *c=NULL;
 int e=scene_capture_lookup(d,token,&c);if(e)return e;
 if(c->submitted)return EBUSY;
 struct memory *held[REMESA_PERSISTENT_BUFFERS];unsigned count=c->count;
 memcpy(held,c->held,count*sizeof(*held));
 for(unsigned i=0;i<count;i++)held[i]->scene_holds--;
 memset(c,0,sizeof(*c));
 for(unsigned i=0;i<count;i++)drop_memory(d,held[i]);
 for(unsigned i=0;i<count;i++)if(held[i]->quarantined)return EOWNERDEAD;
 return d->failed?EOWNERDEAD:0;
}

int remesa_persistent_set_native_scene(struct remesa_device *device,remesa_gpu_scene_fn fn)
{
 if(!device||device->ops!=&device_ops||!device->priv||!fn)return EINVAL;
 struct persistent *d=device->priv;
 if(d->failed)return EOWNERDEAD;
 if(d->native_scene||d->registry.next_generation!=1||d->provider.next_operation!=1||
    d->scene_serial||d->shader_generation||d->allocated||d->shaders)return EBUSY;
 d->native_scene=fn;return 0;
}
int remesa_persistent_native_scene_ready(const struct remesa_device *device)
{
 if(!device||device->ops!=&device_ops||!device->priv)return EINVAL;
 const struct persistent *d=device->priv;
 if(d->failed)return EOWNERDEAD;
 return d->native_scene?0:EOPNOTSUPP;
}
int remesa_persistent_set_native_draw_scene(struct remesa_device *device,remesa_gpu_scene_fn fn)
{
 int e=remesa_persistent_set_native_scene(device,fn);if(e)return e;
 ((struct persistent *)device->priv)->native_draw_scene=true;return 0;
}
int remesa_persistent_native_draw_ready(const struct remesa_device *device)
{
 int e=remesa_persistent_native_scene_ready(device);if(e)return e;
 return ((const struct persistent *)device->priv)->native_draw_scene?0:EOPNOTSUPP;
}
int remesa_persistent_set_native_scene_parameters(struct remesa_device *device,
 remesa_gpu_scene_parameters_fn fn)
{
 if(!device||device->ops!=&device_ops||!device->priv||!fn)return EINVAL;
 struct persistent *d=device->priv;
 if(d->failed)return EOWNERDEAD;
 if(d->native_scene_parameters||d->registry.next_generation!=1||d->provider.next_operation!=1||
    d->scene_serial||d->shader_generation||d->allocated||d->shaders)return EBUSY;
 d->native_scene_parameters=fn;return 0;
}
static int scene_submit_fault(struct persistent *d,struct submission *j)
{
 (void)remesa_provider_fault(&d->provider,j->fence,d->cfg.context,j->operation);
 /* A failed private-root operation poisons the entire address-space owner,
  * including CPU-only graph storage and currently unused buffers. */
 for(unsigned i=0;i<REMESA_PERSISTENT_BUFFERS;i++)if(d->buffers[i].live)quarantine(d,&d->buffers[i]);
 d->failed=true;j->error=EOWNERDEAD;return EOWNERDEAD;
}
static int scene_capture_submit(struct remesa_device *device,
 struct remesa_scene_capture token,void **out,bool keep)
{
 if((uintptr_t)out%_Alignof(void *))return EINVAL;
 int e=remesa_persistent_shader_output_check(device,out,sizeof(*out));if(e)return e;
 if(*out)return EBUSY;
 struct persistent *d=device->priv;
 if(d->failed)return EOWNERDEAD;
 if(!d->native_scene)return EOPNOTSUPP;
 struct scene_capture *c=NULL;e=scene_capture_lookup(d,token,&c);if(e)return e;
 if(c->submitted)return EBUSY;
 struct submission *j=NULL;
 for(unsigned i=0;i<REMESA_PROVIDER_MAX_JOBS;i++){
  if(d->jobs[i].live&&!d->jobs[i].released)return EBUSY;
  if(!j&&!d->jobs[i].live)j=&d->jobs[i];
 }
 if(!j)return EBUSY;
 struct remesa_scene_packet packet;
 e=remesa_persistent_scene_packet_snapshot(device,token,&packet);if(e)return e;
 struct remesa_use uses[REMESA_PERSISTENT_BUFFERS]={0};
 struct memory *bound[REMESA_PERSISTENT_BUFFERS]={0};
 if(packet.binding_count!=c->footprint.allocation_count||packet.binding_count>REMESA_PROVIDER_MAX_USES)return EPROTO;
 for(unsigned i=0;i<packet.binding_count;i++){
  struct memory *b=NULL;const uint32_t id=packet.bindings[i].id;
  for(unsigned k=0;k<c->count;k++)if(c->held[k]->handle==id){b=c->held[k];break;}
  if(!b||b->token.generation!=c->footprint.allocations[i].generation)return ESTALE;
  bound[i]=b;uses[i]=(struct remesa_use){b->token,0,b->allocated,packet.bindings[i].access};
 }
 *j=(struct submission){.live=true,.scene=true,.scene_packet=packet};c->submitted=true;
 d->dispatching=j;int s=remesa_provider_submit(&d->provider,uses,packet.binding_count,&j->fence);d->dispatching=NULL;
 if(s!=REMESA_OK&&s!=REMESA_AMBIGUOUS){c->submitted=false;memset(j,0,sizeof(*j));return status(s);}
 *out=j;
 if(s==REMESA_AMBIGUOUS||!j->scene_verified)return scene_submit_fault(d,j);
 if(remesa_provider_complete(&d->provider,j->fence,d->cfg.context,j->operation,1)!=REMESA_OK||
    remesa_provider_publish(&d->provider,j->fence,d->cfg.context,j->operation)!=REMESA_OK||
    remesa_provider_release(&d->provider,j->fence)!=REMESA_OK)return scene_submit_fault(d,j);
 for(unsigned i=0;i<packet.binding_count;i++)if(uses[i].access&REMESA_ACCESS_WRITE)
  bound[i]->readable=true;
 j->released=true;c->submitted=false;
 if(keep){c->published_operation=j->operation;return 0;}
 e=remesa_persistent_scene_capture_release(device,token);
 if(e){d->failed=true;j->error=EOWNERDEAD;return EOWNERDEAD;}
 return 0;
}
int remesa_persistent_scene_capture_submit(struct remesa_device *device,
 struct remesa_scene_capture token,void **out)
{return scene_capture_submit(device,token,out,false);}
int remesa_persistent_scene_capture_submit_keep(struct remesa_device *device,
 struct remesa_scene_capture token,void **out)
{return scene_capture_submit(device,token,out,true);}

static int scene_color_fault(struct persistent *d)
{
 for(unsigned i=0;i<REMESA_PERSISTENT_BUFFERS;i++)
  if(d->buffers[i].live)quarantine(d,&d->buffers[i]);
 d->failed=true;return EOWNERDEAD;
}
int remesa_persistent_scene_parameters_update(struct remesa_device *device,
 struct remesa_scene_capture token,uint32_t draw,const uint32_t *bits,uint32_t words)
{
 if(!device||device->ops!=&device_ops||!device->priv||!bits||
    (uintptr_t)bits%_Alignof(uint32_t)||!words||words>127||!draw)return EINVAL;
 struct persistent *d=device->priv;
 if(d->failed)return EOWNERDEAD;
 int e=depth_output_check(device,(void *)bits,(size_t)words*4);if(e)return e;
 if(!d->native_scene_parameters)return EOPNOTSUPP;
 struct scene_capture *c=NULL;e=scene_capture_lookup(d,token,&c);if(e)return e;
 if(c->submitted||c->read_color||d->dispatching)return EBUSY;
 if(!c->published_operation)return EAGAIN;
 if(c->parameter_sequence==UINT64_MAX)return EOVERFLOW;
 for(unsigned i=0;i<REMESA_PROVIDER_MAX_JOBS;i++)
  if(d->jobs[i].live&&!d->jobs[i].released)return EBUSY;
 struct remesa_persistent_scene_parameters parameters;
 e=remesa_persistent_scene_parameters_snapshot(device,token,&parameters);if(e)return e;
 const struct remesa_parameter_window *window=NULL;
 for(unsigned i=0;i<parameters.window_count;i++)if(parameters.windows[i].draw==draw){
  if(window)return EPROTO;
  window=&parameters.windows[i].window;
 }
 if(!window||words!=window->coefficient_dwords)return EINVAL;
 struct memory *b=NULL;
 for(unsigned i=0;i<c->count;i++)if(c->held[i]->handle==window->id)b=c->held[i];
 if(!b||b->token.generation!=window->generation)return ESTALE;
 /* Another capture must not silently observe a change to its frozen inputs. */
 if(b->scene_holds!=1)return EBUSY;
 uint32_t copied[127];memcpy(copied,bits,(size_t)words*4);
 struct remesa_gpu_scene_parameters update={.context=token.owner,.serial=token.serial,
  .published_operation=c->published_operation,.sequence=c->parameter_sequence+1,
  .window=*window,.draw=draw,.words=words,.bits=copied};
 struct remesa_gpu_scene_parameter_receipt receipt={0};
 e=d->native_scene_parameters(d->cfg.priv,&update,&receipt);
 if(e){
  if(e==EOWNERDEAD||e<0)return scene_color_fault(d);
  return e;
 }
 if(receipt.context!=update.context||receipt.serial!=update.serial||
    receipt.published_operation!=update.published_operation||receipt.sequence!=update.sequence)
  return scene_color_fault(d);
 c->parameter_sequence=receipt.sequence;return 0;
}
static int scene_attachment_role(const struct scene_capture *c,const struct memory *b,uint32_t role)
{
 unsigned matches=0;
 if(c->submitted||c->footprint.allocation_count>REMESA_PERSISTENT_BUFFERS)return EBUSY;
 for(unsigned i=0;i<c->footprint.allocation_count;i++)if(c->footprint.allocations[i].handle==b->handle){
  if(c->footprint.allocations[i].generation!=b->token.generation||
     c->footprint.allocations[i].gpu_va!=b->va||
     c->footprint.allocations[i].allocation_bytes!=b->allocated||
     c->footprint.allocations[i].logical_bytes>b->allocated||
     c->footprint.allocations[i].roles!=role||
     !(c->footprint.allocations[i].access&REMESA_ACCESS_WRITE))return EACCES;
  matches++;
 }
 return matches==1?0:EACCES;
}
static int scene_color_role(const struct scene_capture *c,const struct memory *b)
{return scene_attachment_role(c,b,REMESA_SCENE_REF_COLOR);}
int remesa_persistent_scene_capture_validate_attachments(const struct remesa_device *device,
 struct remesa_scene_capture token,void *color,void *depth,uint32_t width,uint32_t height)
{
 if(!device||device->ops!=&device_ops||!device->priv||!color||!depth||color==depth)return EINVAL;
 struct persistent *d=device->priv;struct scene_capture *c=NULL;
 int e=scene_capture_lookup(d,token,&c);if(e)return e;
 if(c->submitted||c->read_color||d->dispatching)return EBUSY;
 if(c->footprint.width!=width||c->footprint.height!=height)return EINVAL;
 struct memory *attachments[2]={color,depth};
 for(unsigned i=0;i<2;i++){
  struct memory *b=attachments[i];unsigned matches=0;
  if(!owns_memory(d,b))return EINVAL;
  for(unsigned k=0;k<c->count;k++)if(c->held[k]==b)matches++;
  if(matches!=1||!b->owner)return EACCES;
  e=scene_attachment_role(c,b,i?REMESA_SCENE_REF_DEPTH:REMESA_SCENE_REF_COLOR);if(e)return e;
 }
 return 0;
}
int remesa_persistent_scene_color_map(struct remesa_device *device,
 struct remesa_scene_capture token,const void **out)
{
 if((uintptr_t)out%_Alignof(void *))return EINVAL;
 int e=remesa_persistent_shader_output_check(device,out,sizeof(*out));if(e)return e;
 if(*out)return EBUSY;
 struct persistent *d=device->priv;struct scene_capture *c=NULL;
 e=scene_capture_lookup(d,token,&c);if(e)return e;
 if(!c->published_operation)return EAGAIN;
 if(c->read_color||d->dispatching)return EBUSY;
 for(unsigned i=0;i<REMESA_PROVIDER_MAX_JOBS;i++)
  if(d->jobs[i].live&&!d->jobs[i].released)return EBUSY;
 for(unsigned i=0;i<REMESA_PERSISTENT_BUFFERS;i++)
  if(d->buffers[i].cpu_mapped)return EBUSY;
 struct memory *b=NULL;
 for(unsigned i=0;i<c->footprint.allocation_count;i++)if(c->footprint.allocations[i].roles&REMESA_SCENE_REF_COLOR){
  if(b)return EPROTO;
  for(unsigned k=0;k<c->count;k++)if(c->held[k]->handle==c->footprint.allocations[i].handle){b=c->held[k];break;}
  if(!b)return ESTALE;
 }
 if(!b)return ENODATA;
 e=scene_color_role(c,b);if(e)return e;
 if(!b->readable)return EAGAIN;
 if(b->refs==UINT32_MAX)return EBUSY;
 /* Every capture contributing a hold must agree that this is an idle COLOR,
  * never a shader/input/DEPTH/DPM alias. No ordinary map guard is relaxed. */
 unsigned holds=0;
 for(unsigned i=0;i<REMESA_PROVIDER_MAX_JOBS;i++)if(d->scenes[i].live){
  struct scene_capture *other=&d->scenes[i];
  if(other->count>REMESA_PERSISTENT_BUFFERS)return EPROTO;
  for(unsigned k=0;k<other->count;k++)if(other->held[k]==b){
   e=scene_color_role(other,b);if(e)return e;holds++;
  }
 }
 if(holds!=b->scene_holds)return ESTALE;
 void *cpu=NULL;e=d->cfg.ops->cpu_map(d->cfg.priv,b->handle,false,&cpu);
 if(e&&e!=EOWNERDEAD&&!cpu)return e; /* provider guarantees atomic refusal */
 /* Retain the actual READ lease on all uncertain map/unmap outcomes. */
 b->cpu_mapped=true;b->scene_color_read=true;++b->refs;c->read_color=b;
 if(e||!cpu||remesa_persistent_shader_output_check(device,cpu,b->allocated)||
    overlaps(out,sizeof(*out),cpu,b->allocated))return scene_color_fault(d);
 *out=cpu;return 0;
}
int remesa_persistent_scene_color_unmap(struct remesa_device *device,
 struct remesa_scene_capture token)
{
 if(!device||device->ops!=&device_ops||!device->priv)return EINVAL;
 struct persistent *d=device->priv;
 if(d->failed)return EOWNERDEAD;
 if(token.reserved||token.owner!=d->cfg.context||!token.serial||token.slot>=REMESA_PROVIDER_MAX_JOBS)return EINVAL;
 struct scene_capture *c=&d->scenes[token.slot];
 if(!c->live||c->serial!=token.serial)return ESTALE;
 struct memory *b=c->read_color;if(!b)return ENOENT;
 if(!owns_memory(d,b)||!b->live||b->quarantined||!b->cpu_mapped||!b->scene_color_read||
    !b->refs||!b->scene_holds||scene_color_role(c,b))return scene_color_fault(d);
 int e=d->cfg.ops->cpu_unmap(d->cfg.priv,b->handle,false);
 if(e)return scene_color_fault(d);
 c->read_color=NULL;b->cpu_mapped=false;b->scene_color_read=false;drop_memory(d,b);return 0;
}
static int map_retired_color(struct remesa_device *device,void *object,void **out)
{
 if((uintptr_t)out%_Alignof(void *))return EINVAL;
 int e=remesa_persistent_shader_output_check(device,out,sizeof(*out));if(e)return e;
 if(*out)return EBUSY;
 struct persistent *d=device->priv;struct memory *b=object;
 if(d->failed)return EOWNERDEAD;
 if(!owns_memory(d,b)||!b->live)return EINVAL;
 if(b->quarantined)return EOWNERDEAD;
 struct scene_capture *selected=NULL;unsigned slot=0;bool held=false;
 for(unsigned i=0;i<REMESA_PROVIDER_MAX_JOBS;i++)if(d->scenes[i].live){
  struct scene_capture *c=&d->scenes[i];
  if(c->count>REMESA_PERSISTENT_BUFFERS)return EPROTO;
  for(unsigned k=0;k<c->count;k++)if(c->held[k]==b){
   held=true;e=scene_color_role(c,b);if(e)return e;
   if(c->published_operation){selected=c;slot=i;}
  }
 }
 if(!held)return ENODATA;
 if(!selected)return EAGAIN;
 const struct remesa_scene_capture token={d->cfg.context,selected->serial,slot,0};
 const void *cpu=NULL;e=remesa_persistent_scene_color_map(device,token,&cpu);if(e)return e;
 if(overlaps(out,sizeof(*out),cpu,b->allocated))return scene_color_fault(d);
 *out=(void *)cpu;return 0;
}
static int unmap_retired_color(struct remesa_device *device,void *object)
{
 if(!device||device->ops!=&device_ops||!device->priv)return EINVAL;
 struct persistent *d=device->priv;struct memory *b=object;
 if(d->failed)return EOWNERDEAD;
 if(!owns_memory(d,b)||!b->live)return EINVAL;
 struct scene_capture *selected=NULL;unsigned slot=0;
 for(unsigned i=0;i<REMESA_PROVIDER_MAX_JOBS;i++)if(d->scenes[i].live&&d->scenes[i].read_color==b){
  if(selected)return scene_color_fault(d);
  selected=&d->scenes[i];slot=i;
 }
 if(!selected)return ENOENT;
 const struct remesa_scene_capture token={d->cfg.context,selected->serial,slot,0};
 return remesa_persistent_scene_color_unmap(device,token);
}

static int linked_scene_prepare(struct remesa_device *device,
 const struct remesa_linked_scene_request *request,void *parameters,bool gears,
 bool homogeneous,bool preserve_depth,struct remesa_scene_capture *out)
{
 if(!request||!out||(uintptr_t)request%_Alignof(struct remesa_linked_scene_request)||
    (uintptr_t)out%_Alignof(struct remesa_scene_capture)||
    overlaps(request,sizeof(*request),out,sizeof(*out)))return EINVAL;
 int e=depth_output_check(device,out,sizeof(*out));if(e)return e;
 const struct remesa_linked_scene_request r=*request;
 if(!r.linkage||(uintptr_t)r.linkage%_Alignof(struct remesa_shader_linkage)||
    overlaps(r.linkage,sizeof(*r.linkage),out,sizeof(*out))||
    (r.geometry.vertices&&overlaps(r.geometry.vertices,r.geometry.vertex_bytes,out,sizeof(*out)))||
    (r.geometry.indices&&overlaps(r.geometry.indices,r.geometry.index_bytes,out,sizeof(*out))))return EINVAL;
 if(out->owner||out->serial||out->slot||out->reserved)return EBUSY;
 if(homogeneous&&(!gears||r.state.viewport.path!=REMESA_TA_VIEWPORT_MATRIX_WINDOW))return ENOTSUP;
 if(preserve_depth&&(!homogeneous||!gears||r.clear_depth!=1))return ENOTSUP;
 if(gears){
  struct remesa_persistent_vertex_parameters_backing pb;
  e=remesa_persistent_vertex_parameters_snapshot(device,parameters,&pb);if(e)return e;
  if(pb.coefficient_dwords!=36||pb.effective_source_dwords!=37||pb.transfer_dwords!=40)return ENOTSUP;
 }
 struct remesa_persistent_render_target_backing color;
 struct remesa_persistent_depth_backing depth;
 e=remesa_persistent_render_target_snapshot(device,r.color,r.width,r.height,r.color_pitch,&color);if(e)return e;
 e=remesa_persistent_depth_snapshot(device,r.depth,&depth);if(e)return e;
 if(r.color==r.depth||depth.width!=r.width||depth.height!=r.height||depth.cpp!=4)return EINVAL;
 if(r.state.viewport.framebuffer_width!=r.width||r.state.viewport.framebuffer_height!=r.height)return EINVAL;
 struct persistent *d=device->priv;void *objects[16]={0};struct remesa_scene_capture capture={0};
 const uint32_t banks[2]={r.vertex_bank,r.pixel_bank};
 /* All GPU bytes still come from their established typed owners. */
 e=gears?remesa_persistent_gears_attribute_program_prepare(device,r.linkage,banks,&objects[0]):
  remesa_persistent_linked_program_prepare(device,r.linkage,banks,&objects[0]);if(e)goto done;
 e=remesa_persistent_linked_geometry_prepare(device,&r.geometry,&objects[1]);if(e)goto done;
 e=gears?remesa_persistent_gears_attribute_pds_prepare(device,objects[0],objects[1],parameters,&objects[2]):
  remesa_persistent_linked_pds_prepare(device,objects[0],objects[1],&objects[2]);if(e)goto done;
 e=linked_state_prepare(device,objects[2],&r.state,r.state_bank,gears,homogeneous,&objects[3]);if(e)goto done;
 e=remesa_persistent_scene_close_prepare(device,r.width,r.height,r.state_bank,&objects[4]);if(e)goto done;
 const struct remesa_linked_body_draw draw={objects[3],0,r.geometry.indices?r.geometry.index_count:r.geometry.vertex_count};
 e=linked_body_prepare(device,&draw,1,objects[4],gears,homogeneous,&objects[5]);if(e)goto done;
 e=remesa_persistent_initialized_prefix_prepare(device,r.width,r.height,r.clear_depth,r.vertex_bank,r.pixel_bank,r.state_bank,&objects[6]);if(e)goto done;
 e=linked_ta_prepare(device,objects[6],objects[5],preserve_depth,&objects[7]);if(e)goto done;
 e=remesa_persistent_finalized_targets_prepare(device,objects[7],r.color,r.depth,r.color_pitch,&objects[8]);if(e)goto done;
 e=remesa_persistent_linked_background_prepare(device,objects[8],r.pixel_bank,&objects[9]);if(e)goto done;
 e=remesa_persistent_linked_event_auxiliary_prepare(device,objects[8],r.pixel_bank,&objects[10]);if(e)goto done;
 e=remesa_persistent_linked_region_allocate(device,objects[8],&objects[11]);if(e)goto done;
 e=remesa_persistent_allocate_placed(device,0x2000000,REMESA_VA_SURFACE,&objects[12]);if(e)goto done;
 e=remesa_persistent_allocate_placed(device,0x2000000,REMESA_VA_RASTER,&objects[13]);if(e)goto done;
 e=remesa_persistent_linked_dpm_prepare(device,objects[12],objects[13],&objects[14]);if(e)goto done;
 e=remesa_persistent_linked_event_profile_prepare(device,objects[10],&objects[15]);if(e)goto done;
 e=remesa_persistent_scene_capture(device,objects[9],objects[15],objects[11],objects[14],&capture);
 done:
 for(unsigned i=16;i>0;i--)if(objects[i-1])destroy_bo(d,objects[i-1]);
 if(d->failed)return EOWNERDEAD;
 for(unsigned i=0;i<REMESA_PERSISTENT_BUFFERS;i++)if(d->buffers[i].live&&d->buffers[i].quarantined)return EOWNERDEAD;
 if(e)return e;
 *out=capture;return 0;
}
int remesa_persistent_linked_scene_prepare(struct remesa_device *device,
 const struct remesa_linked_scene_request *request,struct remesa_scene_capture *out)
{return linked_scene_prepare(device,request,NULL,false,false,false,out);}
int remesa_persistent_gears_attribute_scene_prepare(struct remesa_device *device,
 const struct remesa_linked_scene_request *request,void *parameters,struct remesa_scene_capture *out)
{if(!parameters)return EINVAL;return linked_scene_prepare(device,request,parameters,true,false,false,out);}
int remesa_persistent_gears_homogeneous_scene_prepare(struct remesa_device *device,
 const struct remesa_linked_scene_request *request,void *parameters,struct remesa_scene_capture *out)
{if(!parameters)return EINVAL;return linked_scene_prepare(device,request,parameters,true,true,false,out);}

int remesa_persistent_gears_homogeneous_preserve_scene_prepare(struct remesa_device *device,
 const struct remesa_linked_scene_request *request,void *parameters,struct remesa_scene_capture *out)
{if(!parameters)return EINVAL;return linked_scene_prepare(device,request,parameters,true,true,true,out);}

int remesa_persistent_set_native_copy(struct remesa_device *device,remesa_gpu_copy_fn fn)
{
 if(!device||device->ops!=&device_ops||!device->priv||!fn)return EINVAL;
 struct persistent *d=device->priv;
 if(d->failed)return EOWNERDEAD;
 if(d->native_copy||d->allocated||d->shaders)return EBUSY;
 d->native_copy=fn;return 0;
}
static int native_copy_surface(struct persistent *d,const struct remesa_copy_surface *s,
 struct memory **out,bool source,bool public_owner)
{
 struct memory *b=s->bo;
 int e=linked_attachment_live(d,b,public_owner);if(e)return e;
 if(b->scene_holds||b->refs==UINT32_MAX)return EBUSY;
 if(b->linked_dpm||b->linked_background||b->linked_targets||b->linked_ta||b->zclear_prefix||
    b->linked_body||b->scene_hw||b->depth_backing||b->isp_update||b->raw_geometry||
    b->shader_program||b->shader_pds||b->scene_image||b->control_image)return ENOTSUP;
 if(source&&!b->readable)return EAGAIN;
 if(!s->width||s->width>4096||!s->height||s->height>4096||s->stride%4||
    s->stride<s->width*4u||s->stride>0x7fffu)return EINVAL;
 if((uint64_t)(s->height-1)*s->stride+(uint64_t)s->width*4u>b->bytes)return ENOSPC;
 *out=b;return 0;
}
int remesa_persistent_copy_region(void *opaque,const struct remesa_copy_region *r)
{
 struct persistent *d=opaque;struct memory *a,*b;int e;
 if(!d||!r)return EINVAL;
 if(d->failed)return EOWNERDEAD;
 if(!d->native_copy)return EOPNOTSUPP;
 e=native_copy_surface(d,&r->source,&a,true,true);if(e)return e;
 e=native_copy_surface(d,&r->destination,&b,false,true);if(e)return e;
 if(a==b)return EOPNOTSUPP; /* Overlap not admitted by this physical profile. */
 if(!r->width||!r->height||r->width>4095||r->height>4095||r->width==8||r->height==8)return EOPNOTSUPP;
 if(r->source_x>=r->source.width||r->source_y>=r->source.height||
    r->destination_x>=r->destination.width||r->destination_y>=r->destination.height||
    r->width>r->source.width-r->source_x||r->height>r->source.height-r->source_y||
    r->width>r->destination.width-r->destination_x||r->height>r->destination.height-r->destination_y)return EINVAL;
 uint64_t base=a->va&UINT64_C(0xf0000000),limit=base+UINT64_C(0x10000000);
 if(a->va>=UINT64_C(0x100000000)||a->allocated>limit-a->va||
    b->va<base||b->va>=limit||b->allocated>limit-b->va)return EOPNOTSUPP;
 struct remesa_gpu_copy copy={.source={a->handle,r->source.width,r->source.height,r->source.stride},
  .destination={b->handle,r->destination.width,r->destination.height,r->destination.stride},
  .source_x=r->source_x,.source_y=r->source_y,.destination_x=r->destination_x,
  .destination_y=r->destination_y,.width=r->width,.height=r->height};
 a->refs++;b->refs++;
 e=d->native_copy(d->cfg.priv,&copy);
 if(e==EOWNERDEAD||e<0){memory_error(d,a,EOWNERDEAD);memory_error(d,b,EOWNERDEAD);return EOWNERDEAD;}
 if(!e)b->readable=true;
 drop_memory(d,a);drop_memory(d,b);return e;
}

int remesa_persistent_set_native_fill(struct remesa_device *device,remesa_gpu_fill_fn fn)
{
 if(!device||device->ops!=&device_ops||!device->priv||!fn)return EINVAL;
 struct persistent *d=device->priv;
 if(d->failed)return EOWNERDEAD;
 if(d->native_fill||d->allocated||d->shaders)return EBUSY;
 d->native_fill=fn;return 0;
}
static int fill_region(struct persistent *d,const struct remesa_fill_region *r,bool public_owner)
{
 struct memory *b;int e;
 if(!d||!r)return EINVAL;
 if(d->failed)return EOWNERDEAD;
 if(!d->native_fill)return EOPNOTSUPP;
 e=native_copy_surface(d,&r->destination,&b,false,public_owner);if(e)return e;
 if(!r->width||!r->height||r->width>4095||r->height>4095||r->width==8||r->height==8)return EOPNOTSUPP;
 if(r->destination_x>=r->destination.width||r->destination_y>=r->destination.height||
    r->width>r->destination.width-r->destination_x||r->height>r->destination.height-r->destination_y)return EINVAL;
 uint64_t base=b->va&UINT64_C(0xf0000000),limit=base+UINT64_C(0x10000000);
 if(b->va>=UINT64_C(0x100000000)||b->allocated>limit-b->va)return EOPNOTSUPP;
 struct remesa_gpu_fill fill={.destination={b->handle,r->destination.width,r->destination.height,r->destination.stride},
  .destination_x=r->destination_x,.destination_y=r->destination_y,.width=r->width,.height=r->height,.color=r->color};
 b->refs++;e=d->native_fill(d->cfg.priv,&fill);
 if(e==EOWNERDEAD||e<0){memory_error(d,b,EOWNERDEAD);return EOWNERDEAD;}
 if(!e)b->readable=true;
 drop_memory(d,b);return e;
}

int remesa_persistent_fill_region(void *opaque,const struct remesa_fill_region *r)
{return fill_region(opaque,r,true);}
int remesa_persistent_linked_targets_color_initialize(struct remesa_device *device,
 void *targets_object,uint32_t packed_bgra)
{
 struct remesa_persistent_linked_targets_backing metadata;
 int e=depth_output_check(device,&metadata,sizeof(metadata));if(e)return e;
 struct persistent *d=device->priv;struct memory *targets=targets_object;
 e=background_targets_metadata(d,targets,true,&metadata);if(e)return e;
 if(targets->scene_holds)return EBUSY;
 /* Cover the complete committed plane, including pitch columns outside the
  * logical viewport. This is an explicit operation, never a cached per-frame
  * clear/preserve decision or inference from a previous partial writer. */
 const uint32_t pitch=metadata.color.state.stride_bytes/4;
 const struct remesa_fill_region request={
  .destination={targets->linked_targets->color,pitch,metadata.commands.height,metadata.color.state.stride_bytes},
  .width=pitch,.height=metadata.commands.height,.color=packed_bgra};
 return fill_region(d,&request,false);
}
int remesa_persistent_take_provider(struct remesa_device *device,int (*release)(void *),void *priv)
{
 if(!device||device->ops!=&device_ops||!device->priv||!release||!priv)return EINVAL;
 struct persistent *d=device->priv;
 if(d->failed)return EOWNERDEAD;
 if(d->release_provider||d->allocated||d->shaders)return EBUSY;
 d->release_provider=release;d->release_priv=priv;return 0;
}
