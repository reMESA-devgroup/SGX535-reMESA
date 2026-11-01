/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual new admission/controller. Endpoint/GEM/crypto/source/execution hooks
 * modeled; separate integration tests exercise the real producer/consumer. */
#include "scene_probe_types.h"
#include "remesa_scene_probe_profiles.h"
#include "remesa_scene_probe_request.h"
#include "remesa_private_session.h"
enum {OK,OK640,CAP,DISABLED,MEMDISABLED,INVALID,ALLOC,DUPALLOC,BACKING,LOST,POWER,CORE,REV,PDS,
 VA,SIZE,GEN,LEASE,HOLD,PIN,RESERVED,QUAR,VMA,ZERO,CPU_ALIAS,STREAM,EXECUTOR,TA,CEILING,LIVE,
 RESERVE,CLAIM,PREPARE,PREPARE_HELD,HASH,EARLY_EXEC,EXEC,NO_RECEIPT,LOAN_DROP,GUARD_DROP,JOB_DROP,SOURCE_END,BACKING_END,LOST_AFTER_CLAIM,OK_INIT,OK640_INIT,OK_FINAL,OK640_FINAL,OK_GALLIUM,OK640_GALLIUM,VA_GALLIUM,HASH_GALLIUM,WRONG_LEGACY,OK_MATRIX,OK640_MATRIX,VA_MATRIX,HASH_MATRIX,WRONG_WINDOW,CASE_COUNT};
static unsigned fault,checks,reads,allocs,frees,dup_calls,hashes,reserves,claims,executions,source_ends,backing_ends;
static bool administrator=true,memory_enabled=true,powered=true;
static struct mutex endpoint_lock;
static struct remesa_gem_memory *endpoint;
static struct remesa_twod_reservation *current_guard;
static const struct remesa_scene_probe_profile *expected_profile;
static void *images[17];
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"failure line%d: %s (fault%u)\n",__LINE__,#x,fault);abort();}}while(0)
#define DEFINE_MUTEX(name) struct mutex name={0}
#define module_param_named(...)
#define BUILD_BUG_ON(x) _Static_assert(!(x),"build invariant")
#define CAP_SYS_ADMIN 1
#define GFP_KERNEL 0
#define PSB_CR_CORE_ID 0u
#define PSB_CR_CORE_REVISION 4u
#define PSB_CR_PDS_EXEC_BASE 8u
static void mutex_lock(struct mutex *m){CHECK(!m->held);m->held=true;}
static void mutex_unlock(struct mutex *m){CHECK(m->held);m->held=false;}
static bool capable(int cap){CHECK(cap==1);return administrator;}
static bool gma_power_is_on(struct drm_device *d){CHECK(d==endpoint->dev);return powered;}
static u32 model_register(u32 reg)
{reads++;if(reg==0)return fault==CORE?0:0x01130000;if(reg==4)return fault==REV?0:0x10201;CHECK(reg==8);return fault==PDS?0:0x20000000;}
#define PSB_RSGX32(reg) model_register(reg)
static unsigned atomic_read(const unsigned *v){return *v;}
static void *kzalloc(size_t n,int flags){CHECK(!flags);if(fault==ALLOC)return NULL;void *p=calloc(1,n);CHECK(p);allocs++;return p;}
static void kfree(void *p){if(p){frees++;free(p);}}
static void *kmemdup(const void *p,size_t n,int flags)
{dup_calls++;if(fault==DUPALLOC)return NULL;void *b=kzalloc(n,flags);if(b)memcpy(b,p,n);return b;}
static unsigned msecs_to_jiffies(unsigned n){CHECK(n==1000);return n;}
static struct retained_job *ticket_job(struct remesa_gem_memory *s,struct remesa_memory_ticket t)
{return s->jobs[0].live&&t.operation==91?&s->jobs[0]:NULL;}
int remesa_private_session_acquire(struct drm_device *dev,struct drm_file *file,u64 context,const void *owner,struct remesa_gem_memory **out)
{
 CHECK(!endpoint_lock.held);*out=NULL;
 if(fault==BACKING||endpoint->dev!=dev||endpoint->file!=file||endpoint->context!=context||endpoint->private_session)return -EBUSY;
 if(endpoint->executor||endpoint->stream_bound)return -EBUSY;
 for(unsigned i=0;i<8;i++)if(endpoint->jobs[i].live)return -EBUSY;
 for(unsigned i=0;i<32;i++)if(endpoint->bo[i].lease||endpoint->bo[i].holds||endpoint->bo[i].quarantine||endpoint->bo[i].color_vmas)return -EBUSY;
 endpoint->private_session=owner;*out=endpoint;
 if(fault==LOST){endpoint->private_device_lost=true;endpoint->file=NULL;}return 0;
}
int remesa_private_session_release(struct remesa_gem_memory *s,const void *who)
{
 CHECK(!endpoint_lock.held&&s==endpoint&&s->private_session==who&&!s->jobs[0].live&&!s->executor&&!s->private_source_held);
 CHECK(!current_guard||!current_guard->held);backing_ends++;
 if(fault==BACKING_END)return -EIO;
 s->private_session=NULL;return 0;
}
int remesa_private_session_source_begin(struct remesa_gem_memory *s,const void *who)
{
 CHECK(!endpoint_lock.held&&s->private_session==who&&current_guard&&current_guard->held&&!s->jobs[0].live);claims++;
 if(fault==CLAIM)return -EIO;
 s->private_source_held=true;
 if(fault==LOST_AFTER_CLAIM){s->private_device_lost=true;s->file=NULL;}return 0;
}
int remesa_private_session_source_end(struct remesa_gem_memory *s,const void *who)
{
 CHECK(!endpoint_lock.held&&s->private_session==who&&s->private_source_held&&!s->jobs[0].live&&!s->executor&&!current_guard->held);source_ends++;
 if(fault==SOURCE_END){s->private_source_poisoned=true;return -EIO;}
 s->private_source_held=false;return 0;
}
int remesa_twod_reserve(struct drm_psb_private *d,struct remesa_twod_reservation *g)
{CHECK(endpoint_lock.held&&d==endpoint->dev->dev_private&&!g->held);reserves++;if(fault==RESERVE)return -EBUSY;g->device=d;g->held=true;current_guard=g;return 0;}
int remesa_twod_release(struct drm_psb_private *d,struct remesa_twod_reservation *g)
{CHECK(endpoint_lock.held&&d==g->device&&g->held&&!g->mmu_owner);if(fault==GUARD_DROP)return -EIO;g->held=false;return 0;}
int remesa_gem_scene_packet_prepare(struct remesa_gem_memory *s,const struct remesa_scene_packet *p,struct remesa_twod_reservation *guard,struct remesa_gem_scene_job *c)
{
 CHECK(endpoint_lock.held&&s->private_session&&s->private_source_held&&guard->held&&remesa_scene_packet_valid(p)&&p->owner==s->context&&p->binding_count==17);
 CHECK(p->actions.region_id>=32&&p->actions.table_id>=32&&p->actions.parameter_id>=32&&p->controls.color_id>=32);
 if(fault==PREPARE)return -EINVAL;
 c->memory=s;c->state=REMESA_SCENE_JOB_PREPARED;c->count=17;c->snapshot.ticket=(struct remesa_memory_ticket){.context=s->context,.generation=1,.operation=91};
 c->loan.live=true;c->loan.memory=s;c->loan.ticket=c->snapshot.ticket;c->loan.guard=guard;guard->mmu_owner=&c->loan;
 s->jobs[0].live=true;s->executor=&c->loan;
 for(unsigned i=0;i<17;i++){
  c->views[i].binding=(struct remesa_memory_binding){p->bindings[i].id,expected_profile->bindings[i].address,expected_profile->bindings[i].bytes};
  c->views[i].cpu=p->bindings[i].access==1?images[i]:NULL;s->bo[i].holds=1;
 }
 return fault==PREPARE_HELD?-EUCLEAN:0;
}
static int remesa_depth1280_sha256(const void *data,size_t size,u8 digest[32])
{CHECK(endpoint_lock.held);for(unsigned i=0;i<17;i++)if(images[i]==data){CHECK(size==expected_profile->bindings[i].bytes&&expected_profile->bindings[i].access==1);memcpy(digest,expected_profile->bindings[i].digest,32);hashes++;if(fault==HASH||fault==HASH_GALLIUM||fault==HASH_MATRIX)digest[0]^=1;return 0;}CHECK(0);return -EIO;}
int remesa_gem_scene_job_cancel(struct remesa_gem_scene_job *c)
{
 CHECK(endpoint_lock.held&&!c->loan.execution_attempted);
 if(fault==PREPARE_HELD)return -EBUSY;
 CHECK(c->memory->jobs[0].live);c->memory->jobs[0].live=false;c->memory->executor=NULL;
 for(unsigned i=0;i<17;i++)c->memory->bo[i].holds=0;
 c->loan.guard->mmu_owner=NULL;memset(c,0,sizeof(*c));return 0;
}
int remesa_gem_scene_job_execute(struct remesa_gem_scene_job *c,struct sgx535_gma500_fixed_backend *b,struct remesa_stream_observation *o,const void *owner,unsigned ticks,unsigned samples)
{
 CHECK(endpoint_lock.held&&c->memory->private_session==owner&&c->memory->private_source_held&&c->loan.guard->held&&hashes==12&&ticks==1000&&samples==100000&&!b->active&&!o->state);executions++;
 if(fault==EARLY_EXEC)return -EIO;
 c->loan.execution_attempted=true;c->loan.execution_started=true;c->memory->jobs[0].cookie=91;
 if(fault==EXEC)return -EIO;
 c->session.phase=SGX535_PHASE_RETIRED;c->session.observed_events=7;c->session.sequence=91;
 c->state=REMESA_SCENE_JOB_ACKNOWLEDGED;c->loan.execution_retired=fault!=NO_RECEIPT;o->state=1;o->event_count=2;o->zero_samples=3;o->events[0]=(struct remesa_stream_source_event){4,91,0x2000,0,0,5};o->events[1]=(struct remesa_stream_source_event){4,91,4,0,3,8};o->event_ns[0]=1234567890123ULL;o->event_ns[1]=1234567890456ULL;return 0;
}
int remesa_gem_private_release(struct remesa_gem_private *loan)
{CHECK(endpoint_lock.held&&loan->execution_retired&&loan->live&&endpoint->jobs[0].live);if(fault==LOAN_DROP)return -EIO;loan->guard->mmu_owner=NULL;memset(loan,0,sizeof(*loan));endpoint->executor=NULL;return 0;}
int remesa_gem_job_retire(struct remesa_gem_memory *s,struct remesa_memory_ticket t)
{CHECK(endpoint_lock.held&&t.operation==91&&s->jobs[0].live&&!s->executor&&!current_guard->held);if(fault==JOB_DROP)return -EIO;s->jobs[0].live=false;for(unsigned i=0;i<17;i++)s->bo[i].holds=0;return 0;}
#include "../remesa_scene_probe.inc"
int main(int argc,char **argv)
{
 CHECK(argc==2);fault=(unsigned)strtoul(argv[1],NULL,10);CHECK(fault<CASE_COUNT);
 struct drm_psb_private priv={(void*)1};struct drm_device dev={&priv};struct drm_file file={1};
 struct remesa_gem_memory memory={.dev=&dev,.file=&file,.context=77,.root={0,0xdfffffffULL}};
 struct remesa_scene_probe_request request={.version=1,.profile=fault==OK640_MATRIX?10:fault>=OK_MATRIX?9:fault==OK640?2:fault==OK_INIT?3:fault==OK640_INIT?4:fault==OK_FINAL?5:fault==OK640_FINAL?6:fault==OK640_GALLIUM?8:fault>=OK_GALLIUM?7:1,.context=77,.budget_ms=1000};
 endpoint=&memory;expected_profile=&scene_probe_profiles[request.profile-1];scene_probe_enabled=true;
 for(unsigned i=0;i<17;i++){
  request.ids[i]=32+i;memory.bo[i]=(struct backing){.live=true,.generation=1,.va={expected_profile->bindings[i].address,0},.gem={expected_profile->bindings[i].bytes},.pinned=true,.reserved=true,.initialized=true,.zero_pristine=true};
  if(expected_profile->bindings[i].access==1){images[i]=malloc(expected_profile->bindings[i].bytes);CHECK(images[i]);}
 }
 if(fault==CAP)administrator=false;if(fault==DISABLED)scene_probe_enabled=false;if(fault==MEMDISABLED)memory_enabled=false;
 if(fault==INVALID){
#define BAD(field,value) do{struct remesa_scene_probe_request bad=request;bad.field=(value);CHECK(remesa_scene_probe_ioctl(&dev,&bad,&file)==-EINVAL);}while(0)
  BAD(version,0);BAD(profile,0);BAD(profile,REMESA_SCENE_PROBE_PROFILE_COUNT+1);BAD(context,0);BAD(budget_ms,0);BAD(budget_ms,10001);
  BAD(ids[0],0);BAD(ids[16],32);BAD(operation,1);BAD(cookie,1);BAD(core,1);BAD(revision,1);BAD(pds_base,1);
  BAD(phase,1);BAD(ledger,1);BAD(error,1);BAD(retained,1);BAD(stages,1);BAD(reserved[0],1);BAD(reserved[1],1);
#undef BAD
  CHECK(!allocs&&!reads);request.reserved[0]=1;
 }
 if(fault==POWER)powered=false;if(fault==VA||fault==VA_GALLIUM||fault==VA_MATRIX)memory.bo[0].va.start++;if(fault==SIZE)memory.bo[0].gem.size++;
 if(fault==WRONG_LEGACY)request.profile=5;
 if(fault==WRONG_WINDOW)request.profile=7;
 if(fault==GEN)memory.bo[0].generation++;
 if(fault==LEASE)memory.bo[0].lease=true;if(fault==HOLD)memory.bo[0].holds=1;if(fault==PIN)memory.bo[0].pinned=false;
 if(fault==RESERVED)memory.bo[0].reserved=false;if(fault==QUAR)memory.bo[0].quarantine=true;if(fault==VMA)memory.bo[0].color_vmas=1;
 unsigned rw=0;while(expected_profile->bindings[rw].access!=3)rw++;
 if(fault==ZERO)memory.bo[rw].zero_pristine=false;if(fault==CPU_ALIAS)memory.bo[rw].cached_job_view=(void*)1;
 if(fault==STREAM)memory.stream_bound=true;if(fault==EXECUTOR)memory.executor=(void*)1;if(fault==TA)memory.ta_cache_domains=true;
 if(fault==CEILING)memory.root.end--;if(fault==LIVE)memory.jobs[1].live=true;
 int error=remesa_scene_probe_ioctl(&dev,&request,&file);
 CHECK(!endpoint_lock.held&&!memory.lock.held&&!scene_probe_lock.held);
 if(fault==CAP)CHECK(error==-EPERM&&!reads&&!allocs);
 if(fault==DISABLED||fault==MEMDISABLED)CHECK(error==-EOPNOTSUPP&&!reads&&!allocs);
 if(fault==INVALID)CHECK(error==-EINVAL&&!reads&&!allocs);
 if(fault==OK||fault==OK640||fault==OK_INIT||fault==OK640_INIT||fault==OK_FINAL||fault==OK640_FINAL||fault==OK_GALLIUM||fault==OK640_GALLIUM||fault==OK_MATRIX||fault==OK640_MATRIX){
  CHECK(!error&&!request.error&&!request.retained&&request.stages==15&&request.phase==9&&request.ledger==7&&request.operation==91&&request.cookie==91);
  CHECK(!memory.private_session&&!memory.private_source_held&&!memory.jobs[0].live&&source_ends==1&&backing_ends==1&&allocs==frees);
  struct remesa_scene_trace_request *query=calloc(1,sizeof(*query));CHECK(query);
  unsigned before_reads=reads;query->version=1;query->context=77;
  CHECK(!remesa_scene_trace_ioctl(&dev,query,&file));
  CHECK(query->trace.version==1&&query->trace.bytes==4256&&query->trace.operation==91&&query->trace.cookie==91&&query->trace.stages==15&&query->trace.profile==request.profile);
  CHECK(query->trace.event_count==2&&query->trace.zero_samples==3&&query->trace.events[0].status1==0x2000&&query->trace.events[1].ns==1234567890456ULL&&query->trace.binding_count==17);
  for(unsigned i=0;i<17;i++)CHECK(query->trace.bindings[i].id==request.ids[i]&&query->trace.bindings[i].address==expected_profile->bindings[i].address);
  CHECK(remesa_scene_trace_ioctl(&dev,query,&file)==-EINVAL); /* nonzero output cannot be replayed as input */
  memset(&query->trace,0,sizeof(query->trace));query->context=78;CHECK(remesa_scene_trace_ioctl(&dev,query,&file)==-EACCES);query->context=77;
  struct drm_file other={2};CHECK(remesa_scene_trace_ioctl(&dev,query,&other)==-EACCES);
  memory.private_device_lost=true;CHECK(remesa_scene_trace_ioctl(&dev,query,&file)==-EACCES);memory.private_device_lost=false;
  administrator=false;CHECK(remesa_scene_trace_ioctl(&dev,query,&file)==-EPERM);administrator=true;
  scene_probe_enabled=false;CHECK(remesa_scene_trace_ioctl(&dev,query,&file)==-EOPNOTSUPP);scene_probe_enabled=true;
  memory_enabled=false;CHECK(remesa_scene_trace_ioctl(&dev,query,&file)==-EOPNOTSUPP);memory_enabled=true;
  query->version=2;CHECK(remesa_scene_trace_ioctl(&dev,query,&file)==-EINVAL);query->version=1;
  query->reserved=1;CHECK(remesa_scene_trace_ioctl(&dev,query,&file)==-EINVAL);query->reserved=0;
  ((u8*)&query->trace)[4255]=1;CHECK(remesa_scene_trace_ioctl(&dev,query,&file)==-EINVAL);memset(&query->trace,0,sizeof(query->trace));
  CHECK(!remesa_scene_trace_ioctl(&dev,query,&file)&&reads==before_reads);free(query);

 }else CHECK(error||request.error);
 bool must_retain=fault==LOST||fault==PREPARE_HELD||fault==EXEC||fault==NO_RECEIPT||
  fault==LOAN_DROP||fault==GUARD_DROP||fault==JOB_DROP||fault==SOURCE_END||fault==BACKING_END||fault==LOST_AFTER_CLAIM;
 CHECK((scene_probe_retained!=NULL)==must_retain);
 if(fault==HASH||fault==HASH_GALLIUM||fault==HASH_MATRIX)CHECK(!executions&&hashes==1&&!request.retained);
 if(fault==VA_GALLIUM||fault==VA_MATRIX||fault==WRONG_LEGACY)CHECK(!executions&&!hashes&&!request.retained);
 if(fault==LOST||fault==LOST_AFTER_CLAIM)CHECK(!executions);
 if(scene_probe_retained){
  CHECK(request.retained&&memory.private_session==scene_probe_retained&&allocs>frees);
  if(fault==SOURCE_END||fault==BACKING_END)CHECK(!memory.jobs[0].live); /* lease alone must retain endpoint */
 }else CHECK(!request.retained&&allocs==frees);
 if(scene_probe_consumed){struct remesa_scene_probe_request retry={.version=1,.profile=1,.context=77,.budget_ms=1000};for(unsigned i=0;i<17;i++)retry.ids[i]=32+i;CHECK(remesa_scene_probe_ioctl(&dev,&retry,&file)==-EBUSY);}
 printf("PASS scene admission case%u retained=%u reads=%u hashes=%u executions=%u GPU0\n",fault,request.retained,reads,hashes,executions);return 0;
}
