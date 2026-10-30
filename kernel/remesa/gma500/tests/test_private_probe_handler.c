/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual ioctl/controller; GEM, crypto, grammar, borrower, source, MMU/execution
 * dependencies MODELED. Real eleven-role profile/layout, no GPU or MMIO. */
#include "probe_handler_types.h"
#include "../remesa_gem_private.h"
#include "../remesa_twod_reservation.h"
#include "remesa_borrowed_owner.h"
#include "remesa_stream_trusted.h"
#include "remesa_stream_grammar.h"
#include "remesa_depth1280_profile.h"
#include "../remesa_private_probe.h"
static const struct remesa_stream_profile private_probe_profile;
enum faults {OK,CAP,DISABLED,MEMDISABLED,CONTEXT,CORE,POWER,VA,SIZE,LIVE,STREAM,
 EXECUTOR,TA,CEILING,ALLOC,PREPARE,SNAPSHOT,VMALLOC,MASK,HASH,GEOMETRY,BORROW,
 VERIFY,RESERVE,LOAN,LOAN_RETAINED,CLAIM,EARLY_EXEC,EXEC,NO_RECEIPT,
 BORROW_CLOSE,SETTLE,SOURCE_CLOSE,LOAN_CLOSE,GUARD_CLOSE,JOB_CLOSE,PRIVATE_SESSION,FAULT_COUNT};
static unsigned checks,fault,reads,allocations,frees,prepares,cancels,retirements;
static unsigned snapshots,hashes,masks,geometries,borrow_opens,borrow_closes;
static unsigned reservations,guard_closes,loan_opens,loan_closes,claims,source_closes,settles,executions;
static bool administrator=true,memory_enabled=true,powered=true,source_held;
static bool hard_loan_failure;
static void *source_token;
static struct mutex endpoint_lock;
static struct remesa_gem_memory *endpoint;
static struct page **pages[11];static void *buffers[11];
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"failure line%d: %s (fault%u)\n",__LINE__,#x,fault);abort();}}while(0)
#define module_param_named(...) /* test only */
#define BUILD_BUG_ON(x) _Static_assert(!(x),"build invariant")
#define CAP_SYS_ADMIN 1
#define PSB_CR_CORE_ID 0u
#define PSB_CR_CORE_REVISION 4u
static bool capable(int cap){CHECK(cap==1);return administrator;}
static bool gma_power_is_on(struct drm_device *d){CHECK(d==endpoint->dev);return powered;}
static u32 model_register(unsigned reg){reads++;return reg==0?(fault==CORE?0:0x01130000u):0x00010201u;}
#define PSB_RSGX32(reg) model_register(reg)
static void mutex_lock(struct mutex *m){CHECK(!m->held);m->held=true;}
static void mutex_unlock(struct mutex *m){CHECK(m->held);m->held=false;}
static void *kzalloc(size_t n,int flags){CHECK(flags==0);
 if(fault==ALLOC)return NULL;
 allocations++;return calloc(1,n);}
static void kfree(void *p){if(p){frees++;free(p);}}
static void *vmalloc(size_t n){return fault==VMALLOC?NULL:malloc(n);}
static void vfree(void *p){free(p);}
#define GFP_KERNEL 0
static unsigned msecs_to_jiffies(unsigned n){return n;}
static struct retained_job *ticket_job(struct remesa_gem_memory *s,struct remesa_memory_ticket t)
{return s->jobs[0].live&&!memcmp(&s->jobs[0].ticket,&t,sizeof t)?&s->jobs[0]:NULL;}
static void in_endpoint(void){
#ifndef REMESA_PRIVATE_LAB_HANDLER_TEST
 CHECK(endpoint_lock.held);
#endif
 CHECK(!endpoint->lock.held);
}
#ifdef REMESA_PRIVATE_LAB_HANDLER_TEST
static unsigned model_operation;
static void model_drop_views(void)
{for(unsigned i=0;i<11;i++){free(buffers[i]);free(pages[i]);buffers[i]=NULL;pages[i]=NULL;}}
#endif
int remesa_gem_job_prepare(struct remesa_gem_memory *s,const struct remesa_memory_binding *v,
 unsigned count,const void *cmd,unsigned bytes,struct remesa_memory_ticket *out)
{
 in_endpoint();prepares++;CHECK(s==endpoint&&count==11&&bytes==264&&!s->jobs[0].live);
 CHECK(!memcmp(cmd,&private_probe_profile.commands,bytes));
 for(unsigned i=0;i<11;i++)CHECK(v[i].id==32+i&&v[i].va==s->bo[i].va.start&&v[i].bytes==s->bo[i].gem.size);
 if(fault==PREPARE)return -ENOMEM;
 s->jobs[0].ticket=(struct remesa_memory_ticket){s->context,
#ifdef REMESA_PRIVATE_LAB_HANDLER_TEST
 ++model_operation,
#else
 1,
#endif
 0,1};s->jobs[0].live=true;
 *out=s->jobs[0].ticket;return 0;
}
int remesa_gem_job_snapshot(struct remesa_gem_memory *s,struct remesa_memory_ticket t,
 struct remesa_memory_snapshot *out,struct remesa_memory_view *v,unsigned count)
{
 in_endpoint();snapshots++;CHECK(ticket_job(s,t)&&count==11);
 if(fault==SNAPSHOT)return -ENOMEM;
 for(unsigned i=0;i<11;i++){
  unsigned n=s->bo[i].gem.size/4096;pages[i]=calloc(n,sizeof(*pages[i]));CHECK(pages[i]);
  for(unsigned j=0;j<n;j++)pages[i][j]=&s->page;
  buffers[i]=calloc(1,s->bo[i].gem.size);CHECK(buffers[i]);
#ifdef REMESA_PRIVATE_LAB_HANDLER_TEST
  if(i<5)memcpy(buffers[i],remesa_canonical_inputs[i],s->bo[i].gem.size);
  if(i==1&&fault==HASH)((unsigned char*)buffers[i])[0]^=1;
#endif
  v[i]=(struct remesa_memory_view){{32+i,s->bo[i].va.start,s->bo[i].gem.size},pages[i],n,buffers[i]};
 }
 *out=(struct remesa_memory_snapshot){s->dev,t,s->root.end+1,&private_probe_profile.commands,264,true,true};return 11;
}
int remesa_gem_job_check_snapshot(struct remesa_gem_memory *s,const struct remesa_memory_snapshot *snap,
 const struct remesa_memory_view *v,unsigned n)
{
 in_endpoint();CHECK(s==endpoint&&ticket_job(s,snap->ticket)&&n==11);
 for(unsigned i=0;i<11;i++)CHECK(v[i].cpu==buffers[i]&&v[i].pages==pages[i]);
 return fault==VERIFY?-EACCES:0;
}
int remesa_depth1280_sha256(const void *p,size_t n,u8 out[32])
{
 in_endpoint();CHECK(p&&hashes<5&&n==private_probe_profile.bindings[hashes].bytes);
 memcpy(out,private_probe_profile.images[hashes++],32);
 if(fault==HASH)out[0]^=1;
 return 0;
}
int remesa_stream_mask_sources(const struct remesa_stream_batch *b,unsigned n,unsigned char *data,__u64 length,__u64 va)
{
 in_endpoint();masks++;CHECK(b==private_probe_profile.batches&&n==private_probe_profile.count&&data&&length==131072&&va==private_probe_profile.bindings[0].va);
 return fault==MASK?-EINVAL:0;
}
int remesa_stream_validate_geometry(const struct remesa_stream_batch *b,unsigned n,const unsigned char *data,__u64 length)
{
 in_endpoint();geometries++;CHECK(b==private_probe_profile.batches&&n==private_probe_profile.count&&data==buffers[2]&&length==private_probe_profile.bindings[2].bytes);
 return fault==GEOMETRY?-EINVAL:0;
}
int remesa_stream_remesa_borrowed_owner_init(struct remesa_borrowed_owner *b,struct sgx535_gma500_owner *o,
 struct drm_device *dev,struct remesa_memory_ticket t,const struct remesa_memory_view *v,unsigned n,u64 end,
 const struct sgx535_frozen_command_pairs *cmd,remesa_borrowed_verify verify,void *lender)
{
 in_endpoint();borrow_opens++;CHECK(!b->live&&dev==endpoint->dev&&n==11&&end==0xe0000000ULL);
 if(fault==BORROW)return -ENOMEM;
 int e=verify(lender,dev,t,v,n,end);if(e)return e;
 e=verify(lender,dev,t,v,n,end);if(e)return e;
 o->dev=dev;o->commands=*cmd;
 for(unsigned i=0;i<11;i++)o->descriptors[i].domain=(i==4||i==9)?SGX535_DOMAIN_LOCAL:SGX535_DOMAIN_MMU;
 *b=(struct remesa_borrowed_owner){o,t,true};return 0;
}
int remesa_stream_remesa_borrowed_owner_deinit(struct remesa_borrowed_owner *b)
{
 in_endpoint();borrow_closes++;CHECK(b->live);
 if(fault==BORROW_CLOSE)return -EIO;
 memset(b->owner,0,sizeof(*b->owner));memset(b,0,sizeof(*b));return 0;
}
int remesa_twod_reserve(struct drm_psb_private *dev,struct remesa_twod_reservation *g)
{
 in_endpoint();reservations++;CHECK(dev==endpoint->dev->dev_private&&!g->held);
 if(fault==RESERVE)return -EBUSY;
 g->device=dev;g->held=true;return 0;
}
int remesa_twod_release(struct drm_psb_private *dev,struct remesa_twod_reservation *g)
{
 in_endpoint();guard_closes++;CHECK(g->held&&g->device==dev&&!g->mmu_owner);
 if(fault==GUARD_CLOSE)return -EIO;
 memset(g,0,sizeof(*g));return 0;
}
int remesa_gem_private_prepare(struct remesa_gem_memory *s,struct remesa_memory_ticket t,
 const struct remesa_gem_private_rule *rules,unsigned n,struct remesa_twod_reservation *g,struct remesa_gem_private *l)
{
 in_endpoint();loan_opens++;CHECK(s==endpoint&&ticket_job(s,t)&&n==11&&g->held&&!g->mmu_owner);
 for(unsigned i=0;i<n;i++)CHECK(rules[i].id==32+i&&rules[i].access==((i==4||i==9)?0:i<4?1:3)&&rules[i].cached==((i==4||i==9)?0:1));
 if(fault==LOAN)return -ENOMEM;
 *l=(struct remesa_gem_private){.memory=s,.ticket=t,.owner=(void*)1,.guard=g,.live=true};g->mmu_owner=l;
 if(fault==LOAN_RETAINED){hard_loan_failure=true;return -EUCLEAN;}
 return 0;
}
int remesa_gem_private_release(struct remesa_gem_private *l)
{
 in_endpoint();loan_closes++;CHECK(l->live&&l->guard->held&&l->guard->mmu_owner==l);
 if(hard_loan_failure||fault==LOAN_CLOSE)return -EBUSY;
 l->guard->mmu_owner=NULL;memset(l,0,sizeof(*l));return 0;
}
int remesa_stream_source_claim(struct drm_device *d,const void *who)
{
 in_endpoint();claims++;CHECK(d==endpoint->dev&&!source_held&&who);
 if(fault==CLAIM)return -EBUSY;
 source_held=true;source_token=(void*)who;return 0;
}
int remesa_stream_source_release(const void *who)
{
 in_endpoint();source_closes++;CHECK(source_held&&source_token==who);
#ifndef REMESA_PRIVATE_LAB_HANDLER_TEST
 CHECK(endpoint->jobs[0].live);
#endif
 if(fault==SOURCE_CLOSE)return -EIO;
 source_held=false;source_token=NULL;return 0;
}
int remesa_stream_source_settled(const void *who)
{
 in_endpoint();settles++;CHECK(source_held&&source_token==who);
#ifndef REMESA_PRIVATE_LAB_HANDLER_TEST
 CHECK(endpoint->jobs[0].live);
#endif
 return fault==SETTLE?-EIO:0;
}
int remesa_gem_private_execute(struct remesa_gem_private *l,struct sgx535_gma500_owner *o,
 struct sgx535_gma500_fixed_backend *b,struct remesa_stream_observation *obs,const void *who,
 const struct remesa_memory_view *v,unsigned n,unsigned ticks,unsigned samples)
{
 in_endpoint();executions++;CHECK(source_held&&source_token==who&&l->live&&l->guard->held&&n==11&&ticks==1000&&samples==100000);
 CHECK(v[0].cpu==buffers[0]);(void)b;(void)obs;
 if(fault==EARLY_EXEC)return -EACCES;
 l->execution_attempted=true;l->execution_started=true;
 endpoint->jobs[0].cookie=l->ticket.operation;
 if(fault==EXEC)return -EIO;
 o->session.phase=SGX535_PHASE_RETIRED;o->session.observed_events=7;o->session.sequence=l->ticket.operation;
 l->execution_retired=fault!=NO_RECEIPT;return 0; /* MODELED receipt only */
}
int remesa_gem_job_cancel(struct remesa_gem_memory *s,struct remesa_memory_ticket t)
{in_endpoint();cancels++;CHECK(ticket_job(s,t));s->jobs[0].live=false;
#ifdef REMESA_PRIVATE_LAB_HANDLER_TEST
 model_drop_views();
#endif
 return 0;}
int remesa_gem_job_retire(struct remesa_gem_memory *s,struct remesa_memory_ticket t)
{
 in_endpoint();retirements++;CHECK(ticket_job(s,t));
#ifndef REMESA_PRIVATE_LAB_HANDLER_TEST
 CHECK(!source_held);
#else
 CHECK(source_held);
#endif
 if(fault==JOB_CLOSE)return -EIO;
 s->jobs[0].live=false;
#ifdef REMESA_PRIVATE_LAB_HANDLER_TEST
 model_drop_views();
#endif
 return 0;
}
#include "../remesa_private_probe.inc"
int main(int argc,char **argv)
{
 fault=argc>1?(unsigned)strtoul(argv[1],NULL,0):0;CHECK(fault<FAULT_COUNT);
 struct drm_file file={0},other={0};struct drm_device dev;struct drm_psb_private priv={.sgx_reg=(void*)1};dev.dev_private=&priv;
 struct remesa_gem_memory memory={.dev=&dev,.file=&file,.context=77,.root={.end=0xdfffffffULL}};endpoint=&memory;
 struct remesa_private_probe_request req={.version=1,.context=77,.budget_ms=1000};
 CHECK(!private_probe_enabled&&!private_probe_consumed&&!private_probe_retained);private_probe_enabled=true;
 for(unsigned i=0;i<11;i++){
  req.ids[i]=32+i;memory.bo[i].live=true;memory.bo[i].generation=1;
  memory.bo[i].va.start=private_probe_profile.bindings[i].va;memory.bo[i].gem.size=private_probe_profile.bindings[i].bytes;
 }
 if(fault==CAP)administrator=false;
 if(fault==DISABLED)private_probe_enabled=false;
 if(fault==MEMDISABLED)memory_enabled=false;
 if(fault==CONTEXT)memory.file=&other;
 if(fault==POWER)powered=false;
 if(fault==VA)memory.bo[3].va.start+=4096;
 if(fault==SIZE)memory.bo[2].gem.size+=4096;
 if(fault==LIVE)memory.jobs[0].live=true;
 if(fault==STREAM)memory.stream_bound=true;
 if(fault==EXECUTOR)memory.executor=(void*)1;
 if(fault==TA)memory.ta_cache_domains=true;
 if(fault==CEILING)memory.root.end--;
 if(fault==PRIVATE_SESSION)memory.private_session=&other;
 struct remesa_private_probe_request initial=req;
 int code=remesa_private_probe_ioctl(&dev,&req,&file);
 CHECK(!endpoint_lock.held&&!memory.lock.held);
 if(fault==CAP||fault==DISABLED||fault==MEMDISABLED||fault==CONTEXT||fault==POWER||fault==PRIVATE_SESSION){
  CHECK(code<0&&!reads&&!allocations&&!executions&&!private_probe_retained);
  if(fault==PRIVATE_SESSION)CHECK(code==-EBUSY&&!prepares&&!hashes&&!masks&&!claims&&
    !memcmp(&req,&initial,sizeof(req))&&!private_probe_consumed);
 }else if(fault>=CORE&&fault<=CEILING){
  CHECK(!code&&req.error<0&&!req.retained&&!allocations&&!executions&&reads==2);
 }else if(fault==ALLOC){CHECK(!code&&req.error==-ENOMEM&&!req.retained&&!prepares);}
 else if(!fault){
  CHECK(!code&&!req.error&&!req.retained&&req.stages==15&&req.core==0x01130000&&req.revision==0x00010201);
  CHECK(req.phase==SGX535_PHASE_RETIRED&&req.ledger==7&&req.cookie==1&&req.operation==1);
  CHECK(!memory.jobs[0].live&&!source_held&&private_probe_consumed&&allocations==frees);
  CHECK(retirements==1&&!cancels&&loan_closes==1&&guard_closes==1&&source_closes==1&&settles==1);
 }else{
  CHECK(!code&&req.error<0);
  bool retained=fault==LOAN_RETAINED||fault>=EXEC;
  CHECK(!!req.retained==retained);
  if(retained){
   CHECK(private_probe_retained&&memory.jobs[0].live&&!frees&&!cancels);
   if(fault>=EXEC)CHECK(private_probe_retained->execution_consumed);
   if(fault==LOAN_RETAINED)CHECK(!executions&&private_probe_retained->loan.live&&private_probe_retained->guard.held);
   if(fault==EXEC||fault==NO_RECEIPT)CHECK(!borrow_closes&&!loan_closes&&!guard_closes&&!source_closes&&!retirements);
   if(fault==GUARD_CLOSE||fault==JOB_CLOSE)CHECK(!private_probe_retained->loan.live&&!private_probe_retained->borrower.live&&!source_held&&!cancels);
  }else{
   CHECK(!private_probe_retained&&!memory.jobs[0].live&&!source_held&&allocations==frees);
   CHECK(!retirements);if(fault!=PREPARE)CHECK(cancels==1);
  }
 }
 if(!req.retained)for(unsigned i=0;i<11;i++){free(buffers[i]);free(pages[i]);}
 printf("PASS %u actual probe-controller checks; fault%u; dependencies/receipts modeled, GPU0\n",checks,fault);
 return 0;
}
