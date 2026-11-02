/* SPDX-License-Identifier: MIT */
#include "remesa_screen.h"
#include <errno.h>
#ifdef REMESA_WITH_MESA_HEADERS
#include "remesa_resource_ref.h"
#include "remesa_device.h"
#include "remesa_persistent.h"
#include "remesa_gears_runtime.h"
#include <stdio.h>
#include "remesa_resource_layout.h"
#include "remesa_draw.h"
#include "remesa_fixed_state.h"
#include "remesa_depth_cpu.h"
#include "remesa_render_pass.h"
#include "remesa_ta_scene.h"
#include "remesa_sampler_state.h"
#include "remesa_texture_binding.h"
#include "remesa_texture_descriptor.h"
#include "remesa_nir_uniform.h"
#ifdef REMESA_WITH_NIR
#include "remesa_nir.h"
#include "remesa_shader_ir.h"
#include "remesa_shader_program.h"
#include "nir.h"
#include "util/ralloc.h"
#include "gallium/auxiliary/util/u_upload_mgr.h"
#endif
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static void draw(struct pipe_context *,const struct pipe_draw_info *,unsigned,
 const struct pipe_draw_indirect_info *,const struct pipe_draw_start_count_bias *,unsigned);
/* Serialized API entry; children retain the backend until final teardown. */
struct screen {
 struct pipe_screen base; struct remesa_device device; unsigned refs; bool closing;
 int (*release_device)(struct remesa_device *);int release_error;bool release_pending;
 uint64_t resource_cookie;bool cpu_depth24_lab,gears_runtime,linked_attachment_storage;
 remesa_copy_region_fn native_copy;
 remesa_fill_region_fn native_fill;
};
static void screen_hold(struct screen *sc)
{ __atomic_add_fetch(&sc->refs,1,__ATOMIC_RELAXED); }
static void screen_drop(struct screen *sc)
{
 if(!__atomic_sub_fetch(&sc->refs,1,__ATOMIC_ACQ_REL)) {
  int error=sc->release_device ? sc->release_device(&sc->device) : 0;
  if(error) {
   /* Backend still owns pending/quarantined resources. Retain the owner. */
   __atomic_store_n(&sc->refs,1,__ATOMIC_RELEASE);
   sc->release_pending=true;sc->release_error=error;return;
  }
  free(sc);
 }
}
/* Screen-wide identities must remain unique across independent contexts.
 * Saturation refuses allocation rather than reusing an old pass identity. */
static bool screen_next_resource_cookie(struct screen *sc,uint64_t *cookie)
{
 uint64_t previous=__atomic_load_n(&sc->resource_cookie,__ATOMIC_RELAXED);
 do {
  if(previous==UINT64_MAX)return false;
 }while(!__atomic_compare_exchange_n(&sc->resource_cookie,&previous,previous+1,
                                   true,__ATOMIC_RELAXED,__ATOMIC_RELAXED));
 *cookie=previous+1;return true;
}
struct resource {
 struct pipe_resource base; void *bo; size_t bytes; unsigned maps;unsigned stride;
 struct remesa_texture_layout layout;
 struct pipe_fence_handle *writer; int write_error; bool has_contents;
 struct remesa_render_pass pass;struct remesa_pass_ticket ticket;
};
enum fixed_kind { FIXED_BLEND,FIXED_DSA,FIXED_RASTER,FIXED_COUNT };
struct fixed { struct fixed *next; enum fixed_kind kind; bool deleted,scissor,depth_clip,full_clip,recording_edge_proof;
 unsigned cull_face,depth_func;bool bottom_edge_rule,front_ccw,depth_clip_near,depth_clip_far,flat,flat_first,clamp_vertex_color,depth_enabled,depth_write; };
struct elements { struct elements *next; struct pipe_vertex_element position,normal,light; unsigned count; bool deleted; };
struct vertex { struct vertex *next; unsigned input_slot; bool deleted;
#ifdef REMESA_WITH_NIR
 struct remesa_shader_program program;
#endif
};
struct fragment { struct fragment *next; uint32_t argb; bool deleted,uses_uniform;
#ifdef REMESA_WITH_NIR
 struct remesa_shader_program program;
#endif
 struct remesa_uniform_color uniform; };
struct transfer;
struct context {
 struct pipe_context base; struct pipe_vertex_buffer vb,second_vb,third_vb; unsigned vb_count; struct pipe_vertex_element position;
 struct pipe_resource *target,*depth_target; bool position_bound,pipeline_bound,diagnostic_discard; uint32_t argb;
 int error; struct pipe_fence_handle *last; uint64_t cpu_clears,cpu_copies,native_copies,native_fills;
 bool failed_submission;
 bool gears_pending,gears_poisoned,gears_bottom_left,gears_origin_valid;unsigned gears_clear_mask;
 struct pipe_resource *gears_target,*gears_depth;
 struct remesa_draw_rejection_status draw_rejections;
 struct remesa_shader_compile_status shader_compile[2];
 struct fragment *fragments,*bound_fragment;
 bool invalid_viewport,invalid_scissor,invalid_constants,sample_enabled; uint32_t constant_rejections[2];
 struct vertex *vertices,*bound_vertex; bool vertex_required,viewport_set;
 struct elements *elements,*bound_elements;bool invalid_elements;
 struct pipe_viewport_state viewport;
 struct fixed *fixed_states,*bound_fixed[FIXED_COUNT];bool invalid_fixed[FIXED_COUNT],scissor_set;
 struct remesa_sampler_bank *samplers;
 struct remesa_texture_binding_bank *textures;
 struct pipe_resource *fragment_constants;unsigned constant_offset,constant_size;
 void *constant_snapshot;
 struct pipe_resource *vertex_constants;unsigned vertex_constant_offset,vertex_constant_size;
 void *vertex_constant_snapshot;
 struct transfer *transfers;
};
struct pipe_fence_handle { unsigned refs; struct screen *screen; void *backend; };
struct transfer {
 struct pipe_transfer base; struct transfer *next;
 unsigned char *backing,*staging; bool write,dirty;
};
static struct screen *s(struct pipe_screen *p) { return (struct screen *)p; }
static struct context *c(struct pipe_context *p) { return (struct context *)p; }
static struct resource *r(struct pipe_resource *p) { return (struct resource *)p; }
/* New host inspection/build APIs must not publish into their own owner or
 * immutable shader handles. Validate the whole output range before any write
 * (including the constructor's output-reference read). */
static bool context_output_valid(struct context *ctx,const void *out,size_t bytes)
{
 uintptr_t start=(uintptr_t)out;
 if(!out||bytes>UINTPTR_MAX-start)return false;
 uintptr_t end=start+bytes;
#define OUTPUT_OVERLAPS(object) (start<(uintptr_t)(object)+sizeof(*(object))&&end>(uintptr_t)(object))
 if(OUTPUT_OVERLAPS(ctx)||OUTPUT_OVERLAPS(s(ctx->base.screen)))return false;
 for(struct vertex *v=ctx->vertices;v;v=v->next)if(OUTPUT_OVERLAPS(v))return false;
 for(struct fragment *f=ctx->fragments;f;f=f->next)if(OUTPUT_OVERLAPS(f))return false;
 for(struct elements *e=ctx->elements;e;e=e->next)if(OUTPUT_OVERLAPS(e))return false;
 for(struct fixed *f=ctx->fixed_states;f;f=f->next)if(OUTPUT_OVERLAPS(f))return false;
#undef OUTPUT_OVERLAPS
 return true;
}
static bool context_state_output_valid(struct context *ctx,const void *out,size_t bytes)
{
 if(!context_output_valid(ctx,out,bytes))return false;
 struct pipe_resource *owners[]={ctx->target,ctx->depth_target,ctx->vb.buffer.resource,
  ctx->second_vb.buffer.resource,ctx->third_vb.buffer.resource,
  ctx->vertex_constants,ctx->fragment_constants};
 uintptr_t at=(uintptr_t)out;
 for(unsigned i=0;i<sizeof(owners)/sizeof(owners[0]);i++)if(owners[i]){
  uintptr_t base=(uintptr_t)owners[i];
  if(at<base+sizeof(struct resource)&&base<at+bytes)return false;
 }
 return true;
}
static void fence_ref(struct pipe_screen *,struct pipe_fence_handle **,struct pipe_fence_handle *);
static void set_capabilities(struct screen *sc)
{
 struct pipe_caps caps={0}; struct pipe_shader_caps shader={0};
 /* Narrow Gallium contract only; this does not advertise an OpenGL version. */
 caps.graphics=!sc->device.resource_only_backend;
 caps.accelerated=!sc->device.resource_only_backend&&!sc->device.retained_fragment_carrier&&sc->device.qualified_hardware_backend ? 1 : 0;
 caps.max_render_targets=1; caps.max_texture_2d_size=sc->cpu_depth24_lab?4096:32;
 /* Binding/storage limits; the legacy draw admission below remains one
  * element/one VB. The complete gears capture has separate validation. */
 caps.max_vertex_buffers=2; caps.max_vertex_attrib_stride=2048;
 /* Mesa copies this hint over its 512-KiB core default. Zero leaves the
  * immediate-mode VBO without storage before shader/draw validation. This
  * bounds CPU stream allocation; it adds no vertex instruction support. */
 caps.gl_begin_end_buffer_size=512*1024;
 caps.constant_buffer_offset_alignment=16;
 caps.max_viewports=1; caps.supported_prim_modes=1u<<MESA_PRIM_TRIANGLES;
 /* Whole-resource invalidation keeps backing stable and synchronizes the
  * authenticated writer. This is a storage contract, not GPU clear support. */
 caps.invalidate_buffer=true;
 /* clear() clips and validates a rectangular CPU write after synchronizing
  * the resource writer. Let Mesa use that callback instead of lowering a
  * scissored clear to an unsupported shader draw. This is not a GPU clear. */
 caps.clear_scissored=true;
 /* Position is a built-in VS output, not an interpolated user varying.
  * Fragment coordinates/interpolation are rejected by the NIR recognizer;
  * their convention flags must not advertise unqualified GPU semantics. */
 caps.max_varyings=0; caps.vendor_id=0x8086; caps.device_id=0x8108;
 caps.viewport_transform_lowered=true;
#ifdef REMESA_WITH_NIR
 if(!sc->device.resource_only_backend) {
  shader.max_instructions=1; shader.max_inputs=1; shader.max_outputs=1;
  shader.supported_irs=1u<<PIPE_SHADER_IR_NIR;
 }
#endif
 /* Gallium places tessellation and geometry stages between VS and FS.
  * Advertising those stages makes Mesa call unsupported lifecycle hooks. */
 memcpy((void *)&sc->base.shader_caps[PIPE_SHADER_VERTEX],&shader,sizeof(shader));
 shader.max_inputs=0;
 memcpy((void *)&sc->base.shader_caps[PIPE_SHADER_FRAGMENT],&shader,sizeof(shader));
 memcpy((void *)&sc->base.caps,&caps,sizeof(caps));
}
static const char *name(struct pipe_screen *p) { return s(p)->device.retained_fragment_carrier ? "reMESA original FS + retained VS/PDS (LAB)" : s(p)->device.resource_only_backend ? "reMESA GEM resources (DRAW DISABLED)" : s(p)->device.offline_test_backend ? "reMESA recording backend (NO SGX)" : "reMESA SGX535"; }
static const char *vendor(struct pipe_screen *p) { (void)p;return "reMESA"; }
#ifdef REMESA_WITH_NIR
static const struct nir_shader_compiler_options *compiler_options(struct pipe_screen *p,enum pipe_shader_type stage)
{
 /* No general opcode lowering or shader capability is claimed. Keep NIR in
  * its original form for the strict semantic recognizers; unsupported IR is
  * rejected by create_vs/fs_state, not silently rewritten into a profile. */
 static const nir_shader_compiler_options options={0};
 if(s(p)->closing||(stage!=PIPE_SHADER_VERTEX&&stage!=PIPE_SHADER_FRAGMENT))return NULL;
 return &options;
}
#endif

/* Renderbuffers use PIPE_TEXTURE_RECT on Mesa screens without NPOT
 * sampling. Its single-level pixel storage and attachment coordinates are
 * identical to 2D here; this helper does not admit rectangle samplers. */
static bool color_storage_target(enum pipe_texture_target target)
{ return target==PIPE_TEXTURE_2D||target==PIPE_TEXTURE_RECT; }
static bool format(struct pipe_screen *p,enum pipe_format f,enum pipe_texture_target t,unsigned samples,unsigned storage,unsigned bind)
{
 if(samples>1 || storage>1) return false;
 if((s(p)->cpu_depth24_lab||s(p)->linked_attachment_storage)&&color_storage_target(t)&&f==PIPE_FORMAT_Z24X8_UNORM)
  return bind==PIPE_BIND_DEPTH_STENCIL;
 /* Mesa's display-list VBO allocator adds VERTEX_STATE to index storage
  * even when HasDrawVertexState is false and playback uses ordinary VAOs.
  * It is a storage hint here, not permission to execute a new draw class.
  * Require an actual vertex/index role and byte storage for this hint. */
 if(t==PIPE_BUFFER && (bind&PIPE_BIND_VERTEX_STATE) &&
    (!(bind&(PIPE_BIND_VERTEX_BUFFER|PIPE_BIND_INDEX_BUFFER)) ||
     f!=PIPE_FORMAT_R8_UNORM))return false;
 if(t==PIPE_BUFFER) return !(bind & ~(PIPE_BIND_VERTEX_BUFFER|PIPE_BIND_INDEX_BUFFER|PIPE_BIND_CONSTANT_BUFFER|PIPE_BIND_VERTEX_STATE)) &&
  (f==PIPE_FORMAT_R8_UNORM || (!(bind&PIPE_BIND_CONSTANT_BUFFER)&&remesa_position_format_bytes(f)));
 if(!color_storage_target(t))return false;
 if(t==PIPE_TEXTURE_RECT)return f==PIPE_FORMAT_B8G8R8A8_UNORM&&bind==PIPE_BIND_RENDER_TARGET;
 /* Transfer-only storage is not sampler or render capability. Real SGX render
  * attachments remain the exact BGRA32 profile; sampler binds are unsupported. */
 if(!bind)return f==PIPE_FORMAT_B8G8R8A8_UNORM||f==PIPE_FORMAT_R8G8B8A8_UNORM;
 return f==PIPE_FORMAT_B8G8R8A8_UNORM && bind==PIPE_BIND_RENDER_TARGET;
}
static struct pipe_resource *create_resource(struct pipe_screen *p,const struct pipe_resource *t)
{
 struct resource *b;size_t bytes;uint64_t cookie;unsigned stride=0;struct remesa_texture_layout layout={0};
 if(s(p)->closing || !t || !format(p,t->format,t->target,t->nr_samples,t->nr_storage_samples,t->bind) ||
    t->array_size!=1 || t->depth0!=1 ||
    (t->flags&~PIPE_RESOURCE_FLAG_SINGLE_THREAD_USE) ||
    (t->flags&&t->target!=PIPE_BUFFER)) return NULL;
 if(t->target==PIPE_BUFFER) {
  uint64_t budget=s(p)->device.resource_limits.max_resource_bytes;
  if(!budget)budget=UINT64_C(64)*1024*1024;
  if(!t->width0 || t->height0!=1 || t->last_level || t->width0>budget) return NULL;
  bytes=t->width0;
  layout.count=1;layout.bytes=bytes;
  layout.levels[0]=(struct remesa_texture_level){t->width0,1,0,0,bytes};
 }
 else {
  if(t->bind) { if((!s(p)->cpu_depth24_lab&&!s(p)->linked_attachment_storage&&!(s(p)->native_fill&&s(p)->device.resource_only_backend&&t->bind==PIPE_BIND_RENDER_TARGET&&t->format==PIPE_FORMAT_B8G8R8A8_UNORM)&&(t->width0!=32||t->height0!=32))||t->last_level)return NULL; }
  /* The exact physical profile requires tight rows; transfer-only storage
   * follows explicit backend alignment and independently bounded capacity. */
  struct remesa_resource_limits limits=s(p)->device.resource_limits;
  if(t->bind&&!s(p)->linked_attachment_storage&&limits.row_alignment>1) {
   if((limits.row_alignment&(limits.row_alignment-1))||
      (((uint64_t)t->width0*4)&(limits.row_alignment-1)))return NULL;
  }
  if(remesa_texture_layout(&limits,t->width0,t->height0,4,t->last_level,&layout))return NULL;
  bytes=layout.bytes;stride=layout.levels[0].stride;
 }
 b=calloc(1,sizeof(*b));if(!b) return NULL;b->base=*t;b->base.screen=p;
 if(!screen_next_resource_cookie(s(p),&cookie)){free(b);return NULL;}
 remesa_render_pass_init(&b->pass,cookie);
 b->base.reference.count=1;b->bytes=bytes;b->stride=stride;b->layout=layout;
 int allocation_error;
 if(s(p)->linked_attachment_storage&&t->bind&&color_storage_target(t->target)){
  if(t->format==PIPE_FORMAT_Z24X8_UNORM)
   allocation_error=s(p)->device.ops->allocate_depth_attachment(&s(p)->device,t->width0,t->height0,4,&b->bo);
  else allocation_error=s(p)->device.ops->allocate_color_attachment(&s(p)->device,t->width0,t->height0,&b->bo);
 }else allocation_error=s(p)->device.ops->allocate(s(p)->device.priv,bytes,&b->bo);
 if(allocation_error || !b->bo) {
  if(b->bo) s(p)->device.ops->destroy_bo(s(p)->device.priv,b->bo);
  free(b);return NULL;
 }
 screen_hold(s(p));return &b->base;
}
static void destroy_resource(struct pipe_screen *p,struct pipe_resource *b)
{
 fence_ref(p,&r(b)->writer,NULL);
 s(p)->device.ops->destroy_bo(s(p)->device.priv,r(b)->bo);free(b);screen_drop(s(p));
}
static void fence_ref(struct pipe_screen *p,struct pipe_fence_handle **dst,struct pipe_fence_handle *src)
{
 struct pipe_fence_handle *old=*dst;(void)p;
 if(src) __atomic_add_fetch(&src->refs,1,__ATOMIC_RELAXED);
 *dst=src;
 if(old && !__atomic_sub_fetch(&old->refs,1,__ATOMIC_ACQ_REL)) {
  struct screen *owner=old->screen;
  owner->device.ops->destroy_fence(owner->device.priv,old->backend);free(old);screen_drop(owner);
 }
}
static bool finish(struct pipe_screen *p,struct pipe_context *ctx,struct pipe_fence_handle *f,uint64_t ns)
{ (void)ctx;return f && f->screen==s(p) && !s(p)->device.ops->wait(s(p)->device.priv,f->backend,ns,true); }
/* Contexts sharing a screen/resource must be externally serialized. A pending
 * writer belongs to the resource, not whichever context currently binds it.
 * Conservatively require CPU publication for both host access and reuse; no
 * inferred GPU-only visibility or implicit failure recovery is permitted. */
static int resource_sync(struct pipe_screen *p,struct resource *b,uint64_t timeout)
{
 int error;
 if(b->write_error) return b->write_error;
 if(!b->writer) return 0;
 error=s(p)->device.ops->wait(s(p)->device.priv,b->writer->backend,timeout,true);
 if(!error) {
  /* wait(cpu_visible=true) must authenticate this backend fence and attest
   * retirement/publication. These are not independently synthesized events. */
  const struct remesa_pass_completion proof={true,true,true};
  error=remesa_render_pass_finish(&b->pass,&b->ticket,&proof);
  if(!error)fence_ref(p,&b->writer,NULL);
 }
 if(error && error!=EAGAIN && error!=ETIMEDOUT && error!=EBUSY&&error!=EINTR) {
  b->write_error=error;remesa_render_pass_fail(&b->pass,&b->ticket,error);
 }
 return error;
}
static void invalidate_resource(struct pipe_context *p,struct pipe_resource *res)
{
 struct context *ctx=c(p);
 if(s(p->screen)->closing){ctx->error=ESHUTDOWN;return;}
 if(!res||res->screen!=p->screen){ctx->error=EINVAL;return;}
 struct resource *b=r(res);
 /* Invalidation is not cancellation. Never drop an in-flight writer, change
  * its backing, or turn a failed/uncertain submission into a fresh target. */
 if(b->maps){ctx->error=EBUSY;return;}
 ctx->error=resource_sync(p->screen,b,UINT64_MAX);if(ctx->error)return;
 ctx->error=remesa_render_pass_invalidate(&b->pass);if(ctx->error)return;
 b->has_contents=false;
}
static void *map(struct pipe_context *p,struct pipe_resource *b,unsigned level,unsigned usage,const struct pipe_box *box,struct pipe_transfer **out)
{
 struct transfer *t;void *ptr=NULL;size_t offset,row,span;struct screen *sc=s(p->screen);int error=EINVAL;
 const unsigned write_hints=PIPE_MAP_DISCARD_RANGE|PIPE_MAP_DISCARD_WHOLE_RESOURCE|PIPE_MAP_FLUSH_EXPLICIT|PIPE_MAP_UNSYNCHRONIZED;
 if(out) *out=NULL;
 if(sc->closing){error=ESHUTDOWN;goto fail;}
 if(!b || b->screen!=p->screen || !box || !out || level>=r(b)->layout.count || !(usage & (PIPE_MAP_READ|PIPE_MAP_WRITE)) ||
    usage & ~(PIPE_MAP_READ|PIPE_MAP_WRITE|PIPE_MAP_DONTBLOCK|PIPE_MAP_DIRECTLY|write_hints) ||
    ((usage & write_hints) && (!(usage & PIPE_MAP_WRITE)||(usage & PIPE_MAP_READ))) ||
    ((usage & PIPE_MAP_FLUSH_EXPLICIT)&&(usage & PIPE_MAP_DIRECTLY)) ||
    box->x<0 || box->width<=0) goto fail;
 if(r(b)->maps){error=EBUSY;goto fail;}
 const struct remesa_texture_level *mip=&r(b)->layout.levels[level];
 if(b->target==PIPE_BUFFER) {
  if(box->y || box->z || box->height!=1 || box->depth!=1 ||
     (uint64_t)box->x+box->width>mip->width) goto fail;
  offset=box->x;row=box->width;
 } else {
  if(box->y<0 || box->z || box->height<=0 || box->depth!=1 ||
     (uint64_t)box->x+box->width>mip->width || (uint64_t)box->y+box->height>mip->height) goto fail;
  offset=(size_t)box->y*mip->stride+(size_t)box->x*4;row=(size_t)box->width*4;
 }
 if(box->height>1 && mip->stride>(SIZE_MAX-row)/(box->height-1))goto fail;
 span=(size_t)(box->height-1)*mip->stride+row;
 if(offset>mip->bytes||span>mip->bytes-offset)goto fail;
 offset+=mip->offset; /* Checked immutable layout keeps this inside the BO. */
 /* Discard is permission to lose old contents, not permission to skip an
  * outstanding writer. Keep backing stable and synchronize even for discard. */
 /* Mesa's upload manager maps fresh/disjoint stream storage unsynchronized.
  * Never wait under that flag and never use it to ignore a retained writer:
  * reject instead, preserving publication, quarantine and backing ownership. */
 if(usage&PIPE_MAP_UNSYNCHRONIZED) {
  if(r(b)->writer||r(b)->write_error){error=r(b)->write_error?r(b)->write_error:EBUSY;goto fail;}
 } else {
  error=resource_sync(p->screen,r(b),(usage & PIPE_MAP_DONTBLOCK) ? 0 : UINT64_MAX);
  if(error)goto fail;
 }
 if(usage & PIPE_MAP_WRITE){error=remesa_render_pass_admit_cpu_write(&r(b)->pass);if(error)goto fail;}
 t=calloc(1,sizeof(*t));if(!t){error=ENOMEM;goto fail;}
 if(usage & PIPE_MAP_FLUSH_EXPLICIT) {
  t->staging=calloc(1,span);if(!t->staging){free(t);error=ENOMEM;goto fail;}
 }
 int mapped=sc->device.ops->map(sc->device.priv,r(b)->bo,(usage & PIPE_MAP_WRITE)!=0,&ptr);
 if(mapped || !ptr) {
  if(!mapped||ptr)sc->device.ops->unmap(sc->device.priv,r(b)->bo,(usage & PIPE_MAP_WRITE)!=0);
  free(t->staging);free(t);error=mapped?mapped:EPROTO;goto fail;
 }
 remesa_resource_reference(&t->base.resource,b);t->base.level=level;t->base.usage=usage;t->base.box=*box;
 t->base.stride=mip->stride;t->base.layer_stride=t->staging?span:mip->bytes;
 t->backing=(unsigned char *)ptr+offset;t->write=(usage & PIPE_MAP_WRITE)!=0;
 t->dirty=t->write&&!t->staging;t->next=c(p)->transfers;c(p)->transfers=t;
 ++r(b)->maps;*out=&t->base;c(p)->error=0;return t->staging?t->staging:t->backing;
fail:
 c(p)->error=error;return NULL;
}
/* Transfers are context-private (Mesa 25.2 p_context.h). Membership is checked
 * before dereferencing a supplied handle, including foreign/unmapped handles. */
static struct transfer **transfer_link(struct context *ctx,struct pipe_transfer *pt)
{
 struct transfer **link=&ctx->transfers;
 while(*link && &(*link)->base!=pt)link=&(*link)->next;
 return link;
}
static void flush_region(struct pipe_context *p,struct pipe_transfer *pt,const struct pipe_box *box)
{
 struct context *ctx=c(p);struct transfer *t=*transfer_link(ctx,pt);
 if(!t||!box||!t->write||!t->staging||box->x<0||box->y<0||box->z||
    box->width<0||box->height<0||box->depth!=1||
    (uint64_t)box->x+box->width>(unsigned)t->base.box.width||
    (uint64_t)box->y+box->height>(unsigned)t->base.box.height||
    (t->base.resource->target==PIPE_BUFFER&&(box->y||box->height!=1))) {
  ctx->error=EINVAL;return;
 }
 /* Mesa docs/gallium/context.rst: the flush box is relative to the mapped
  * range, not the resource origin. Unflushed staging bytes never reach BO. */
 if(box->width&&box->height) {
  size_t bpp=t->base.resource->target==PIPE_BUFFER?1:4;
  size_t offset=(size_t)box->y*t->base.stride+(size_t)box->x*bpp;
  size_t row=(size_t)box->width*bpp;
  for(unsigned y=0;y<(unsigned)box->height;y++)
   memcpy(t->backing+offset+(size_t)y*t->base.stride,
          t->staging+offset+(size_t)y*t->base.stride,row);
  t->dirty=true;
 }
 ctx->error=0;
}
static void unmap(struct pipe_context *p,struct pipe_transfer *pt)
{
 struct transfer **link=transfer_link(c(p),pt),*t=*link;struct screen *sc=s(p->screen);
 if(!t){c(p)->error=EINVAL;return;}
 *link=t->next;
 sc->device.ops->unmap(sc->device.priv,r(pt->resource)->bo,t->dirty);
 if(t->dirty){
  int error=remesa_render_pass_cpu_write(&r(pt->resource)->pass);
  if(error)r(pt->resource)->write_error=error;
  else r(pt->resource)->has_contents=true;
 }
 --r(pt->resource)->maps;
 remesa_resource_reference(&pt->resource,NULL);free(t->staging);free(t);
}
/* CPU upload into owned backing, not GPU texture sampling or a clear. These
 * entry points use exactly the same writer/lease gate as normal transfers. */
static void buffer_subdata(struct pipe_context *p,struct pipe_resource *b,unsigned usage,
                          unsigned offset,unsigned size,const void *data)
{
 struct context *ctx=c(p);struct pipe_transfer *t=NULL;void *dst;
 if(s(p->screen)->closing){ctx->error=ESHUTDOWN;return;}
 if(!b||b->screen!=p->screen||b->target!=PIPE_BUFFER||offset>b->width0||size>b->width0-offset||
    offset>INT32_MAX||size>INT32_MAX||(size&&!data)||
    usage&~(PIPE_MAP_WRITE|PIPE_MAP_DONTBLOCK|PIPE_MAP_DIRECTLY|PIPE_MAP_DISCARD_RANGE|PIPE_MAP_DISCARD_WHOLE_RESOURCE)) {ctx->error=EINVAL;return;}
 if(!size){ctx->error=0;return;}
 ctx->error=resource_sync(p->screen,r(b),usage&PIPE_MAP_DONTBLOCK?0:UINT64_MAX);if(ctx->error)return;
 struct pipe_box box={.x=offset,.width=size,.height=1,.depth=1};
 dst=map(p,b,0,usage|PIPE_MAP_WRITE,&box,&t);if(!dst)return;
 memmove(dst,data,size);unmap(p,t);ctx->error=0;
}
static void texture_subdata(struct pipe_context *p,struct pipe_resource *b,unsigned level,unsigned usage,
                           const struct pipe_box *box,const void *data,unsigned stride,uintptr_t layer_stride)
{
 struct context *ctx=c(p);struct pipe_transfer *t=NULL;void *dst;size_t row,span;
 if(s(p->screen)->closing){ctx->error=ESHUTDOWN;return;}
 if(!b||b->screen!=p->screen||!color_storage_target(b->target)||level>=r(b)->layout.count||!box||box->x<0||box->y<0||box->z||
    box->width<0||box->height<0||box->depth!=1||
    (uint64_t)box->x+box->width>r(b)->layout.levels[level].width||
    (uint64_t)box->y+box->height>r(b)->layout.levels[level].height||
    usage&~(PIPE_MAP_WRITE|PIPE_MAP_DONTBLOCK|PIPE_MAP_DIRECTLY|PIPE_MAP_DISCARD_RANGE|PIPE_MAP_DISCARD_WHOLE_RESOURCE)) {ctx->error=EINVAL;return;}
 if(!box->width||!box->height){ctx->error=0;return;}
 row=(size_t)box->width*4;
 if(!data||stride<row||(box->height>1&&stride>(SIZE_MAX-row)/(box->height-1))){ctx->error=EINVAL;return;}
 span=(size_t)(box->height-1)*stride+row;
 if(layer_stride&&layer_stride<span){ctx->error=EINVAL;return;}
 ctx->error=resource_sync(p->screen,r(b),usage&PIPE_MAP_DONTBLOCK?0:UINT64_MAX);if(ctx->error)return;
 dst=map(p,b,level,usage|PIPE_MAP_WRITE,box,&t);if(!dst)return;
 for(unsigned y=0;y<(unsigned)box->height;y++)memcpy((unsigned char *)dst+y*t->stride,(const unsigned char *)data+y*stride,row);
 unmap(p,t);ctx->error=0;
}
/* Opt-in native copies authenticate synchronous GPU retirement through the
 * owner/transport callback. The default CPU transfer implementation snapshots
 * first for deterministic overlap. Neither path performs format conversion. */
static void copy_region(struct pipe_context *p,struct pipe_resource *dst,unsigned dst_level,
 unsigned dstx,unsigned dsty,unsigned dstz,struct pipe_resource *src,unsigned src_level,
 const struct pipe_box *box)
{
 struct context *ctx=c(p);struct pipe_transfer *transfer=NULL;
 struct pipe_box dest;void *ptr;unsigned char *snapshot=NULL;size_t row,bytes;
 int error=EINVAL;
 if(s(p->screen)->closing){ctx->error=ESHUTDOWN;return;}
 if(!dst||!src||dst->screen!=p->screen||src->screen!=p->screen||
    dst_level>=r(dst)->layout.count||src_level>=r(src)->layout.count||
    dst->format!=src->format||dst->target!=src->target||
    dst->nr_samples>1||src->nr_samples>1||!box||box->x<0||box->y<0||box->z||
    box->width<0||box->height<0||box->depth!=1||dstz||
    dstx>INT32_MAX||dsty>INT32_MAX)goto done;
 if(src->target==PIPE_BUFFER){
  if(box->y||box->height!=1||dsty)goto done;
  row=(size_t)box->width;
 }else if(color_storage_target(src->target)){
  if(src->format!=PIPE_FORMAT_B8G8R8A8_UNORM&&src->format!=PIPE_FORMAT_R8G8B8A8_UNORM)goto done;
  if((size_t)box->width>SIZE_MAX/4)goto done;
  row=(size_t)box->width*4;
 }else goto done;
 if((uint64_t)box->x+box->width>r(src)->layout.levels[src_level].width||
    (uint64_t)box->y+box->height>r(src)->layout.levels[src_level].height||
    (uint64_t)dstx+box->width>r(dst)->layout.levels[dst_level].width||
    (uint64_t)dsty+box->height>r(dst)->layout.levels[dst_level].height)goto done;
 if(!box->width||!box->height){error=0;goto done;}
 if((size_t)box->height>SIZE_MAX/row){error=EOVERFLOW;goto done;}
 bytes=row*(size_t)box->height;
 if(r(src)->maps||r(dst)->maps){error=EBUSY;goto done;}
 error=resource_sync(p->screen,r(src),UINT64_MAX);if(error)goto done;
 error=resource_sync(p->screen,r(dst),UINT64_MAX);if(error)goto done;
 if(s(p->screen)->native_copy){
  if(src->target!=PIPE_TEXTURE_2D||src_level||dst_level||
     r(src)->layout.levels[0].offset||r(dst)->layout.levels[0].offset){error=ENOTSUP;goto done;}
  struct remesa_copy_region request={
   .source={r(src)->bo,r(src)->layout.levels[0].width,r(src)->layout.levels[0].height,r(src)->layout.levels[0].stride},
   .destination={r(dst)->bo,r(dst)->layout.levels[0].width,r(dst)->layout.levels[0].height,r(dst)->layout.levels[0].stride},
   .source_x=box->x,.source_y=box->y,.destination_x=dstx,.destination_y=dsty,.width=box->width,.height=box->height};
  struct remesa_pass_ticket ticket;
  error=remesa_render_pass_begin(&r(dst)->pass,REMESA_PASS_PRESERVE,REMESA_PASS_STORE,true,&ticket);if(error)goto done;
  r(dst)->ticket=ticket;
  error=s(p->screen)->native_copy(s(p->screen)->device.priv,&request);
  if(error&&error!=EOWNERDEAD&&error>0){int cancel=remesa_render_pass_cancel(&r(dst)->pass,&ticket);if(cancel)error=EOWNERDEAD;else goto done;}
  if(error<0)error=EOWNERDEAD;
  if(!error){
   const struct remesa_pass_completion proof={true,true,true};
   error=remesa_render_pass_submitted(&r(dst)->pass,&ticket);
   if(!error)error=remesa_render_pass_finish(&r(dst)->pass,&ticket,&proof);
   if(!error){r(dst)->has_contents=true;ctx->native_copies++;goto done;}
   error=EOWNERDEAD;
  }
  if(error==EOWNERDEAD){
   (void)remesa_render_pass_submitted(&r(dst)->pass,&ticket);
   (void)remesa_render_pass_fail(&r(dst)->pass,&ticket,error);
   if(src!=dst){struct remesa_pass_ticket read; if(!remesa_render_pass_import_writer(&r(src)->pass,&read))(void)remesa_render_pass_fail(&r(src)->pass,&read,error);}
   r(src)->write_error=r(dst)->write_error=error;ctx->failed_submission=true;
  }
  goto done;
 }
 snapshot=malloc(bytes);if(!snapshot){error=ENOMEM;goto done;}
 ptr=map(p,src,src_level,PIPE_MAP_READ,box,&transfer);if(!ptr){error=ctx->error;goto done;}
 for(unsigned y=0;y<(unsigned)box->height;y++)
  memcpy(snapshot+(size_t)y*row,(unsigned char *)ptr+(size_t)y*transfer->stride,row);
 unmap(p,transfer);transfer=NULL;
 dest=*box;dest.x=dstx;dest.y=dsty;
 ptr=map(p,dst,dst_level,PIPE_MAP_WRITE,&dest,&transfer);if(!ptr){error=ctx->error;goto done;}
 for(unsigned y=0;y<(unsigned)box->height;y++)
  memcpy((unsigned char *)ptr+(size_t)y*transfer->stride,snapshot+(size_t)y*row,row);
 unmap(p,transfer);transfer=NULL;++ctx->cpu_copies;error=0;
 done:
 free(snapshot);ctx->error=error;
}
/* The native callback may publish only after synchronous authenticated
 * completion/retirement. All clocks/pixel checks stay in external tools. */
static int native_fill(struct pipe_context *p,struct pipe_resource *res,unsigned level,
 unsigned x,unsigned y,unsigned width,unsigned height,const unsigned char pixel[4])
{
 struct context *ctx=c(p);struct resource *b=r(res);int error;
 if(res->target!=PIPE_TEXTURE_2D||level||b->layout.levels[0].offset)return ENOTSUP;
 if(b->maps)return EBUSY;
 error=resource_sync(p->screen,b,UINT64_MAX);if(error)return error;
 struct remesa_fill_region request={.destination={b->bo,b->layout.levels[0].width,b->layout.levels[0].height,b->layout.levels[0].stride},
  .destination_x=x,.destination_y=y,.width=width,.height=height,
  .color=(uint32_t)pixel[0]|(uint32_t)pixel[1]<<8|(uint32_t)pixel[2]<<16|(uint32_t)pixel[3]<<24};
 struct remesa_pass_ticket ticket;
 error=remesa_render_pass_begin(&b->pass,REMESA_PASS_PRESERVE,REMESA_PASS_STORE,true,&ticket);if(error)return error;
 b->ticket=ticket;error=s(p->screen)->native_fill(s(p->screen)->device.priv,&request);
 if(error&&error!=EOWNERDEAD&&error>0){
  if(!remesa_render_pass_cancel(&b->pass,&ticket))return error;
  error=EOWNERDEAD;
 }
 if(error<0)error=EOWNERDEAD;
 if(!error){
  const struct remesa_pass_completion proof={true,true,true};
  error=remesa_render_pass_submitted(&b->pass,&ticket);
  if(!error)error=remesa_render_pass_finish(&b->pass,&ticket,&proof);
  if(!error){b->has_contents=true;ctx->native_fills++;return 0;}
  error=EOWNERDEAD;
 }
 (void)remesa_render_pass_fail(&b->pass,&ticket,EOWNERDEAD);
 b->write_error=EOWNERDEAD;ctx->failed_submission=true;return EOWNERDEAD;
}
/* Raw texel clear: explicit native opt-in or the default CPU transfer. */
static void clear_texture(struct pipe_context *p,struct pipe_resource *res,unsigned level,
 const struct pipe_box *box,const void *data)
{
 struct context *ctx=c(p);struct pipe_transfer *transfer=NULL;void *ptr;
 unsigned char pixel[4];int error=EINVAL;
 if(s(p->screen)->closing){ctx->error=ESHUTDOWN;return;}
 if(!res||res->screen!=p->screen||!color_storage_target(res->target)||level>=r(res)->layout.count||!box||!data||
    (res->format!=PIPE_FORMAT_B8G8R8A8_UNORM&&res->format!=PIPE_FORMAT_R8G8B8A8_UNORM)||
    box->x<0||box->y<0||box->z||box->width<0||box->height<0||box->depth!=1||
    (uint64_t)box->x+box->width>r(res)->layout.levels[level].width||
    (uint64_t)box->y+box->height>r(res)->layout.levels[level].height)goto done;
 if(!box->width||!box->height){error=0;goto done;}
 if(r(res)->maps){error=EBUSY;goto done;}
 error=resource_sync(p->screen,r(res),UINT64_MAX);if(error)goto done;
 memcpy(pixel,data,sizeof(pixel));
 if(s(p->screen)->native_fill){error=native_fill(p,res,level,box->x,box->y,box->width,box->height,pixel);goto done;}
 ptr=map(p,res,level,PIPE_MAP_WRITE,box,&transfer);if(!ptr){error=ctx->error;goto done;}
 for(unsigned y=0;y<(unsigned)box->height;y++)
  for(unsigned x=0;x<(unsigned)box->width;x++)
   memcpy((unsigned char *)ptr+(size_t)y*transfer->stride+(size_t)x*4,pixel,4);
 unmap(p,transfer);++ctx->cpu_clears;error=0;
 done:ctx->error=error;
}
static void set_vb(struct pipe_context *p,unsigned count,const struct pipe_vertex_buffer *v)
{
 struct context *ctx=c(p);remesa_resource_reference(&ctx->vb.buffer.resource,NULL);
 remesa_resource_reference(&ctx->second_vb.buffer.resource,NULL);
 remesa_resource_reference(&ctx->third_vb.buffer.resource,NULL);
 memset(&ctx->vb,0,sizeof(ctx->vb));memset(&ctx->second_vb,0,sizeof(ctx->second_vb));
 memset(&ctx->third_vb,0,sizeof(ctx->third_vb));ctx->vb_count=0;
 if(!count) return;
 /* Mesa 25.2 transfers these resource references to the callback. */
 int error=count<=3&&v?0:ENOTSUP;
 if(!error)for(unsigned i=0;i<count;i++) {
  struct pipe_resource *res=v[i].buffer.resource;
  if(v[i].is_user_buffer){error=ENOTSUP;break;}
  /* Pinned Mesa display lists allocate current_bo as ELEMENT_ARRAY_BUFFER
   * + VERTEX_STATE, then use it as the VAO's vertex source. Binding flags
   * describe storage usage, not a distinct allocation format. Accept that
   * raw storage here; legacy GPU admission still requires VERTEX_BUFFER. */
  bool vertex_storage=res&&((res->bind&PIPE_BIND_VERTEX_BUFFER)||
   (res->bind&(PIPE_BIND_INDEX_BUFFER|PIPE_BIND_VERTEX_STATE))==
              (PIPE_BIND_INDEX_BUFFER|PIPE_BIND_VERTEX_STATE));
  if(res&&(res->screen!=p->screen||res->target!=PIPE_BUFFER||!vertex_storage)){
   error=EINVAL;break;
  }
 }
 if(error) {
  ctx->error=error;if(v) for(unsigned i=0;i<count;i++) if(!v[i].is_user_buffer) {
   struct pipe_resource *owned=v[i].buffer.resource;remesa_resource_reference(&owned,NULL);
  }
  return;
 }
 ctx->vb=v[0];if(count>=2)ctx->second_vb=v[1];if(count==3)ctx->third_vb=v[2];
 ctx->vb_count=count;ctx->error=0;
}
static void *create_elements(struct pipe_context *p,unsigned count,const struct pipe_vertex_element *e)
{
 struct context *ctx=c(p);struct elements *out;
 bool valid=(count>=1&&count<=3)&&e;
 if(valid)for(unsigned i=0;i<count;i++)if(!remesa_position_format_bytes(e[i].src_format)||
    (i>=1&&e[i].src_format==PIPE_FORMAT_R32G32_FLOAT)||e[i].vertex_buffer_index>=3||
    e[i].instance_divisor||e[i].dual_slot)valid=false;
 if(!valid) {
  ctx->invalid_elements=true;ctx->error=ENOTSUP;return NULL;
 }
 out=calloc(1,sizeof(*out));if(!out){ctx->invalid_elements=true;ctx->error=ENOMEM;return NULL;}
 out->position=*e;if(count>=2)out->normal=e[1];if(count==3)out->light=e[2];out->count=count;
 out->next=ctx->elements;ctx->elements=out;ctx->error=0;return out;
}
static struct elements *find_elements(struct context *ctx,void *handle)
{
 for(struct elements *e=ctx->elements;e;e=e->next)if(e==handle&&!e->deleted)return e;
 return NULL;
}
static void bind_elements(struct pipe_context *p,void *handle)
{
 struct context *ctx=c(p);struct elements *e=find_elements(ctx,handle);
 ctx->bound_elements=e;ctx->position_bound=e!=NULL;ctx->invalid_elements=handle&&!e;
 if(e)ctx->position=e->position;
 ctx->error=ctx->invalid_elements?EINVAL:0;
}
static void delete_elements(struct pipe_context *p,void *handle)
{
 struct context *ctx=c(p);struct elements *e=find_elements(ctx,handle);
 if(!e){ctx->error=EINVAL;return;}
 if(ctx->bound_elements==e){ctx->bound_elements=NULL;ctx->position_bound=false;}
 e->deleted=true;
}
static void framebuffer(struct pipe_context *p,const struct pipe_framebuffer_state *f)
{
 struct context *ctx=c(p);
 /* Validate while the previous binding is live: the incoming texture can
  * be that same resource and the context can own its final reference. */
 if(f&&!f->nr_cbufs&&!f->zsbuf.texture&&!f->resolve&&!f->viewmask){remesa_resource_reference(&ctx->target,NULL);remesa_resource_reference(&ctx->depth_target,NULL);ctx->error=0;return;}
 if(!f || f->nr_cbufs!=1 || (!s(p->screen)->cpu_depth24_lab&&!s(p->screen)->linked_attachment_storage&&!(s(p->screen)->native_fill&&s(p->screen)->device.resource_only_backend)&&(f->width!=32||f->height!=32)) || !f->width||!f->height || f->samples>1 || (f->zsbuf.texture&&(!(s(p->screen)->cpu_depth24_lab||s(p->screen)->linked_attachment_storage)||
    f->zsbuf.format!=PIPE_FORMAT_Z24X8_UNORM||f->zsbuf.texture->format!=PIPE_FORMAT_Z24X8_UNORM||
    f->zsbuf.texture->screen!=p->screen||f->zsbuf.texture->bind!=PIPE_BIND_DEPTH_STENCIL||
    !color_storage_target(f->zsbuf.texture->target)||f->zsbuf.texture->width0!=f->width||f->zsbuf.texture->height0!=f->height||
    f->zsbuf.texture->depth0!=1||f->zsbuf.texture->array_size!=1||f->zsbuf.texture->nr_samples>1||
    f->zsbuf.level||f->zsbuf.first_layer||f->zsbuf.last_layer||f->zsbuf.nr_samples>1)) ||
    f->resolve || f->viewmask || f->cbufs[0].level || f->cbufs[0].first_layer || f->cbufs[0].last_layer ||
    f->cbufs[0].nr_samples>1 ||
    !f->cbufs[0].texture || f->cbufs[0].format!=PIPE_FORMAT_B8G8R8A8_UNORM ||
    f->cbufs[0].texture->screen!=p->screen || !color_storage_target(f->cbufs[0].texture->target) ||
    !(f->cbufs[0].texture->bind&PIPE_BIND_RENDER_TARGET) ||
    f->cbufs[0].texture->format!=PIPE_FORMAT_B8G8R8A8_UNORM || f->cbufs[0].texture->width0!=f->width || f->cbufs[0].texture->height0!=f->height ||
    f->cbufs[0].texture->depth0!=1 || f->cbufs[0].texture->array_size!=1 || f->cbufs[0].texture->nr_samples>1) { remesa_resource_reference(&ctx->target,NULL);remesa_resource_reference(&ctx->depth_target,NULL);ctx->error=ENOTSUP;return; }
 remesa_resource_reference(&ctx->target,f->cbufs[0].texture);
 remesa_resource_reference(&ctx->depth_target,f->zsbuf.texture);
 ctx->error=0;
}
static struct pipe_surface *create_surface(struct pipe_context *p,struct pipe_resource *b,const struct pipe_surface *t)
{
 struct pipe_surface *out;
 if(!b || b->screen!=p->screen || !color_storage_target(b->target) ||
    !((b->bind==PIPE_BIND_RENDER_TARGET&&b->format==PIPE_FORMAT_B8G8R8A8_UNORM)||
      ((s(p->screen)->cpu_depth24_lab||s(p->screen)->linked_attachment_storage)&&b->bind==PIPE_BIND_DEPTH_STENCIL&&b->format==PIPE_FORMAT_Z24X8_UNORM))||!t ||
    t->format!=b->format || t->level || t->first_layer || t->last_layer || t->nr_samples>1) return NULL;
 out=calloc(1,sizeof(*out));if(!out) return NULL;*out=*t;
 out->reference.count=1;out->texture=NULL;remesa_resource_reference(&out->texture,b);
 out->context=p;return out;
}
static void surface_destroy(struct pipe_context *p,struct pipe_surface *surface)
{ (void)p;remesa_resource_reference(&surface->texture,NULL);free(surface); }
static void *unsupported_shader(struct pipe_context *p,const struct pipe_shader_state *state)
{
#ifdef REMESA_WITH_NIR
 if(state && state->type==PIPE_SHADER_IR_NIR) ralloc_free(state->ir.nir);
#else
 (void)state;
#endif
 c(p)->error=ENOTSUP;return NULL;
}
#ifdef REMESA_WITH_NIR
static void record_shader_compile(struct context *ctx,unsigned stage,int error,
 const struct remesa_shader_program *program,bool uniform)
{
 struct remesa_shader_compile_status *status=&ctx->shader_compile[stage];
 uint64_t attempts=status->attempts==UINT64_MAX?UINT64_MAX:status->attempts+1;
 *status=(struct remesa_shader_compile_status){.attempts=attempts,.error=error,
  .binary_error=ENOTSUP,.launch_error=ENOTSUP,.uniform_specialization=!error&&uniform};
 if(!error&&!uniform) {
  struct remesa_shader_program_report report;
  int err=remesa_shader_program_report(program,&report);
  if(!err) {status->binary_error=report.binary_error;status->binary_bytes=report.binary_bytes;
   status->launch_error=report.launch_error;}
  else status->binary_error=err;
 }
}
static void *create_vertex(struct pipe_context *p,const struct pipe_shader_state *state)
{
 struct vertex *v=NULL;unsigned slot;int err=ENOTSUP;
 struct remesa_shader_ir semantic={0};
 struct remesa_shader_program program={0};
 if(state&&state->type==PIPE_SHADER_IR_NIR) {
  /* Transform feedback is a separate, unsupported API contract. */
  err=state->stream_output.num_outputs ? ENOTSUP : remesa_shader_ir_lower(state->ir.nir,&semantic);
  if(!err)err=remesa_shader_ir_vertex_input(&semantic,&slot);
  if(!err)err=remesa_shader_program_prepare(&semantic,&program);
  ralloc_free(state->ir.nir);
  if(!err) {v=calloc(1,sizeof(*v));if(!v)err=ENOMEM;else {v->input_slot=slot;v->program=program;v->next=c(p)->vertices;c(p)->vertices=v;}}
 }
 record_shader_compile(c(p),0,err,&program,false);
 c(p)->error=err;return v;
}
static struct vertex *find_vertex(struct context *ctx,void *handle)
{
 for(struct vertex *v=ctx->vertices;v;v=v->next)if(v==handle&&!v->deleted)return v;
 return NULL;
}
static void bind_vertex(struct pipe_context *p,void *handle)
{
 struct context *ctx=c(p);ctx->diagnostic_discard=false;ctx->vertex_required=true;ctx->bound_vertex=NULL;
 if(!handle){ctx->error=0;return;}
 struct vertex *v=find_vertex(ctx,handle);if(!v){ctx->error=EINVAL;return;}
 ctx->bound_vertex=v;ctx->error=0;
}
static void delete_vertex(struct pipe_context *p,void *handle)
{
 struct context *ctx=c(p);struct vertex *v=find_vertex(ctx,handle);
 if(!v){ctx->error=EINVAL;return;}
 if(ctx->bound_vertex==v)ctx->bound_vertex=NULL;
 remesa_shader_program_finish(&v->program);
 v->deleted=true;
}
static void *create_fragment(struct pipe_context *p,const struct pipe_shader_state *state)
{
 struct fragment *f=NULL;uint32_t argb=0;int err=ENOTSUP;bool uniform=false;
 struct remesa_uniform_color descriptor={0};
 struct remesa_shader_ir semantic={0};
 struct remesa_shader_program program={0};
 if(state && state->type==PIPE_SHADER_IR_NIR) {
  err=state->stream_output.num_outputs ? ENOTSUP : remesa_shader_ir_lower(state->ir.nir,&semantic);
  if(!err&&remesa_shader_ir_fragment_color_input(&semantic)&&
     remesa_shader_ir_fragment_flat_color_input(&semantic))
   err=remesa_shader_ir_fragment_color(&semantic,&argb);
  if(!err)err=remesa_shader_program_prepare(&semantic,&program);
  if(err==ENOTSUP&&!state->stream_output.num_outputs) {
   err=remesa_nir_uniform_fragment(state->ir.nir,&descriptor);uniform=!err;
  }
  /* Gallium transfers NIR ownership even if compilation is rejected. */
  ralloc_free(state->ir.nir);
  if(!err) {
   f=calloc(1,sizeof(*f));
   if(!f) err=ENOMEM;
   else {f->argb=argb;f->program=program;f->uses_uniform=uniform;f->uniform=descriptor;f->next=c(p)->fragments;c(p)->fragments=f;}
  }
 }
 record_shader_compile(c(p),1,err,&program,uniform);
 c(p)->error=err;return f;
}
static struct fragment *find_fragment(struct context *ctx,void *handle)
{
 for(struct fragment *f=ctx->fragments;f;f=f->next)
  if(f==handle && !f->deleted)return f;
 return NULL;
}
static void bind_fragment(struct pipe_context *p,void *handle)
{
 struct context *ctx=c(p);ctx->diagnostic_discard=false;struct fragment *f=find_fragment(ctx,handle);
 ctx->pipeline_bound=false;ctx->bound_fragment=NULL;
 if(!handle){ctx->error=0;return;}
 if(!f){ctx->error=EINVAL;return;}
 ctx->bound_fragment=f;ctx->argb=f->argb;ctx->pipeline_bound=true;ctx->error=0;
}
static void delete_fragment(struct pipe_context *p,void *handle)
{
 struct context *ctx=c(p);struct fragment *f=find_fragment(ctx,handle);
 if(!f){ctx->error=EINVAL;return;}
 if(ctx->bound_fragment==f){ctx->pipeline_bound=false;ctx->bound_fragment=NULL;}
 remesa_shader_program_finish(&f->program);
 /* Keep a tombstone until context teardown so a stale handle cannot alias a
  * freshly allocated shader. No NIR/program allocation is retained. */
 f->deleted=true;
}
int remesa_context_shader_program_report(struct pipe_context *p,unsigned stage,
                                        void *handle,struct remesa_shader_program_report *out)
{
 if(!p||!out||!handle)return EINVAL;
 if(s(p->screen)->closing)return ESHUTDOWN;
 if(stage==PIPE_SHADER_VERTEX) {
  struct vertex *v=find_vertex(c(p),handle);
  return v ? remesa_shader_program_report(&v->program,out) : EINVAL;
 }
 if(stage==PIPE_SHADER_FRAGMENT) {
  struct fragment *f=find_fragment(c(p),handle);
  if(!f)return EINVAL;
  /* CPU uniform snapshot specialization is not a GPU uniform program. */
  return f->uses_uniform ? ENOTSUP : remesa_shader_program_report(&f->program,out);
 }
 return ENOTSUP;
}
int remesa_context_shader_program_encode(struct pipe_context *p,unsigned stage,
                                        void *handle,void *out,size_t capacity,size_t *written)
{
 if(!p||!handle||!written||(!out&&capacity))return EINVAL;
 if(s(p->screen)->closing)return ESHUTDOWN;
 if(stage==PIPE_SHADER_VERTEX) {
  struct vertex *v=find_vertex(c(p),handle);
  return v ? remesa_shader_program_encode(&v->program,out,capacity,written) : EINVAL;
 }
 if(stage==PIPE_SHADER_FRAGMENT) {
  struct fragment *f=find_fragment(c(p),handle);
  if(!f)return EINVAL;
  return f->uses_uniform ? ENOTSUP : remesa_shader_program_encode(&f->program,out,capacity,written);
 }
 return ENOTSUP;
}
int remesa_context_shader_pds_preview(struct pipe_context *p,unsigned stage,
 void *handle,const struct remesa_pds_candidate_request *request,struct remesa_shader_pds_preview *out)
{
 if(!p||!handle||!request||!out)return EINVAL;
 if(s(p->screen)->closing)return ESHUTDOWN;
 if(stage==PIPE_SHADER_VERTEX) {
  struct vertex *v=find_vertex(c(p),handle);
  return v ? remesa_shader_program_preview_pds(&v->program,request,out) : EINVAL;
 }
 if(stage==PIPE_SHADER_FRAGMENT) {
  struct fragment *f=find_fragment(c(p),handle);if(!f)return EINVAL;
  return f->uses_uniform ? ENOTSUP : remesa_shader_program_preview_pds(&f->program,request,out);
 }
 return ENOTSUP;
}
int remesa_context_shader_pipeline_report(struct pipe_context *p,struct remesa_shader_pipeline *out)
{
 if(!p||!out)return EINVAL;
 if(s(p->screen)->closing)return ESHUTDOWN;
 struct context *ctx=c(p);
 if(!ctx->bound_vertex||!ctx->bound_fragment)return ENODATA;
 struct vertex *v=find_vertex(ctx,ctx->bound_vertex);
 struct fragment *f=find_fragment(ctx,ctx->bound_fragment);
 if(!v||!f)return EINVAL;
 if(f->uses_uniform)return ENOTSUP;
 return remesa_shader_pipeline_prepare(&v->program,&f->program,out);
}
static int context_shader_programs(struct pipe_context *p,struct remesa_shader_draw_programs *out)
{
 if(!p||p->draw_vbo!=draw||!out)return EINVAL;
 if(s(p->screen)->closing)return ESHUTDOWN;
 struct context *ctx=c(p);
 if(!ctx->bound_vertex||!ctx->bound_fragment)return ENODATA;
 struct vertex *v=find_vertex(ctx,ctx->bound_vertex);
 struct fragment *f=find_fragment(ctx,ctx->bound_fragment);
 if(!v||!f)return EINVAL;
 if(f->uses_uniform)return ENOTSUP;
 return remesa_shader_draw_programs_prepare(&v->program,&f->program,out);
}
int remesa_context_prepare_host_shader_pipeline(struct pipe_context *p,
 const struct remesa_shader_host_pipeline_request *request,struct remesa_shader_host_pipeline **out)
{
 if(!p||p->draw_vbo!=draw||!request||!context_output_valid(c(p),out,sizeof(*out)))return EINVAL;
 struct remesa_shader_draw_programs programs;
 int error=context_shader_programs(p,&programs);if(error)return error;
 return remesa_shader_host_pipeline_create(&programs,request,out);
}
int remesa_context_revalidate_host_shader_pipeline(struct pipe_context *p,
 const struct remesa_shader_host_pipeline *pipeline,const struct remesa_shader_host_pipeline_request *request)
{
 if(!pipeline||!request)return EINVAL;
 struct remesa_shader_draw_programs programs;
 int error=context_shader_programs(p,&programs);if(error)return error;
 return remesa_shader_host_pipeline_revalidate(pipeline,&programs,request);
}
int remesa_context_prepare_shader_contract(struct pipe_context *p,
 const struct remesa_shader_host_pipeline *pipeline,const struct remesa_shader_host_pipeline_request *current,
 const struct remesa_pds_host_transfer_request *transfers,struct remesa_shader_execution_contract *out)
{
 if(!p||p->draw_vbo!=draw||!current||sizeof(*current)>UINTPTR_MAX-(uintptr_t)current||
    !context_output_valid(c(p),out,sizeof(*out)))return EINVAL;
 int error=remesa_context_revalidate_host_shader_pipeline(p,pipeline,current);if(error)return error;
 uintptr_t begin=(uintptr_t)out,end=begin+sizeof(*out);
 if(begin<(uintptr_t)current+sizeof(*current)&&end>(uintptr_t)current)return EINVAL;
 /* Revalidation checked each recipe length/pointer before these range checks.
  * The output must also preserve the fresh caller inputs, not just our copy. */
 for(unsigned i=0;i<2;i++) {
  uintptr_t source=(uintptr_t)current->data[i].cells;
  size_t bytes=current->data[i].cell_count*sizeof(*current->data[i].cells);
  if(begin<source+bytes&&end>source)return EINVAL;
 }
 return remesa_shader_host_pipeline_prepare_contract(pipeline,transfers,out);
}
int remesa_context_revalidate_shader_contract(struct pipe_context *p,
 const struct remesa_shader_host_pipeline *pipeline,const struct remesa_shader_host_pipeline_request *current,
 const struct remesa_pds_host_transfer_request *transfers,const struct remesa_shader_execution_contract *contract)
{
 int error=remesa_context_revalidate_host_shader_pipeline(p,pipeline,current);if(error)return error;
 return remesa_shader_host_pipeline_validate_contract(pipeline,transfers,contract);
}
int remesa_context_prepare_pixel_task_pipeline(struct pipe_context *p,
 const struct remesa_shader_host_pipeline_request *request,uint32_t metadata6,
 struct remesa_shader_host_pipeline **out)
{
 if(!p||p->draw_vbo!=draw||!request||!context_output_valid(c(p),out,sizeof(*out)))return EINVAL;
 struct remesa_shader_draw_programs programs;
 int error=context_shader_programs(p,&programs);if(error)return error;
 return remesa_shader_host_pipeline_create_pixel_task(&programs,request,metadata6,out);
}
int remesa_context_prepare_pds_structure_pipeline(struct pipe_context *p,
 const struct remesa_shader_host_pipeline_request *request,const struct remesa_pds_structure_request *source,
 struct remesa_shader_host_pipeline **out)
{
 if(!p||p->draw_vbo!=draw||!request||!source||!context_output_valid(c(p),out,sizeof(*out)))return EINVAL;
 struct remesa_shader_draw_programs programs;
 int error=context_shader_programs(p,&programs);if(error)return error;
 return remesa_shader_host_pipeline_create_pds_structure(&programs,request,source,out);
}
int remesa_context_revalidate_pds_structure_pipeline(struct pipe_context *p,
 const struct remesa_shader_host_pipeline *pipeline,const struct remesa_shader_host_pipeline_request *request,
 const struct remesa_pds_structure_request *source)
{
 if(!pipeline||!request||!source)return EINVAL;
 struct remesa_shader_draw_programs programs;
 int error=context_shader_programs(p,&programs);if(error)return error;
 return remesa_shader_host_pipeline_revalidate_pds_structure(pipeline,&programs,request,source);
}
static int context_pixel_task_current(struct pipe_context *p,
 const struct remesa_shader_host_pipeline *pipeline,
 const struct remesa_shader_host_pipeline_request *current,uint32_t metadata6,
 const struct remesa_pds_cpu_pixel_task_output *out)
{
 if(!p||p->draw_vbo!=draw||!current||sizeof(*current)>UINTPTR_MAX-(uintptr_t)current||
    !context_output_valid(c(p),out,sizeof(*out)))return EINVAL;
 struct remesa_shader_draw_programs programs;
 int error=context_shader_programs(p,&programs);if(error)return error;
 error=remesa_shader_host_pipeline_revalidate_pixel_task(pipeline,&programs,current,metadata6);
 if(error)return error;
 /* Revalidation checked the consumed VS recipe. The FS recipe in current
  * is explicitly ignored by this API and need not point to accessible data. */
 uintptr_t begin=(uintptr_t)out,end=begin+sizeof(*out),source=(uintptr_t)current;
 if(begin<source+sizeof(*current)&&end>source)return EINVAL;
 source=(uintptr_t)current->data[0].cells;
 size_t bytes=current->data[0].cell_count*sizeof(*current->data[0].cells);
 if(begin<source+bytes&&end>source)return EINVAL;
 return 0;
}
int remesa_context_prepare_pixel_task_output(struct pipe_context *p,
 const struct remesa_shader_host_pipeline *pipeline,
 const struct remesa_shader_host_pipeline_request *current,uint32_t metadata6,
 const struct remesa_pds_host_transfer_request *transfers,struct remesa_pds_cpu_pixel_task_output *out)
{
 int error=context_pixel_task_current(p,pipeline,current,metadata6,out);if(error)return error;
 return remesa_shader_host_pipeline_prepare_pixel_task_output(pipeline,transfers,out);
}
int remesa_context_revalidate_pixel_task_output(struct pipe_context *p,
 const struct remesa_shader_host_pipeline *pipeline,
 const struct remesa_shader_host_pipeline_request *current,uint32_t metadata6,
 const struct remesa_pds_host_transfer_request *transfers,const struct remesa_pds_cpu_pixel_task_output *out)
{
 int error=context_pixel_task_current(p,pipeline,current,metadata6,out);if(error)return error;
 return remesa_shader_host_pipeline_validate_pixel_task_output(pipeline,transfers,out);
}
int remesa_context_prepare_shader_launch(struct pipe_context *p,
 const struct remesa_pds_request *vertex,const struct remesa_pds_request *fragment,
 struct remesa_shader_pipeline_launch *out)
{
 if(!p||!vertex||!fragment||!out)return EINVAL;
 if(s(p->screen)->closing)return ESHUTDOWN;
 struct context *ctx=c(p);
 if(!ctx->bound_vertex||!ctx->bound_fragment)return ENODATA;
 struct vertex *v=find_vertex(ctx,ctx->bound_vertex);
 struct fragment *f=find_fragment(ctx,ctx->bound_fragment);
 if(!v||!f)return EINVAL;
 if(f->uses_uniform)return ENOTSUP;
 return remesa_shader_pipeline_prepare_launch(&v->program,&f->program,vertex,fragment,out);
}
int remesa_context_revalidate_shader_launch(struct pipe_context *p,
 const struct remesa_shader_pipeline_launch *launch,const struct remesa_pds_request *vertex,
 const struct remesa_pds_request *fragment)
{
 if(!p||!launch||!vertex||!fragment)return EINVAL;
 if(s(p->screen)->closing)return ESHUTDOWN;
 struct context *ctx=c(p);
 if(!ctx->bound_vertex||!ctx->bound_fragment)return ENODATA;
 struct vertex *v=find_vertex(ctx,ctx->bound_vertex);
 struct fragment *f=find_fragment(ctx,ctx->bound_fragment);
 if(!v||!f)return EINVAL;
 if(f->uses_uniform)return ENOTSUP;
 return remesa_shader_pipeline_revalidate_launch(launch,&v->program,&f->program,vertex,fragment);
}
#endif
int remesa_context_shader_linkage_snapshot(struct pipe_context *p,
 struct remesa_shader_linkage *out)
{
 if(!p||p->draw_vbo!=draw||!out)return EINVAL;
 if(s(p->screen)->closing)return ESHUTDOWN;
#ifdef REMESA_WITH_NIR
 struct context *ctx=c(p);
 if(!context_output_valid(ctx,out,sizeof(*out)))return EINVAL;
 if(!ctx->bound_vertex||!ctx->bound_fragment)return ENODATA;
 struct vertex *v=find_vertex(ctx,ctx->bound_vertex);
 struct fragment *f=find_fragment(ctx,ctx->bound_fragment);
 if(!v||!f)return EINVAL;
 if(f->uses_uniform)return ENOTSUP;
 return remesa_shader_linkage_prepare(&v->program,&f->program,out);
#else
 return ENOTSUP;
#endif
}
int remesa_context_shader_compile_status(struct pipe_context *p,unsigned stage,
 struct remesa_shader_compile_status *out)
{
 if(!p||p->draw_vbo!=draw||!context_output_valid(c(p),out,sizeof(*out)))return EINVAL;
 if(s(p->screen)->closing)return ESHUTDOWN;
 if(stage!=PIPE_SHADER_VERTEX&&stage!=PIPE_SHADER_FRAGMENT)return ENOTSUP;
#ifdef REMESA_WITH_NIR
 const struct remesa_shader_compile_status *status=&c(p)->shader_compile[stage==PIPE_SHADER_FRAGMENT];
 if(!status->attempts)return ENODATA;
 *out=*status;return 0;
#else
 return ENOTSUP;
#endif
}
/* Pure state admission does not generate new SGX state bytes. These immutable
 * descriptors require semantics equivalent to the unchanged restricted profile.
 * Invalid creates/binds remain latched independently of transient API errors. */
static void *create_fixed(struct pipe_context *p,enum fixed_kind kind,int error,
                         bool scissor,bool depth_clip)
{
 struct context *ctx=c(p);struct fixed *state=NULL;
 if(!error) {
  state=calloc(1,sizeof(*state));if(!state)error=ENOMEM;
  else {state->kind=kind;state->scissor=scissor;state->depth_clip=depth_clip;state->next=ctx->fixed_states;ctx->fixed_states=state;}
 }
 if(error)ctx->invalid_fixed[kind]=true;
 ctx->error=error;return state;
}
static struct fixed *find_fixed(struct context *ctx,void *handle,enum fixed_kind kind)
{
 for(struct fixed *state=ctx->fixed_states;state;state=state->next)
  if(state==handle&&!state->deleted&&state->kind==kind)return state;
 return NULL;
}
static void bind_fixed(struct pipe_context *p,void *handle,enum fixed_kind kind)
{
 struct context *ctx=c(p);struct fixed *state=find_fixed(ctx,handle,kind);
 /* Mesa CSO cleanup explicitly unbinds these states with NULL. Unbinding
  * is valid lifecycle state, not an implicit executable default: NIR draws
  * still require all three bound objects in draw(). Foreign/deleted non-NULL
  * handles retain their separate rejection until a valid bind or unbind. */
 ctx->bound_fixed[kind]=state;ctx->invalid_fixed[kind]=handle&&!state;
 ctx->error=ctx->invalid_fixed[kind]?EINVAL:0;
}
static void delete_fixed(struct pipe_context *p,void *handle,enum fixed_kind kind)
{
 struct context *ctx=c(p);struct fixed *state=find_fixed(ctx,handle,kind);
 if(!state){ctx->error=EINVAL;return;}
 if(ctx->bound_fixed[kind]==state){ctx->bound_fixed[kind]=NULL;ctx->invalid_fixed[kind]=true;}
 state->deleted=true;
}
static void *create_blend(struct pipe_context *p,const struct pipe_blend_state *state)
{ return create_fixed(p,FIXED_BLEND,remesa_validate_blend(state),false,false); }
static void bind_blend(struct pipe_context *p,void *state){bind_fixed(p,state,FIXED_BLEND);}
static void delete_blend(struct pipe_context *p,void *state){delete_fixed(p,state,FIXED_BLEND);}
static void *create_dsa(struct pipe_context *p,const struct pipe_depth_stencil_alpha_state *state)
{
 /* The typed GEM path shares the already qualified opaque LESS/write
  * contract. This admits state ownership only, never ordinary draw or CPU Z. */
 int error=(s(p->screen)->gears_runtime||s(p->screen)->linked_attachment_storage)?
  remesa_validate_gears_dsa(state):remesa_validate_dsa(state);
 struct fixed *out=create_fixed(p,FIXED_DSA,error,false,false);
 if(out&&state){out->depth_enabled=state->depth_enabled;out->depth_write=state->depth_writemask;out->depth_func=state->depth_func;}
 return out;
}
static void bind_dsa(struct pipe_context *p,void *state){bind_fixed(p,state,FIXED_DSA);}
static void delete_dsa(struct pipe_context *p,void *state){delete_fixed(p,state,FIXED_DSA);}
int remesa_context_linked_isp_snapshot(struct pipe_context *p,
 struct remesa_isp_opaque_triangle_request *out)
{
 if(!p||p->draw_vbo!=draw||!out||
    (uintptr_t)out%_Alignof(struct remesa_isp_opaque_triangle_request)||
    !context_state_output_valid(c(p),out,sizeof(*out)))return EINVAL;
 struct context *ctx=c(p);struct screen *sc=s(p->screen);
 if(sc->closing)return ESHUTDOWN;
 if(!sc->linked_attachment_storage)return ENOTSUP;
 if(ctx->failed_submission)return EOWNERDEAD;
 if(!ctx->target||!ctx->depth_target)return ENODATA;
 if(r(ctx->target)->write_error)return r(ctx->target)->write_error;
 if(r(ctx->depth_target)->write_error)return r(ctx->depth_target)->write_error;
 if(ctx->invalid_fixed[FIXED_BLEND]||ctx->invalid_fixed[FIXED_DSA])return EINVAL;
 struct fixed *blend=ctx->bound_fixed[FIXED_BLEND],*depth=ctx->bound_fixed[FIXED_DSA];
 if(!blend||!depth)return ENODATA;
 if(depth->depth_enabled&&(!depth->depth_write||depth->depth_func!=PIPE_FUNC_LESS))return ENOTSUP;
 /* create_blend admitted only opaque/full RGBA; disabled depth ignores both
  * the API comparison operand and its write mask. No ISA/register encoding. */
 const struct remesa_isp_opaque_triangle_request value={
  .depth_bits_present=true,.depth_test=depth->depth_enabled,
  .depth_write=depth->depth_enabled&&depth->depth_write,
  .compare=depth->depth_enabled?REMESA_DEPTH_LESS:REMESA_DEPTH_ALWAYS,.rgba_mask=15};
 *out=value;return 0;
}
static void *create_raster(struct pipe_context *p,const struct pipe_rasterizer_state *state)
{
 if(s(p->screen)->gears_runtime){
  if(state&&getenv("REMESA_GEARS_TRACE"))fprintf(stderr,"GEARS_RASTER half=%u bottom=%u cull=%u ccw=%u flat=%u first=%u clamp_v=%u clamp_f=%u twoside=%u fill=%u/%u clip=%u/%u halfz=%u precision=%u/%u\n",state->half_pixel_center,state->bottom_edge_rule,state->cull_face,state->front_ccw,state->flatshade,state->flatshade_first,state->clamp_vertex_color,state->clamp_fragment_color,state->light_twoside,state->fill_front,state->fill_back,state->depth_clip_near,state->depth_clip_far,state->clip_halfz,state->subpixel_precision_x,state->subpixel_precision_y);
  struct fixed *out=create_fixed(p,FIXED_RASTER,remesa_validate_gears_rasterizer(state),state&&state->scissor,state&&(state->depth_clip_near||state->depth_clip_far));
  if(out){out->bottom_edge_rule=state->bottom_edge_rule;out->flat=state->flatshade;out->clamp_vertex_color=state->clamp_vertex_color;out->flat_first=state->flatshade_first;out->front_ccw=state->front_ccw;out->cull_face=state->cull_face;out->depth_clip_near=state->depth_clip_near;out->depth_clip_far=state->depth_clip_far;}
  return out;
 }
 const struct pipe_rasterizer_state *admitted=state;struct pipe_rasterizer_state host;
 bool recording=s(p->screen)->device.offline_test_backend&&!s(p->screen)->device.qualified_hardware_backend;
 /* Host-facing culling is connected only to the recording preparation path.
  * Physical state admission stays unchanged; no SGX cull bit is generated. */
 if(state&&state->cull_face&&recording&&state->depth_clip_near&&state->depth_clip_far) {
  host=*state;host.cull_face=PIPE_FACE_NONE;admitted=&host;
 }
 const struct remesa_device *device=&s(p->screen)->device;
 bool finite_lab=device->qualified_hardware_backend&&device->retained_fragment_carrier&&
  device->retained_gl_fbo_half_pixel_center;
 bool recording_window=recording&&admitted&&admitted->half_pixel_center&&admitted->bottom_edge_rule;
 int error=recording_window?remesa_validate_recording_window_rasterizer(admitted):
  finite_lab?remesa_validate_lab_rasterizer(admitted,REMESA_LAB_RASTER_QUARTER_FBO):
  remesa_validate_rasterizer(admitted);
 struct fixed *out=create_fixed(p,FIXED_RASTER,error,state&&state->scissor,state&&(state->depth_clip_near||state->depth_clip_far));
 if(out){out->recording_edge_proof=recording_window;
  out->full_clip=state->depth_clip_near&&state->depth_clip_far;
  out->depth_clip_near=state->depth_clip_near;out->depth_clip_far=state->depth_clip_far;
  out->cull_face=state->cull_face;out->front_ccw=state->front_ccw;
  out->flat_first=state->flatshade_first;}
 return out; }
static void bind_raster(struct pipe_context *p,void *state){bind_fixed(p,state,FIXED_RASTER);}
static void delete_raster(struct pipe_context *p,void *state){delete_fixed(p,state,FIXED_RASTER);}
static void viewport_state(struct pipe_context *p,unsigned start,unsigned count,const struct pipe_viewport_state *v)
{
 struct context *ctx=c(p);ctx->invalid_viewport=true;
 if(start || count!=1 || !v){ctx->error=ENOTSUP;return;}
 if(!remesa_viewport_swizzle_is_identity(v)){ctx->error=ENOTSUP;return;}
 if(s(p->screen)->device.resource_only_backend&&!s(p->screen)->gears_runtime){
  for(unsigned i=0;i<3;i++)if(!isfinite(v->scale[i])||!isfinite(v->translate[i])){ctx->error=EINVAL;return;}
  /* State binding is independent of the selected serializer and never admits
   * a draw on a resource-only endpoint. Preserve the exact application bits. */
  ctx->viewport=*v;ctx->viewport_set=true;ctx->invalid_viewport=false;ctx->error=0;return;
 }
 if(s(p->screen)->gears_runtime){
  if(v->scale[0]!=640.0f||(v->scale[1]!=400.0f&&v->scale[1]!=-400.0f)||v->scale[2]!=0.5f||
     v->translate[0]!=640.0f||v->translate[1]!=400.0f||v->translate[2]!=0.5f){ctx->error=ENOTSUP;return;}
  ctx->viewport=*v;ctx->viewport_set=true;ctx->invalid_viewport=false;ctx->error=0;return;
 }

 bool identity=true,canonical=true;
 for(unsigned i=0;i<3;i++) {
  if(v->scale[i]!=1||v->translate[i]!=0)identity=false;
  float required=i==2?.5f:16;
  bool scale=v->scale[i]==required;
  if(i<2&&s(p->screen)->device.offline_test_backend&&!s(p->screen)->device.qualified_hardware_backend)
   scale=scale||v->scale[i]==-required;
  if(!scale||v->translate[i]!=required)canonical=false;
 }
 if(!identity&&!canonical){ctx->error=ENOTSUP;return;}
 ctx->viewport=*v;ctx->viewport_set=true;ctx->invalid_viewport=false;ctx->error=0;
}
static int window_viewport_request(struct context *ctx,uint32_t depth_bits,
 struct remesa_ta_window_viewport_request *out)
{
 struct screen *sc=s(ctx->base.screen);
 if(sc->closing)return ESHUTDOWN;
 if(!sc->device.resource_only_backend||sc->gears_runtime)return ENOTSUP;
 if(!ctx->viewport_set||ctx->invalid_viewport||!ctx->target)return ENODATA;
 const struct pipe_viewport_state *v=&ctx->viewport;
 double width=2.0*(double)v->scale[0],height=-2.0*(double)v->scale[1];
 double x=(double)v->translate[0]-(double)v->scale[0];
 double y=(double)ctx->target->height0-(double)v->translate[1]+(double)v->scale[1];
 double near_z=(double)v->translate[2]-(double)v->scale[2];
 double far_z=(double)v->translate[2]+(double)v->scale[2];
 /* Check all bounds before floating-to-integer conversion. Positive-Y/FBO
  * and mirrored-X viewports are not this original window CPU contract. */
 if(!isfinite(width)||!isfinite(height)||!isfinite(x)||!isfinite(y)||
    width<=0||height<=0||x<0||y<0||width>2048||height>2048||x>2048||y>2048)return ENOTSUP;
 if(width!=(double)(uint32_t)width||height!=(double)(uint32_t)height||
    x!=(double)(uint32_t)x||y!=(double)(uint32_t)y)return ENOTSUP;
 if(!isfinite(near_z)||!isfinite(far_z)||near_z<0||near_z>1||far_z<0||far_z>1)return ENOTSUP;
 /* Conversion must not silently round the source viewport's depth interval. */
 if((double)(float)near_z!=near_z||(double)(float)far_z!=far_z)return ENOTSUP;
 struct remesa_ta_window_viewport_request request={
  .framebuffer_width=ctx->target->width0,.framebuffer_height=ctx->target->height0,
  .x=(uint32_t)x,.y=(uint32_t)y,.width=(uint32_t)width,.height=(uint32_t)height,
  .depth_bits=depth_bits,.near_z=(float)near_z,.far_z=(float)far_z,
  .path=REMESA_TA_VIEWPORT_MATRIX_WINDOW};
 unsigned char encoded[28];int error=remesa_ta_window_viewport_encode(&request,encoded);
 if(!error)*out=request;
 return error;
}
int remesa_context_window_viewport_snapshot(struct pipe_context *p,uint32_t depth_bits,
 struct remesa_ta_window_viewport_request *out)
{
 if(!p||p->draw_vbo!=draw||!out||
    (uintptr_t)out%_Alignof(struct remesa_ta_window_viewport_request)||
    !context_state_output_valid(c(p),out,sizeof(*out)))return EINVAL;
 return window_viewport_request(c(p),depth_bits,out);
}
int remesa_context_window_viewport_parameters_snapshot(struct pipe_context *p,
 struct remesa_vertex_parameter_values *out)
{
 if(!p||p->draw_vbo!=draw||!out||
    (uintptr_t)out%_Alignof(struct remesa_vertex_parameter_values)||
    !context_state_output_valid(c(p),out,sizeof(*out)))return EINVAL;
 struct context *ctx=c(p);
 struct remesa_ta_window_viewport_request viewport;
 int e=window_viewport_request(ctx,24,&viewport);if(e)return e;
 if(ctx->failed_submission)return EOWNERDEAD;
 if(r(ctx->target)->write_error)return r(ctx->target)->write_error;
 struct remesa_vertex_parameter_values values={0};
 for(unsigned i=0;i<3;i++){
  memcpy(&values.coefficient_bits[i],&ctx->viewport.scale[i],4);
  memcpy(&values.coefficient_bits[4+i],&ctx->viewport.translate[i],4);
 }
 values.coefficient_bits[3]=UINT32_C(0x3f800000);
 *out=values;return 0;
}
int remesa_context_window_viewport_encode(struct pipe_context *p,uint32_t depth_bits,
 unsigned char out[28])
{
 if(!p||p->draw_vbo!=draw||!context_state_output_valid(c(p),out,28))return EINVAL;
 struct remesa_ta_window_viewport_request request;
 int error=window_viewport_request(c(p),depth_bits,&request);if(error)return error;
 return remesa_ta_window_viewport_encode(&request,out);
}
static void scissor_state(struct pipe_context *p,unsigned start,unsigned count,const struct pipe_scissor_state *v)
{
 struct context *ctx=c(p);ctx->invalid_scissor=true;
 if(start || count!=1 || !v || v->minx || v->miny || v->maxx!=32 || v->maxy!=32){ctx->error=ENOTSUP;return;}
 ctx->invalid_scissor=false;ctx->scissor_set=true;ctx->error=0;
}
/* Mesa state tracker/CSO owns these host descriptors. Creating a nearest
 * descriptor does not admit GPU texture sampling. Nonempty bindings remain
 * draw-blocking independently of transient errors on unrelated callbacks. */
static void *create_sampler(struct pipe_context *p,const struct pipe_sampler_state *state)
{
 void *handle=NULL;
 c(p)->error=remesa_sampler_create(c(p)->samplers,state,&handle);
 return handle;
}
static void bind_samplers(struct pipe_context *p,enum pipe_shader_type stage,
 unsigned start,unsigned count,void **states)
{ c(p)->error=remesa_sampler_bind(c(p)->samplers,stage,start,count,states); }
static void delete_sampler(struct pipe_context *p,void *state)
{ c(p)->error=state?remesa_sampler_delete(c(p)->samplers,state):0; }
/* Query only resources from this screen. Backing metadata is not a kernel
 * generation or proof of GPU visibility; quarantined backing is inadmissible. */
static int texture_storage(void *owner,struct pipe_resource *resource,
 unsigned level,struct remesa_texture_storage *out)
{
 struct pipe_screen *screen=owner;
 if(!resource || resource->screen!=screen || !out) return EINVAL;
 if(s(screen)->closing) return ESHUTDOWN;
 if(r(resource)->write_error) return r(resource)->write_error;
 if(r(resource)->maps) return EBUSY;
 if(!r(resource)->bo || !r(resource)->pass.resource_cookie) return ESTALE;
 if(level>=r(resource)->layout.count)return EINVAL;
 const struct remesa_texture_level *mip=&r(resource)->layout.levels[level];
 *out=(struct remesa_texture_storage){.resource_cookie=r(resource)->pass.resource_cookie,
     .bytes=r(resource)->bytes,.stride=mip->stride,.offset=mip->offset,
     .width=mip->width,.height=mip->height,.levels=r(resource)->layout.count,
     .content_epoch=r(resource)->pass.content_epoch};return 0;
}
/* Frontend metadata bridge only. The provider must independently attest the
 * retained mapping; this API never encodes a texture or admits sampling. */
int remesa_context_texture_view_storage(struct pipe_context *p,
 const struct pipe_sampler_view *view,struct remesa_texture_storage *out)
{
 if(!p||p->draw_vbo!=draw||!out)return EINVAL;
 if(s(p->screen)->closing)return ESHUTDOWN;
 return remesa_texture_view_storage(c(p)->textures,view,out);
}
int remesa_context_texture_view_level_storage(struct pipe_context *p,
 const struct pipe_sampler_view *view,unsigned level,struct remesa_texture_storage *out)
{
 if(!p||p->draw_vbo!=draw||!out)return EINVAL;
 if(s(p->screen)->closing)return ESHUTDOWN;
 return remesa_texture_view_level_storage(c(p)->textures,view,level,out);
}
int remesa_context_prepare_texture_descriptor(struct pipe_context *p,
 const struct pipe_sampler_view *view,const struct pipe_sampler_state *sampler,
 uint64_t owner,remesa_texture_mapping_query query,void *user,
 struct remesa_texture_descriptor *out)
{
 struct remesa_texture_storage storage;int error;
 if(!p||p->draw_vbo!=draw||!out||!query)return EINVAL;
 error=remesa_context_texture_view_storage(p,view,&storage);if(error)return error;
 /* Pointer membership was checked before view dereference. Do not let a
  * provider's claimed completion override an unfinished frontend writer. */
 error=resource_sync(p->screen,r(view->texture),0);if(error)return error;
 return remesa_texture_descriptor_prepare(c(p)->textures,view,sampler,
     owner,query,user,out);
}
int remesa_context_revalidate_texture_descriptor(struct pipe_context *p,
 const struct remesa_texture_descriptor *descriptor,const struct pipe_sampler_view *view,
 const struct pipe_sampler_state *sampler,uint64_t owner,
 remesa_texture_mapping_query query,void *user)
{
 struct remesa_texture_storage storage;
 if(!p||p->draw_vbo!=draw||!descriptor||!query)return EINVAL;
 int error=remesa_context_texture_view_storage(p,view,&storage);if(error)return error;
 error=resource_sync(p->screen,r(view->texture),0);if(error)return error;
 return remesa_texture_descriptor_revalidate(descriptor,c(p)->textures,view,sampler,
     owner,query,user);
}
static struct pipe_sampler_view *create_sampler_view(struct pipe_context *p,
 struct pipe_resource *resource,const struct pipe_sampler_view *view)
{
 struct pipe_sampler_view *out=NULL;
 c(p)->error=remesa_texture_view_create(c(p)->textures,resource,view,&out);
 return out;
}
static void destroy_sampler_view(struct pipe_context *p,struct pipe_sampler_view *view)
{
 c(p)->error=view?remesa_texture_view_destroy(c(p)->textures,view):0;
}
/* Mesa state_tracker releases even a NULL pixel-map view at teardown.
 * Release the frontend reference, preserving the view bank's owner callback.
 * This lifetime hook does not advertise texture execution support. */
static void release_sampler_view(struct pipe_context *p,struct pipe_sampler_view *view)
{
 (void)p;
 if(view && !__atomic_sub_fetch(&view->reference.count,1,__ATOMIC_ACQ_REL))
  view->context->sampler_view_destroy(view->context,view);
}
static void set_sampler_views(struct pipe_context *p,enum pipe_shader_type stage,
 unsigned start,unsigned count,unsigned trailing,struct pipe_sampler_view **views)
{ c(p)->error=remesa_texture_views_bind(c(p)->textures,stage,start,count,trailing,views); }
/* Mesa CSO cleanup invokes these even with zero shader/texture caps. */
static void constant_buffer(struct pipe_context *p,enum pipe_shader_type shader,unsigned slot,
 bool take_ownership,const struct pipe_constant_buffer *buffer)
{
 struct context *ctx=c(p);struct pipe_resource *owned=NULL,*incoming=NULL;
 if(buffer&&take_ownership)owned=buffer->buffer;
 /* A borrowed rebind may name the buffer held solely by the old binding.
  * Keep it alive while validation replaces that binding, including failures. */
 if(buffer&&buffer->buffer&&(shader==PIPE_SHADER_FRAGMENT||shader==PIPE_SHADER_VERTEX)&&!slot)
  remesa_resource_reference(&incoming,buffer->buffer);
 bool stage=shader==PIPE_SHADER_VERTEX||shader==PIPE_SHADER_FRAGMENT;
 bool empty=!buffer||(!buffer->buffer&&!buffer->user_buffer&&!buffer->buffer_size&&!buffer->buffer_offset);
 if(!stage||slot>=32){ctx->invalid_constants=true;ctx->error=ENOTSUP;}
 else {
  unsigned index=shader==PIPE_SHADER_VERTEX?0:1;
  if(shader==PIPE_SHADER_FRAGMENT&&!slot) {
   remesa_resource_reference(&ctx->fragment_constants,NULL);
   free(ctx->constant_snapshot);ctx->constant_snapshot=NULL;ctx->constant_size=0;ctx->constant_offset=0;
  }
  if(shader==PIPE_SHADER_VERTEX&&!slot) {
   remesa_resource_reference(&ctx->vertex_constants,NULL);
   free(ctx->vertex_constant_snapshot);ctx->vertex_constant_snapshot=NULL;
   ctx->vertex_constant_size=0;ctx->vertex_constant_offset=0;
  }
#ifdef REMESA_WITH_NIR
  if(!empty&&shader==PIPE_SHADER_VERTEX&&!slot) {
   int error=ENOTSUP;
   /* The complete state records declare9 or10 float4 vectors. This is
    * binding capacity, not the amount a particular shader reads. */
   if(buffer->buffer_size==144u||buffer->buffer_size==160u) {
    if(buffer->buffer&&!buffer->user_buffer) {
     struct pipe_resource *b=buffer->buffer;
     if(b->screen==p->screen&&b->target==PIPE_BUFFER&&(b->bind&PIPE_BIND_CONSTANT_BUFFER)&&
        buffer->buffer_offset<=r(b)->bytes&&buffer->buffer_size<=r(b)->bytes-buffer->buffer_offset&&
        !(buffer->buffer_offset&15u)) {
      remesa_resource_reference(&ctx->vertex_constants,b);
      ctx->vertex_constant_offset=buffer->buffer_offset;error=0;
     }
    } else if(buffer->user_buffer&&!buffer->buffer&&!buffer->buffer_offset) {
     ctx->vertex_constant_snapshot=malloc(buffer->buffer_size);
     if(!ctx->vertex_constant_snapshot)error=ENOMEM;
     else {memcpy(ctx->vertex_constant_snapshot,buffer->user_buffer,buffer->buffer_size);error=0;}
    }
   }
   if(error)ctx->constant_rejections[index]|=1u<<slot;
   else {ctx->vertex_constant_size=buffer->buffer_size;ctx->constant_rejections[index]&=~(1u<<slot);}
   ctx->error=error;
  } else if(!empty&&shader==PIPE_SHADER_FRAGMENT&&!slot) {
   int error=ENOTSUP;
   if(buffer->buffer_size>=16&&buffer->buffer_size<=REMESA_UNIFORM_BYTES_MAX) {
    if(buffer->buffer&&!buffer->user_buffer) {
     struct pipe_resource *b=buffer->buffer;
     if(b->screen==p->screen&&b->target==PIPE_BUFFER&&(b->bind&PIPE_BIND_CONSTANT_BUFFER)&&
        buffer->buffer_offset<=r(b)->bytes&&buffer->buffer_size<=r(b)->bytes-buffer->buffer_offset&&
        !(buffer->buffer_offset&15u)) {
      remesa_resource_reference(&ctx->fragment_constants,b);
      ctx->constant_offset=buffer->buffer_offset;error=0;
     }
    } else if(buffer->user_buffer&&!buffer->buffer&&!buffer->buffer_offset) {
     ctx->constant_snapshot=malloc(buffer->buffer_size);
     if(!ctx->constant_snapshot)error=ENOMEM;
     else {memcpy(ctx->constant_snapshot,buffer->user_buffer,buffer->buffer_size);error=0;}
    }
   }
   if(error)ctx->constant_rejections[index]|=1u<<slot;
   else {ctx->constant_size=buffer->buffer_size;ctx->constant_rejections[index]&=~(1u<<slot);}
   ctx->error=error;
  } else
#endif
  if(!empty){ctx->constant_rejections[index]|=1u<<slot;ctx->error=ENOTSUP;}
  else {ctx->constant_rejections[index]&=~(1u<<slot);ctx->error=0;}
 }
 /* Rejected bindings still consume an explicitly transferred reference. */
 remesa_resource_reference(&owned,NULL);
 remesa_resource_reference(&incoming,NULL);
}
#ifdef REMESA_WITH_NIR
/* Specialize the existing constant fragment profile from synchronized CPU
 * bytes. This is not a GPU UBO instruction or a general shader compiler. */
static int fragment_color(struct pipe_context *p,uint32_t *argb)
{
 struct context *ctx=c(p);struct fragment *f=ctx->bound_fragment;
 /* Offline varying compilation is not a constant-color draw contract. */
 if(f&&!f->uses_uniform&&f->program.status==REMESA_PROGRAM_COMPILE_OFFLINE)return ENOTSUP;
 if(!f||!f->uses_uniform){*argb=ctx->argb;return 0;}
 if(!ctx->constant_size)return ENODATA;
 if(!ctx->fragment_constants)
  return remesa_uniform_color_resolve(&f->uniform,ctx->constant_snapshot,ctx->constant_size,argb);
 struct resource *b=r(ctx->fragment_constants);struct screen *sc=s(p->screen);void *bytes=NULL;
 if(b->maps)return EBUSY;
 int error=resource_sync(p->screen,b,UINT64_MAX);if(error)return error;
 error=sc->device.ops->map(sc->device.priv,b->bo,false,&bytes);
 if(error){if(bytes)sc->device.ops->unmap(sc->device.priv,b->bo,false);return error;}
 error=bytes ? remesa_uniform_color_resolve(&f->uniform,(unsigned char *)bytes+ctx->constant_offset,
                                          ctx->constant_size,argb) : EPROTO;
 sc->device.ops->unmap(sc->device.priv,b->bo,false);return error;
}
#endif
int remesa_context_vertex_parameter_source(struct pipe_context *p,
 const struct remesa_pds_parameter_source_request *request,void *out,size_t capacity,size_t *written)
{
#ifdef REMESA_WITH_NIR
 if(!p||p->draw_vbo!=draw||!request||
    (uintptr_t)request%_Alignof(struct remesa_pds_parameter_source_request)||
    sizeof(*request)>UINTPTR_MAX-(uintptr_t)request||
    !context_output_valid(c(p),out,capacity)||
    !context_output_valid(c(p),written,sizeof(*written)))return EINVAL;
 struct context *ctx=c(p);struct vertex *v=ctx->bound_vertex;
 if(!v||v->deleted||remesa_shader_ir_gears_vertex(&v->program.semantic)||
    ctx->invalid_constants||ctx->constant_rejections[0])return ENOTSUP;
 const unsigned coefficients=v->program.semantic.instructions[0].bits[0]*4u;
 if(request->coefficient_dwords!=coefficients||request->transfer_dwords!=((coefficients+4u)&~3u))return ENOTSUP;
 if(!ctx->vertex_constant_size)return ENODATA;
 if(ctx->vertex_constant_size<coefficients*4u)return ENOSPC;
 if(!ctx->vertex_constants)
  return remesa_pds_structure_encode_parameter_source(request,ctx->vertex_constant_snapshot,
   coefficients*4u,out,capacity,written);
 struct resource *b=r(ctx->vertex_constants);struct screen *sc=s(p->screen);void *bytes=NULL;
 if(b->maps)return EBUSY;
 int error=resource_sync(p->screen,b,UINT64_MAX);if(error)return error;
 error=sc->device.ops->map(sc->device.priv,b->bo,false,&bytes);
 if(error){if(bytes)sc->device.ops->unmap(sc->device.priv,b->bo,false);return error;}
 error=bytes?remesa_pds_structure_encode_parameter_source(request,
  (unsigned char *)bytes+ctx->vertex_constant_offset,coefficients*4u,out,capacity,written):EPROTO;
 sc->device.ops->unmap(sc->device.priv,b->bo,false);return error;
#else
 (void)p;(void)request;(void)out;(void)capacity;(void)written;return ENOTSUP;
#endif
}

int remesa_context_prepare_vertex_parameters(struct pipe_context *p,uint32_t bank,void **out)
{
#ifdef REMESA_WITH_NIR
 if(!p||p->draw_vbo!=draw||!out||(uintptr_t)out%_Alignof(void *)||
    !context_state_output_valid(c(p),out,sizeof(*out)))return EINVAL;
 struct context *ctx=c(p);struct screen *sc=s(p->screen);
 if(sc->closing)return ESHUTDOWN;
 if(ctx->failed_submission)return EOWNERDEAD;
 if(!sc->device.resource_only_backend||sc->gears_runtime)return ENOTSUP;
 int e=remesa_persistent_shader_output_check(&sc->device,out,sizeof(*out));if(e)return e;
 if(*out)return EBUSY;
 uint32_t offset;e=remesa_use_base_offset(bank,&offset);if(e)return e;
 struct vertex *v=ctx->bound_vertex;
 if(!v||v->deleted||remesa_shader_ir_gears_vertex(&v->program.semantic))return ENOTSUP;
 const uint32_t count=v->program.semantic.instructions[0].bits[0]*4u,transfer=(count+4u)&~3u;
 const struct remesa_pds_parameter_source_request request={0x20000000,4096,0,count,transfer};
 uint8_t source[176];size_t written;
 e=remesa_context_vertex_parameter_source(p,&request,source,sizeof source,&written);if(e)return e;
 if(written!=transfer*4u||count>40)return EPROTO;
 uint32_t coefficients[40];
 for(unsigned i=0;i<count;i++)coefficients[i]=(uint32_t)source[4+4*i]|
  (uint32_t)source[5+4*i]<<8|(uint32_t)source[6+4*i]<<16|(uint32_t)source[7+4*i]<<24;
 return remesa_persistent_vertex_parameter_block_prepare(&sc->device,coefficients,count,transfer,bank,out);
#else
 (void)p;(void)bank;(void)out;return ENOTSUP;
#endif
}
int remesa_context_gears_geometry_snapshot(struct pipe_context *p,
 const struct pipe_draw_info *info,const struct pipe_draw_start_count_bias *draw_info,
 struct remesa_gears_geometry **out)
{
#ifdef REMESA_WITH_NIR
 if(!p||p->draw_vbo!=draw||!info||!draw_info||
    !context_output_valid(c(p),out,sizeof(*out)))return EINVAL;
 struct context *ctx=c(p);struct vertex *v=ctx->bound_vertex;struct elements *e=ctx->bound_elements;
 if(s(p->screen)->closing)return ESHUTDOWN;
 if(!v||v->deleted||remesa_shader_ir_gears_vertex(&v->program.semantic)||
    !e||e->deleted||ctx->invalid_elements)return ENOTSUP;
 unsigned attr=v->program.semantic.instructions[0].input_slot;
 if(attr>1||e->count!=1+attr)return ENOTSUP;
 struct pipe_vertex_buffer *vb[3]={&ctx->vb,&ctx->second_vb,&ctx->third_vb};
 const struct pipe_vertex_element *element[2]={&e->position,&e->normal};
 struct resource *resources[3]={0};void *bytes[3]={0};bool mapped[3]={0};
 unsigned resource_count=0,slot[2]={0},index_slot=0;
 int error=0;
 for(unsigned a=0;a<1+attr;a++) {
  unsigned b=element[a]->vertex_buffer_index;
  if(b>=ctx->vb_count||!vb[b]->buffer.resource)return ENODATA;
  struct resource *res=r(vb[b]->buffer.resource);unsigned j;
  for(j=0;j<resource_count&&resources[j]!=res;j++);
  if(j==resource_count)resources[resource_count++]=res;
  slot[a]=j;
 }
 if(info->index_size&&!info->has_user_indices) {
  struct pipe_resource *ib=info->index.resource;
  if(!ib||ib->screen!=p->screen||ib->target!=PIPE_BUFFER||!(ib->bind&PIPE_BIND_INDEX_BUFFER))return EINVAL;
  unsigned j;for(j=0;j<resource_count&&resources[j]!=r(ib);j++);
  if(j==resource_count)resources[resource_count++]=r(ib);
  index_slot=j;
 }
 /* Reject output aliases before inspecting its previous reference or mapping. */
 for(unsigned j=0;j<resource_count;j++) {
  uintptr_t x=(uintptr_t)out,y=(uintptr_t)resources[j];
  if(x<y+sizeof(*resources[j])&&y<x+sizeof(*out))return EINVAL;
  if(resources[j]->maps)return EBUSY;
 }
 if(*out)return EBUSY;
 for(unsigned j=0;j<resource_count;j++) {
  error=resource_sync(p->screen,resources[j],UINT64_MAX);if(error)goto done;
 }
 struct screen *sc=s(p->screen);
 for(unsigned j=0;j<resource_count;j++) {
  error=sc->device.ops->map(sc->device.priv,resources[j]->bo,false,&bytes[j]);
  mapped[j]=!error||bytes[j];if(error)goto done;
  if(!bytes[j]){error=EPROTO;goto done;}
 }
 struct remesa_gears_attribute attributes[2]={{0}};
 for(unsigned a=0;a<1+attr;a++) {
  const struct pipe_vertex_element *el=element[a];unsigned b=el->vertex_buffer_index;
  attributes[a]=(struct remesa_gears_attribute){bytes[slot[a]],resources[slot[a]]->bytes,
   (uint64_t)vb[b]->buffer_offset+el->src_offset,el->src_stride,el->src_format};
 }
 const void *indices=NULL;size_t index_bytes=0;
 if(info->index_size) {
  if(info->has_user_indices) {
   uint64_t n=((uint64_t)draw_info->start+draw_info->count-draw_info->count%3)*info->index_size;
   if(n>SIZE_MAX){error=EOVERFLOW;goto done;}
   indices=info->index.user;index_bytes=(size_t)n;
  } else {indices=bytes[index_slot];index_bytes=resources[index_slot]->bytes;}
 }
 error=remesa_gears_geometry_create(info,draw_info,attr,&attributes[0],attr?&attributes[1]:NULL,
                                   indices,index_bytes,out);
done:
 for(unsigned j=resource_count;j>0;j--)if(mapped[j-1])
  s(p->screen)->device.ops->unmap(s(p->screen)->device.priv,resources[j-1]->bo,false);
 return error;
#else
 (void)p;(void)info;(void)draw_info;(void)out;return ENOTSUP;
#endif
}
int remesa_context_linked_vertex_inputs_snapshot(struct pipe_context *p,
 const struct pipe_draw_info *info,const struct pipe_draw_start_count_bias *draw_info,
 struct remesa_linked_vertex_inputs **out)
{
#ifdef REMESA_WITH_NIR
 if(!p||p->draw_vbo!=draw||!info||!draw_info||
    !context_output_valid(c(p),out,sizeof(*out)))return EINVAL;
 uintptr_t at=(uintptr_t)out;
 if(sizeof(*info)>UINTPTR_MAX-(uintptr_t)info||sizeof(*draw_info)>UINTPTR_MAX-(uintptr_t)draw_info||
    (at<(uintptr_t)info+sizeof(*info)&&(uintptr_t)info<at+sizeof(*out))||
    (at<(uintptr_t)draw_info+sizeof(*draw_info)&&(uintptr_t)draw_info<at+sizeof(*out)))return EINVAL;
 struct context *ctx=c(p);struct remesa_shader_linkage link;
 int error=remesa_context_shader_linkage_snapshot(p,&link);if(error)return error;
 if(remesa_shader_ir_white_lambert(&link.vertex.semantic)||link.vertex_layout.input_count!=3||
    link.vertex_layout.stride_dwords!=12)return ENOTSUP;
 struct elements *el=ctx->bound_elements;
 if(!el||el->deleted||ctx->invalid_elements||el->count!=3)return ENOTSUP;
 if(info->start_instance||info->instance_count>1||
    (info->index_size&&info->primitive_restart))return ENOTSUP;
 if(info->mode!=MESA_PRIM_TRIANGLES&&info->mode!=MESA_PRIM_TRIANGLE_STRIP&&info->mode!=MESA_PRIM_TRIANGLE_FAN)return ENOTSUP;
 if(!info->instance_count)return ENODATA;
 struct remesa_linked_topology topology;
 error=remesa_linked_topology_prepare(info->mode,draw_info->count,&topology);if(error)return error;
 const uint32_t count=topology.input_count;
 struct fixed *raster=ctx->bound_fixed[FIXED_RASTER];
 if(info->mode!=MESA_PRIM_TRIANGLES&&(!raster||raster->deleted||ctx->invalid_fixed[FIXED_RASTER]))return ENODATA;
 if(info->index_size&&info->index_size!=1&&info->index_size!=2&&info->index_size!=4)return EINVAL;
 size_t user_index_bytes=0;
 if(info->index_size&&info->has_user_indices){
  uint64_t n=((uint64_t)draw_info->start+count)*info->index_size;
  uintptr_t base=(uintptr_t)info->index.user;
  if(n>SIZE_MAX)return EOVERFLOW;
  if(!base||n>UINTPTR_MAX-base||
     (at<base+n&&base<at+sizeof(*out)))return EINVAL;
  user_index_bytes=(size_t)n;
 }
 struct pipe_vertex_buffer *vb[3]={&ctx->vb,&ctx->second_vb,&ctx->third_vb};
 const struct pipe_vertex_element *element[3]={&el->position,&el->normal,&el->light};
 struct resource *resources[4]={0};void *bytes[4]={0};bool mapped[4]={0};
 unsigned resource_count=0,slot[3]={0},index_slot=0;
 for(unsigned a=0;a<3;a++){
  if(link.vertex_layout.input_slot[a]!=a||element[a]->src_format!=PIPE_FORMAT_R32G32B32A32_FLOAT)return ENOTSUP;
  unsigned b=element[a]->vertex_buffer_index;
  if(b>=3||b>=ctx->vb_count||!vb[b]->buffer.resource)return ENODATA;
  struct resource *res=r(vb[b]->buffer.resource);unsigned j;
  for(j=0;j<resource_count&&resources[j]!=res;j++);
  if(j==resource_count)resources[resource_count++]=res;
  slot[a]=j;
 }
 if(info->index_size&&!info->has_user_indices){
  struct pipe_resource *ib=info->index.resource;
  if(!ib||ib->screen!=p->screen||ib->target!=PIPE_BUFFER||!(ib->bind&PIPE_BIND_INDEX_BUFFER))return EINVAL;
  unsigned j;for(j=0;j<resource_count&&resources[j]!=r(ib);j++);
  if(j==resource_count)resources[resource_count++]=r(ib);
  index_slot=j;
 }
 for(unsigned j=0;j<resource_count;j++){
  uintptr_t res=(uintptr_t)resources[j];
  if(at<res+sizeof(*resources[j])&&res<at+sizeof(*out))return EINVAL;
  if(resources[j]->maps)return EBUSY;
 }
 if(*out)return EBUSY;
 for(unsigned j=0;j<resource_count;j++){
  error=resource_sync(p->screen,resources[j],UINT64_MAX);if(error)goto done;
 }
 for(unsigned j=0;j<resource_count;j++){
  error=s(p->screen)->device.ops->map(s(p->screen)->device.priv,resources[j]->bo,false,&bytes[j]);
  mapped[j]=!error||bytes[j];if(error)goto done;
  if(!bytes[j]){error=EPROTO;goto done;}
 }
 struct remesa_linked_attribute attributes[3];
 for(unsigned a=0;a<3;a++){
  unsigned b=element[a]->vertex_buffer_index;
  attributes[a]=(struct remesa_linked_attribute){bytes[slot[a]],resources[slot[a]]->bytes,
   (uint64_t)vb[b]->buffer_offset+element[a]->src_offset,element[a]->src_stride,element[a]->src_format};
 }
 const void *indices=NULL;size_t index_bytes=0;
 if(info->index_size){
  if(info->has_user_indices){
   indices=info->index.user;index_bytes=user_index_bytes;
  }else{indices=bytes[index_slot];index_bytes=resources[index_slot]->bytes;}
 }
 error=remesa_linked_vertex_inputs_create_topology(info,draw_info,attributes,indices,index_bytes,
                                                 raster&&raster->flat_first,out);
done:
 for(unsigned j=resource_count;j>0;j--)if(mapped[j-1])
  s(p->screen)->device.ops->unmap(s(p->screen)->device.priv,resources[j-1]->bo,false);
 return error;
#else
 (void)p;(void)info;(void)draw_info;(void)out;return ENOTSUP;
#endif
}

static int context_prepare_scene(struct pipe_context *p,
 const struct pipe_draw_info *info,const struct pipe_draw_start_count_bias *draw_info,
 const struct remesa_linked_scene_request *options,bool gears,struct remesa_scene_capture *out)
{
#ifdef REMESA_WITH_NIR
 if(!p||p->draw_vbo!=draw||!info||!draw_info||!options||!out||
    (uintptr_t)options%_Alignof(struct remesa_linked_scene_request)||
    (uintptr_t)out%_Alignof(struct remesa_scene_capture)||
    !context_output_valid(c(p),out,sizeof(*out)))return EINVAL;
 struct context *ctx=c(p);struct screen *sc=s(p->screen);
 if(gears&&(!context_state_output_valid(ctx,out,sizeof(*out))||
    !context_state_output_valid(ctx,options,sizeof(*options))||
    !context_state_output_valid(ctx,info,sizeof(*info))||
    !context_state_output_valid(ctx,draw_info,sizeof(*draw_info))))return EINVAL;
 if(!context_output_valid(ctx,options,sizeof(*options))||
    !context_output_valid(ctx,info,sizeof(*info))||!context_output_valid(ctx,draw_info,sizeof(*draw_info)))return EINVAL;
 uintptr_t at=(uintptr_t)out;
 if(sizeof(*info)>UINTPTR_MAX-(uintptr_t)info||sizeof(*draw_info)>UINTPTR_MAX-(uintptr_t)draw_info||
    (at<(uintptr_t)info+sizeof(*info)&&(uintptr_t)info<at+sizeof(*out))||
    (at<(uintptr_t)draw_info+sizeof(*draw_info)&&(uintptr_t)draw_info<at+sizeof(*out)))return EINVAL;
 if(info->index_size&&info->has_user_indices){
  if(info->index_size!=1&&info->index_size!=2&&info->index_size!=4)return EINVAL;
  uint64_t n=((uint64_t)draw_info->start+draw_info->count)*info->index_size;
  uintptr_t base=(uintptr_t)info->index.user;
  if(!base||n>UINTPTR_MAX-base||(at<base+n&&base<at+sizeof(*out)))return EINVAL;
 }
 if(sizeof(*options)>UINTPTR_MAX-(uintptr_t)options||
    (at<(uintptr_t)options+sizeof(*options)&&(uintptr_t)options<at+sizeof(*out)))return EINVAL;
 if(sc->closing)return ESHUTDOWN;
 if(!sc->linked_attachment_storage||!ctx->target||!ctx->depth_target)return ENOTSUP;
 const struct remesa_linked_scene_request requested=*options;
 const struct remesa_linked_geometry_input *g=&requested.geometry;
 if(requested.color||requested.depth||requested.width||requested.height||requested.color_pitch||requested.linkage||
    g->vertices||g->vertex_bytes||g->vertex_count||g->indices||g->index_bytes||g->index_count)return EINVAL;
 struct resource *color=r(ctx->target),*depth=r(ctx->depth_target);
 struct pipe_resource *output_owners[]={ctx->target,ctx->depth_target,ctx->vb.buffer.resource,
  ctx->second_vb.buffer.resource,ctx->third_vb.buffer.resource,
  info->index_size&&!info->has_user_indices?info->index.resource:NULL};
 for(unsigned i=0;i<sizeof(output_owners)/sizeof(output_owners[0]);i++)if(output_owners[i]){
  uintptr_t base=(uintptr_t)output_owners[i];
  if(sizeof(struct resource)>UINTPTR_MAX-base||
     (at<base+sizeof(struct resource)&&base<at+sizeof(*out)))return EINVAL;
 }
 if(color->maps||depth->maps)return EBUSY;
 if(color->has_contents||depth->has_contents)return ENOTSUP;
 if(out->owner||out->serial||out->slot||out->reserved)return EBUSY;
 int error=resource_sync(p->screen,color,UINT64_MAX);if(error)return error;
 error=resource_sync(p->screen,depth,UINT64_MAX);if(error)return error;
 struct remesa_shader_linkage link;error=remesa_context_shader_linkage_snapshot(p,&link);if(error)return error;
 if(gears){
  struct remesa_shader_selected_resources resources;
  error=remesa_shader_program_gears_attribute_resources(&link.vertex,&resources);if(error)return error;
  if(!sc->device.resource_only_backend||sc->gears_runtime)return ENOTSUP;
  struct remesa_gears_geometry *geometry=NULL;void *parameters=NULL;uint8_t *packed=NULL;
  error=remesa_context_gears_geometry_snapshot(p,info,draw_info,&geometry);if(error)return error;
  struct remesa_gears_geometry_view view;
  error=remesa_gears_geometry_inspect(geometry,&view);if(error)goto gears_done;
  if(view.normal_from_attribute!=1||view.stride_dwords!=8||!view.vertex_count||view.vertex_count>65535||
     view.bytes!=(size_t)view.vertex_count*32){error=ENOTSUP;goto gears_done;}
  packed=calloc(view.vertex_count,48);if(!packed){error=ENOMEM;goto gears_done;}
  /* The established snapshot already expands the selected index window.
   * Preserve P/N bits and initialize the unconsumed third float4. No vertex,
   * matrix, lighting or pixel arithmetic takes place on the CPU. */
  for(uint32_t i=0;i<view.vertex_count;i++)memcpy(packed+(size_t)i*48,(const uint8_t *)view.vertices+(size_t)i*32,32);
  error=remesa_context_prepare_vertex_parameters(p,requested.vertex_bank,&parameters);if(error)goto gears_done;
  struct remesa_linked_scene_request request=requested;
  request.color=color->bo;request.depth=depth->bo;request.width=ctx->target->width0;
  request.height=ctx->target->height0;request.color_pitch=color->stride/4;request.linkage=&link;
  request.geometry=(struct remesa_linked_geometry_input){.vertices=packed,.vertex_bytes=(size_t)view.vertex_count*48,.vertex_count=view.vertex_count};
  error=remesa_persistent_gears_attribute_scene_prepare(&sc->device,&request,parameters,out);
 gears_done:
  if(parameters)sc->device.ops->destroy_bo(sc->device.priv,parameters);
  free(packed);remesa_gears_geometry_destroy(geometry);return error;
 }
 struct remesa_linked_vertex_inputs *owned=NULL;
 error=remesa_context_linked_vertex_inputs_snapshot(p,info,draw_info,&owned);if(error)return error;
 struct remesa_linked_vertex_inputs_view view;
 error=remesa_linked_vertex_inputs_inspect(owned,&view);
 if(!error){
  struct remesa_linked_scene_request request=requested;
  request.color=color->bo;request.depth=depth->bo;request.width=ctx->target->width0;
  request.height=ctx->target->height0;request.color_pitch=color->stride/4;request.linkage=&link;
  request.geometry=(struct remesa_linked_geometry_input){.vertices=view.vertices,.vertex_bytes=view.vertex_bytes,
   .vertex_count=view.vertex_count,.indices=view.indices,.index_count=view.index_count,.index_bytes=view.index_bytes};
  error=sc->device.ops->prepare_linked_scene(&sc->device,&request,out);
 }
 remesa_linked_vertex_inputs_destroy(owned);return error;
#else
 (void)p;(void)info;(void)draw_info;(void)options;(void)gears;(void)out;return ENOTSUP;
#endif
}
int remesa_context_prepare_linked_scene(struct pipe_context *p,
 const struct pipe_draw_info *info,const struct pipe_draw_start_count_bias *draw_info,
 const struct remesa_linked_scene_request *options,struct remesa_scene_capture *out)
{return context_prepare_scene(p,info,draw_info,options,false,out);}
int remesa_context_prepare_gears_attribute_scene(struct pipe_context *p,
 const struct pipe_draw_info *info,const struct pipe_draw_start_count_bias *draw_info,
 const struct remesa_linked_scene_request *options,struct remesa_scene_capture *out)
{return context_prepare_scene(p,info,draw_info,options,true,out);}

int remesa_context_submit_linked_scene(struct pipe_context *p,
 const struct pipe_draw_info *info,const struct pipe_draw_start_count_bias *draw_info,
 const struct remesa_linked_scene_request *options,struct pipe_fence_handle **out)
{
 if(!p||p->draw_vbo!=draw||!info||!draw_info||!options||!out||
    (uintptr_t)out%_Alignof(void *)||!context_output_valid(c(p),out,sizeof(*out)))return EINVAL;
 struct context *ctx=c(p);struct screen *sc=s(p->screen);
 /* Validate output aliases before inspecting it or preparing any scene. */
 const void *inputs[]={info,draw_info,options,ctx->target,ctx->depth_target,
  ctx->vb.buffer.resource,ctx->second_vb.buffer.resource,ctx->third_vb.buffer.resource};
 const size_t sizes[]={sizeof(*info),sizeof(*draw_info),sizeof(*options),
  sizeof(struct resource),sizeof(struct resource),sizeof(struct resource),sizeof(struct resource),sizeof(struct resource)};
 uintptr_t at=(uintptr_t)out;
 for(unsigned i=0;i<sizeof(inputs)/sizeof(inputs[0]);i++)if(inputs[i]){
  uintptr_t base=(uintptr_t)inputs[i];
  if(sizes[i]>UINTPTR_MAX-base||(at<base+sizes[i]&&base<at+sizeof(*out)))return EINVAL;
 }
 if(!context_output_valid(ctx,info,sizeof(*info))||
    !context_output_valid(ctx,draw_info,sizeof(*draw_info))||
    !context_output_valid(ctx,options,sizeof(*options)))return EINVAL;
 if(info->index_size){
  if(info->index_size!=1&&info->index_size!=2&&info->index_size!=4)return EINVAL;
  uintptr_t base=(uintptr_t)(info->has_user_indices?info->index.user:(void *)info->index.resource);
  uint64_t bytes=info->has_user_indices?((uint64_t)draw_info->start+draw_info->count)*info->index_size:sizeof(struct resource);
  if(!base||bytes>UINTPTR_MAX-base||(at<base+bytes&&base<at+sizeof(*out)))return EINVAL;
 }
 if(*out)return EBUSY;
 if(sc->closing)return ESHUTDOWN;
 if(ctx->failed_submission)return EOWNERDEAD;
 if(!sc->linked_attachment_storage||!ctx->target||!ctx->depth_target||
    !sc->device.ops->submit_linked_scene||!sc->device.ops->release_linked_scene)return ENOTSUP;
 struct resource *color=r(ctx->target),*depth=r(ctx->depth_target);
 if(color==depth)return EINVAL;
 struct pipe_fence_handle *f=calloc(1,sizeof(*f));if(!f)return ENOMEM;
 struct remesa_scene_capture capture={0};
 int error=remesa_context_prepare_linked_scene(p,info,draw_info,options,&capture);
 if(error){
  if(error==EOWNERDEAD){
   /* Preparation can itself lose ownership of a provider allocation. An
    * older successful context fence must never hide that endpoint failure. */
   ctx->failed_submission=true;ctx->error=error;
   color->write_error=depth->write_error=error;
  }
  free(f);return error;
 }
 /* Reserve on local copies: neither attachment may change if the second
  * reservation fails or the backend conclusively reports NOT_ISSUED. */
 struct remesa_render_pass cp=color->pass,zp=depth->pass;
 struct remesa_pass_ticket ct=color->ticket,zt=depth->ticket;
 error=remesa_render_pass_begin(&cp,REMESA_PASS_DISCARD,REMESA_PASS_STORE,false,&ct);
 if(!error)error=remesa_render_pass_begin(&zp,REMESA_PASS_DISCARD,REMESA_PASS_STORE,false,&zt);
 if(!error)error=remesa_render_pass_submitted(&cp,&ct);
 if(!error)error=remesa_render_pass_submitted(&zp,&zt);
 void *backend=NULL;
 if(!error)error=sc->device.ops->submit_linked_scene(&sc->device,capture,&backend);
 bool uncertain=error==EOWNERDEAD||(error&&backend)||(!error&&!backend);
 if(error&&!uncertain){
  int release=sc->device.ops->release_linked_scene(&sc->device,capture);
  if(release){error=EOWNERDEAD;uncertain=true;}
  else {free(f);return error;}
 }
 if(uncertain){
  error=EOWNERDEAD;ctx->failed_submission=true;
  /* Even pre-issue ownership failure must prevent resource reuse. */
  color->pass=cp;depth->pass=zp;
  color->ticket=ct;depth->ticket=zt;
  color->write_error=depth->write_error=error;
  (void)remesa_render_pass_fail(&color->pass,&color->ticket,error);
  (void)remesa_render_pass_fail(&depth->pass,&depth->ticket,error);
  if(backend)sc->device.ops->destroy_fence(sc->device.priv,backend);
  free(f);ctx->error=error;return error;
 }
 color->pass=cp;depth->pass=zp;color->ticket=ct;depth->ticket=zt;
 color->has_contents=depth->has_contents=true;
 f->refs=1;f->screen=sc;f->backend=backend;screen_hold(sc);
 fence_ref(p->screen,&color->writer,f);fence_ref(p->screen,&depth->writer,f);
 fence_ref(p->screen,&ctx->last,f);*out=f;return 0;
}

static void stencil_ref(struct pipe_context *p,const struct pipe_stencil_ref reference)
{ (void)reference;c(p)->error=0; /* All admitted DSA states disable stencil. */ }
static void blend_color(struct pipe_context *p,const struct pipe_blend_color *color)
{
 if(!color){c(p)->error=EINVAL;return;}
 for(unsigned i=0;i<4;i++)if(!isfinite(color->color[i])){c(p)->error=EINVAL;return;}
 c(p)->error=0; /* All admitted blend states disable blending. */
}
/* Mesa uploads the initial polygon pattern even when stippling is disabled.
 * Every admitted rasterizer state rejects poly_stipple_enable, so the pattern
 * cannot affect an admitted draw. Keep the required lifecycle hook non-NULL;
 * this neither implements stippling nor advertises additional raster caps. */
static void polygon_stipple(struct pipe_context *p,const struct pipe_poly_stipple *pattern)
{ c(p)->error=pattern?0:EINVAL; }
static void sample_mask(struct pipe_context *p,unsigned mask)
{ c(p)->sample_enabled=(mask&1)!=0;c(p)->error=0; /* Single sample only. */ }
/* Pack the existing color-clear semantics, then opt-in native fill or default
 * explicit CPU transfer. Depth is not admitted by the native color-fill path. */
static int clear_resource(struct pipe_context *p,struct pipe_resource *target,
 const union pipe_color_union *color,double depth,unsigned x,unsigned y,unsigned width,unsigned height)
{
 struct context *ctx=c(p);struct screen *sc=s(p->screen);struct remesa_color_clear region;void *ptr=NULL;int error;
 if(sc->closing)return ESHUTDOWN;
 if(!target||target->screen!=p->screen||!color_storage_target(target->target)||
    !color)return ENOTSUP;
 if(target->format==PIPE_FORMAT_Z24X8_UNORM&&sc->cpu_depth24_lab)
  error=remesa_prepare_depth24_clear(target->format,target->width0,target->height0,r(target)->stride,
   r(target)->bytes,depth,x,y,width,height,&region);
 else error=remesa_prepare_color_clear(target->format,target->width0,target->height0,r(target)->stride,
   r(target)->bytes,color,x,y,width,height,&region);if(error)return error;
 if(!width||!height)return 0;
 if(sc->native_fill){
  if(target->format!=PIPE_FORMAT_B8G8R8A8_UNORM&&target->format!=PIPE_FORMAT_R8G8B8A8_UNORM)return ENOTSUP;
  return native_fill(p,target,0,x,y,width,height,region.pixel);
 }
 if(r(target)->maps)return EBUSY;
 error=resource_sync(p->screen,r(target),UINT64_MAX);if(error)return error;
 error=remesa_render_pass_admit_cpu_write(&r(target)->pass);if(error)return error;
 error=sc->device.ops->map(sc->device.priv,r(target)->bo,true,&ptr);
 if(error){if(ptr)sc->device.ops->unmap(sc->device.priv,r(target)->bo,true);return error;}
 if(!ptr){sc->device.ops->unmap(sc->device.priv,r(target)->bo,true);return EPROTO;}
 for(unsigned row=0;row<region.rows;row++)
  for(size_t byte=0;byte<region.row_bytes;byte+=4)
   memcpy((unsigned char *)ptr+region.offset+(size_t)row*region.stride+byte,region.pixel,4);
 sc->device.ops->unmap(sc->device.priv,r(target)->bo,true);
 error=remesa_render_pass_cpu_write(&r(target)->pass);if(error){r(target)->write_error=error;return error;}
 r(target)->has_contents=true;++ctx->cpu_clears;return 0;
}
static void clear(struct pipe_context *p,unsigned buffers,const struct pipe_scissor_state *scissor,
 const union pipe_color_union *color,double depth,unsigned stencil)
{
 (void)stencil;struct context *ctx=c(p);
 if(s(p->screen)->gears_runtime&&buffers){
  bool bad=(buffers&~(PIPE_CLEAR_COLOR0|PIPE_CLEAR_DEPTH))||ctx->gears_pending||ctx->gears_poisoned||!ctx->target||ctx->target->width0!=1280||ctx->target->height0!=800;
  if(scissor&&(scissor->minx||scissor->miny||scissor->maxx!=1280||scissor->maxy!=800))bad=true;
  if((buffers&PIPE_CLEAR_DEPTH)&&depth!=1.0)bad=true;
  if(buffers&PIPE_CLEAR_COLOR0){if(!color)bad=true;else for(unsigned i=0;i<4;i++)if(color->f[i]!=0.0f)bad=true;}
  if(bad){if(getenv("REMESA_GEARS_TRACE"))fprintf(stderr,"GEARS_CLEAR_REJECT buffers=%x pending=%u poisoned=%u size=%ux%u depth=%g color=%g,%g,%g,%g\n",buffers,ctx->gears_pending,ctx->gears_poisoned,ctx->target?ctx->target->width0:0,ctx->target?ctx->target->height0:0,depth,color?color->f[0]:0,color?color->f[1]:0,color?color->f[2]:0,color?color->f[3]:0);ctx->gears_poisoned=true;ctx->error=ENOTSUP;return;}
 }
 if(!buffers){ctx->error=0;return;}
 if(s(p->screen)->native_fill&&(buffers&PIPE_CLEAR_DEPTH)){ctx->error=ENOTSUP;return;}
 if((buffers&~(PIPE_CLEAR_COLOR0|PIPE_CLEAR_DEPTH))||
    ((buffers&PIPE_CLEAR_DEPTH)&&(!s(p->screen)->cpu_depth24_lab||!ctx->depth_target))||!ctx->target){ctx->error=ENOTSUP;return;}
 unsigned w=ctx->target->width0,h=ctx->target->height0;
 unsigned x=scissor?scissor->minx:0,y=scissor?scissor->miny:0;
 if(scissor&&(x>scissor->maxx||y>scissor->maxy||scissor->maxx>w||scissor->maxy>h)){ctx->error=EINVAL;return;}
 unsigned rw=scissor?scissor->maxx-x:w,rh=scissor?scissor->maxy-y:h;
 /* Validate both plans before either attachment is written. */
 struct remesa_color_clear plan;
 if(buffers&PIPE_CLEAR_DEPTH){ctx->error=remesa_prepare_depth24_clear(PIPE_FORMAT_Z24X8_UNORM,w,h,
  r(ctx->depth_target)->stride,r(ctx->depth_target)->bytes,depth,x,y,rw,rh,&plan);if(ctx->error)return;}
 if(buffers&PIPE_CLEAR_COLOR0){ctx->error=remesa_prepare_color_clear(ctx->target->format,w,h,
  r(ctx->target)->stride,r(ctx->target)->bytes,color,x,y,rw,rh,&plan);if(ctx->error)return;}
 ctx->error=0;
 if(buffers&PIPE_CLEAR_COLOR0)ctx->error=clear_resource(p,ctx->target,color,depth,x,y,rw,rh);
 if(!ctx->error&&(buffers&PIPE_CLEAR_DEPTH)){union pipe_color_union unused={0};
  ctx->error=clear_resource(p,ctx->depth_target,&unused,depth,x,y,rw,rh);} if(s(p->screen)->gears_runtime){
  if(ctx->error)ctx->gears_poisoned=true;
  else ctx->gears_clear_mask|=buffers;
 }
}
static void clear_target(struct pipe_context *p,struct pipe_surface *surface,
 const union pipe_color_union *color,unsigned x,unsigned y,unsigned width,unsigned height,bool conditional)
{
 if(conditional||!surface||surface->context!=p||surface->format!=PIPE_FORMAT_B8G8R8A8_UNORM||
    surface->level||surface->first_layer||surface->last_layer||surface->nr_samples>1) {
  c(p)->error=ENOTSUP;return;
 }
 c(p)->error=clear_resource(p,surface->texture,color,0,x,y,width,height);
}
/* Whole-frame gears preparation never calls the legacy CPU transform/clip
 * path. Only the existing pre-VS snapshot and typed uniform encoder run. */
#ifdef REMESA_WITH_NIR
static int gears_reject(struct context *ctx,unsigned line,int error)
{
 if(getenv("REMESA_GEARS_TRACE"))fprintf(stderr,"GEARS_DRIVER_REJECT line=%u error=%d clear=%x poisoned=%u viewport=%g,%g,%g/%g,%g,%g invalid=%u,%u,%u,%u fixed=%u,%u,%u\n",line,error,ctx->gears_clear_mask,ctx->gears_poisoned,ctx->viewport.scale[0],ctx->viewport.scale[1],ctx->viewport.scale[2],ctx->viewport.translate[0],ctx->viewport.translate[1],ctx->viewport.translate[2],ctx->invalid_viewport,ctx->invalid_scissor,ctx->invalid_constants,ctx->invalid_elements,ctx->invalid_fixed[0],ctx->invalid_fixed[1],ctx->invalid_fixed[2]);
 return error;
}
#endif
static int queue_gears(struct pipe_context *p,const struct pipe_draw_info *info,unsigned drawid,
 const struct pipe_draw_indirect_info *indirect,const struct pipe_draw_start_count_bias *draws,unsigned count)
{
#ifdef REMESA_WITH_NIR
 struct context *ctx=c(p);struct screen *sc=s(p->screen);int error=ENOTSUP;
 struct remesa_gears_geometry *geometry=NULL;struct remesa_gears_geometry_view view;
 unsigned char parameters[176];size_t written=0;
 if(ctx->gears_poisoned)return gears_reject(ctx,__LINE__,EIO);
 if(!info||!draws||count!=1||indirect||drawid||info->mode!=MESA_PRIM_TRIANGLES||info->instance_count!=1||info->start_instance)return gears_reject(ctx,__LINE__,ENOTSUP);
 if(!draws->count||!ctx->sample_enabled)return 0;
 if(!ctx->target||!ctx->depth_target||ctx->target->format!=PIPE_FORMAT_B8G8R8A8_UNORM||
    ctx->depth_target->format!=PIPE_FORMAT_Z24X8_UNORM||ctx->target->width0!=1280||ctx->target->height0!=800||
    ctx->depth_target->width0!=1280||ctx->depth_target->height0!=800||r(ctx->target)->maps||r(ctx->depth_target)->maps||
    r(ctx->target)->write_error||r(ctx->depth_target)->write_error||
    (ctx->gears_clear_mask&(PIPE_CLEAR_COLOR0|PIPE_CLEAR_DEPTH))!=(PIPE_CLEAR_COLOR0|PIPE_CLEAR_DEPTH))return gears_reject(ctx,__LINE__,ENOTSUP);
 if(ctx->invalid_viewport||ctx->invalid_scissor||ctx->invalid_constants||ctx->invalid_elements||ctx->constant_rejections[0]||ctx->constant_rejections[1])return gears_reject(ctx,__LINE__,ENOTSUP);
 for(unsigned i=0;i<FIXED_COUNT;i++)if(ctx->invalid_fixed[i]||!ctx->bound_fixed[i])return gears_reject(ctx,__LINE__,ENOTSUP);
 struct fixed *depth=ctx->bound_fixed[FIXED_DSA],*raster=ctx->bound_fixed[FIXED_RASTER];
 if(!depth->depth_enabled||!depth->depth_write||depth->depth_func!=PIPE_FUNC_LESS||raster->scissor||
    raster->cull_face!=PIPE_FACE_BACK||!raster->depth_clip_near||!raster->depth_clip_far)return gears_reject(ctx,__LINE__,ENOTSUP);
 if(!ctx->viewport_set||!remesa_viewport_swizzle_is_identity(&ctx->viewport)||
    ctx->viewport.scale[0]!=640.0f||ctx->viewport.scale[2]!=0.5f||
    ctx->viewport.translate[0]!=640.0f||ctx->viewport.translate[1]!=400.0f||ctx->viewport.translate[2]!=0.5f)return gears_reject(ctx,__LINE__,ENOTSUP);
 /* Mesa mirrors viewport Y and front-face together for bottom-origin
  * storage. Canonicalize only this paired convention, never vertices. */
 bool bottom_left=ctx->viewport.scale[1]==400.0f&&!raster->front_ccw;
 bool top_left=ctx->viewport.scale[1]==-400.0f&&raster->front_ccw;
 if((!bottom_left&&!top_left)||raster->bottom_edge_rule!=top_left)return gears_reject(ctx,__LINE__,ENOTSUP);
 if(ctx->gears_pending&&ctx->gears_bottom_left!=bottom_left)return gears_reject(ctx,__LINE__,ENOTSUP);
 if(!ctx->bound_vertex||!ctx->bound_fragment||ctx->bound_vertex->deleted||ctx->bound_fragment->deleted||
    remesa_shader_program_validate(&ctx->bound_vertex->program)||remesa_shader_program_validate(&ctx->bound_fragment->program)||
    remesa_shader_ir_gears_vertex(&ctx->bound_vertex->program.semantic))return gears_reject(ctx,__LINE__,ENOTSUP);
 bool flat=!remesa_shader_ir_fragment_flat_color_input(&ctx->bound_fragment->program.semantic);
 if((!flat&&remesa_shader_ir_fragment_color_input(&ctx->bound_fragment->program.semantic))||
    ctx->bound_fragment->program.semantic.instructions[0].input_slot!=1||
    (raster->flat&&!flat)||(flat&&raster->flat_first))return gears_reject(ctx,__LINE__,ENOTSUP);
 error=remesa_sampler_draw_admit(ctx->samplers);if(error)return error;
 error=remesa_texture_draw_admit(ctx->textures);if(error)return error;
 struct pipe_draw_start_count_bias complete=*draws;complete.count-=complete.count%3;
 if(!complete.count)return 0;
 error=remesa_context_gears_geometry_snapshot(p,info,&complete,&geometry);if(error)goto done;
 error=remesa_gears_geometry_inspect(geometry,&view);if(error)goto done;
 unsigned attr=view.normal_from_attribute;
 const struct remesa_pds_parameter_source_request source={0x20000000,131072,0x6400,attr?36u:40u,attr?40u:44u};
 error=remesa_context_vertex_parameter_source(p,&source,parameters,sizeof(parameters),&written);if(error)goto done;
 if(written!=source.transfer_dwords*4u){error=EPROTO;goto done;}
 if(ctx->gears_pending&&(ctx->gears_target!=ctx->target||ctx->gears_depth!=ctx->depth_target)){error=ENOTSUP;goto done;}
 if(!ctx->gears_pending){remesa_resource_reference(&ctx->gears_target,ctx->target);remesa_resource_reference(&ctx->gears_depth,ctx->depth_target);ctx->gears_pending=true;ctx->gears_bottom_left=bottom_left;ctx->gears_origin_valid=true;}
 const struct remesa_gears_runtime_draw request={.vertex=&ctx->bound_vertex->program,.fragment=&ctx->bound_fragment->program,
  .geometry=&view,.coefficients=parameters+4,.coefficient_bytes=source.coefficient_dwords*4u,
  .width=1280,.height=800,.stride=r(ctx->target)->stride,.format=ctx->target->format,
  .depth_target=r(ctx->depth_target)->bo,.depth_stride=r(ctx->depth_target)->stride,.depth_format=ctx->depth_target->format,
  .flat=flat,.front_ccw=true,.bottom_left_storage=bottom_left,.clamp_vertex_color=raster->clamp_vertex_color,.cull_face=raster->cull_face};
 error=sc->device.gears_runtime->queue(sc->device.priv,r(ctx->target)->bo,&request);
 done:remesa_gears_geometry_destroy(geometry);return error?gears_reject(ctx,__LINE__,error):0;
#else
 (void)p;(void)info;(void)drawid;(void)indirect;(void)draws;(void)count;return ENOTSUP;
#endif
}
static void draw(struct pipe_context *p,const struct pipe_draw_info *info,unsigned drawid,
 const struct pipe_draw_indirect_info *indirect,const struct pipe_draw_start_count_bias *draws,unsigned count)
{
 struct context *ctx=c(p);struct screen *sc=s(p->screen);
 struct pipe_draw_info validated_info;struct pipe_resource *owned_index=NULL;
 struct pipe_draw_start_count_bias complete_draw;
 struct remesa_shader_draw_inputs inputs={0};
 void *ip=NULL,*vp=NULL,*backend=NULL;struct remesa_draw_packet *packet=NULL;
 void *shader_programs=NULL;
#ifdef REMESA_WITH_NIR
 struct remesa_shader_draw_programs *shader_snapshot=NULL;
#endif
 enum remesa_draw_status packed;int err=ENOTSUP;uint32_t draw_argb=ctx->argb;bool submitted=false,v_mapped=false,i_mapped=false;struct pipe_fence_handle *f=NULL;
 if(!info) goto done;
 /* Nonindexed draws do not initialize the index union. */
 if(info->index_size && info->take_index_buffer_ownership && !info->has_user_indices)
  owned_index=info->index.resource;
 if(sc->closing) { err=ESHUTDOWN;goto done; }
 if(sc->gears_runtime){err=queue_gears(p,info,drawid,indirect,draws,count);if(err)ctx->gears_poisoned=true;goto done;}
 if(sc->device.resource_only_backend||ctx->depth_target) goto done; /* CPU-only depth attachment never admits a draw. */
 if(!draws || count!=1 || indirect || drawid) goto done;
 /* Triangle-list assembly ignores an incomplete final primitive. Do this
  * before buffer mapping or range checks: its unused vertices/indices need
  * not be present in the supplied resources. Other modes remain unsupported. */
 if(info->mode==MESA_PRIM_TRIANGLES){complete_draw=*draws;complete_draw.count-=complete_draw.count%3;draws=&complete_draw;}
 if(!draws->count || !info->instance_count || !ctx->sample_enabled) { err=0;goto done; }
#ifdef REMESA_WITH_NIR
 /* A recognized flat FS cannot inherit a smooth legacy raster contract. */
 if(ctx->bound_fragment&&!remesa_shader_ir_fragment_flat_color_input(&ctx->bound_fragment->program.semantic))goto done;
#endif
 /* Expanded host bindings never widen the historical GPU draw contract. */
 if(ctx->vb_count!=1||!ctx->bound_elements||ctx->bound_elements->count!=1)goto done;
 if(ctx->vb.buffer.resource&&!(ctx->vb.buffer.resource->bind&PIPE_BIND_VERTEX_BUFFER))goto done;
 int sampling=remesa_sampler_draw_admit(ctx->samplers);if(sampling){err=sampling;goto done;}
 sampling=remesa_texture_draw_admit(ctx->textures);if(sampling){err=sampling;goto done;}
 if(ctx->invalid_viewport || ctx->invalid_scissor || ctx->invalid_elements || ctx->invalid_constants || ctx->constant_rejections[0] || ctx->constant_rejections[1]) goto done;
 for(unsigned kind=0;kind<FIXED_COUNT;kind++)
  if(ctx->invalid_fixed[kind]||(ctx->vertex_required&&!ctx->bound_fixed[kind]))goto done;
 struct fixed *raster=ctx->bound_fixed[FIXED_RASTER];
 if(raster&&((raster->scissor&&!ctx->scissor_set)||(raster->depth_clip&&!ctx->bound_vertex)))goto done;
 if(ctx->vertex_required) {
  bool host=sc->device.offline_test_backend&&!sc->device.qualified_hardware_backend;
  if(!ctx->bound_vertex||!ctx->viewport_set||
     (ctx->viewport.scale[0]!=16&&(!host||ctx->viewport.scale[0]!=-16))||
     (ctx->viewport.scale[1]!=16&&(!host||ctx->viewport.scale[1]!=-16))||
     ctx->viewport.scale[2]!=.5f||ctx->viewport.translate[0]!=16||ctx->viewport.translate[1]!=16||ctx->viewport.translate[2]!=.5f)goto done;
 } else if(ctx->viewport_set) {
  for(unsigned axis=0;axis<3;axis++)if(ctx->viewport.scale[axis]!=1||ctx->viewport.translate[axis]!=0)goto done;
 }
 if((info->index_size && info->has_user_indices) || !ctx->target || !ctx->pipeline_bound ||
    !ctx->position_bound || !ctx->vb.buffer.resource ||
    (info->index_size && (!info->index.resource || info->index.resource->screen!=p->screen||
     info->index.resource->target!=PIPE_BUFFER||!(info->index.resource->bind&PIPE_BIND_INDEX_BUFFER)))) goto done;
#ifdef REMESA_WITH_NIR
 /* Validate actual owned host programs together before synchronization or
  * mapping. Linkage metadata never admits hardware encoding or execution. */
 if(ctx->bound_vertex) {
  int shader_error=remesa_shader_program_validate(&ctx->bound_vertex->program);
  if(shader_error){err=shader_error;goto done;}
  if(ctx->bound_fragment&&!ctx->bound_fragment->uses_uniform) {
   struct remesa_shader_pipeline pair;
   shader_error=remesa_context_shader_pipeline_report(p,&pair);
   if(shader_error){err=shader_error;goto done;}
   if(pair.position_input_slot!=ctx->bound_vertex->input_slot||pair.fragment_argb!=draw_argb)
    {err=EPROTO;goto done;}
  }
 }
 bool original_shader_draw=ctx->vertex_required||ctx->bound_fragment;
 /* Reject a missing original-program backend before uniform BO reads too. */
 if(original_shader_draw&&!sc->device.ops->prepare_shader_programs)goto done;
 int color_error=fragment_color(p,&draw_argb);if(color_error){err=color_error;goto done;}
 if(original_shader_draw) {
  if(!ctx->bound_vertex||!ctx->bound_fragment){err=ENODATA;goto done;}
  struct remesa_shader_program uniform_program;
  const struct remesa_shader_program *fragment_program=&ctx->bound_fragment->program;
  if(ctx->bound_fragment->uses_uniform) {
   err=remesa_shader_program_prepare_color(draw_argb,&uniform_program);if(err)goto done;
   fragment_program=&uniform_program;
  }
  shader_snapshot=malloc(sizeof(*shader_snapshot));if(!shader_snapshot){err=ENOMEM;goto done;}
  err=remesa_shader_draw_programs_prepare(&ctx->bound_vertex->program,
       fragment_program,shader_snapshot);if(err)goto done;
  if(shader_snapshot->pipeline.fragment_argb!=draw_argb){err=EPROTO;goto done;}
  err=ENOTSUP;
 }
#else
 const bool original_shader_draw=false;
#endif
 /* The retained native executor still checks its exact diagnostic profile.
  * Original shader preparation must independently qualify its own programs;
  * semantic color equality never selects or admits a retained program. */
 if(r(ctx->target)->maps || r(ctx->vb.buffer.resource)->maps ||
    (info->index_size && r(info->index.resource)->maps)) { err=EBUSY;goto done; }
 /* A retained handle is not proof of preserved pixels. The current native
  * profile initializes COLOR to zero and replaces the whole render resource.
  * Fail before dispatch rather than erase a clear/prior ordinary draw. */
 if(r(ctx->target)->has_contents&&!ctx->diagnostic_discard&&
    sc->device.framebuffer_mode!=REMESA_FRAMEBUFFER_PRESERVE)goto done;
 /* NIR draws never fall through to the retained fixed-shader submit path.
  * A real backend owns encoding, upload and PDS/USSE admission. Preparation
  * failures return before framebuffer reservation or any draw dispatch. */
#ifdef REMESA_WITH_NIR
 if(original_shader_draw&&!(raster&&raster->recording_edge_proof)) {
  err=sc->device.ops->prepare_shader_programs(sc->device.priv,shader_snapshot,&shader_programs);
  if(err)goto done;
  if(!shader_programs){err=EPROTO;goto done;}
 }
#endif
 err=resource_sync(p->screen,r(ctx->target),UINT64_MAX);if(err) goto done;
 err=resource_sync(p->screen,r(ctx->vb.buffer.resource),UINT64_MAX);if(err)goto done;
 if(info->index_size) {err=resource_sync(p->screen,r(info->index.resource),UINT64_MAX);if(err)goto done;}
 if(info->index_size) {
  err=sc->device.ops->map(sc->device.priv,r(info->index.resource)->bo,false,&ip);if(err) goto done;
  i_mapped=true;if(!ip){err=EPROTO;goto done;}
 }
 validated_info=*info;validated_info.take_index_buffer_ownership=false;
 err=sc->device.ops->map(sc->device.priv,r(ctx->vb.buffer.resource)->bo,false,&vp);if(err) goto done;
 v_mapped=true;if(!vp){err=EPROTO;goto done;}
 packet=malloc(sizeof(*packet));f=calloc(1,sizeof(*f));if(!packet || !f) { err=ENOMEM;goto done; }
 /* CPU primitive clipping is connected to the real callback only for the
  * recording backend and explicit depth-clip state. This does not qualify
  * GPU edge rules or expand the live executor's historical profile. */
 if(sc->device.offline_test_backend && !sc->device.qualified_hardware_backend &&
    ctx->bound_vertex && raster && raster->full_clip) {
  struct remesa_prepared_triangles *prepared=malloc(sizeof(*prepared));
  if(!prepared){err=ENOMEM;goto done;}
  packed=remesa_prepare_constant_draw(&validated_info,draws,&ctx->vb,&ctx->position,
      vp,r(ctx->vb.buffer.resource)->bytes,ip,info->index_size?r(info->index.resource)->bytes:0,
      draw_argb,&ctx->viewport,ctx->target->width0,ctx->target->height0,prepared);
  if(packed==REMESA_DRAW_VALIDATED&&raster->cull_face) {
   unsigned kept=0;
   for(unsigned vertex=0;vertex<prepared->vertex_count;vertex+=3) {
    bool discard=false;
    float triangle[3][4];memcpy(triangle,&prepared->positions[vertex],sizeof(triangle));
    int facing=remesa_cull_window_triangle((const float (*)[4])triangle,
      raster->cull_face,raster->front_ccw,&discard);
    if(facing){free(prepared);err=facing;goto done;}
    if(!discard){memmove(&prepared->positions[kept],&prepared->positions[vertex],3*sizeof(prepared->positions[0]));kept+=3;}
   }
   prepared->vertex_count=kept;
   if(!kept)packed=REMESA_DRAW_EMPTY;
  }
  if(packed==REMESA_DRAW_VALIDATED)packed=remesa_pack_prepared_triangles(prepared,packet);
  free(prepared);
  if(packed==REMESA_DRAW_EMPTY){err=0;goto done;}
 } else {
 packed=remesa_pack_triangle_draw_vertex(&validated_info,draws,&ctx->vb,&ctx->position,
     vp,r(ctx->vb.buffer.resource)->bytes,ip,info->index_size ? r(info->index.resource)->bytes : 0,draw_argb,ctx->bound_vertex ? &ctx->viewport : NULL,packet);
 }
 if(packed!=REMESA_DRAW_VALIDATED) { err=packed==REMESA_DRAW_NOMEM ? ENOMEM :
     packed==REMESA_DRAW_UNSUPPORTED ? ENOTSUP : EINVAL;goto done; }
 if(raster&&raster->recording_edge_proof) {
  /* Never enter this model on a real backend, regardless of raster handle. */
  if(!sc->device.offline_test_backend||sc->device.qualified_hardware_backend){err=ENOTSUP;goto done;}
  for(unsigned i=0;i<packet->index_count;i+=3) {
   float triangle[3][4];
   for(unsigned v=0;v<3;v++)memcpy(triangle[v],packet->positions[packet->indices[i+v]],sizeof(triangle[v]));
   err=remesa_prove_half_pixel_edge_independence((const float (*)[4])triangle,packet->width,packet->height);
   if(err)goto done;
  }
#ifdef REMESA_WITH_NIR
  if(original_shader_draw) {
   err=sc->device.ops->prepare_shader_programs(sc->device.priv,shader_snapshot,&shader_programs);
   if(err)goto done;
   if(!shader_programs){err=EPROTO;goto done;}
  }
#endif
 }
 enum remesa_pass_load load=r(ctx->target)->has_contents&&!ctx->diagnostic_discard ? REMESA_PASS_PRESERVE : REMESA_PASS_DISCARD;
 if(original_shader_draw) {
  /* Preserve the actual pre-VS data and fixed state. Passing just packet's
   * projected positions would apply the viewport twice or silently replace
   * the vertex shader with CPU execution. All pointers expire on unmap. */
  inputs=(struct remesa_shader_draw_inputs){
   .vertex_data=vp,.index_data=info->index_size?ip:NULL,
   .vertex_bytes=ctx->vb.buffer.resource->width0,
   .index_bytes=info->index_size?info->index.resource->width0:0,
   .color_bytes=r(ctx->target)->bytes,
   .vertex_offset=ctx->vb.buffer_offset,.element_offset=ctx->position.src_offset,
   .vertex_stride=ctx->position.src_stride,.position_input_slot=ctx->bound_vertex->input_slot,
   .start=draws->start,.count=draws->count,.index_size=info->index_size,
   .index_bias=info->index_size?draws->index_bias:0,
   .position_format=ctx->position.src_format,.color_format=ctx->target->format,
   .color_width=ctx->target->width0,.color_height=ctx->target->height0,
   .color_stride=r(ctx->target)->stride,.viewport=ctx->viewport,
   .cull_face=raster?raster->cull_face:0,.front_ccw=raster&&raster->front_ccw,
   .depth_clip_near=raster&&raster->depth_clip_near,.depth_clip_far=raster&&raster->depth_clip_far,
   .scissor_enabled=raster&&raster->scissor,.preserve_color=load==REMESA_PASS_PRESERVE,
   .retained_fragment_carrier=sc->device.retained_fragment_carrier
  };
  if(inputs.scissor_enabled)inputs.scissor=(struct pipe_scissor_state){.maxx=32,.maxy=32};
  struct remesa_draw_plan raw_plan;
  err=remesa_shader_draw_inputs_validate(&inputs,&raw_plan);if(err)goto done;
 }
 err=remesa_render_pass_begin(&r(ctx->target)->pass,load,REMESA_PASS_STORE,
    sc->device.framebuffer_mode==REMESA_FRAMEBUFFER_PRESERVE,&r(ctx->target)->ticket);if(err)goto done;
 err=remesa_render_pass_submitted(&r(ctx->target)->pass,&r(ctx->target)->ticket);if(err)goto done;
 submitted=true;
 if(original_shader_draw) {
  err=sc->device.ops->submit_shader_programs(sc->device.priv,r(ctx->target)->bo,
                                          packet,&inputs,shader_programs,&backend);
 }
 else err=sc->device.ops->submit(sc->device.priv,r(ctx->target)->bo,packet,&backend);
 if(!err && !backend) err=EPROTO;
 if(!err) {
  r(ctx->target)->has_contents=true;
  f->refs=1;f->screen=sc;f->backend=backend;screen_hold(sc);
  fence_ref(p->screen,&r(ctx->target)->writer,f);
  fence_ref(p->screen,&ctx->last,f);fence_ref(p->screen,&f,NULL);
 }
 done:
 if(v_mapped||vp) sc->device.ops->unmap(sc->device.priv,r(ctx->vb.buffer.resource)->bo,false);
 if(i_mapped||ip) sc->device.ops->unmap(sc->device.priv,r(info->index.resource)->bo,false);
 /* A backend-returned fence is owned even on failure. Its destructor must
  * retain uncertain device backing; frontend cleanup cannot imply retirement. */
 if(err && submitted) {
  /* A returned error fence may represent uncertain issuance. Never let a CPU
   * overwrite or another draw turn it into an apparently valid resource. */
  ctx->failed_submission=true;
  r(ctx->target)->write_error=err;
  remesa_render_pass_fail(&r(ctx->target)->pass,&r(ctx->target)->ticket,err);
  if(backend) sc->device.ops->destroy_fence(sc->device.priv,backend);
 }
 remesa_resource_reference(&owned_index,NULL);
 if(shader_programs)sc->device.ops->destroy_shader_programs(sc->device.priv,shader_programs);
#ifdef REMESA_WITH_NIR
 free(shader_snapshot);
#endif
 free(packet);free(f);ctx->error=err;
 if(err) {
  if(ctx->draw_rejections.failures!=UINT64_MAX)ctx->draw_rejections.failures++;
  ctx->draw_rejections.error=err;
 }
}
static int flush_gears(struct pipe_context *p)
{
 struct context *ctx=c(p);struct screen *sc=s(p->screen);void *backend=NULL;int error=0;
 if(!ctx->gears_pending)return ctx->gears_poisoned?EIO:0;
 if(ctx->gears_poisoned)return EIO;
 struct resource *color=r(ctx->gears_target),*depth=r(ctx->gears_depth);
 struct pipe_fence_handle *f=calloc(1,sizeof(*f));if(!f)return ENOMEM;
 error=resource_sync(p->screen,color,UINT64_MAX);if(!error)error=resource_sync(p->screen,depth,UINT64_MAX);
 if(!error)error=remesa_render_pass_begin(&color->pass,REMESA_PASS_DISCARD,REMESA_PASS_STORE,false,&color->ticket);
 if(!error)error=remesa_render_pass_begin(&depth->pass,REMESA_PASS_DISCARD,REMESA_PASS_STORE,false,&depth->ticket);
 if(!error)error=remesa_render_pass_submitted(&color->pass,&color->ticket);
 if(!error)error=remesa_render_pass_submitted(&depth->pass,&depth->ticket);
 if(!error)error=sc->device.gears_runtime->flush(sc->device.priv,color->bo,&backend);
 if(!error&&!backend)error=EPROTO;
 if(error){
  ctx->failed_submission=ctx->gears_poisoned=true;color->write_error=depth->write_error=error;
  remesa_render_pass_fail(&color->pass,&color->ticket,error);remesa_render_pass_fail(&depth->pass,&depth->ticket,error);
  if(backend)sc->device.ops->destroy_fence(sc->device.priv,backend);
  free(f);return error;
 }
 f->refs=1;f->screen=sc;f->backend=backend;screen_hold(sc);
 color->has_contents=depth->has_contents=true;
 fence_ref(p->screen,&color->writer,f);fence_ref(p->screen,&depth->writer,f);fence_ref(p->screen,&ctx->last,f);fence_ref(p->screen,&f,NULL);
 ctx->gears_pending=false;ctx->gears_clear_mask=0;
 remesa_resource_reference(&ctx->gears_target,NULL);remesa_resource_reference(&ctx->gears_depth,NULL);
 return 0;
}
static void flush(struct pipe_context *p,struct pipe_fence_handle **f,unsigned flags)
{
 (void)flags;
 if(s(p->screen)->gears_runtime){int error=flush_gears(p);if(error)c(p)->error=error;}
 /* A rejected, unissued draw does not cancel earlier work. Preserve its
  * real fence even when validation or a resource poll reports an error.
  * Conversely an uncertain failed submission must never be represented by
  * an older successful fence. That condition remains sticky for this context. */
 if(f)fence_ref(p->screen,f,c(p)->failed_submission ? NULL : c(p)->last);
}
static void destroy_context(struct pipe_context *p)
{ struct context *ctx=c(p);struct screen *sc=s(p->screen);
 if(ctx->gears_pending){
  int error=sc->device.gears_runtime->cancel(sc->device.priv,r(ctx->gears_target)->bo);
  if(error){ctx->error=error;ctx->failed_submission=true;return;}
  ctx->gears_pending=false;
 }
 remesa_resource_reference(&ctx->gears_target,NULL);remesa_resource_reference(&ctx->gears_depth,NULL);
#ifdef REMESA_WITH_NIR
 /* Release upload-owned map leases and references while the context callbacks
  * and screen owner are still live, including partial construction failure. */
 if(p->const_uploader)u_upload_destroy(p->const_uploader);
 if(p->stream_uploader)u_upload_destroy(p->stream_uploader);
#endif
 while(ctx->transfers)unmap(p,&ctx->transfers->base);
 remesa_resource_reference(&ctx->vb.buffer.resource,NULL);
 remesa_resource_reference(&ctx->second_vb.buffer.resource,NULL);
 remesa_resource_reference(&ctx->third_vb.buffer.resource,NULL);
 remesa_resource_reference(&ctx->target,NULL);
 remesa_resource_reference(&ctx->depth_target,NULL);
 remesa_resource_reference(&ctx->fragment_constants,NULL);
 remesa_resource_reference(&ctx->vertex_constants,NULL);free(ctx->vertex_constant_snapshot);
 free(ctx->constant_snapshot);fence_ref(p->screen,&ctx->last,NULL);
 while(ctx->fragments){struct fragment *f=ctx->fragments;ctx->fragments=f->next;free(f);}
 while(ctx->vertices){struct vertex *v=ctx->vertices;ctx->vertices=v->next;free(v);}
 while(ctx->fixed_states){struct fixed *state=ctx->fixed_states;ctx->fixed_states=state->next;free(state);}
 while(ctx->elements){struct elements *e=ctx->elements;ctx->elements=e->next;free(e);}
 remesa_sampler_bank_destroy(ctx->samplers);remesa_texture_binding_close(ctx->textures);
 free(ctx);screen_drop(sc);
}
static struct pipe_context *create_context(struct pipe_screen *p,void *priv,unsigned flags)
{
 /* PREFER_THREADED is a hint; NO_LOD_BIAS adds no unsupported sampler state.
  * Other semantic/context-security flags remain unavailable. */
 struct context *ctx;if((flags & ~(PIPE_CONTEXT_PREFER_THREADED|PIPE_CONTEXT_NO_LOD_BIAS)) || s(p)->closing) return NULL;ctx=calloc(1,sizeof(*ctx));if(!ctx) return NULL;
 ctx->samplers=remesa_sampler_bank_create(NULL,NULL,NULL);if(!ctx->samplers){free(ctx);return NULL;}
 ctx->textures=remesa_texture_binding_create(p,texture_storage,p,NULL,NULL,NULL);
 if(!ctx->textures){remesa_sampler_bank_destroy(ctx->samplers);free(ctx);return NULL;}
 ctx->sample_enabled=true;ctx->base.screen=p;ctx->base.priv=priv;ctx->base.destroy=destroy_context;
 ctx->base.buffer_map=map;ctx->base.texture_map=map;ctx->base.buffer_unmap=unmap;ctx->base.texture_unmap=unmap;
 ctx->base.transfer_flush_region=flush_region;
 ctx->base.buffer_subdata=buffer_subdata;ctx->base.texture_subdata=texture_subdata;
 ctx->base.resource_copy_region=copy_region;
 ctx->base.invalidate_resource=invalidate_resource;
 ctx->base.clear_texture=clear_texture;
 ctx->base.set_vertex_buffers=set_vb;ctx->base.create_vertex_elements_state=create_elements;
 ctx->base.bind_vertex_elements_state=bind_elements;ctx->base.delete_vertex_elements_state=delete_elements;
 ctx->base.create_surface=create_surface;ctx->base.surface_destroy=surface_destroy;
 ctx->base.create_fs_state=unsupported_shader;ctx->base.create_vs_state=unsupported_shader;ctx->base.clear=clear;ctx->base.clear_render_target=clear_target;
#ifdef REMESA_WITH_NIR
 ctx->base.create_vs_state=create_vertex;ctx->base.bind_vs_state=bind_vertex;ctx->base.delete_vs_state=delete_vertex;
 ctx->base.create_fs_state=create_fragment;ctx->base.bind_fs_state=bind_fragment;ctx->base.delete_fs_state=delete_fragment;
#endif
 ctx->base.create_blend_state=create_blend;ctx->base.bind_blend_state=bind_blend;ctx->base.delete_blend_state=delete_blend;
 ctx->base.create_depth_stencil_alpha_state=create_dsa;ctx->base.bind_depth_stencil_alpha_state=bind_dsa;ctx->base.delete_depth_stencil_alpha_state=delete_dsa;
 ctx->base.create_rasterizer_state=create_raster;ctx->base.bind_rasterizer_state=bind_raster;ctx->base.delete_rasterizer_state=delete_raster;
 ctx->base.set_stencil_ref=stencil_ref;ctx->base.set_blend_color=blend_color;
 ctx->base.set_constant_buffer=constant_buffer;ctx->base.set_sample_mask=sample_mask;
 ctx->base.set_polygon_stipple=polygon_stipple;
 ctx->base.create_sampler_state=create_sampler;ctx->base.bind_sampler_states=bind_samplers;ctx->base.delete_sampler_state=delete_sampler;
 ctx->base.create_sampler_view=create_sampler_view;ctx->base.sampler_view_destroy=destroy_sampler_view;ctx->base.sampler_view_release=release_sampler_view;ctx->base.set_sampler_views=set_sampler_views;
 ctx->base.set_viewport_states=viewport_state;ctx->base.set_scissor_states=scissor_state;
 ctx->base.set_framebuffer_state=framebuffer;ctx->base.draw_vbo=draw;ctx->base.flush=flush;screen_hold(s(p));
#ifdef REMESA_WITH_NIR
 /* Standard Mesa-owned uploads for the production NIR context. Keep stream
  * and constant backing separate; no persistent mappings or GPU capability
  * is implied. Start at one page, sufficient for the restricted draw profile. */
 ctx->base.stream_uploader=u_upload_create(&ctx->base,4096,
  PIPE_BIND_VERTEX_BUFFER|PIPE_BIND_INDEX_BUFFER,PIPE_USAGE_STREAM,0);
 if(!ctx->base.stream_uploader){destroy_context(&ctx->base);return NULL;}
 ctx->base.const_uploader=u_upload_create(&ctx->base,4096,
  PIPE_BIND_CONSTANT_BUFFER,PIPE_USAGE_STREAM,0);
 if(!ctx->base.const_uploader){destroy_context(&ctx->base);return NULL;}
#endif
 return &ctx->base;
}
static void destroy_screen(struct pipe_screen *p)
{ if(!s(p)->closing) { s(p)->closing=true;screen_drop(s(p)); } }
struct pipe_screen *remesa_screen_create_owned(struct remesa_device *device,int (*release)(struct remesa_device *))
{
 struct screen *sc;
 if(!device || (device->framebuffer_mode!=REMESA_FRAMEBUFFER_REPLACE&&device->framebuffer_mode!=REMESA_FRAMEBUFFER_PRESERVE) ||
    (device->framebuffer_mode==REMESA_FRAMEBUFFER_PRESERVE&&(!device->offline_test_backend||device->qualified_hardware_backend)) || (!device->offline_test_backend && !device->qualified_hardware_backend && !device->resource_only_backend) || !device->ops ||
    !device->ops->allocate || !device->ops->destroy_bo || !device->ops->map || !device->ops->unmap ||
    !device->ops->submit || !device->ops->wait || !device->ops->destroy_fence) return NULL;
 if((device->ops->prepare_shader_programs||device->ops->submit_shader_programs||device->ops->destroy_shader_programs)&&
    (!device->ops->prepare_shader_programs||!device->ops->submit_shader_programs||!device->ops->destroy_shader_programs))return NULL;
 sc=calloc(1,sizeof(*sc));if(!sc) return NULL;sc->device=*device;sc->refs=1;sc->release_device=release;
 sc->base.destroy=destroy_screen;sc->base.get_name=name;sc->base.get_vendor=vendor;sc->base.context_create=create_context;
 set_capabilities(sc);
 #ifdef REMESA_WITH_NIR
 sc->base.get_compiler_options=compiler_options;
#endif
 sc->base.is_format_supported=format;sc->base.resource_create=create_resource;sc->base.resource_destroy=destroy_resource;
 sc->base.fence_reference=fence_ref;sc->base.fence_finish=finish;
 if(release) memset(device,0,sizeof(*device));
 return &sc->base;
}
int remesa_screen_collect(struct pipe_screen *p)
{
 struct screen *sc;
 if(!p || p->destroy!=destroy_screen) return EINVAL;
 sc=s(p);if(!sc->closing || !sc->release_pending || __atomic_load_n(&sc->refs,__ATOMIC_ACQUIRE)!=1) return EBUSY;
 if(sc->release_error!=EBUSY && sc->release_error!=EAGAIN) return sc->release_error;
 /* Only backend collection/release may retry. No draw can be redispatched. */
 int error=sc->release_device(&sc->device);
 if(error) { sc->release_error=error;return error; }
 free(sc);return 0;
}
struct pipe_screen *remesa_screen_create(struct remesa_device *device)
{ return remesa_screen_create_owned(device,NULL); }
int remesa_context_gears_bottom_left(struct pipe_context *p,bool *bottom_left)
{
 if(!p||p->draw_vbo!=draw||!bottom_left||!context_output_valid(c(p),bottom_left,sizeof(*bottom_left)))return EINVAL;
 if(!s(p->screen)->gears_runtime||!c(p)->gears_origin_valid||c(p)->gears_poisoned)return ENOTSUP;
 *bottom_left=c(p)->gears_bottom_left;return 0;
}
int remesa_screen_enable_gears_runtime(struct pipe_screen *p)
{
#ifdef REMESA_WITH_NIR
 if(!p||p->destroy!=destroy_screen)return EINVAL;
 struct screen *sc=s(p);const struct remesa_gears_runtime_ops *ops=sc->device.gears_runtime;
 if(!ops||!ops->queue||!ops->flush||!ops->cancel||sc->device.resource_only_backend||
    sc->device.retained_fragment_carrier||sc->device.offline_test_backend==sc->device.qualified_hardware_backend)return ENOTSUP;
 if(sc->closing||__atomic_load_n(&sc->refs,__ATOMIC_ACQUIRE)!=1||
    __atomic_load_n(&sc->resource_cookie,__ATOMIC_RELAXED))return EBUSY;
 sc->gears_runtime=true;sc->cpu_depth24_lab=true;set_capabilities(sc);return 0;
#else
 (void)p;return ENOTSUP;
#endif
}
int remesa_screen_enable_linked_attachment_storage(struct pipe_screen *p)
{
 if(!p||p->destroy!=destroy_screen)return EINVAL;
 struct screen *sc=s(p);
 if(!sc->device.resource_only_backend)return ENOTSUP;
 if(sc->closing||sc->linked_attachment_storage||sc->cpu_depth24_lab||sc->gears_runtime||
    __atomic_load_n(&sc->refs,__ATOMIC_ACQUIRE)!=1||
    __atomic_load_n(&sc->resource_cookie,__ATOMIC_RELAXED))return EBUSY;
 if(!sc->device.ops->validate_attachment_storage||!sc->device.ops->allocate_color_attachment||
    !sc->device.ops->allocate_depth_attachment||!sc->device.ops->prepare_linked_scene)return ENOTSUP;
 int error=sc->device.ops->validate_attachment_storage(&sc->device);if(error)return error;
 sc->linked_attachment_storage=true;return 0;
}
int remesa_screen_enable_cpu_depth24_lab(struct pipe_screen *p)
{
 if(!p||p->destroy!=destroy_screen)return EINVAL;
 struct screen *sc=s(p);
 if(!sc->device.offline_test_backend||sc->device.qualified_hardware_backend||sc->device.resource_only_backend)return ENOTSUP;
 if(sc->closing||__atomic_load_n(&sc->refs,__ATOMIC_ACQUIRE)!=1||
    __atomic_load_n(&sc->resource_cookie,__ATOMIC_RELAXED))return EBUSY;
 sc->cpu_depth24_lab=true;set_capabilities(sc);return 0;
}
int remesa_resource_wait_writer(struct pipe_context *ctx,struct pipe_resource *res,uint64_t timeout)
{
 if(!ctx || ctx->draw_vbo!=draw || !res || res->screen!=ctx->screen) return EINVAL;
 if(r(res)->maps) return EBUSY;
 return resource_sync(ctx->screen,r(res),timeout);
}
int remesa_resource_attach_writer(struct pipe_context *ctx,struct pipe_resource *res,void *backend,struct pipe_fence_handle **out)
{
 struct pipe_fence_handle *f;
 if(!ctx || ctx->draw_vbo!=draw || s(ctx->screen)->closing || !s(ctx->screen)->device.resource_only_backend || !res || res->screen!=ctx->screen || !backend || !out || *out ||
    r(res)->maps || r(res)->writer || r(res)->write_error)return EINVAL;
 f=calloc(1,sizeof(*f));if(!f)return ENOMEM;
 int error=remesa_render_pass_import_writer(&r(res)->pass,&r(res)->ticket);
 if(error){free(f);return error;}
 f->refs=1;f->screen=s(ctx->screen);f->backend=backend;screen_hold(f->screen);
 r(res)->has_contents=true;fence_ref(ctx->screen,&r(res)->writer,f);*out=f;return 0;
}
int remesa_bind_diagnostic_pipeline(struct pipe_context *p,uint32_t argb)
{ if(!p || p->draw_vbo!=draw) return EINVAL;
  c(p)->bound_vertex=NULL;c(p)->bound_fragment=NULL;c(p)->vertex_required=false;
  c(p)->argb=argb;c(p)->pipeline_bound=true;c(p)->diagnostic_discard=true;return 0; }
int remesa_context_set_framebuffer_load(struct pipe_context *p,bool preserve)
{
 if(!p||p->draw_vbo!=draw)return EINVAL;
 if(s(p->screen)->closing)return ESHUTDOWN;
 /* The qualified hardware profile is explicitly REPLACE-only.  Reject a
  * preserve request at admission instead of accepting state that can only
  * fail later, after a caller has assumed load semantics.  Offline recording
  * fixtures may still exercise the state machine with an explicit PRESERVE
  * device mode. */
 if(preserve && s(p->screen)->device.qualified_hardware_backend &&
    s(p->screen)->device.framebuffer_mode!=REMESA_FRAMEBUFFER_PRESERVE)
  return ENOTSUP;
 c(p)->diagnostic_discard=!preserve;return 0;
}
uint64_t remesa_context_cpu_clear_count(const struct pipe_context *p) { return p && p->draw_vbo==draw ? ((const struct context *)p)->cpu_clears : 0; }
int remesa_screen_enable_native_fill(struct pipe_screen *p,remesa_fill_region_fn fn)
{
 if(!p||p->context_create!=create_context||!fn)return EINVAL;
 struct screen *sc=s(p);if(sc->closing||__atomic_load_n(&sc->refs,__ATOMIC_ACQUIRE)!=1||sc->native_fill)return EBUSY;
 sc->native_fill=fn;return 0;
}
uint64_t remesa_context_native_fill_count(const struct pipe_context *p)
{return p&&p->draw_vbo==draw?((const struct context *)p)->native_fills:0;}
int remesa_screen_enable_native_copy(struct pipe_screen *p,remesa_copy_region_fn fn)
{
 if(!p||p->context_create!=create_context||!fn)return EINVAL;
 struct screen *sc=s(p);if(sc->closing||__atomic_load_n(&sc->refs,__ATOMIC_ACQUIRE)!=1||sc->native_copy)return EBUSY;
 sc->native_copy=fn;return 0;
}
uint64_t remesa_context_native_copy_count(const struct pipe_context *p)
{return p&&p->draw_vbo==draw?((const struct context *)p)->native_copies:0;}
uint64_t remesa_context_cpu_copy_count(const struct pipe_context *p) { return p && p->draw_vbo==draw ? ((const struct context *)p)->cpu_copies : 0; }
int remesa_context_error(const struct pipe_context *p) { return p && p->draw_vbo==draw ? ((const struct context *)p)->error : EINVAL; }
int remesa_context_draw_rejections(struct pipe_context *p,struct remesa_draw_rejection_status *out)
{
 if(!p||p->draw_vbo!=draw||!context_output_valid(c(p),out,sizeof(*out)))return EINVAL;
 *out=c(p)->draw_rejections;return 0;
}
int remesa_drawable_buffers_replace(struct pipe_screen *p,uint32_t width,uint32_t height,
 bool double_buffer,bool depth,struct remesa_drawable_buffers *out)
{
 if(!p||p->resource_create!=create_resource||!out)return EINVAL;
 if(s(p)->closing)return ESHUTDOWN;
 if((out->front&&out->front->screen!=p)||(out->back&&out->back->screen!=p)||
    (!out->front&&out->back)||(out->front&&out->front==out->back))return EINVAL;
 if(out->generation==UINT64_MAX)return EOVERFLOW;
 /* Storage planning does not authorize new hardware surface state. */
 if(width!=32||height!=32||depth)return ENOTSUP;
 struct remesa_drawable_layout layout;
 uint64_t budget=s(p)->device.resource_limits.max_resource_bytes;
 if(!budget)budget=UINT64_C(64)*1024*1024;
 int error=remesa_drawable_layout(&s(p)->device.resource_limits,width,height,
  double_buffer,0,budget>SIZE_MAX?SIZE_MAX:(size_t)budget,&layout);
 if(error)return error;
 struct pipe_resource t={.target=PIPE_TEXTURE_2D,.format=PIPE_FORMAT_B8G8R8A8_UNORM,
  .width0=width,.height0=height,.depth0=1,.array_size=1,.bind=PIPE_BIND_RENDER_TARGET};
 struct pipe_resource *fresh[2]={0};
 for(unsigned i=0;i<layout.count;i++) {
  fresh[i]=p->resource_create(p,&t);
  if(!fresh[i]){
   for(unsigned j=0;j<i;j++)remesa_resource_reference(&fresh[j],NULL);
   return ENOMEM;
  }
 }
 struct remesa_drawable_buffers previous=*out;
 *out=(struct remesa_drawable_buffers){.front=fresh[0],.back=fresh[1],
  .width=width,.height=height,.generation=previous.generation+1};
 remesa_drawable_buffers_release(&previous);return 0;
}
void remesa_drawable_buffers_release(struct remesa_drawable_buffers *out)
{
 if(!out)return;
 remesa_resource_reference(&out->front,NULL);remesa_resource_reference(&out->back,NULL);
 memset(out,0,sizeof(*out));
}
#else
int remesa_context_prepare_vertex_parameters(struct pipe_context *p,uint32_t bank,void **out)
{(void)p;(void)bank;(void)out;return ENOTSUP;}
int remesa_context_window_viewport_parameters_snapshot(struct pipe_context *p,struct remesa_vertex_parameter_values *out)
{(void)p;(void)out;return ENOTSUP;}
int remesa_context_linked_isp_snapshot(struct pipe_context *p,struct remesa_isp_opaque_triangle_request *out)
{(void)p;(void)out;return ENOTSUP;}
int remesa_context_window_viewport_snapshot(struct pipe_context *p,uint32_t bits,struct remesa_ta_window_viewport_request *out)
{(void)p;(void)bits;(void)out;return ENOTSUP;}
int remesa_screen_enable_linked_attachment_storage(struct pipe_screen *p)
{(void)p;return ENOTSUP;}
int remesa_context_prepare_linked_scene(struct pipe_context *p,const struct pipe_draw_info *i,
 const struct pipe_draw_start_count_bias *d,const struct remesa_linked_scene_request *r,struct remesa_scene_capture *o)
{(void)p;(void)i;(void)d;(void)r;(void)o;return ENOTSUP;}
int remesa_context_prepare_gears_attribute_scene(struct pipe_context *p,const struct pipe_draw_info *i,
 const struct pipe_draw_start_count_bias *d,const struct remesa_linked_scene_request *r,struct remesa_scene_capture *o)
{(void)p;(void)i;(void)d;(void)r;(void)o;return ENOTSUP;}
int remesa_context_submit_linked_scene(struct pipe_context *p,const struct pipe_draw_info *i,
 const struct pipe_draw_start_count_bias *d,const struct remesa_linked_scene_request *r,struct pipe_fence_handle **o)
{(void)p;(void)i;(void)d;(void)r;(void)o;return ENOTSUP;}
int remesa_context_window_viewport_encode(struct pipe_context *p,uint32_t bits,unsigned char out[28])
{(void)p;(void)bits;(void)out;return ENOTSUP;}
int remesa_screen_enable_gears_runtime(struct pipe_screen *p){(void)p;return ENOTSUP;}
int remesa_context_gears_bottom_left(struct pipe_context *p,bool *b){(void)p;(void)b;return ENOTSUP;}
int remesa_context_gears_geometry_snapshot(struct pipe_context *p,const struct pipe_draw_info *i,
 const struct pipe_draw_start_count_bias *d,struct remesa_gears_geometry **o)
{(void)p;(void)i;(void)d;(void)o;return ENOTSUP;}
int remesa_context_vertex_parameter_source(struct pipe_context *p,const struct remesa_pds_parameter_source_request *r,void *o,size_t n,size_t *w)
{(void)p;(void)r;(void)o;(void)n;(void)w;return ENOTSUP;}
int remesa_drawable_buffers_replace(struct pipe_screen *p,uint32_t w,uint32_t h,bool b,bool z,struct remesa_drawable_buffers *o)
{(void)p;(void)w;(void)h;(void)b;(void)z;(void)o;return ENOTSUP;}
void remesa_drawable_buffers_release(struct remesa_drawable_buffers *o) {(void)o;}
int remesa_context_set_framebuffer_load(struct pipe_context *p,bool preserve) { (void)p;(void)preserve;return ENOTSUP; }
uint64_t remesa_context_cpu_clear_count(const struct pipe_context *p) { (void)p;return 0; }
int remesa_screen_enable_native_fill(struct pipe_screen *p,remesa_fill_region_fn fn)
{(void)p;(void)fn;return EOPNOTSUPP;}
uint64_t remesa_context_native_fill_count(const struct pipe_context *p){(void)p;return 0;}
int remesa_screen_enable_native_copy(struct pipe_screen *p,remesa_copy_region_fn fn)
{(void)p;(void)fn;return EOPNOTSUPP;}
uint64_t remesa_context_native_copy_count(const struct pipe_context *p){(void)p;return 0;}
uint64_t remesa_context_cpu_copy_count(const struct pipe_context *p) { (void)p;return 0; }
int remesa_screen_collect(struct pipe_screen *screen) { (void)screen;return ENOTSUP; }
struct pipe_screen *remesa_screen_create_owned(struct remesa_device *device,int (*release)(struct remesa_device *)) { (void)device;(void)release;return 0; }
struct pipe_screen *remesa_screen_create(struct remesa_device *device) { (void)device;return 0; }
#endif
const char *remesa_screen_unavailable_reason(void) { return "Qualified clean hardware backend required; recording tests do not render"; }
