#ifdef REMESA_LAB_DEPTH1280_PROFILE
#include "remesa_gem_publish_inputs.h"
#endif
// SPDX-License-Identifier: GPL-2.0-only
/* Retained private GEM shmem objects, using verified in-tree gma500 MMU APIs.
 * No display GTT PTE is changed. Default-PD use requires exclusive SGX ownership.
 * Intentionally unreachable from the active DRM ioctl table. */
#ifdef REMESA_MEMORY_TEST
#include "test_gem_shim.h"
#else
#include <linux/err.h>
#include <linux/capability.h>
#include <linux/highmem.h>
#include <linux/ioport.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <drm/drm_cache.h>
#include <asm/cpufeature.h>
#include "psb_drv.h"
#include "power.h"
#include "mmu.h"
#include "gma500_bo_owner.h"
#endif
#include "remesa_gem_memory.h"
#include "remesa_color_view.h"
#define OBJECTS 32
static bool memory_enabled;
module_param_named(remesa_memory_enabled,memory_enabled,bool,0600);
/* Fixed proven scene needs two 32 MiB TA objects. Capacity is software policy,
 * not a hardware opcode. Runtime access remains disabled; new module needs its
 * own qualification/authorization, never replaces the sealed memory probe. */
#define MAX_BYTES (128u*1024u*1024u)
struct backing {
 struct drm_gem_object gem;
 struct page **pages;
 struct resource va;
 void *cpu,*cached_job_view;
 u32 generation,inserted,holds;unsigned input_seal;
 atomic_t color_vmas;
 struct remesa_gem_memory *memory;
 struct address_space *color_mapping;
 bool live,pinned,reserved,lease,write,quarantine,initialized,lease_borrowed;
 /* Used only for exact OPEN8 TA pools. No clean inference for other BOs. */
 bool ta_cpu_cached,ta_cpu_dirty,ta_gpu_dirty;
};
struct retained_job {
 void *commands,*cpu_views[16];unsigned command_bytes,count;u32 generation,ids[16];
 u64 operation,cookie;enum remesa_job_state state;bool live,quarantined,attached,ta_policy_set,ta_reuse;
};
struct remesa_gem_memory {
 struct drm_device *dev;struct drm_file *file;struct psb_mmu_pd *pd;
 struct mutex lock;struct resource root,exclude;
 /* Deliberately NULL in production: no runtime knob/registration installs it. */
 const struct remesa_job_executor *executor;void *executor_context;
 struct backing bo[OBJECTS];struct retained_job jobs[8];u64 operation,context;size_t bytes;bool excluded,stream_bound,ta_pools_sealed,ta_cache_domains;
 int (*pds_write_guard)(u64,const void *,unsigned);
};
static int unmap(struct remesa_gem_memory *s,struct backing *b)
{
 if(b->lease || b->quarantine || b->holds || atomic_read(&b->color_vmas)) return -EBUSY;
 if(b->inserted) {
  unsigned page;
  psb_mmu_remove_pages(s->pd,b->va.start,b->inserted,0,0);
  for(page=0;page<b->inserted;page++) {
   unsigned long pfn;
   int e=psb_mmu_virtual_to_pfn(s->pd,b->va.start+((u64)page<<PAGE_SHIFT),&pfn);
   if((e && e!=-EINVAL) || (!e && pfn==page_to_pfn(b->pages[page]))) {b->quarantine=true;return -EIO;}
  }
 }
 b->inserted=0;
 if(b->reserved) release_resource(&b->va);
 b->reserved=false;return 0;
}
static void discard(struct remesa_gem_memory *s,struct backing *b)
{
 if(b->cached_job_view){vunmap(b->cached_job_view);b->cached_job_view=NULL;}
 if(b->pages) drm_gem_put_pages(&b->gem,b->pages,true,false);
 b->pages=NULL;b->pinned=false;s->bytes-=b->gem.size;
 drm_gem_object_release(&b->gem);b->live=false;b->initialized=false;b->lease_borrowed=false;
 b->ta_cpu_cached=b->ta_cpu_dirty=b->ta_gpu_dirty=false;
}
int remesa_gem_memory_open(struct drm_device *dev,struct drm_file *file,u64 context,
                           struct remesa_gem_memory **out)
{
 struct remesa_gem_memory *s;struct drm_psb_private *p;u64 start,end;int e;
 if(!out || !dev || !file || !context) return -EINVAL;
 if(!capable(CAP_SYS_ADMIN)) return -EPERM;
 *out=NULL;p=dev->dev_private;
 if(!p || !p->mmu || !p->sgx_reg || !p->mmu->has_clflush ||
    !static_cpu_has(X86_FEATURE_CLFLUSH)) return -ENODEV;
 if(p->gtt.mmu_gatt_start<=0x40000000ULL ||
    (p->gtt.mmu_gatt_start&(PAGE_SIZE-1))) return -ERANGE;
 if(!gma_power_begin(dev,false)) return -ENODEV;
 e=sgx535_gma500_exclusive_claim(dev);if(e) {gma_power_end(dev);return e;}
 s=kzalloc(sizeof(*s),GFP_KERNEL);if(!s) {e=-ENOMEM;goto claim;}
 s->pd=psb_mmu_get_default_pd(p->mmu);
 if(!s->pd || s->pd->hw_context!=0) {e=-ENODEV;goto alloc;}
 s->dev=dev;s->file=file;s->context=context;mutex_init(&s->lock);
 s->root.start=0x20000000;s->root.end=(u64)p->gtt.mmu_gatt_start-1;
 s->root.name="reMESA private SGX VA";
 start=p->gtt.gatt_start;end=start+((u64)p->gtt.gatt_pages<<PAGE_SHIFT);
 if(end>0x100000000ULL || end<=start) {e=-ERANGE;goto alloc;}
 if(start<=s->root.end && end>s->root.start) {
  s->exclude.start=max_t(u64,start,s->root.start);
  s->exclude.end=min_t(u64,end-1,s->root.end);s->exclude.name="display GTT exclusion";
  e=request_resource(&s->root,&s->exclude);if(e) goto alloc;
  s->excluded=true;
 }
 drm_dev_get(dev);__module_get(THIS_MODULE);
 *out=s;return 0;
alloc:kfree(s);
claim:sgx535_gma500_exclusive_release(dev);gma_power_end(dev);return e;
}
static int info(struct remesa_gem_memory *s,struct remesa_memory_request *r)
{
 struct remesa_memory_info v={0};unsigned i,page;
 if(r->flags || r->bytes || r->va || r->offset || r->length) return -EINVAL;
 v.allocated_bytes=s->bytes;
 for(i=0;i<OBJECTS;i++) {
  struct backing *b=&s->bo[i];
  if(!b->live) continue;
  v.objects++;v.pinned_objects+=b->pinned;v.mapped_pages+=b->inserted;
  v.leases+=b->lease;v.quarantined+=b->quarantine;
  if(r->id && r->id==((b->generation<<5)|i)) {
   v.object_bytes=b->gem.size;v.gpu_va=b->reserved?b->va.start:0;
   v.object_pinned=b->pinned;v.object_mapped_pages=b->inserted;v.object_lease=b->lease;
   for(page=0;page<b->inserted;page++) {
    unsigned long pfn;
    if(psb_mmu_virtual_to_pfn(s->pd,b->va.start+((u64)page<<PAGE_SHIFT),&pfn) ||
       pfn!=page_to_pfn(b->pages[page])) {b->quarantine=true;return -EIO;}
    v.verified_pages++;
   }
  }
 }
 for(i=0;i<8;i++) v.held_jobs+=s->jobs[i].live;
 if(r->id && !v.object_bytes) return -ENOENT;
 memcpy(r->data,&v,sizeof(v));return 0;
}
static bool is_managed_ta_pool(const struct backing *b)
{
 return b->reserved && b->gem.size==33554432ULL &&
  (b->va.start==0x407ef000ULL || b->va.start==0x30100000ULL);
}
static bool ta_domain(const struct remesa_gem_memory *s,const struct backing *b)
{return s->ta_cache_domains && s->ta_pools_sealed && is_managed_ta_pool(b);}
static void ta_cache_flushed(struct backing *b)
{b->ta_cpu_cached=false;b->ta_cpu_dirty=false;b->ta_gpu_dirty=false;}
/* Caller holds endpoint mutex; idle leases or prepared exclusive kernel writes
 * only. Device completion/visibility is established by the existing job path. */
static void ta_for_cpu(struct backing *b)
{
 if(b->ta_gpu_dirty){
  drm_clflush_pages(b->pages,b->gem.size/PAGE_SIZE);dma_rmb();
  ta_cache_flushed(b);
 }
}
static int act(struct remesa_gem_memory *s,struct remesa_memory_request *r)
{
 struct backing *b;unsigned slot,page;int e;
 if(r->version!=REMESA_MEMORY_VERSION || r->context!=s->context || r->reserved ||
    r->flags>1) return -EINVAL;
 if(r->action!=REMESA_MEM_WRITE) {
  for(page=0;page<REMESA_MEMORY_CHUNK;page++) if(r->data[page]) return -EINVAL;
 }
 if(r->action==REMESA_MEM_INFO) return info(s,r);
 if(s->stream_bound) {
  /* Stream admission freezes the complete MMU/backing topology. Once any
   * job owns holds, ALL CPU mutation/publication is excluded, including BOs
   * outside its role list. Upload/readback leases are allowed only when idle. */
  for(slot=0;slot<8;slot++)if(s->jobs[slot].live)return -EBUSY;
  if(r->action!=REMESA_MEM_LEASE && r->action!=REMESA_MEM_READ &&
     r->action!=REMESA_MEM_WRITE && r->action!=REMESA_MEM_UNLEASE &&
     r->action!=REMESA_MEM_SYNC)return -EBUSY;
 }
 if(r->action==REMESA_MEM_CREATE) {
  if(r->id || r->flags || r->va || r->offset || r->length || !r->bytes ||
     (r->bytes&(PAGE_SIZE-1)) || r->bytes>MAX_BYTES || r->bytes>MAX_BYTES-s->bytes) return -EINVAL;
  for(slot=0;slot<OBJECTS;slot++)
   if(!s->bo[slot].live && s->bo[slot].generation<0x7ffffff) break;
  if(slot==OBJECTS) return -ENOSPC;
  b=&s->bo[slot];e=drm_gem_object_init(s->dev,&b->gem,r->bytes);if(e) return e;
  b->generation++;b->live=true;b->lease_borrowed=false;b->memory=s;atomic_set(&b->color_vmas,0);b->color_mapping=NULL;s->bytes+=r->bytes;
  r->id=(b->generation<<5)|slot;return 0;
 }
 slot=r->id&31;b=&s->bo[slot];
 if(!b->live || !r->id || (r->id>>5)!=b->generation) return -ENOENT;
 if(b->quarantine) return -EIO;
 if(b->holds) return -EBUSY;
 /* FAST kernel-managed TA pools may be reused only if user CPU writes are
  * impossible for the complete stream lifetime. Reads remain lease gated. */
 if(s->ta_pools_sealed && is_managed_ta_pool(b) &&
    (r->action==REMESA_MEM_WRITE || (r->action==REMESA_MEM_LEASE&&r->flags)))return -EPERM;
 if(b->input_seal==2 && (r->action==REMESA_MEM_WRITE ||
    (r->action==REMESA_MEM_LEASE && r->flags)))return -EPERM;
 if(r->bytes || (r->action!=REMESA_MEM_MAP && r->va) ||
    (r->action!=REMESA_MEM_READ && r->action!=REMESA_MEM_WRITE && (r->offset || r->length)) ||
    (r->action!=REMESA_MEM_LEASE && r->flags)) return -EINVAL;
 switch(r->action) {
 case REMESA_MEM_PIN:
  if(b->pinned) return -EBUSY;
  b->pages=drm_gem_get_pages(&b->gem);
  if(IS_ERR(b->pages)) {e=PTR_ERR(b->pages);b->pages=NULL;return e;}
  if(!b->initialized) {
   for(page=0;page<b->gem.size/PAGE_SIZE;page++) clear_highpage(b->pages[page]);
   b->initialized=true;b->ta_cpu_cached=true;b->ta_cpu_dirty=true;
  }
  b->pinned=true;return 0;
 case REMESA_MEM_MAP:
  if(!b->pinned || b->reserved || b->lease || r->va<s->root.start ||
     r->va>s->root.end || (r->va&(PAGE_SIZE-1)) || b->gem.size>s->root.end-r->va+1) return -EINVAL;
  b->va.start=r->va;b->va.end=r->va+b->gem.size-1;b->va.name="reMESA GEM SGX mapping";
  e=request_resource(&s->root,&b->va);if(e) return e;b->reserved=true;
  for(page=0;page<b->gem.size/PAGE_SIZE;page++) {
   e=psb_mmu_insert_pages(s->pd,&b->pages[page],r->va+((u64)page<<PAGE_SHIFT),1,0,0,PSB_MMU_CACHED_MEMORY);
   if(e) {int cleanup=unmap(s,b);return cleanup?cleanup:e;}
   b->inserted++;
  }
  return 0;
 case REMESA_MEM_UNMAP:return unmap(s,b);
 case REMESA_MEM_UNPIN:
  if(!b->pinned || b->reserved || b->lease || atomic_read(&b->color_vmas)) return -EBUSY;
  if(b->cached_job_view){vunmap(b->cached_job_view);b->cached_job_view=NULL;}
  drm_gem_put_pages(&b->gem,b->pages,true,false);b->pages=NULL;b->pinned=false;b->lease_borrowed=false;
  b->ta_cpu_cached=b->ta_cpu_dirty=b->ta_gpu_dirty=false;return 0;
 case REMESA_MEM_DESTROY:
  if(b->pinned || b->reserved || b->lease || atomic_read(&b->color_vmas)) return -EBUSY;
  discard(s,b);return 0;
 case REMESA_MEM_LEASE:
  if(!b->pinned || b->lease) return -EBUSY;
  /* A retired OPEN8 TA job already retains a full mapping. Borrow only
   * that exact readonly/sealed pool view: an extra32MiB vmap can exhaust
   * the Mini12's32-bit vmalloc VA. Leases/holds still exclude GPU reuse. */
  b->lease_borrowed=false;
  if(ta_domain(s,b) && !r->flags && b->cached_job_view){
   b->cpu=b->cached_job_view;b->lease_borrowed=true;
  }else b->cpu=vmap(b->pages,b->gem.size/PAGE_SIZE,VM_MAP,PAGE_KERNEL);
  if(!b->cpu)return -ENOMEM;
  if(ta_domain(s,b)){
   ta_for_cpu(b); /* before CPU access; mapping alone has read no data */
   b->ta_cpu_cached=true;
  }
  b->lease=true;b->write=r->flags;return 0;
 case REMESA_MEM_READ:case REMESA_MEM_WRITE:
  if(!b->lease || (r->action==REMESA_MEM_WRITE && !b->write) ||
     r->offset>b->gem.size || r->length>REMESA_MEMORY_CHUNK || r->length>b->gem.size-r->offset) return -EINVAL;
  if(r->action==REMESA_MEM_WRITE) {
   /* The caller holds s->lock. Validate the complete replacement page before
    * modifying any byte; guard is pure, bounded, allocation/lock free. */
   if(b->input_seal==1){
    if(!s->pds_write_guard)return -EACCES;
    e=s->pds_write_guard(r->offset,r->data,r->length);if(e)return e;
   }
   memcpy((u8 *)b->cpu+r->offset,r->data,r->length);
   if(ta_domain(s,b)){b->ta_cpu_cached=true;b->ta_cpu_dirty=true;}
  }
  else memcpy(r->data,(u8 *)b->cpu+r->offset,r->length);
  return 0;
 case REMESA_MEM_UNLEASE:
  if(!b->lease) return -EINVAL;
  /* PTE insertion in color_fault() and revocation share s->lock. Mark
   * inaccessible before zapping and finish all TLB invalidation before any
   * subsequent job can obtain holds. even_cows=1 is necessary: these GEM
   * pages belong to a shmem mapping, not the DRM character-device mapping. */
  b->lease=false;
  if(atomic_read(&b->color_vmas))
   unmap_mapping_range(b->color_mapping,REMESA_COLOR_VIEW_OFFSET,
    REMESA_COLOR_VIEW_BYTES,1);
  if(!b->lease_borrowed)vunmap(b->cpu);
  b->cpu=NULL;b->lease_borrowed=false;return 0;
 case REMESA_MEM_SYNC:
  if(!b->pinned || b->lease) return -EBUSY;
  drm_clflush_pages(b->pages,b->gem.size/PAGE_SIZE);dma_wmb();
  if(ta_domain(s,b)){dma_rmb();ta_cache_flushed(b);}return 0;
 default:return -EOPNOTSUPP;
 }
}
int remesa_gem_memory_request(struct remesa_gem_memory *s,struct drm_file *file,
                              struct remesa_memory_request *r)
{
 int e;if(!s || !r || file!=s->file) return -EACCES;
 mutex_lock(&s->lock);e=act(s,r);mutex_unlock(&s->lock);return e;
}
static void drop_job_views(struct retained_job *j)
{
 unsigned i;for(i=0;i<j->count;i++)if(j->cpu_views[i]) {
  /* The backing owns this mapping until unpin; job holds only lend it. */
  j->cpu_views[i]=NULL;
 }
}
int remesa_gem_memory_close(struct remesa_gem_memory *s)
{
 unsigned i;if(!s) return -EINVAL;mutex_lock(&s->lock);
 if(s->stream_bound){mutex_unlock(&s->lock);return -EBUSY;}
 for(i=0;i<8;i++) if(s->jobs[i].live && (s->jobs[i].state!=REMESA_JOB_PREPARED || s->jobs[i].attached)) {mutex_unlock(&s->lock);return -EBUSY;}
 for(i=0;i<OBJECTS;i++) if(s->bo[i].lease || s->bo[i].quarantine || atomic_read(&s->bo[i].color_vmas)) {mutex_unlock(&s->lock);return -EBUSY;}
 for(i=0;i<8;i++) if(s->jobs[i].live) {
  unsigned use;struct retained_job *j=&s->jobs[i];
  for(use=0;use<j->count;use++) s->bo[j->ids[use]&31].holds--;
  drop_job_views(j);kfree(j->commands);j->commands=NULL;j->live=false;
 }
 for(i=0;i<OBJECTS;i++) if(s->bo[i].live) {int e=unmap(s,&s->bo[i]);if(e){mutex_unlock(&s->lock);return e;}discard(s,&s->bo[i]);}
 if(s->excluded) release_resource(&s->exclude);
 mutex_unlock(&s->lock);sgx535_gma500_exclusive_release(s->dev);gma_power_end(s->dev);drm_dev_put(s->dev);kfree(s);module_put(THIS_MODULE);return 0;
}

static struct retained_job *ticket_job(struct remesa_gem_memory *s,struct remesa_memory_ticket t)
{
 struct retained_job *j;
 if(t.context!=s->context || t.slot>=8) return NULL;
 j=&s->jobs[t.slot];
 return j->live && j->operation==t.operation && j->generation==t.generation?j:NULL;
}
int remesa_gem_job_prepare(struct remesa_gem_memory *s,const struct remesa_memory_binding *uses,
                           unsigned count,const void *commands,unsigned bytes,struct remesa_memory_ticket *out)
{
 unsigned i,k,slot;struct retained_job *j;int e=-EINVAL;
 if(!s || !uses || !count || count>16 || !commands || !bytes || bytes>4096 || !out) return -EINVAL;
 mutex_lock(&s->lock);
 if(s->operation==~(u64)0) {e=-EOVERFLOW;goto done;}
 for(i=0;i<count;i++) {
  struct backing *b=&s->bo[uses[i].id&31];
  if(!uses[i].id || !b->live || uses[i].id>>5!=b->generation) {e=-ENOENT;goto done;}
  if(b->quarantine || b->lease || b->holds || !b->pinned || !b->reserved ||
     b->inserted!=b->gem.size/PAGE_SIZE) {e=-EBUSY;goto done;}
  if(uses[i].va!=b->va.start || uses[i].bytes!=b->gem.size) goto done;
  if(s->pds_write_guard){
   if(psb_mmu_verify_pages(s->pd,b->va.start,b->pages,b->inserted)){
    b->quarantine=true;e=-EIO;goto done;
   }
  }else{
  for(k=0;k<b->inserted;k++) {
   unsigned long pfn;
   if(psb_mmu_virtual_to_pfn(s->pd,b->va.start+((u64)k<<PAGE_SHIFT),&pfn) ||
      pfn!=page_to_pfn(b->pages[k])) {b->quarantine=true;e=-EIO;goto done;}
  }
  }
  for(k=0;k<i;k++) if(uses[k].id==uses[i].id) goto done;
 }
 for(slot=0;slot<8;slot++) if(!s->jobs[slot].live && s->jobs[slot].generation!=~(u32)0) break;
 if(slot==8) {e=-ENOSPC;goto done;}
 j=&s->jobs[slot];j->commands=kmemdup(commands,bytes,GFP_KERNEL);
 if(!j->commands) {e=-ENOMEM;goto done;}
 j->generation++;j->operation=++s->operation;j->command_bytes=bytes;j->count=count;j->live=true;j->quarantined=false;j->cookie=0;j->attached=false;j->ta_policy_set=false;j->ta_reuse=false;j->state=REMESA_JOB_PREPARED;
 for(i=0;i<count;i++) {j->ids[i]=uses[i].id;s->bo[uses[i].id&31].holds++;}
 *out=(struct remesa_memory_ticket){s->context,j->operation,slot,j->generation};e=0;
done:mutex_unlock(&s->lock);return e;
}
int remesa_gem_job_cancel(struct remesa_gem_memory *s,struct remesa_memory_ticket ticket)
{
 struct retained_job *j;unsigned i;int e=-ENOENT;if(!s)return -EINVAL;
 mutex_lock(&s->lock);j=ticket_job(s,ticket);if(!j)goto done;
 if(j->state!=REMESA_JOB_PREPARED || j->attached) {e=-EBUSY;goto done;}
 for(i=0;i<j->count;i++) s->bo[j->ids[i]&31].holds--;
 drop_job_views(j);kfree(j->commands);j->commands=NULL;j->live=false;e=0;
done:mutex_unlock(&s->lock);return e;
}
int remesa_gem_job_quarantine(struct remesa_gem_memory *s,struct remesa_memory_ticket ticket)
{
 struct retained_job *j;unsigned i;int e=-ENOENT;if(!s)return -EINVAL;
 mutex_lock(&s->lock);j=ticket_job(s,ticket);if(j) {
  j->quarantined=true;j->state=REMESA_JOB_QUARANTINED;for(i=0;i<j->count;i++) s->bo[j->ids[i]&31].quarantine=true;e=0;
 }
 mutex_unlock(&s->lock);return e;
}
static int job_fault(struct remesa_gem_memory *s,struct retained_job *j,int e)
{
 unsigned i;j->quarantined=true;j->state=REMESA_JOB_QUARANTINED;
 for(i=0;i<j->count;i++) s->bo[j->ids[i]&31].quarantine=true;
 return e;
}
static int verify_job_bindings(struct remesa_gem_memory *s,struct retained_job *j,
                                struct remesa_memory_binding *uses)
{
 unsigned i,k;
 for(i=0;i<j->count;i++) {
  struct backing *b=&s->bo[j->ids[i]&31];
  if(!b->live || b->generation!=j->ids[i]>>5 || b->quarantine ||
     !b->holds || b->lease || !b->pinned || !b->reserved ||
     b->inserted!=b->gem.size/PAGE_SIZE) return -EIO;
  if(s->pds_write_guard){
   if(psb_mmu_verify_pages(s->pd,b->va.start,b->pages,b->inserted))return -EIO;
  }else{
  for(k=0;k<b->inserted;k++) {
   unsigned long pfn;
   if(psb_mmu_virtual_to_pfn(s->pd,b->va.start+((u64)k<<PAGE_SHIFT),&pfn) ||
      pfn!=page_to_pfn(b->pages[k])) return -EIO;
  }
  }
  uses[i]=(struct remesa_memory_binding){j->ids[i],b->va.start,b->gem.size};
 }
 return 0;
}
/* Existing snapshot/validate path checks every PFN before these helpers.
 * Its mutex+all holds freeze pages/topology. Do not add an extra PFN pass. */
static int ta_job(struct remesa_gem_memory *s,struct retained_job *j)
{
 unsigned role;
 if(!s->stream_bound || !s->ta_cache_domains || !s->ta_pools_sealed ||
    !j || j->count!=11 || j->quarantined)return -EACCES;
 for(role=0;role<11;role++){
  struct backing *b=&s->bo[j->ids[role]&31];
  if(!j->ids[role] || !b->live || b->generation!=j->ids[role]>>5 ||
     !b->holds || b->lease || !b->pinned || !b->reserved || b->quarantine ||
     !b->pages || b->inserted!=b->gem.size/PAGE_SIZE)return -EIO;
  if((role==7||role==8) && (b->inserted!=8192 || b->gem.size!=33554432ULL ||
     b->va.start!=(role==7?0x407ef000ULL:0x30100000ULL)))return -EIO;
 }
 return 0;
}
int remesa_gem_stream_enable_ta_domains(struct remesa_gem_memory *s)
{
 unsigned i;int e=0;if(!s)return -EINVAL;mutex_lock(&s->lock);
 if(!s->stream_bound || !s->ta_pools_sealed || s->ta_cache_domains){e=-EACCES;goto done;}
 for(i=0;i<8;i++)if(s->jobs[i].live){e=-EBUSY;goto done;}
 /* Claim already attested exact two pools and excludes leases. Conservative
  * first publication; kernel clears are marked again under job holds. */
 for(i=0;i<OBJECTS;i++)if(s->bo[i].live&&is_managed_ta_pool(&s->bo[i])){
  s->bo[i].ta_cpu_cached=true;s->bo[i].ta_cpu_dirty=true;s->bo[i].ta_gpu_dirty=false;
 }
 s->ta_cache_domains=true;
 done:mutex_unlock(&s->lock);return e;
}
int remesa_gem_job_ta_policy(struct remesa_gem_memory *s,
 struct remesa_memory_ticket t,bool reuse)
{
 struct retained_job *j;unsigned role;int e=-EINVAL;if(!s)return e;
 mutex_lock(&s->lock);j=ticket_job(s,t);
 e=ta_job(s,j);if(e)goto done;
 if(j->state!=REMESA_JOB_PREPARED || j->ta_policy_set){e=-EBUSY;goto done;}
 if(!reuse)for(role=7;role<=8;role++){
  struct backing *b=&s->bo[j->ids[role]&31];ta_for_cpu(b);
  b->ta_cpu_cached=true;b->ta_cpu_dirty=true;
 }
 j->ta_reuse=reuse;j->ta_policy_set=true;
 done:mutex_unlock(&s->lock);return e;
}
int remesa_gem_job_prefire_cache_locked(struct remesa_gem_memory *s,
 struct remesa_memory_ticket t)
{
 struct retained_job *j;unsigned role;int e;
 if(!s)return -EINVAL;
#ifndef REMESA_MEMORY_TEST
 lockdep_assert_held(&s->lock);
#endif
 j=ticket_job(s,t);e=ta_job(s,j);if(e)return e;
 if(j->state!=REMESA_JOB_ISSUED || !j->ta_policy_set)return -EBUSY;
 for(role=0;role<j->count;role++){
  struct backing *b=&s->bo[j->ids[role]&31];
  /* Other nine BOs deliberately keep qualified unconditional maintenance. */
  if(role<7 || role>8 || b->ta_cpu_cached || b->ta_cpu_dirty){
   drm_clflush_pages(b->pages,b->gem.size/PAGE_SIZE);
   if(role==7||role==8){b->ta_cpu_cached=false;b->ta_cpu_dirty=false;}
  }
 }
 /* owner_flush retains its original dma_wmb and alias-drop ordering. */
 return 0;
}
int remesa_gem_stream_seal_inputs(struct remesa_gem_memory *s,
 struct remesa_memory_ticket t,int (*guard)(u64,const void *,unsigned))
{
 struct retained_job *j;unsigned i;int e=-EINVAL;
 if(!s || !guard)return -EINVAL;
 mutex_lock(&s->lock);
 if(!s->stream_bound || s->pds_write_guard){e=-EACCES;goto done;}
 j=ticket_job(s,t);if(!j || j->state!=REMESA_JOB_PREPARED || j->count!=11){e=-EBUSY;goto done;}
 /* First full authentication just ran under these same persistent job holds.
  * A live job excludes leases and topology mutation for the entire endpoint. */
 for(i=0;i<5;i++){
  struct backing *b=&s->bo[j->ids[i]&31];
  if(!b->live || b->generation!=j->ids[i]>>5 || !b->holds || b->lease ||
     !b->pinned || !b->reserved || b->quarantine || b->input_seal){e=-EBUSY;goto done;}
 }
 for(i=0;i<5;i++)s->bo[j->ids[i]&31].input_seal=i?2:1;
 s->pds_write_guard=guard;e=0;
 done:mutex_unlock(&s->lock);return e;
}
static int job_views_locked(struct remesa_gem_memory *s,struct retained_job *j,
                            struct remesa_memory_view *out,unsigned capacity)
{
 struct remesa_memory_binding uses[16];bool added[16]={false};unsigned i;int e;
 if(j->state!=REMESA_JOB_PREPARED)return -EBUSY;
 if(capacity<j->count)return -ENOSPC;
 e=verify_job_bindings(s,j,uses);if(e)return job_fault(s,j,e);
 for(i=0;i<j->count;i++)if(!j->cpu_views[i]) {
  struct backing *b=&s->bo[j->ids[i]&31];
  if(!b->cached_job_view){
   b->cached_job_view=vmap(b->pages,b->gem.size/PAGE_SIZE,VM_MAP,PAGE_KERNEL);
   if(!b->cached_job_view){e=-ENOMEM;goto rollback;}
   added[i]=true;
  }
  j->cpu_views[i]=b->cached_job_view;
 }
 for(i=0;i<j->count;i++) {
  struct backing *b=&s->bo[j->ids[i]&31];
  out[i]=(struct remesa_memory_view){uses[i],b->pages,b->gem.size/PAGE_SIZE,j->cpu_views[i]};
 }
 return j->count;
rollback:
 for(i=0;i<j->count;i++)if(added[i]){
  struct backing *b=&s->bo[j->ids[i]&31];
  vunmap(b->cached_job_view);b->cached_job_view=NULL;j->cpu_views[i]=NULL;
 }
 return e;
}
int remesa_gem_job_views(struct remesa_gem_memory *s,struct remesa_memory_ticket t,
                         struct remesa_memory_view *out,unsigned capacity)
{
 struct retained_job *j;int e=-ENOENT;if(!s || !out)return -EINVAL;
 mutex_lock(&s->lock);j=ticket_job(s,t);if(j)e=job_views_locked(s,j,out,capacity);
 mutex_unlock(&s->lock);return e;
}
int remesa_gem_job_snapshot(struct remesa_gem_memory *s,struct remesa_memory_ticket t,
                            struct remesa_memory_snapshot *out,struct remesa_memory_view *views,unsigned capacity)
{
 struct retained_job *j;int e=-ENOENT;if(!s || !out || !views)return -EINVAL;
 mutex_lock(&s->lock);j=ticket_job(s,t);if(!j)goto done;
 e=job_views_locked(s,j,views,capacity);if(e<0)goto done;
 *out=(struct remesa_memory_snapshot){s->dev,t,s->root.end+1,j->commands,j->command_bytes,true,true};
done:mutex_unlock(&s->lock);return e;
}
int remesa_gem_job_check_snapshot_locked(struct remesa_gem_memory *s,
 const struct remesa_memory_snapshot *snapshot,const struct remesa_memory_view *views,unsigned count)
{
 struct retained_job *j;struct remesa_memory_binding uses[16];unsigned i;int e;
 if(!s || !snapshot || !views)return -EINVAL;
 j=ticket_job(s,snapshot->ticket);if(!j)return -ENOENT;
 if(j->quarantined)return -EIO;
 if(snapshot->dev!=s->dev || snapshot->mmu_end!=s->root.end+1 ||
    snapshot->commands!=j->commands || snapshot->command_bytes!=j->command_bytes ||
    !snapshot->exclusive_held || !snapshot->power_held || count!=j->count)return -EACCES;
 e=verify_job_bindings(s,j,uses);if(e)return job_fault(s,j,e);
 for(i=0;i<count;i++) {
  struct backing *b=&s->bo[j->ids[i]&31];
  if(views[i].binding.id!=uses[i].id || views[i].binding.va!=uses[i].va ||
     views[i].binding.bytes!=uses[i].bytes || views[i].pages!=b->pages ||
     views[i].page_count!=b->gem.size/PAGE_SIZE || !j->cpu_views[i] ||
     views[i].cpu!=j->cpu_views[i] || j->cpu_views[i]!=b->cached_job_view)return -EACCES;
 }
 return 0;
}
int remesa_gem_job_check_snapshot(struct remesa_gem_memory *s,
 const struct remesa_memory_snapshot *snapshot,const struct remesa_memory_view *views,unsigned count)
{
 int e;if(!s)return -EINVAL;mutex_lock(&s->lock);
 e=remesa_gem_job_check_snapshot_locked(s,snapshot,views,count);
 mutex_unlock(&s->lock);return e;
}
int remesa_gem_job_issue(struct remesa_gem_memory *s,struct remesa_memory_ticket t)
{
 struct retained_job *j;struct remesa_memory_binding uses[16];unsigned slot;int e=-ENOENT;
 if(!s)return -EINVAL;
 mutex_lock(&s->lock);j=ticket_job(s,t);if(!j)goto done;
 if(j->state!=REMESA_JOB_PREPARED){e=-EBUSY;goto done;}
 if(!s->executor){e=-EOPNOTSUPP;goto done;}
 if(!s->executor->validate || !s->executor->start || !s->executor->poll ||
    !s->executor->visible || !s->executor->acknowledge){e=-EOPNOTSUPP;goto done;}
 for(slot=0;slot<8;slot++)
  if(s->jobs[slot].live && s->jobs[slot].state!=REMESA_JOB_PREPARED){e=-EBUSY;goto done;}
 e=verify_job_bindings(s,j,uses);if(e){e=job_fault(s,j,e);goto done;}
 e=s->executor->validate(s->executor_context,j->commands,j->command_bytes,uses,j->count);
 if(e)goto done; /* No device actions permitted by validation contract. */
 /* Mark possible issuance BEFORE the callback, including interrupted start. */
 if(s->ta_cache_domains){
  e=ta_job(s,j);if(e)goto done;
  if(!j->ta_policy_set){e=-EPROTO;goto done;}
 }
 j->state=REMESA_JOB_ISSUED;
 /* Possible GPU writes are recorded before start, including an interrupted
  * callback. Prefire removes CPU cache but retains this pending GPU state. */
 if(s->ta_cache_domains)for(slot=7;slot<=8;slot++)s->bo[j->ids[slot]&31].ta_gpu_dirty=true;
 e=s->executor->start(s->executor_context,t,j->commands,j->command_bytes,&j->cookie);
 if(e || !j->cookie)e=job_fault(s,j,e?e:-EPROTO);
done:mutex_unlock(&s->lock);return e;
}
int remesa_gem_job_wait(struct remesa_gem_memory *s,struct remesa_memory_ticket t,unsigned budget)
{
 struct retained_job *j;struct remesa_job_completion c={0};int e=-ENOENT;
 if(!s || !budget || budget>10000)return -EINVAL;
 mutex_lock(&s->lock);j=ticket_job(s,t);if(!j)goto done;
 if(j->state!=REMESA_JOB_ISSUED){e=-EBUSY;goto done;}
 e=s->executor->poll(s->executor_context,t,j->cookie,budget,&c);
 if(e){e=job_fault(s,j,e);goto done;}
 if(c.ticket.context!=t.context || c.ticket.operation!=t.operation ||
    c.ticket.slot!=t.slot || c.ticket.generation!=t.generation ||
    c.cookie!=j->cookie || c.lifecycle!=REMESA_JOB_LIFECYCLE_ALL) {
  e=job_fault(s,j,-EPROTO);goto done;
 }
 j->state=REMESA_JOB_COMPLETE;
done:mutex_unlock(&s->lock);return e;
}
int remesa_gem_job_publish(struct remesa_gem_memory *s,struct remesa_memory_ticket t)
{
 struct retained_job *j;struct remesa_memory_binding uses[16];unsigned i;int e=-ENOENT;
 if(!s)return -EINVAL;
 mutex_lock(&s->lock);j=ticket_job(s,t);if(!j)goto done;
 if(j->state!=REMESA_JOB_COMPLETE){e=-EBUSY;goto done;}
 e=verify_job_bindings(s,j,uses);if(e){e=job_fault(s,j,e);goto done;}
 e=s->executor->visible(s->executor_context,t,j->cookie);
 if(e){e=job_fault(s,j,e);goto done;}
 for(i=0;i<j->count;i++) {
  struct backing *b=&s->bo[j->ids[i]&31];
  if(s->ta_cache_domains && j->ta_reuse && (i==7||i==8))continue;
  drm_clflush_pages(b->pages,b->gem.size/PAGE_SIZE);
  if(ta_domain(s,b))ta_cache_flushed(b);
 }
 dma_rmb();j->state=REMESA_JOB_VISIBLE;
done:mutex_unlock(&s->lock);return e;
}
int remesa_gem_job_acknowledge(struct remesa_gem_memory *s,struct remesa_memory_ticket t)
{
 struct retained_job *j;int e=-ENOENT;if(!s)return -EINVAL;
 mutex_lock(&s->lock);j=ticket_job(s,t);if(!j)goto done;
 if(j->state!=REMESA_JOB_VISIBLE){e=-EBUSY;goto done;}
 e=s->executor->acknowledge(s->executor_context,t,j->cookie);
 if(e)e=job_fault(s,j,e);else j->state=REMESA_JOB_ACKNOWLEDGED;
done:mutex_unlock(&s->lock);return e;
}
int remesa_gem_job_retire(struct remesa_gem_memory *s,struct remesa_memory_ticket t)
{
 struct retained_job *j;struct remesa_memory_binding uses[16];unsigned i;int e=-ENOENT;if(!s)return -EINVAL;
 mutex_lock(&s->lock);j=ticket_job(s,t);if(!j)goto done;
 if(j->state!=REMESA_JOB_ACKNOWLEDGED || j->attached){e=-EBUSY;goto done;}
 e=verify_job_bindings(s,j,uses);if(e){e=job_fault(s,j,e);goto done;}
 for(i=0;i<j->count;i++)s->bo[j->ids[i]&31].holds--;
 drop_job_views(j);kfree(j->commands);j->commands=NULL;j->live=false;e=0;
done:mutex_unlock(&s->lock);return e;
}
int remesa_gem_job_state(struct remesa_gem_memory *s,struct remesa_memory_ticket t,
                         enum remesa_job_state *out)
{
 struct retained_job *j;int e=-ENOENT;if(!s || !out)return -EINVAL;
 mutex_lock(&s->lock);j=ticket_job(s,t);if(j){*out=j->state;e=0;}
 mutex_unlock(&s->lock);return e;
}

#if !defined(REMESA_MEMORY_TEST) || defined(REMESA_RETAINED_SESSION_TEST)
#include "remesa_retained_session.h"
#include "remesa_borrowed_owner.h"
#include "remesa_gem_executor_adapter.h"
struct remesa_retained_session {
 struct remesa_gem_memory *memory;
 struct remesa_memory_snapshot snapshot;
 struct remesa_memory_view views[REMESA_ADAPTER_ROLES];
 struct sgx535_gma500_owner owner;
 struct remesa_borrowed_owner borrower;
 struct remesa_gem_executor_adapter adapter;
 remesa_retained_images verify_images;void *profile;
};
static int retained_verify(void *opaque,struct drm_device *dev,struct remesa_memory_ticket t,
 const struct remesa_memory_view *views,unsigned count,u64 mmu_end)
{
 struct remesa_retained_session *r=opaque;int e;
 if(dev!=r->snapshot.dev || mmu_end!=r->snapshot.mmu_end ||
    t.context!=r->snapshot.ticket.context || t.operation!=r->snapshot.ticket.operation ||
    t.slot!=r->snapshot.ticket.slot || t.generation!=r->snapshot.ticket.generation)return -EACCES;
 e=remesa_gem_job_check_snapshot_locked(r->memory,&r->snapshot,views,count);
 if(e)return e;
 return r->verify_images(r->profile,views,count,r->snapshot.commands,r->snapshot.command_bytes);
}
static int retained_adapter_verify(void *opaque,struct remesa_memory_ticket t,
 const struct remesa_executor_view *views,unsigned count)
{
 struct remesa_retained_session *r=opaque;unsigned i;
 if(count!=REMESA_ADAPTER_ROLES)return -EINVAL;
 for(i=0;i<count;i++)if(views[i].binding.id!=r->views[i].binding.id ||
  views[i].binding.va!=r->views[i].binding.va || views[i].binding.bytes!=r->views[i].binding.bytes ||
  views[i].pages!=r->views[i].pages || views[i].page_count!=r->views[i].page_count ||
  views[i].cpu!=r->views[i].cpu)return -EACCES;
 return retained_verify(r,r->snapshot.dev,t,r->views,count,r->snapshot.mmu_end);
}
int remesa_retained_session_create(struct remesa_gem_memory *s,struct remesa_memory_ticket t,
 remesa_retained_images images,void *profile,u32 revision,u32 ticks,u32 samples,
 struct remesa_retained_session **out)
{
 struct remesa_retained_session *r;struct retained_job *j;
 struct remesa_executor_view views[REMESA_ADAPTER_ROLES];unsigned i;int e=-ENOENT;
 if(!out)return -EINVAL;
 *out=NULL;
 if(!s || !images || !profile)return -EINVAL;
 mutex_lock(&s->lock);j=ticket_job(s,t);if(!j)goto done;
 if(j->state!=REMESA_JOB_PREPARED || j->attached || s->executor){e=-EBUSY;goto done;}
 if(j->count!=REMESA_ADAPTER_ROLES || j->command_bytes!=sizeof(struct sgx535_frozen_command_pairs)){
  e=-EINVAL;goto done;
 }
 r=kzalloc(sizeof(*r),GFP_KERNEL);if(!r){e=-ENOMEM;goto done;}
 r->memory=s;r->verify_images=images;r->profile=profile;
 e=job_views_locked(s,j,r->views,REMESA_ADAPTER_ROLES);if(e<0)goto failed;
 r->snapshot=(struct remesa_memory_snapshot){s->dev,t,s->root.end+1,j->commands,j->command_bytes,true,true};
#ifdef REMESA_LAB_DEPTH1280_PROFILE
 e=retained_verify(r,s->dev,t,r->views,REMESA_ADAPTER_ROLES,r->snapshot.mmu_end);if(e)goto failed;
 e=remesa_gem_job_publish_inputs_locked(s,&r->snapshot,r->views,REMESA_ADAPTER_ROLES);if(e)goto failed;
#endif
 e=remesa_borrowed_owner_init(&r->borrower,&r->owner,s->dev,t,r->views,
  REMESA_ADAPTER_ROLES,r->snapshot.mmu_end,j->commands,retained_verify,r);if(e)goto failed;
 for(i=0;i<REMESA_ADAPTER_ROLES;i++)views[i]=(struct remesa_executor_view){
  r->views[i].binding,r->views[i].pages,r->views[i].page_count,r->views[i].cpu};
 e=remesa_gem_executor_bind(&r->adapter,&r->owner,t,views,REMESA_ADAPTER_ROLES,
  retained_adapter_verify,r,revision,ticks,samples);
 if(e){int cleanup=remesa_borrowed_owner_deinit(&r->borrower);if(cleanup)e=cleanup;goto failed;}
 /* Attachment prevents job cancellation/close/retirement before adapter
  * disposal. Installing these ops does NOT enable start: production refuses. */
 j->attached=true;s->executor=&remesa_gem_executor_ops;s->executor_context=&r->adapter;
 *out=r;e=0;goto done;
failed:
 if(r->borrower.live) {
  /* Do not free an owner if partial teardown could leave a callback or alias. */
  e=job_fault(s,j,e);j->attached=true;s->executor_context=&r->adapter;
 }else kfree(r);
done:mutex_unlock(&s->lock);return e;
}
int remesa_retained_session_snapshots(struct remesa_retained_session *r,struct remesa_retained_snapshots *out)
{
#ifndef REMESA_MEMORY_TEST
 if(!r || !out)return -EINVAL;
 return remesa_gem_executor_snapshots(&r->adapter,out);
#elif defined(REMESA_RETAINED_IOCTL_TEST)
 extern int test_retained_snapshots(struct remesa_retained_snapshots *);
 (void)r;return test_retained_snapshots(out);
#else
 (void)r;(void)out;return -EOPNOTSUPP;
#endif
}
int remesa_retained_session_detach(struct remesa_retained_session *r)
{
 struct remesa_gem_memory *s;struct retained_job *j;int e=-ENOENT;
 if(!r)return -EINVAL;
 s=r->memory;mutex_lock(&s->lock);
 j=ticket_job(s,r->snapshot.ticket);if(!j)goto done;
 if(!j->attached || s->executor_context!=&r->adapter){e=-EACCES;goto done;}
 if(j->state!=REMESA_JOB_PREPARED && j->state!=REMESA_JOB_ACKNOWLEDGED){e=-EBUSY;goto done;}
 e=remesa_gem_executor_dispose(&r->adapter);if(e)goto done;
 e=remesa_borrowed_owner_deinit(&r->borrower);if(e){e=job_fault(s,j,e);goto done;}
 j->attached=false;s->executor=NULL;s->executor_context=NULL;kfree(r);e=0;
done:mutex_unlock(&s->lock);return e;
}
#endif

/* Device-exclusive resource laboratory: server-issued context, per-file owner.
 * This is not an execution context and cannot generate completion receipts. */
static DEFINE_MUTEX(endpoint_lock);
static struct remesa_gem_memory *endpoint;
static u64 next_context;
#ifndef REMESA_MEMORY_TEST
/* Read-only persistent VMA; access is permitted only inside a successful
 * existing CPU READ lease, after trusted GPU publication and before UNLEASE.
 * VMA->vm_file holds the DRM file; VMA counts prevent object destruction.
 * No imported pages, arbitrary PFNs, CODE/input mapping or CPU write access. */
static bool color_readable(struct remesa_gem_memory *s,struct backing *b)
{
 unsigned i;
 if(!memory_enabled || !b->live || !b->pinned || !b->reserved ||
    b->quarantine || !b->lease || b->write || b->holds ||
    b->gem.size!=REMESA_COLOR_VIEW_BYTES ||
    b->va.start!=REMESA_COLOR_VIEW_GPU_VA ||
    b->inserted!=REMESA_COLOR_VIEW_PAGES)return false;
 for(i=0;i<8;i++)if(s->jobs[i].live)return false;
 return true;
}
static void color_vma_open(struct vm_area_struct *vma)
{
 struct backing *b=vma->vm_private_data;
 atomic_inc(&b->color_vmas);
}
static void color_vma_close(struct vm_area_struct *vma)
{
 struct backing *b=vma->vm_private_data;
 atomic_dec(&b->color_vmas);
}
static vm_fault_t color_fault(struct vm_fault *vmf)
{
 struct vm_area_struct *vma=vmf->vma;
 struct backing *b=vma->vm_private_data;
 struct remesa_gem_memory *s=b->memory;
 unsigned long index;
 vm_fault_t result=VM_FAULT_SIGBUS;
 if(vmf->pgoff<REMESA_COLOR_VIEW_PGOFF)return result;
 index=vmf->pgoff-REMESA_COLOR_VIEW_PGOFF;
 if(index>=REMESA_COLOR_VIEW_PAGES || (vmf->flags&FAULT_FLAG_WRITE))return result;
 mutex_lock(&s->lock);
 if(color_readable(s,b)) {
  /* Install under the lease lock. Returning vmf->page would let UNLEASE zap
   * first and the core install a stale PTE after revocation. VM_MIXEDMAP was
   * set in mmap, as Linux5.10 requires for vm_insert_page from a fault. */
  result=vmf_insert_page(vma,vmf->address,b->pages[index]);
 }
 mutex_unlock(&s->lock);return result;
}
static const struct vm_operations_struct color_vm_ops={
 .open=color_vma_open,.close=color_vma_close,.fault=color_fault,
};
int remesa_gem_color_mmap(struct file *filp,struct vm_area_struct *vma)
{
 struct drm_file *file=filp->private_data;
 struct remesa_gem_memory *s;
 struct backing *b=NULL;
 unsigned i;
 int e=-EINVAL;
 /* Preserve the original GEM mmap path for every other namespace. Offsets
  * within our reserved range never fall through into a different BO. */
 if(vma->vm_pgoff<REMESA_COLOR_VIEW_PGOFF ||
    vma->vm_pgoff>=REMESA_COLOR_VIEW_PGOFF+REMESA_COLOR_VIEW_PAGES)
  return drm_gem_mmap(filp,vma);
 if(!file || !capable(CAP_SYS_ADMIN))return -EPERM;
 if(vma->vm_pgoff!=REMESA_COLOR_VIEW_PGOFF ||
    vma->vm_end-vma->vm_start!=REMESA_COLOR_VIEW_BYTES ||
    !(vma->vm_flags&VM_SHARED) || !(vma->vm_flags&VM_READ) ||
    (vma->vm_flags&(VM_WRITE|VM_EXEC)))return -EINVAL;
 mutex_lock(&endpoint_lock);s=endpoint;
 if(!s || s->file!=file){e=-EACCES;goto done;}
 mutex_lock(&s->lock);
 for(i=0;i<OBJECTS;i++)if(s->bo[i].live && s->bo[i].reserved &&
    s->bo[i].va.start==REMESA_COLOR_VIEW_GPU_VA){b=&s->bo[i];break;}
 if(!b || !color_readable(s,b)){e=-EBUSY;goto unlock;}
 if(b->color_mapping && b->color_mapping!=filp->f_mapping){e=-EACCES;goto unlock;}
 b->color_mapping=filp->f_mapping;
 vma->vm_flags&=~(VM_MAYWRITE|VM_MAYEXEC);
 vma->vm_flags|=VM_DONTEXPAND|VM_DONTDUMP|VM_MIXEDMAP;
 vma->vm_private_data=b;vma->vm_ops=&color_vm_ops;
 color_vma_open(vma);e=0;
unlock:mutex_unlock(&s->lock);
done:mutex_unlock(&endpoint_lock);return e;
}

#endif

#ifdef REMESA_LAB_DEPTH1280_PROFILE
#include "remesa_depth1280_capture.inc"
#include "remesa_depth1280_profile.h"
#endif
#if !defined(REMESA_MEMORY_TEST) || defined(REMESA_RETAINED_IOCTL_TEST)
#include "remesa_retained_submit.h"
#include "remesa_retained_profile.h"
#include "remesa/retained_uapi.h"
static struct remesa_retained_submit retained_entry;
static u64 retained_mmu_end;
int remesa_retained_submit_ioctl(struct drm_device *dev,void *data,struct drm_file *file)
{
 struct remesa_retained_request *r=data;struct remesa_memory_binding uses[REMESA_RETAINED_ROLES];
 struct remesa_retained_result result;unsigned i;int e=-EINVAL;
 if(!r || !dev || !file || !capable(CAP_SYS_ADMIN))return -EPERM;
 if(!remesa_retained_lab_admitted())return -EOPNOTSUPP;
 if(r->version!=REMESA_RETAINED_VERSION || r->flags || r->reserved[0] || r->reserved[1] ||
    r->operation || r->serial || r->slot || r->generation || r->state || r->error || r->admitted || r->attached || r->observer_request_bytes || r->observer_response_bytes)return -EINVAL;
 for(i=0;i<sizeof(r->observer_request);i++)if(r->observer_request[i])return -EINVAL;
 for(i=0;i<sizeof(r->observer_response);i++)if(r->observer_response[i])return -EINVAL;
 for(i=0;i<REMESA_RETAINED_ROLES;i++){if(r->bindings[i].reserved)return -EINVAL;
  uses[i]=(struct remesa_memory_binding){r->bindings[i].id,r->bindings[i].va,r->bindings[i].bytes};}
 mutex_lock(&endpoint_lock);
 if(!endpoint || endpoint->dev!=dev || endpoint->file!=file || endpoint->context!=r->context){e=-EACCES;goto done;}
 if(endpoint->stream_bound){e=-EBUSY;goto done;}
 if(!retained_entry.initialized){
  if(retained_entry.attempted || retained_entry.closing){e=-EBUSY;goto done;}
  retained_mmu_end=endpoint->root.end+1;
#ifdef REMESA_LAB_DEPTH1280_PROFILE
  e=remesa_retained_submit_init(&retained_entry,endpoint,remesa_retained_depth1280_images,
#elif defined(REMESA_EXPERIMENTAL_GREEN_FRAGMENT) && REMESA_EXPERIMENTAL_GREEN_FRAGMENT == 1
  e=remesa_retained_submit_init(&retained_entry,endpoint,remesa_retained_green_fragment_images,
#else
  e=remesa_retained_submit_init(&retained_entry,endpoint,remesa_retained_triangle_images,
#endif
    &retained_mmu_end,0x00010201,10*HZ,100000);if(e)goto done;
 }
#ifdef REMESA_LAB_DEPTH1280_PROFILE
 if(retained_entry.attempted || retained_entry.closing){e=-EBUSY;goto done;}
 e=depth1280_capture_prepare(dev,file,r->context,uses[SGX535_BO_COLOR].id,uses[SGX535_BO_Z].id);if(e)goto done;
#endif
 e=remesa_retained_submit_once(&retained_entry,uses,REMESA_RETAINED_ROLES,r->commands,sizeof(r->commands),10000,&result);
#ifdef REMESA_LAB_DEPTH1280_PROFILE
 if(!e && !result.error && result.state==REMESA_SEQUENCE_RETIRED && !result.attached &&
    depth_capture.complete && retained_entry.snapshots.response_bytes) depth_capture.ready=true;
#endif
 r->operation=result.fence.ticket.operation;r->serial=result.fence.serial;
 r->slot=result.fence.ticket.slot;r->generation=result.fence.ticket.generation;
 r->state=result.state;r->error=result.error;r->admitted=result.admitted;r->attached=result.attached;
 r->observer_request_bytes=retained_entry.snapshots.request_bytes;
 r->observer_response_bytes=retained_entry.snapshots.response_bytes;
 memcpy(r->observer_request,retained_entry.snapshots.request,sizeof(r->observer_request));
 memcpy(r->observer_response,retained_entry.snapshots.response,sizeof(r->observer_response));
 /* Existing observer closes with ioctlresult0 after admission. Actual failure
  * remains structured; no zero ioctl is completion evidence. */
 if(result.admitted)e=0;
done:mutex_unlock(&endpoint_lock);return e;
}


#endif
int remesa_gem_memory_ioctl(struct drm_device *dev,void *data,struct drm_file *file)
{
 struct remesa_memory_request *r=data;int e=-EINVAL;
 if(!r || !dev || !file || !capable(CAP_SYS_ADMIN)) return -EPERM;
 if(!memory_enabled && r->action!=REMESA_MEM_CLOSE) return -EOPNOTSUPP;
 mutex_lock(&endpoint_lock);
 if(r->action==REMESA_MEM_OPEN || r->action==REMESA_MEM_CLOSE) {
  unsigned i;
  if(r->version!=REMESA_MEMORY_VERSION || r->id || r->flags || r->bytes ||
     r->va || r->offset || r->length || r->reserved) goto done;
  for(i=0;i<REMESA_MEMORY_CHUNK;i++) if(r->data[i]) goto done;
  if(r->action==REMESA_MEM_OPEN) {
   if(r->context) goto done;
   if(endpoint || next_context==~(u64)0) {e=-EBUSY;goto done;}
   e=remesa_gem_memory_open(dev,file,++next_context,&endpoint);
   if(!e) r->context=endpoint->context;
  } else {
   if(!endpoint || endpoint->dev!=dev || endpoint->file!=file ||
      endpoint->context!=r->context) {e=-EACCES;goto done;}
   #if !defined(REMESA_MEMORY_TEST) || defined(REMESA_RETAINED_IOCTL_TEST)
   if(retained_entry.initialized){e=remesa_retained_submit_close(&retained_entry);if(e)goto done;}
#endif
   e=remesa_gem_memory_close(endpoint);if(!e) {
    endpoint=NULL;
#ifdef REMESA_LAB_DEPTH1280_PROFILE
    depth1280_capture_clear();
#endif
   }
  }
 } else if(!endpoint || endpoint->dev!=dev) e=-ENODEV;
 else e=remesa_gem_memory_request(endpoint,file,r);
done:mutex_unlock(&endpoint_lock);return e;
}
void remesa_gem_memory_postclose(struct drm_device *dev,struct drm_file *file)
{
 unsigned i;mutex_lock(&endpoint_lock);
 if(endpoint && endpoint->dev==dev && endpoint->file==file) {
  /* Leases cannot coexist with job holds. FD death ends CPU leases only;
   * close refuses issued/unretired work and retains device/module ownership. */
  for(i=0;i<OBJECTS;i++) if(endpoint->bo[i].lease) {
   if(!endpoint->bo[i].lease_borrowed)vunmap(endpoint->bo[i].cpu);
   endpoint->bo[i].cpu=NULL;endpoint->bo[i].lease=false;endpoint->bo[i].lease_borrowed=false;
  }
  #if !defined(REMESA_MEMORY_TEST) || defined(REMESA_RETAINED_IOCTL_TEST)
  if(retained_entry.initialized && remesa_retained_submit_close(&retained_entry)) {endpoint->file=NULL;goto post_done;}
#endif
  if(!remesa_gem_memory_close(endpoint)) {
   endpoint=NULL;
#ifdef REMESA_LAB_DEPTH1280_PROFILE
   depth1280_capture_clear();
#endif
  }
  else endpoint->file=NULL; /* No dangling drm_file after quarantine. */
 }
#if !defined(REMESA_MEMORY_TEST) || defined(REMESA_RETAINED_IOCTL_TEST)
post_done:
#endif
 mutex_unlock(&endpoint_lock);
}

#ifdef REMESA_LAB_DEPTH1280_PROFILE
#include "remesa_gem_publish_inputs.inc"
#endif

/* Stream endpoint lease is kernel-only. It prevents CLOSE/postclose/reuse from
 * releasing backing even in the gap before prepare has acquired BO holds. */
int remesa_gem_stream_claim(struct drm_device *dev,struct drm_file *file,u64 context,
 struct remesa_gem_memory **out)
{
 unsigned i;int e=-EACCES;struct remesa_gem_memory *s;
 if(!out)return -EINVAL;
 *out=NULL;mutex_lock(&endpoint_lock);s=endpoint;
 if(!s || s->dev!=dev || s->file!=file || s->context!=context)goto done;
 mutex_lock(&s->lock);
 if(s->stream_bound || s->executor){e=-EBUSY;goto unlock;}
#if !defined(REMESA_MEMORY_TEST) || defined(REMESA_RETAINED_IOCTL_TEST)
 if(retained_entry.initialized || retained_entry.attempted){e=-EBUSY;goto unlock;}
#endif
 for(i=0;i<8;i++)if(s->jobs[i].live){e=-EBUSY;goto unlock;}
 s->stream_bound=true;*out=s;e=0;
unlock:mutex_unlock(&s->lock);
done:mutex_unlock(&endpoint_lock);return e;
}
int remesa_gem_stream_seal_ta_pools(struct remesa_gem_memory *s)
{
 unsigned i,found=0;int e=0;if(!s)return -EINVAL;
 mutex_lock(&s->lock);
 if(!s->stream_bound){e=-EACCES;goto done;}
 for(i=0;i<8;i++)if(s->jobs[i].live){e=-EBUSY;goto done;}
 for(i=0;i<OBJECTS;i++)if(s->bo[i].live&&is_managed_ta_pool(&s->bo[i])){
  struct backing *b=&s->bo[i];
  if(!b->pinned || b->quarantine || b->holds || b->lease || b->inserted!=8192){e=-EBUSY;goto done;}
  found++;
 }
 if(found!=2){e=-ENOENT;goto done;}
 s->ta_pools_sealed=true;
 done:mutex_unlock(&s->lock);return e;
}
int remesa_gem_stream_release(struct remesa_gem_memory *s)
{
 unsigned i;int e=0;if(!s)return -EINVAL;mutex_lock(&s->lock);
 if(!s->stream_bound || s->executor){e=-EBUSY;goto done;}
 for(i=0;i<8;i++)if(s->jobs[i].live){e=-EBUSY;goto done;}
 for(i=0;i<OBJECTS;i++)if(s->bo[i].lease || s->bo[i].holds){e=-EBUSY;goto done;}
 /* Return any deferred TA CPU ownership before endpoint reuse/unpin. This
  * one-off teardown maintenance is outside the render loop. */
 for(i=0;i<OBJECTS;i++)if(ta_domain(s,&s->bo[i]))ta_for_cpu(&s->bo[i]);
 for(i=0;i<OBJECTS;i++)s->bo[i].input_seal=0;
 s->ta_cache_domains=false;
 s->pds_write_guard=NULL;s->stream_bound=false;s->ta_pools_sealed=false;
done:mutex_unlock(&s->lock);return e;
}
int remesa_gem_stream_attach(struct remesa_gem_memory *s,struct remesa_memory_ticket t,
 const struct remesa_job_executor *ops,void *ctx)
{
 struct retained_job *j;int e=-ENOENT;if(!s || !ops || !ctx || !ops->validate ||
 !ops->start || !ops->poll || !ops->visible || !ops->acknowledge)return -EINVAL;
 mutex_lock(&s->lock);j=ticket_job(s,t);if(!j)goto done;
 if(!s->stream_bound || s->executor || j->attached || j->state!=REMESA_JOB_PREPARED){e=-EBUSY;goto done;}
 j->attached=true;s->executor=ops;s->executor_context=ctx;e=0;
done:mutex_unlock(&s->lock);return e;
}
int remesa_gem_stream_detach(struct remesa_gem_memory *s,struct remesa_memory_ticket t,void *ctx)
{
 struct retained_job *j;int e=-ENOENT;if(!s)return -EINVAL;
 mutex_lock(&s->lock);j=ticket_job(s,t);if(!j)goto done;
 if(!s->stream_bound || !j->attached || s->executor_context!=ctx ||
    j->state!=REMESA_JOB_ACKNOWLEDGED){e=-EBUSY;goto done;}
 j->attached=false;s->executor=NULL;s->executor_context=NULL;e=0;
done:mutex_unlock(&s->lock);return e;
}
