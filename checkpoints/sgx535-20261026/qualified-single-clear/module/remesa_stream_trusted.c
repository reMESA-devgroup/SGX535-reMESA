/* SPDX-License-Identifier: GPL-2.0-only */
/* Real retained provider/fixed-service integration. No legacy native admission,
 * no fabricated event ledger, no raw command/MMIO ABI and no rearm. */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/highmem.h>
#include <linux/jiffies.h>
#include <linux/ktime.h>
#include "gears_visible_gate.h"
#include <drm/drm_cache.h>
#include "psb_drv.h"
#include "remesa_stream_bridge.h"
#include "remesa_blake3_namespace.h"
#include "blake3.h"
#include "remesa_blake3_output.h"
#include "remesa_stream_trusted.h"
#include "stream_profile_asset.h"
#include "remesa_stream_grammar.h"
#include "remesa_canonical_inputs.h"
#include "remesa_pds_write_guard.h"
#include "remesa_stream_source.h"
#include "remesa_gem_memory.h"
#include "remesa_borrowed_owner.h"
#include "remesa_depth1280_profile.h"
#include "gma500_bo_owner.h"
#include "gma500_fixed_backend.h"
#include "remesa_stream_contract_api.h"
#include "remesa_stream_contract_aliases.h"
struct trusted_operation {
 struct remesa_memory_ticket ticket;
 struct remesa_memory_snapshot snapshot;
 struct remesa_memory_view views[11];
 struct sgx535_gma500_owner owner;
 struct remesa_borrowed_owner borrower;
 struct sgx535_gma500_fixed_backend backend;
 struct remesa_stream_observation observation;
 struct remesa_stream_receipt receipt;
 u8 source_digest[32],image_digest[32];
 u64 begin_ns,freeze_ns,check_ns,clear_ns,service_ns,issue_ns,publish_ns,hash_ns,source_ns;
 u64 prepare_ns,snapshot_ns,aliases_ns,visible_gate_ns,role_clear_ns[6];
 bool attached,started,completed,visible,acknowledged,inputs_authenticated;
 unsigned input_check_calls,input_hash_passes,input_byte_passes;
};
struct trusted_stream {
 struct drm_device *dev;struct drm_file *file;
 struct remesa_gem_memory *memory;
 struct trusted_operation *op;
 u64 session,sequence,generation;
 bool claimed,quarantined,fast_blake3,fast_nohash,inputs_sealed;
 blake3_hasher output_hasher; /* Serialized by stream_lock; no large kernel stack allocation. */
};
static struct trusted_stream stream;
static struct remesa_stream_profile profile;
static bool profile_present;
int remesa_stream_profile_install(const struct remesa_stream_profile *p)
{
 unsigned i;
 if(!p || profile_present || stream.claimed || !p->count || p->count>64 ||
    !memchr_inv(p->digest,0,32))return -EINVAL;
 for(i=0;i<11;i++)if(!p->bindings[i].bytes || (p->bindings[i].va&4095) ||
   (p->bindings[i].bytes&4095))return -EINVAL;
 profile=*p;profile_present=true;return 0;
}
int remesa_stream_trusted_owner(struct drm_device *dev,struct drm_file *file)
{
 if(stream.claimed || stream.quarantined)return -EBUSY;
 stream.dev=dev;stream.file=file;return 0;
}
static u64 diagnostic_clock(const struct trusted_stream *s)
{return s->fast_nohash?0:ktime_get_ns();}
static int guarded_pds_write(u64 offset,const void *data,unsigned bytes)
{return remesa_canonical_pds_page(profile.batches,profile.count,
 remesa_canonical_inputs[0],offset,data,bytes);}
/* The real owner_flush route is stream-source-owned and serialized. Return
 * handled=false for LAB/FAST7. No callback pointer is exposed or registered. */
int remesa_stream_trusted_prefire_cache(struct sgx535_gma500_owner *owner,bool *handled)
{
 struct trusted_stream *s=&stream;struct trusted_operation *o=s->op;int e;
 if(!handled || !o || &o->owner!=owner || !s->claimed)return -EACCES;
 *handled=false;if(!s->fast_nohash)return 0;
 e=remesa_gem_job_prefire_cache_locked(s->memory,o->ticket);
 if(!e)*handled=true;return e;
}
static int check_images(struct trusted_stream *s,struct trusted_operation *o,bool locked)
{
 unsigned i;int e;u8 hashes[5][32],digest[32];unsigned char *masked;
 o->input_check_calls++;
 if(o->started)return -EBUSY;
 e=locked?remesa_gem_job_check_snapshot_locked(s->memory,&o->snapshot,o->views,11):
  remesa_gem_job_check_snapshot(s->memory,&o->snapshot,o->views,11);
 if(e)return e;
 if(o->snapshot.mmu_end!=0xe0000000ULL || o->snapshot.command_bytes!=264 ||
    memcmp(o->snapshot.commands,&profile.commands,264))return -EKEYREJECTED;
 for(i=0;i<11;i++)if(o->views[i].binding.va!=profile.bindings[i].va ||
    o->views[i].binding.bytes!=profile.bindings[i].bytes)return -EKEYREJECTED;
 /* The same prepared job owns all11 holds, excludes every CPU lease and
  * has not started hardware. Constructors between calls only read inputs.
  * Recheck snapshot/commands/PTE/address/size above on EVERY call, but reuse
  * the content authentication for this operation only. New kzalloc op
  * starts unauthenticated; LAB deliberately retains every full pass. */
 if((s->fast_blake3 || s->fast_nohash) && o->inputs_authenticated)return 0;
 if(s->fast_nohash) {
  if(s->inputs_sealed)return 0;
  if(locked)return -EPROTO; /* first authentication must not relock endpoint */
  o->input_byte_passes++;
  for(i=0;i<5;i++) {
   if(o->views[i].binding.bytes!=remesa_canonical_input_sizes[i])return -EKEYREJECTED;
   if(i==SGX535_BO_PDS) {
    masked=vmalloc(o->views[i].binding.bytes);if(!masked)return -ENOMEM;
    memcpy(masked,o->views[i].cpu,o->views[i].binding.bytes);
    e=remesa_stream_mask_sources(profile.batches,profile.count,masked,o->views[i].binding.bytes,o->views[i].binding.va);
    if(!e && memcmp(masked,remesa_canonical_inputs[i],o->views[i].binding.bytes))e=-EKEYREJECTED;
    vfree(masked);if(e)return e;
   }else if(memcmp(o->views[i].cpu,remesa_canonical_inputs[i],o->views[i].binding.bytes))return -EKEYREJECTED;
  }
  e=remesa_stream_validate_geometry(profile.batches,profile.count,o->views[SGX535_BO_VERTEX_TA].cpu,o->views[SGX535_BO_VERTEX_TA].binding.bytes);
  if(!e)e=remesa_gem_stream_seal_inputs(s->memory,o->ticket,guarded_pds_write);
  if(!e){s->inputs_sealed=true;o->inputs_authenticated=true;}return e;
 }
 o->input_hash_passes++;
 for(i=0;i<5;i++) {
  e=remesa_depth1280_sha256(o->views[i].cpu,o->views[i].binding.bytes,hashes[i]);if(e)return e;
  if(i==SGX535_BO_PDS) {
   masked=vmalloc(o->views[i].binding.bytes);if(!masked)return -ENOMEM;
   memcpy(masked,o->views[i].cpu,o->views[i].binding.bytes);
   e=remesa_stream_mask_sources(profile.batches,profile.count,masked,o->views[i].binding.bytes,o->views[i].binding.va);
   if(!e)e=remesa_depth1280_sha256(masked,o->views[i].binding.bytes,digest);
   vfree(masked);if(e)return e;
  } else memcpy(digest,hashes[i],32);
  if(memcmp(digest,profile.images[i],32))return -EKEYREJECTED;
 }
 e=remesa_stream_validate_geometry(profile.batches,profile.count,o->views[SGX535_BO_VERTEX_TA].cpu,
  o->views[SGX535_BO_VERTEX_TA].binding.bytes);if(e)return e;
 e=remesa_depth1280_sha256(hashes,sizeof(hashes),o->image_digest);
 if(!e)o->inputs_authenticated=true;return e;
}
static int verify_borrower(void *ctx,struct drm_device *dev,struct remesa_memory_ticket t,
 const struct remesa_memory_view *v,unsigned n,u64 mmu_end)
{
 struct trusted_stream *s=ctx;struct trusted_operation *o=s->op;
 if(!o || dev!=s->dev || n!=11 || mmu_end!=o->snapshot.mmu_end ||
    memcmp(&t,&o->ticket,sizeof(t)) || v!=o->views)return -EINVAL;
 return check_images(s,o,false);
}
static int claim(void *ctx,const struct remesa_stream_request *r)
{
 struct trusted_stream *s=ctx;struct drm_psb_private *dev_priv;unsigned i;int e;
 /* Absence of reviewed GPU images remains explicit, despite installed ops. */
 if(!profile_present)return -EOPNOTSUPP;
 if(s->claimed || s->quarantined || !s->dev || !s->file ||
    memcmp(r->profile_sha256,profile.digest,32))return -EACCES;
 for(i=0;i<11;i++)if(r->bindings[i].va!=profile.bindings[i].va ||
   r->bindings[i].bytes!=profile.bindings[i].bytes)return -EKEYREJECTED;
 e=remesa_gem_stream_claim(s->dev,s->file,r->context,&s->memory);if(e)return e;
 /* Endpoint lease already owns a live power/device/module reference. */
 dev_priv=s->dev->dev_private;
 if(PSB_RSGX32(PSB_CR_CORE_ID)!=0x01130000 ||
    PSB_RSGX32(PSB_CR_CORE_REVISION)!=0x00010201){e=-ENODEV;goto release;}
 if(r->action==REMESA_STREAM_OPEN_FAST_BLAKE3 || r->action==REMESA_STREAM_OPEN_FAST_NOHASH){
  e=remesa_gem_stream_seal_ta_pools(s->memory);if(e)goto release;
 }
 if(r->action==REMESA_STREAM_OPEN_FAST_NOHASH){
  e=remesa_gem_stream_enable_ta_domains(s->memory);if(e)goto release;
 }
 e=remesa_stream_source_claim(s->dev,s);if(e)goto release;
 s->session=r->session;s->claimed=true;s->fast_blake3=r->action==REMESA_STREAM_OPEN_FAST_BLAKE3;s->fast_nohash=r->action==REMESA_STREAM_OPEN_FAST_NOHASH;return 0;
release:remesa_gem_stream_release(s->memory);s->memory=NULL;return e;
}
static const struct remesa_job_executor executor;
static int freeze(void *ctx,const struct remesa_stream *r)
{
 struct trusted_stream *s=ctx;struct trusted_operation *o;struct remesa_memory_binding uses[11];
 unsigned i;int e;u64 mark;
 if(!s->claimed || s->op || s->quarantined || r->count!=profile.count)return -EINVAL;
 for(i=0;i<r->count;i++) {
  struct remesa_stream_batch b=r->batches[i],expected=profile.batches[i];
  if(b.image_generation!=r->next_generation)return -ESTALE;
  b.image_generation=0;expected.image_generation=0;
  if(memcmp(&b,&expected,sizeof(b)))return -EKEYREJECTED;
 }
 o=kzalloc(sizeof(*o),GFP_KERNEL);if(!o)return -ENOMEM;s->op=o;o->begin_ns=diagnostic_clock(s);
 s->sequence=r->next_sequence;s->generation=r->next_generation;
 for(i=0;i<11;i++)uses[i]=(struct remesa_memory_binding){r->profile.bindings[i].id,
  r->profile.bindings[i].va,r->profile.bindings[i].bytes};
 mark=diagnostic_clock(s);e=remesa_gem_job_prepare(s->memory,uses,11,&profile.commands,264,&o->ticket);o->prepare_ns=diagnostic_clock(s)-mark;if(e)return e;
 if(o->ticket.operation>0xffffffffULL)return -EOVERFLOW;
 mark=diagnostic_clock(s);e=remesa_gem_job_snapshot(s->memory,o->ticket,&o->snapshot,o->views,11);o->snapshot_ns=diagnostic_clock(s)-mark;if(e!=11)return e<0?e:-EPROTO;
 mark=diagnostic_clock(s);e=check_images(s,o,false);o->check_ns+=diagnostic_clock(s)-mark;if(e)return e;
 if(s->fast_nohash){e=remesa_gem_job_ta_policy(s->memory,o->ticket,s->sequence>3);if(e)return e;}
 /* Initialize every mutable engine/output object only while provider holds
  * exclude all CPU leases, before any publication or hardware action. */
 mark=diagnostic_clock(s);
 for(i=5;i<10;i++){
  u64 clear_mark=diagnostic_clock(s);
  if(!((s->fast_blake3 || s->fast_nohash) && s->sequence>3 && (i==7||i==8)))
   memset(o->views[i].cpu,0,o->views[i].binding.bytes);
  o->role_clear_ns[i-5]=diagnostic_clock(s)-clear_mark;
 }
 /* Z24 clear is explicit for the entire viewport; high X8 is zero. */
 {
  u64 clear_mark=diagnostic_clock(s);u32 *z=o->views[10].cpu;for(i=0;i<1024000;i++)z[i]=0x00ffffff;
  o->role_clear_ns[5]=diagnostic_clock(s)-clear_mark;
 }
 o->clear_ns=diagnostic_clock(s)-mark;
 e=remesa_borrowed_owner_init(&o->borrower,&o->owner,s->dev,o->ticket,o->views,11,
  o->snapshot.mmu_end,&profile.commands,verify_borrower,s);if(e)return e;
 /* Fixed publisher consumes private aliases, independently from job views. */
 mark=diagnostic_clock(s);
 for(i=0;i<=SGX535_BO_COLOR;i++) {
  o->owner.objects[i].cpu=vmap(o->views[i].pages,o->views[i].page_count,VM_MAP,PAGE_KERNEL);
  if(!o->owner.objects[i].cpu)return -ENOMEM;
  o->owner.views[i].bytes=o->owner.objects[i].cpu;o->owner.views[i].length=o->views[i].binding.bytes;
 }
 o->aliases_ns=diagnostic_clock(s)-mark;
 e=remesa_stream_source_bind(s,&o->observation,&o->backend,o->ticket.operation,s->sequence);if(e)return e;
 e=remesa_gem_stream_attach(s->memory,o->ticket,&executor,s);if(e)return e;
 o->attached=true;o->freeze_ns=diagnostic_clock(s)-o->begin_ns;return 0;
}
static int validate(void *ctx,const void *commands,unsigned bytes,
 const struct remesa_memory_binding *bindings,unsigned count)
{
 struct trusted_stream *s=ctx;struct trusted_operation *o=s->op;unsigned i;int e;u64 mark;
 if(!o || o->started || bytes!=264 || count!=11 || memcmp(commands,&profile.commands,264))return -EINVAL;
 for(i=0;i<11;i++)if(bindings[i].id!=o->views[i].binding.id || bindings[i].va!=o->views[i].binding.va || bindings[i].bytes!=o->views[i].binding.bytes)return -EACCES;
 mark=diagnostic_clock(s);e=check_images(s,o,true);o->check_ns+=diagnostic_clock(s)-mark;return e;
}
static int start(void *ctx,struct remesa_memory_ticket t,const void *commands,unsigned bytes,u64 *cookie)
{
 struct trusted_stream *s=ctx;struct trusted_operation *o=s->op;int e;u64 mark;
 if(!o || o->started || memcmp(&t,&o->ticket,sizeof(t)) || bytes!=264 ||
    memcmp(commands,&profile.commands,264))return -EINVAL;
 o->started=true;*cookie=t.operation;
 mark=diagnostic_clock(s);e=sgx535_gma500_fixed_backend_begin(&o->backend,&o->owner);if(e)return e;
 e=sgx535_fixed_service_run_once(&o->owner.fixed_service,&sgx535_gma500_fixed_backend_ops,
  &sgx535_gma500_fixed_backend_status_source,&o->backend,0x00010201,t.operation,10*HZ,100000);
 o->service_ns=diagnostic_clock(s)-mark;if(e)return e;
 if(o->owner.session.sequence!=t.operation || o->owner.session.phase!=SGX535_PHASE_RETIRED ||
    o->owner.session.observed_events!=7 || !sgx535_fixed_service_may_release(&o->owner.fixed_service))return -EPROTO;
 o->completed=true;return 0;
}
static int poll(void *ctx,struct remesa_memory_ticket t,u64 cookie,unsigned budget,struct remesa_job_completion *out)
{
 struct trusted_stream *s=ctx;struct trusted_operation *o=s->op;
 if(!o || !o->completed || !budget || memcmp(&t,&o->ticket,sizeof(t)) || cookie!=t.operation)return -EINVAL;
 *out=(struct remesa_job_completion){t,cookie,o->owner.session.observed_events};return 0;
}
static int visible(void *ctx,struct remesa_memory_ticket t,u64 cookie)
{
 struct trusted_stream *s=ctx;struct trusted_operation *o=s->op;
 if(!o || !o->completed || o->visible || cookie!=t.operation || memcmp(&t,&o->ticket,sizeof(t)) ||
   o->owner.session.phase!=9 || !sgx535_fixed_service_may_release(&o->owner.fixed_service))return -EIO;
 /* Same fixed-profile visibility contract as the qualified retained path:
  * attributable end-render + memory-free precede provider CLFLUSH/dma_rmb.
  * General cache visibility for arbitrary programs is not authorized. */
 o->visible=true;return 0;
}
static int acknowledge(void *ctx,struct remesa_memory_ticket t,u64 cookie)
{
 struct trusted_stream *s=ctx;struct trusted_operation *o=s->op;int e;
 if(!o || !o->visible || o->acknowledged || cookie!=t.operation || memcmp(&t,&o->ticket,sizeof(t)))return -EINVAL;
 e=sgx535_gma500_fixed_backend_end(&o->backend);if(e)return e;
 o->acknowledged=true;return 0;
}
static const struct remesa_job_executor executor={validate,start,poll,visible,acknowledge};
static int execute(void *ctx,struct remesa_stream_receipt *r)
{
 struct trusted_stream *s=ctx;struct trusted_operation *o=s->op;int e;u64 mark;
 if(!o || r->sequence!=s->sequence || r->generation!=s->generation)return -EINVAL;
 mark=diagnostic_clock(s);e=remesa_gem_job_issue(s->memory,o->ticket);if(e)return e;
 o->issue_ns=diagnostic_clock(s)-mark;mark=diagnostic_clock(s);e=remesa_gem_job_wait(s->memory,o->ticket,10000);if(e)return e;
 e=remesa_gem_job_publish(s->memory,o->ticket);if(e)return e;
 o->publish_ns=diagnostic_clock(s)-mark;mark=diagnostic_clock(s);
 /* FULL image hashes are taken from live held provider views after actual
  * retirement/publication. No client digest or prefix is completion evidence. */
 e=s->fast_nohash?0:remesa_gears_visible_gate(o->views[5].cpu,o->views[10].cpu);o->visible_gate_ns=diagnostic_clock(s)-mark;mark=diagnostic_clock(s);if(e)return -e;
 if(s->fast_nohash){
  memset(r->color_hash,0,32);memset(r->depth_hash,0,32);
 }else if(s->fast_blake3){
  remesa_blake3_output(&s->output_hasher,o->views[5].cpu,r->color_hash);
  remesa_blake3_output(&s->output_hasher,o->views[10].cpu,r->depth_hash);
 }else{
 e=remesa_depth1280_sha256(o->views[5].cpu,4096000,r->color_hash);if(e)return e;
 e=remesa_depth1280_sha256(o->views[10].cpu,4096000,r->depth_hash);if(e)return e;
 }
 o->hash_ns=diagnostic_clock(s)-mark;mark=diagnostic_clock(s);e=s->fast_nohash?remesa_stream_source_finish_nohash(s,&o->observation):remesa_stream_source_finish(s,&o->observation,o->views[5].cpu);if(e)return e;
 o->source_ns=diagnostic_clock(s)-mark;
 r->operation=o->ticket.operation;r->phase=o->owner.session.phase;
 r->ledger=o->owner.session.observed_events;r->proof=s->fast_nohash?(REMESA_STREAM_PROOF_NOHASH&~64u):(63|(s->fast_blake3?REMESA_STREAM_PROOF_BLAKE3:0));o->receipt=*r;
 return 0;
}
static int settle(void *ctx)
{
 struct trusted_stream *s=ctx;struct trusted_operation *o=s->op;unsigned i;int e;u64 mark;
 struct source_proof {
  u32 version,boot,state,reasons,producers,prepared,asserted,released;
  u32 startup_pending,startup_reads,startup_fault,startup_autonomous;
  u32 count,zero_samples;
  struct remesa_stream_source_event events[REMESA_STREAM_AUDIT_EVENTS];
  u8 capsule[SGX535_CAPSULE_BYTES];
 } *encoded;
 if(!o || !o->visible)return -EINVAL;
 mark=diagnostic_clock(s);e=remesa_gem_job_acknowledge(s->memory,o->ticket);if(e)return e;
 e=remesa_stream_source_unbind(s,&o->observation);if(e)return e;
 if(!s->fast_nohash){
 encoded=kzalloc(sizeof(*encoded),GFP_KERNEL);if(!encoded)return -ENOMEM;
 encoded->version=2;encoded->boot=o->observation.boundary.boot;encoded->state=o->observation.boundary.state;
 encoded->reasons=o->observation.boundary.reasons;encoded->producers=o->observation.boundary.producers_active;
 encoded->prepared=o->observation.boundary.prepared;encoded->asserted=o->observation.boundary.asserted_reset;
 encoded->released=o->observation.boundary.released_reset;
 encoded->startup_pending=o->observation.boundary.startup_pending;
 encoded->startup_reads=o->observation.boundary.startup_reads;encoded->startup_fault=o->observation.boundary.startup_fault;
 encoded->startup_autonomous=o->observation.boundary.startup_autonomous;
 encoded->count=o->observation.event_count;encoded->zero_samples=o->observation.zero_samples;
 memcpy(encoded->events,o->observation.events,sizeof(encoded->events));
 e=sgx535_capsule_encode(&o->observation.evidence,encoded->capsule,SGX535_CAPSULE_BYTES);
 if(!e)e=remesa_depth1280_sha256(encoded,sizeof(*encoded),o->source_digest);
 kfree(encoded);if(e)return e;
 }
 e=remesa_gem_stream_detach(s->memory,o->ticket,s);if(e)return e;o->attached=false;
 o->owner.fixed_service.observer=NULL;o->owner.fixed_service.observer_context=NULL;
 /* Successful publish consumed aliases. Unexpected survivors cannot be freed. */
 for(i=0;i<=SGX535_BO_COLOR;i++)if(o->owner.objects[i].cpu)return -EBUSY;
 e=remesa_borrowed_owner_deinit(&o->borrower);if(e)return e;
 e=remesa_gem_job_retire(s->memory,o->ticket);if(e)return e;
 e=remesa_stream_source_settled(s);
 if(!e && !s->fast_nohash && s->sequence<=128)pr_info("REMESA_STREAM_TIMING sequence=%llu freeze_ns=%llu checks_ns=%llu clear_ns=%llu service_ns=%llu issue_ns=%llu publication_ns=%llu bulk_output_digest_ns=%llu source_finish_ns=%llu settle_ns=%llu\n",(unsigned long long)s->sequence,(unsigned long long)o->freeze_ns,(unsigned long long)o->check_ns,(unsigned long long)o->clear_ns,(unsigned long long)o->service_ns,(unsigned long long)o->issue_ns,(unsigned long long)o->publish_ns,(unsigned long long)o->hash_ns,(unsigned long long)o->source_ns,(unsigned long long)(diagnostic_clock(s)-mark));
 if(!e && !s->fast_nohash && s->sequence<=128)pr_info("REMESA_STREAM_MEMORY_TIMING sequence=%llu prepare_ns=%llu snapshot_ns=%llu aliases_ns=%llu visible_gate_ns=%llu color_clear_ns=%llu region_clear_ns=%llu table_clear_ns=%llu param_clear_ns=%llu xhw_clear_ns=%llu z_clear_ns=%llu\n",(unsigned long long)s->sequence,(unsigned long long)o->prepare_ns,(unsigned long long)o->snapshot_ns,(unsigned long long)o->aliases_ns,(unsigned long long)o->visible_gate_ns,(unsigned long long)o->role_clear_ns[0],(unsigned long long)o->role_clear_ns[1],(unsigned long long)o->role_clear_ns[2],(unsigned long long)o->role_clear_ns[3],(unsigned long long)o->role_clear_ns[4],(unsigned long long)o->role_clear_ns[5]);
 if(!e && !s->fast_nohash && s->sequence<=128){
  u64 ta=0,render=0;unsigned event_index;
  for(event_index=0;event_index<o->observation.event_count;event_index++){
   const struct remesa_stream_source_event *ev=&o->observation.events[event_index];
   if(ev->kind!=6)continue;
   if(!ta&&(ev->ledger&1))ta=o->observation.event_ns[event_index];
   if(!render&&(ev->ledger&2))render=o->observation.event_ns[event_index];
  }
  pr_info("REMESA_GPU_WALL_TIMING sequence=%llu ta_fire_actions_ns=%llu ta_submit_to_observed_done_ns=%llu raster_fire_actions_ns=%llu raster_submit_to_observed_done_ns=%llu cpu_prefire_flush_ns=%llu\n",(unsigned long long)s->sequence,(unsigned long long)(o->backend.ta_fire_end_ns-o->backend.ta_fire_begin_ns),(unsigned long long)(ta>=o->backend.ta_fire_end_ns?ta-o->backend.ta_fire_end_ns:0),(unsigned long long)(o->backend.raster_fire_end_ns-o->backend.raster_fire_begin_ns),(unsigned long long)(render>=o->backend.raster_fire_end_ns?render-o->backend.raster_fire_end_ns:0),(unsigned long long)o->backend.cpu_flush_ns);
 }
 if(!e&&!s->fast_nohash&&s->sequence<=128)pr_info("REMESA_FROZEN_INPUT_AUDIT sequence=%llu provider_checks=%u content_hash_passes=%u content_byte_passes=%u mode=%s\n",(unsigned long long)s->sequence,o->input_check_calls,o->input_hash_passes,o->input_byte_passes,s->fast_nohash?"FAST_NOHASH":(s->fast_blake3?"FAST":"LAB"));
 return e;
}
static int seal(void *ctx,struct remesa_stream_receipt *r)
{
 struct trusted_stream *s=ctx;struct trusted_operation *o=s->op;int e;
 struct remesa_stream_receipt canonical;
 if(!o || !o->acknowledged || o->attached || !o->observation.finished || r->proof!=(s->fast_nohash?REMESA_STREAM_PROOF_NOHASH:(REMESA_STREAM_PROOF_ALL|(s->fast_blake3?REMESA_STREAM_PROOF_BLAKE3:0))))return -EINVAL;
 e=remesa_stream_source_settled(s);if(e)return e;
 if(s->fast_nohash){
  memset(r->color_hash,0,32);memset(r->depth_hash,0,32);
  memset(r->previous_sha256,0,32);memset(r->source_sha256,0,32);
  memset(r->images_sha256,0,32);memset(r->receipt_sha256,0,32);
  kfree(o);s->op=NULL;return 0;
 }
 memcpy(r->source_sha256,o->source_digest,32);memcpy(r->images_sha256,o->image_digest,32);
 canonical=*r;memset(canonical.receipt_sha256,0,32);
 e=remesa_depth1280_sha256(&canonical,sizeof(canonical),r->receipt_sha256);
 if(!e){kfree(o);s->op=NULL;}return e;
}
static void quarantine(void *ctx)
{
 struct trusted_stream *s=ctx;s->quarantined=true;
 remesa_stream_source_poison(s);
 if(s->op && s->op->ticket.operation)remesa_gem_job_quarantine(s->memory,s->op->ticket);
 /* Retain owner/aliases/provider/module/source lease on every ambiguity. */
}
static int close_stream(void *ctx)
{
 struct trusted_stream *s=ctx;int e;
 if(!s->claimed || s->quarantined || s->op)return -EBUSY;
 e=remesa_stream_source_release(s);if(e)return e;
 e=remesa_gem_stream_release(s->memory);if(e)return e;
 s->claimed=false;s->inputs_sealed=false;s->memory=NULL;return 0;
}
static const struct remesa_stream_ops trusted_ops={claim,freeze,execute,settle,seal,quarantine,close_stream};
int remesa_stream_trusted_install(void)
{int e=remesa_stream_profile_install(&gears_stream_profile_asset);if(e)return e;return remesa_stream_backend_install(&trusted_ops,&stream);}
