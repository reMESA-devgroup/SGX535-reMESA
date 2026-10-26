/* SPDX-License-Identifier: GPL-2.0-only */
/* Retained backing -> existing qualified fixed executor. No register logic,
 * GEM allocation, exclusion acquisition, or owner_release is duplicated here.
 * This adapter is compiled but has no live admission/registration path. */
#ifdef REMESA_GEM_ADAPTER_TEST
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
typedef uint32_t u32;typedef uint64_t u64;
#else
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#ifdef REMESA_LAB_DEPTH1280_PROFILE
#include <linux/highmem.h>
#include <drm/drm_cache.h>
#include "remesa/depth1280_snapshot_layout.h"
#include "remesa_depth1280_profile.h"
#include "remesa_depth1280_capture.h"
#endif
#include "gma500_bo_owner.h"
#include "gma500_fixed_backend.h"
#ifdef REMESA_LAB_BIF_DIAGNOSTIC
#ifndef REMESA_LAB_DEPTH1280_PROFILE
#error BIF diagnostic record requires the explicit private depth1280 response ABI
#endif
#include "remesa_bif_debug.h"
#endif
#include "gma500_capsule_observer.h"
#include <linux/module.h>
#endif
#include "remesa_gem_executor_adapter.h"
#ifndef REMESA_GEM_ADAPTER_TEST
#include "remesa_retained_submit.h"
#endif
static bool same_ticket(struct remesa_memory_ticket a,struct remesa_memory_ticket b)
{return a.context==b.context && a.operation==b.operation && a.slot==b.slot && a.generation==b.generation;}
static int bind_common(struct remesa_gem_executor_adapter *a,
 const struct remesa_executor_platform *p,void *pc,struct remesa_memory_ticket t,
 const struct remesa_executor_view *v,unsigned n,const void *commands,unsigned bytes,
 int (*revalidate)(void *,struct remesa_memory_ticket,const struct remesa_executor_view *,unsigned),
 void *lender,u32 revision,u32 ticks,u32 samples)
{
 unsigned i,j;int e;
 if(!a || a->bound || !p || !p->validate_owner || !p->run || !p->visible ||
    !p->acknowledge || !pc || !v || n!=REMESA_ADAPTER_ROLES || !commands ||
    bytes!=REMESA_ADAPTER_COMMAND_BYTES || !revalidate || !lender ||
    !t.context || !t.operation || t.operation>0xffffffffULL || !t.generation ||
    t.slot>=8 || !revision || !ticks || !samples || samples>300000)return -EINVAL;
 for(i=0;i<n;i++) {
  if(!v[i].binding.id || !v[i].binding.bytes || !v[i].pages || !v[i].page_count || !v[i].cpu ||
     v[i].binding.bytes!=(u64)v[i].page_count*4096 ||
     (v[i].binding.va&4095) || v[i].binding.va>0xffffffffULL ||
     v[i].binding.bytes>0x100000000ULL-v[i].binding.va)return -EINVAL;
  for(j=0;j<i;j++)if(v[i].binding.id==v[j].binding.id || v[i].pages==v[j].pages ||
     (v[i].binding.va<v[j].binding.va+v[j].binding.bytes &&
      v[j].binding.va<v[i].binding.va+v[i].binding.bytes))return -EINVAL;
 }
 e=revalidate(lender,t,v,n);if(e)return e;
 e=p->validate_owner(pc,v,n,commands,bytes);if(e)return e;
 memset(a,0,sizeof(*a));a->ticket=t;memcpy(a->views,v,sizeof(a->views));
 memcpy(a->commands,commands,bytes);a->platform=p;a->platform_context=pc;
 a->revalidate=revalidate;a->lender=lender;a->revision=revision;
 a->timeout_ticks=ticks;a->max_samples=samples;a->bound=true;return 0;
}
static int validate(void *ctx,const void *commands,unsigned bytes,
 const struct remesa_memory_binding *uses,unsigned count)
{
 struct remesa_gem_executor_adapter *a=ctx;unsigned i,j;bool found;
 if(!a || !a->bound || a->attempted || a->quarantined || !commands || !uses ||
    bytes!=sizeof(a->commands) || count!=REMESA_ADAPTER_ROLES ||
    memcmp(commands,a->commands,bytes))return -EINVAL;
 for(i=0;i<count;i++) {
  found=false;
  for(j=0;j<count;j++)if(uses[i].id==a->views[j].binding.id &&
      uses[i].va==a->views[j].binding.va && uses[i].bytes==a->views[j].binding.bytes)found=true;
  if(!found)return -EINVAL;
  for(j=0;j<i;j++)if(uses[i].id==uses[j].id)return -EINVAL;
 }
 {
  int e=a->platform->validate_owner(a->platform_context,a->views,count,commands,bytes);
  if(e)return e;
#ifndef REMESA_GEM_ADAPTER_TEST
  return remesa_retained_lab_admitted()?0:-EOPNOTSUPP; /* Isolated gate stays default off. */
#else
  return 0;
#endif
 }
}
static int start(void *ctx,struct remesa_memory_ticket t,const void *commands,unsigned bytes,u64 *cookie)
{
 struct remesa_gem_executor_adapter *a=ctx;int e;
 if(!a || !cookie || !a->bound || a->attempted || a->quarantined ||
    !same_ticket(t,a->ticket) || !commands || bytes!=sizeof(a->commands) ||
    memcmp(commands,a->commands,bytes))return -EINVAL;
#ifndef REMESA_GEM_ADAPTER_TEST
 if(!remesa_retained_lab_admitted())return -EOPNOTSUPP;
#else
 if(!a->test_execution)return -EOPNOTSUPP;
#endif
 /* Repeat lender PFN/view and owner validation immediately before admitting
  * execution. The caller holds the endpoint mutex; no CPU lease can intervene. */
 e=a->revalidate(a->lender,t,a->views,REMESA_ADAPTER_ROLES);if(e)return e;
 e=a->platform->validate_owner(a->platform_context,a->views,REMESA_ADAPTER_ROLES,
                              a->commands,sizeof(a->commands));if(e)return e;
 a->attempted=true;*cookie=t.operation;
 e=a->platform->run(a->platform_context,a->revision,(u32)t.operation,
                     a->timeout_ticks,a->max_samples,&a->lifecycle);
 if(e || a->lifecycle!=REMESA_JOB_LIFECYCLE_ALL){a->quarantined=true;return e?e:-EIO;}
 a->completed=true;return 0;
}
static int poll(void *ctx,struct remesa_memory_ticket t,u64 cookie,unsigned budget,
 struct remesa_job_completion *out)
{
 struct remesa_gem_executor_adapter *a=ctx;
 if(!a || !out || !budget || !same_ticket(t,a->ticket) || cookie!=t.operation)return -EINVAL;
 if(a->quarantined)return -EIO;
 if(!a->attempted || !a->completed || a->polled)return -EINVAL;
 *out=(struct remesa_job_completion){t,cookie,a->lifecycle};a->polled=true;return 0;
}
static int visible(void *ctx,struct remesa_memory_ticket t,u64 cookie)
{
 struct remesa_gem_executor_adapter *a=ctx;int e;
 if(!a || !same_ticket(t,a->ticket) || cookie!=t.operation ||
    !a->completed || !a->polled || a->visible || a->quarantined)return -EINVAL;
 e=a->platform->visible(a->platform_context);
 if(e){a->quarantined=true;return e;}a->visible=true;return 0;
}
static int acknowledge(void *ctx,struct remesa_memory_ticket t,u64 cookie)
{
 struct remesa_gem_executor_adapter *a=ctx;int e;
 if(!a || !same_ticket(t,a->ticket) || cookie!=t.operation ||
    !a->visible || a->acknowledged || a->quarantined)return -EINVAL;
 e=a->platform->acknowledge(a->platform_context);
 if(e){a->quarantined=true;return e;}a->acknowledged=true;return 0;
}
const struct remesa_job_executor remesa_gem_executor_ops={validate,start,poll,visible,acknowledge};
#ifndef REMESA_GEM_ADAPTER_TEST
/* Never owner_construct/release: GEM pages, VA nodes, module/device/exclusion
 * references continue to belong to the persistent endpoint. */
struct remesa_retained_admission_request {
 u32 version;struct remesa_memory_ticket ticket;u64 mmu_end;
 struct remesa_memory_binding roles[REMESA_ADAPTER_ROLES];
 struct sgx535_frozen_command_pairs commands;
#ifdef REMESA_LAB_DEPTH1280_PROFILE
 u8 generated_digest[32]; /* exact independent packed image, before issue */
#endif
};
struct remesa_retained_admission_response {
 struct remesa_memory_ticket ticket;u32 phase,ledger,published,acknowledged;
 int error;unsigned char color[4096];
#ifdef REMESA_LAB_DEPTH1280_PROFILE
 /* color4096 is only a prefix; these extents/hashes bind full captures. */
 u32 width,height,color_bytes,z_bytes;
 u8 color_sha256[32],z_sha256[32];
#ifdef REMESA_LAB_BIF_DIAGNOSTIC
 struct remesa_bif_debug_record bif_debug;
#endif
#endif
};
struct remesa_executor_owner_context {
 struct sgx535_gma500_owner *owner;
 struct sgx535_gma500_fixed_backend backend;
 unsigned char readback[4096];
 void *aliases[SGX535_BO_COLOR+1];
 struct remesa_retained_admission_request admission;
 bool admitted,closed,published;
 struct remesa_retained_admission_response response;

};
static int owner_drop_aliases(struct remesa_executor_owner_context *c)
{
 unsigned i;
 for(i=0;i<=SGX535_BO_COLOR;i++) {
  struct sgx535_gma500_bo *b=&c->owner->objects[i];
  /* fixed PUBLISH_CPU consumes these aliases and clears b->cpu. The lender's
   * separate job CPU map is never handed to that stage. */
  if(b->cpu && b->cpu!=c->aliases[i])return -EBUSY;
 }
 for(i=0;i<=SGX535_BO_COLOR;i++) {
  if(c->owner->objects[i].cpu)vunmap(c->aliases[i]);
  c->owner->objects[i].cpu=NULL;c->owner->views[i].bytes=NULL;
  c->owner->views[i].length=0;c->aliases[i]=NULL;
 }
 return 0;
}
static int owner_validate(void *ctx,const struct remesa_executor_view *v,unsigned n,
 const void *commands,unsigned bytes)
{
 struct remesa_executor_owner_context *c=ctx;
 struct sgx535_gma500_owner *o=c->owner;unsigned i;
 if(!o || !o->initialized || !o->borrowed_backing || !o->device_claimed || !o->power_held ||
    o->session.phase!=SGX535_PHASE_HOST_IMAGE_FINAL || o->fixed_service.session!=&o->session ||
    o->fixed_service.started || o->user_images_flushed || n!=SGX535_BO_COUNT ||
    bytes!=sizeof(o->commands) || memcmp(commands,&o->commands,bytes))return -EINVAL;
 for(i=0;i<n;i++) {
  struct sgx535_gma500_bo *b=&o->objects[i];struct sgx535_frozen_bo *d=&o->descriptors[i];
  if(b->gem_live || b->pages!=v[i].pages || b->page_count!=v[i].page_count ||
     b->gem.size!=v[i].binding.bytes || d->role!=i || d->size!=b->gem.size ||
     d->owner_token==0)return -EINVAL;
  if(d->domain!=SGX535_DOMAIN_LOCAL && (!b->va_reserved ||
      b->mapped_pages!=b->page_count || b->va.start!=v[i].binding.va ||
      d->gpu_va!=v[i].binding.va))return -EINVAL;
  if(d->domain==SGX535_DOMAIN_LOCAL && (d->gpu_va || b->va_reserved || b->mapped_pages))return -EINVAL;
  if(i<=SGX535_BO_COLOR && (!b->cpu || b->cpu!=c->aliases[i] || b->cpu==v[i].cpu || o->views[i].bytes!=b->cpu ||
      o->views[i].length!=b->gem.size))return -EINVAL;
 }
 return sgx535_frozen_validate_bos(o->descriptors,SGX535_BO_COUNT,o->mmu_end)?-EINVAL:0;
}
/* Backend remains retained until acknowledge: IRQ ownership cannot outlive a
 * stack variable. This object is never used by the disabled production start. */
/* The existing observer hashes these immutable kernel records and enforces
 * same-task/source/one-attempt admission. It intentionally pins evidence refs;
 * those are not new GEM ownership. No admission is performed during prepare. */
static int owner_close(struct remesa_executor_owner_context *c,int error,bool acknowledged)
{
 struct remesa_retained_admission_response *r;int e;
 if(!c->admitted || c->closed)return error;
 r=kzalloc(sizeof(*r),GFP_KERNEL);if(!r)return -ENOMEM; /* keep admission held */
 r->ticket=c->admission.ticket;r->phase=c->owner->session.phase;
 r->ledger=c->owner->session.observed_events;r->error=error;
 r->published=c->published;r->acknowledged=acknowledged;
 if(c->published)memcpy(r->color,c->readback,sizeof(r->color));
#ifdef REMESA_LAB_DEPTH1280_PROFILE
 r->width=1280;r->height=800;r->color_bytes=4096000;r->z_bytes=4096000;
 if(!error&&acknowledged) {
  e=remesa_depth1280_capture_hashes(c->admission.ticket,r->color_sha256,r->z_sha256);
  /* A success response without a same-ticket full snapshot is forbidden.
   * Leave native admission open/invalid and GEM holds quarantined on error. */
  if(e){kfree(r);return e;}
 }
#endif
#ifdef REMESA_LAB_BIF_DIAGNOSTIC
 /* Copy only; no MMIO after backend_end. Missing capture remains explicit. */
 (void)sgx535_gma500_fixed_bif_snapshot(&c->backend,&r->bif_debug);
#endif
 e=sgx535_provenance_native_close(c->owner->dev,c->owner,0,r,sizeof(*r));
 memcpy(&c->response,r,sizeof(*r));c->closed=true;kfree(r);return error?error:e;
}
static const struct remesa_executor_platform owner_platform;
int remesa_gem_executor_snapshots(struct remesa_gem_executor_adapter *a,struct remesa_retained_snapshots *out)
{
 struct remesa_executor_owner_context *c;
#ifdef REMESA_LAB_DEPTH1280_PROFILE
 BUILD_BUG_ON(offsetof(struct remesa_retained_admission_response,width)!=REMESA_DEPTH1280_RESPONSE_WIDTH);
 BUILD_BUG_ON(offsetof(struct remesa_retained_admission_response,height)!=REMESA_DEPTH1280_RESPONSE_HEIGHT);
 BUILD_BUG_ON(offsetof(struct remesa_retained_admission_response,color_bytes)!=REMESA_DEPTH1280_RESPONSE_COLOR_BYTES);
 BUILD_BUG_ON(offsetof(struct remesa_retained_admission_response,z_bytes)!=REMESA_DEPTH1280_RESPONSE_Z_BYTES);
 BUILD_BUG_ON(offsetof(struct remesa_retained_admission_response,color_sha256)!=REMESA_DEPTH1280_RESPONSE_COLOR_SHA);
 BUILD_BUG_ON(offsetof(struct remesa_retained_admission_response,z_sha256)!=REMESA_DEPTH1280_RESPONSE_Z_SHA);
#ifdef REMESA_LAB_BIF_DIAGNOSTIC
 BUILD_BUG_ON(offsetof(struct remesa_retained_admission_response,bif_debug)!=REMESA_BIF_DEBUG_RESPONSE_OFFSET);
 BUILD_BUG_ON(sizeof(struct remesa_retained_admission_response)!=REMESA_BIF_DEBUG_RESPONSE_BYTES);
 BUILD_BUG_ON(REMESA_BIF_DEBUG_RESPONSE_BYTES>4352);
#else
 BUILD_BUG_ON(sizeof(struct remesa_retained_admission_response)!=REMESA_DEPTH1280_RESPONSE_BYTES);
#endif
 BUILD_BUG_ON(offsetof(struct remesa_retained_admission_request,generated_digest)!=REMESA_DEPTH1280_REQUEST_GEN_SHA);
 BUILD_BUG_ON(sizeof(struct remesa_retained_admission_request)!=REMESA_DEPTH1280_REQUEST_BYTES);
#endif
 if(!a || !out || !a->bound || a->platform!=&owner_platform)return -EINVAL;
 c=a->platform_context;memset(out,0,sizeof(*out));
 if(!c->admitted || !c->closed)return -EAGAIN;
 BUILD_BUG_ON(sizeof(c->admission)>sizeof(out->request));
 BUILD_BUG_ON(sizeof(c->response)>sizeof(out->response));
 out->request_bytes=sizeof(c->admission);out->response_bytes=sizeof(c->response);
 memcpy(out->request,&c->admission,sizeof(c->admission));
 memcpy(out->response,&c->response,sizeof(c->response));return 0;
}
static int owner_run(void *ctx,u32 revision,u32 sequence,u32 ticks,u32 samples,u32 *ledger)
{
 struct remesa_executor_owner_context *c=ctx;int e;
 e=sgx535_provenance_native_call(c->owner->dev,THIS_MODULE,c->owner,
                                &c->admission,sizeof(c->admission));
 if(e)return e;c->admitted=true;
 e=sgx535_gma500_fixed_backend_begin(&c->backend,c->owner);
 if(e)return owner_close(c,e,false);
#ifdef REMESA_LAB_BIF_DIAGNOSTIC
 /* Diagnostics never change existing admission/execution decisions. */
 (void)sgx535_gma500_fixed_bif_pre(&c->backend,sequence);
#endif
 e=sgx535_fixed_service_run_once(&c->owner->fixed_service,
    &sgx535_gma500_fixed_backend_ops,&sgx535_gma500_fixed_backend_status_source,
    &c->backend,revision,sequence,ticks,samples);
#ifdef REMESA_LAB_BIF_DIAGNOSTIC
 (void)sgx535_gma500_fixed_bif_post(&c->backend);
#endif
 if(e || c->owner->session.sequence!=sequence ||
    c->owner->session.phase!=SGX535_PHASE_RETIRED ||
    c->owner->session.observed_events!=REMESA_JOB_LIFECYCLE_ALL ||
    !sgx535_fixed_service_may_release(&c->owner->fixed_service))return owner_close(c,e?e:-EIO,false);
 *ledger=c->owner->session.observed_events;return 0;
}
static int owner_visible(void *ctx)
{
#ifdef REMESA_LAB_DEPTH1280_PROFILE
 struct remesa_executor_owner_context *c=ctx;
 struct sgx535_gma500_bo *bo=&c->owner->objects[SGX535_BO_COLOR];
 void *bytes;
 /* The capsule keeps a4096-byte PREFIX, not a32x32 frame/summary. Full
  * framebuffer capture uses held-job readback while GEM remains VISIBLE. */
 if(c->owner->session.phase!=SGX535_PHASE_RETIRED ||
    c->owner->session.observed_events!=REMESA_JOB_LIFECYCLE_ALL ||
    !c->owner->power_held || !sgx535_fixed_service_may_release(&c->owner->fixed_service) ||
    !bo->pages || bo->page_count!=1000 || bo->mapped_pages!=1000)
  return owner_close(c,-EINVAL,false);
 drm_clflush_pages(bo->pages,bo->page_count);dma_rmb();
 bytes=kmap(bo->pages[0]);if(!bytes)return owner_close(c,-ENOMEM,false);
 memcpy(c->readback,bytes,sizeof(c->readback));
 sgx535_provenance_color(c->owner,bytes);kunmap(bo->pages[0]);
 c->published=true;return 0;
#else

 struct remesa_executor_owner_context *c=ctx;
 struct sgx535_frozen_color_summary summary;
 /* Uses the existing fixed-profile capture after device-maintenance and
  * attributable retirement. This is NOT a generic render-resource contract. */
 {
  int e=sgx535_gma500_owner_capture_color(c->owner,&summary,c->readback);
  if(e)return owner_close(c,e,false);c->published=true;return 0;
 }

#endif
}
static int owner_acknowledge(void *ctx)
{
 struct remesa_executor_owner_context *c=ctx;
 if(!sgx535_fixed_service_may_release(&c->owner->fixed_service))return -EBUSY;
 {
  int e=sgx535_gma500_fixed_backend_end(&c->backend);
  return owner_close(c,e,!e);
 }
}
static const struct remesa_executor_platform owner_platform={owner_validate,owner_run,owner_visible,owner_acknowledge};
int remesa_gem_executor_bind(struct remesa_gem_executor_adapter *a,
 struct sgx535_gma500_owner *o,struct remesa_memory_ticket t,
 const struct remesa_executor_view *v,unsigned n,
 int (*check)(void *,struct remesa_memory_ticket,const struct remesa_executor_view *,unsigned),
 void *lender,u32 revision,u32 ticks,u32 samples)
{
 struct remesa_executor_owner_context *c;int e;unsigned i;
 if(!o || !a || a->bound || !v || !check || !lender || n!=SGX535_BO_COUNT || ticks>60*HZ)return -EINVAL;
 /* A borrower initializer must have attached the retained pages and exact
  * descriptors without allocating GEM objects or claiming the device again.
  * CPU aliases are exclusively ours; pre-existing owner CPU maps are rejected. */
 for(i=0;i<=SGX535_BO_COLOR;i++)if(o->objects[i].cpu || o->views[i].bytes ||
    !v[i].pages || !v[i].page_count || o->objects[i].pages!=v[i].pages)return -EINVAL;
 e=check(lender,t,v,n);if(e)return e;
 c=kzalloc(sizeof(*c),GFP_KERNEL);if(!c)return -ENOMEM;c->owner=o;
 for(i=0;i<=SGX535_BO_COLOR;i++) {
  c->aliases[i]=vmap(v[i].pages,v[i].page_count,VM_MAP,PAGE_KERNEL);
  if(!c->aliases[i]){e=-ENOMEM;goto rollback;}
  o->objects[i].cpu=c->aliases[i];o->views[i].bytes=c->aliases[i];
  o->views[i].length=o->objects[i].gem.size;
 }
#ifdef REMESA_LAB_DEPTH1280_PROFILE
 /* Images are held/immutable; bind_common repeats lender/image validation. */
 e=remesa_depth1280_sha256(o->views[SGX535_BO_VERTEX_TA].bytes,4096,c->admission.generated_digest);
 if(e)goto rollback;
#endif
 e=bind_common(a,&owner_platform,c,t,v,n,&o->commands,sizeof(o->commands),check,lender,revision,ticks,samples);
 if(!e){
#ifdef REMESA_LAB_DEPTH1280_PROFILE
#ifdef REMESA_LAB_BIF_DIAGNOSTIC
  c->admission.version=3; /* same request size, extended authenticated response */
#else
  c->admission.version=2;
#endif
#else
  c->admission.version=1;
#endif
  c->admission.ticket=t;c->admission.mmu_end=o->mmu_end;
  c->admission.commands=o->commands;
  for(i=0;i<n;i++)c->admission.roles[i]=v[i].binding;
  a->allocated_platform=c;return 0;
 }
rollback:
 owner_drop_aliases(c);kfree(c);return e;
}
#else
int remesa_gem_executor_test_bind(struct remesa_gem_executor_adapter *a,
 const struct remesa_executor_platform *p,void *pc,struct remesa_memory_ticket t,
 const struct remesa_executor_view *v,unsigned n,const void *commands,unsigned bytes,
 int (*check)(void *,struct remesa_memory_ticket,const struct remesa_executor_view *,unsigned),
 void *lender,u32 revision,u32 ticks,u32 samples)
{int e=bind_common(a,p,pc,t,v,n,commands,bytes,check,lender,revision,ticks,samples);
 if(!e)a->test_execution=true;
 return e;}
#endif

int remesa_gem_executor_dispose(struct remesa_gem_executor_adapter *a)
{
 if(!a || !a->bound)return -EINVAL;
 if(a->quarantined || (a->attempted && !a->acknowledged))return -EBUSY;
#ifndef REMESA_GEM_ADAPTER_TEST
 if(a->allocated_platform) {
  struct remesa_executor_owner_context *c=a->allocated_platform;
  int e=owner_drop_aliases(c);if(e)return e;
  /* Never leave a stale backend pointer in a retained owner's observer. */
  if(c->owner->fixed_service.observer_context==&c->backend) {
   c->owner->fixed_service.observer_context=NULL;c->owner->fixed_service.observer=NULL;
  }
  kfree(c);
 }
#endif
 memset(a,0,sizeof(*a));return 0;
}
