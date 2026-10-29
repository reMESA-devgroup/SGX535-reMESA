/* SPDX-License-Identifier: GPL-2.0-only */
/* Real GEM/loan/execution code, THREE-resource role model. Service/source/MMU
 * callbacks are modeled, not GPU completion or full fixed-scene qualification. */
#define REMESA_GEM_PRIVATE_TEST_MAIN existing_private_gem_test_main
#include "test_gem_private.c"
#define REMESA_GEM_PRIVATE_EXECUTION_TEST
#define SGX535_BO_COUNT 3u
#define SGX535_PHASE_RETIRED 15u /* model state only; no frozen protocol encoding */
#define HZ 100u
struct model_service {void *observer_context;const void *observer;};
struct sgx535_gma500_owner {
 struct drm_device *dev;u64 mmu_end;u32 commands;
 struct {u64 sequence;unsigned phase,observed_events;} session;
 struct model_service fixed_service;
};
struct sgx535_gma500_fixed_backend {struct sgx535_gma500_owner *owner;
 struct remesa_gem_private *private_loan;bool active;};
struct remesa_stream_observation {bool bound,finished;};
static const char sgx535_gma500_fixed_backend_ops,sgx535_gma500_fixed_backend_status_source;
static unsigned fault,source_phase,flush_count,execute_downs,execute_ups;
static struct remesa_gem_private *executing;
static void down_write(struct private_test_sem *s)
{CHECK(!s->held&&expected_memory->lock.held);s->held=1;execute_downs++;}
static void up_write(struct private_test_sem *s)
{CHECK(s->held&&expected_memory->lock.held);s->held=0;execute_ups++;}
static int remesa_stream_source_bind(const void *who,struct remesa_stream_observation *o,
 struct sgx535_gma500_fixed_backend *b,u64 operation,u64 sequence)
{
 CHECK(who==expected_memory&&expected_memory->lock.held&&!driver.sem.held);
 CHECK(!o->bound&&!b->active&&operation==executing->ticket.operation&&sequence==operation);
 if(fault==1)return -EBUSY;
 o->bound=true;source_phase=1;return 0;
}
int remesa_mmu_private_bind_locked(struct remesa_mmu_private *o)
{
 CHECK(driver.sem.held&&source_phase==1&&o==executing->owner);
 o->root_active=1;source_phase=2;return fault==2?-EIO:0;
}
int remesa_mmu_private_restore_locked(struct remesa_mmu_private *o)
{
 CHECK(driver.sem.held&&source_phase==6&&o==executing->owner&&o->root_active);
 if(fault==8)return -EIO;
 o->root_active=0;source_phase=7;return 0;
}
int remesa_mmu_private_quarantine_locked(struct remesa_mmu_private *o)
{CHECK(driver.sem.held&&o==executing->owner);if(o->root_active)o->blocked=1;return 0;}
static int sgx535_gma500_fixed_backend_begin_private(struct sgx535_gma500_fixed_backend *b,
 struct sgx535_gma500_owner *o,struct remesa_gem_private *l,
 const struct remesa_memory_view *v,unsigned n)
{
 CHECK(driver.sem.held&&source_phase==2&&l==executing);
 CHECK(!remesa_gem_private_validate_locked(l,v,n));
 if(fault==3)return -EAGAIN;
 b->active=true;b->owner=o;b->private_loan=l;o->fixed_service.observer_context=b;
 source_phase=3;return 0;
}
static void execute_flush(struct page **p,unsigned n)
{
 CHECK(driver.sem.held&&(source_phase==3||source_phase==4));
 struct retained_job *j=ticket_job(expected_memory,executing->ticket);unsigned found=0;
 for(unsigned i=0;i<3;i++){
  struct backing *b=&expected_memory->bo[j->ids[i]&31];
  if(b->pages==p){CHECK(n==b->inserted&&executing->rules[i].access);found++;
   if(source_phase==4)CHECK(executing->rules[i].access&REMESA_MMU_GPU_WRITE);}
 }
 CHECK(found==1);flush_count++;
}
static int remesa_stream_sgx535_fixed_service_run_once(struct model_service *s,
 const void *ops,const void *src,struct sgx535_gma500_fixed_backend *b,
 u32 revision,u32 sequence,unsigned ticks,unsigned samples)
{
 struct retained_job *j=ticket_job(expected_memory,executing->ticket);
 struct remesa_memory_view views[3];
 CHECK(driver.sem.held&&source_phase==3&&s==&b->owner->fixed_service);
 CHECK(ops==&sgx535_gma500_fixed_backend_ops&&src==&sgx535_gma500_fixed_backend_status_source);
 CHECK(revision==0x00010201u&&sequence==j->operation&&ticks==10&&samples==20);
 CHECK(j->state==REMESA_JOB_ISSUED&&j->cookie==j->operation);
 CHECK(executing->execution_started&&executing->execution_attempted);
 for(unsigned i=0;i<3;i++){
  struct backing *bo=&expected_memory->bo[j->ids[i]&31];
  views[i]=(struct remesa_memory_view){{j->ids[i],bo->va.start,bo->gem.size},bo->pages,bo->inserted,j->cpu_views[i]};
 }
 CHECK(!remesa_gem_private_publish_locked(executing,views,3));
 if(fault==4)return -EIO;
 b->owner->session.sequence=sequence;b->owner->session.phase=SGX535_PHASE_RETIRED;
 b->owner->session.observed_events=fault==5?3:7;source_phase=4;return 0;
}
static int sgx535_fixed_service_may_release(struct model_service *s){CHECK(s);return 1;}
static int remesa_stream_source_finish_nohash(const void *who,struct remesa_stream_observation *o)
{
 struct retained_job *j=ticket_job(expected_memory,executing->ticket);
 CHECK(driver.sem.held&&who==expected_memory&&source_phase==4&&o->bound);
 CHECK(j->state==REMESA_JOB_VISIBLE);
 if(fault==6)return -EIO;
 o->finished=true;source_phase=5;return 0;
}
static int sgx535_gma500_fixed_backend_end(struct sgx535_gma500_fixed_backend *b)
{
 CHECK(driver.sem.held&&source_phase==5&&b->active);
 if(fault==7)return -EBUSY;
 b->active=false;b->private_loan=NULL;source_phase=6;return 0;
}
static int remesa_stream_source_unbind(const void *who,struct remesa_stream_observation *o)
{
 CHECK(driver.sem.held&&who==expected_memory&&source_phase==7&&o->bound&&o->finished);
 if(fault==9)return -EIO;
 o->bound=false;source_phase=8;return 0;
}
#include "../remesa_gem_private_execute.inc"
static void execute_case(unsigned failure,unsigned cpu_only)
{
 struct test_mmu mmu={.has_clflush=true,.pd={.driver=&driver}};
 struct drm_psb_private priv={&mmu,(void*)1,{0x28000000,32,0x80000000}};
 struct drm_device dev={&priv};struct remesa_gem_memory *s=NULL;
 struct remesa_twod_reservation guard={.device=&priv,.held=true};
 struct remesa_memory_binding uses[3];struct remesa_gem_private_rule rules[3];
 struct remesa_memory_view views[3];struct remesa_memory_ticket ticket;
 struct remesa_gem_private loan={0};struct sgx535_gma500_fixed_backend backend={0};
 struct remesa_stream_observation observation={0};u32 command=0;
 CHECK(!remesa_gem_memory_open(&dev,&file,77,&s));expected_memory=s;expected_guard=&guard;
 mode=cpu_only?7:0;fault=failure;source_phase=flush_count=0;
 const u64 addresses[3]={0x42000000,0x40000000,0x41000000};
 for(unsigned i=0;i<3;i++){
  u32 id=create(s,32,32,addresses[i]);struct backing *b=&s->bo[id&31];
  uses[i]=(struct remesa_memory_binding){id,b->va.start,b->gem.size};
  rules[i]=(struct remesa_gem_private_rule){id,i+1,i!=0};
 }
 if(cpu_only)rules[0].access=0;
 CHECK(!remesa_gem_job_prepare(s,uses,3,&command,sizeof command,&ticket));
 CHECK(remesa_gem_job_views(s,ticket,views,3)==3);
 CHECK(!remesa_gem_private_prepare(s,ticket,rules,3,&guard,&loan));executing=&loan;
 struct sgx535_gma500_owner owner={.dev=&dev,.mmu_end=s->root.end+1,.commands=command};
 struct retained_job *j=ticket_job(s,ticket);unsigned d=execute_downs,u=execute_ups;
 if(!failure){
  CHECK(remesa_gem_private_execute(&loan,&owner,&backend,&observation,s,views,2,10,20)==-EINVAL);
  CHECK(remesa_gem_private_execute(&loan,&owner,&backend,&observation,s,views,3,0,20)==-EINVAL);
  CHECK(remesa_gem_private_execute(&loan,&owner,&backend,&observation,s,views,3,60*HZ+1,20)==-EINVAL);
  CHECK(remesa_gem_private_execute(&loan,&owner,&backend,&observation,s,views,3,10,300001)==-EINVAL);
  owner.commands++;
  CHECK(remesa_gem_private_execute(&loan,&owner,&backend,&observation,s,views,3,10,20)==-EACCES);
  owner.commands--;
  struct remesa_memory_view bad[3];memcpy(bad,views,sizeof bad);bad[2].cpu=(void*)1;
  CHECK(remesa_gem_private_execute(&loan,&owner,&backend,&observation,s,bad,3,10,20)==-EACCES);
  CHECK(!loan.execution_attempted&&!observation.bound&&!source_phase&&!flush_count);
  CHECK(execute_downs==d&&execute_ups==u&&!j->quarantined);
 }
 test_flush_hook=execute_flush;
 int error=remesa_gem_private_execute(&loan,&owner,&backend,&observation,s,views,3,10,20);
 test_flush_hook=NULL;
 CHECK(!driver.sem.held&&!s->lock.held&&execute_downs-d==execute_ups-u);
 if(failure==1){
  CHECK(error==-EBUSY&&!loan.execution_attempted&&!j->quarantined&&j->state==REMESA_JOB_PREPARED);
  CHECK(!loan.owner->root_active&&!observation.bound&&!flush_count);
  CHECK(!remesa_gem_private_release(&loan));CHECK(!remesa_gem_job_cancel(s,ticket));
 }else if(failure){
  CHECK(error&&loan.execution_attempted&&!loan.execution_retired&&j->quarantined);
  CHECK(j->state==REMESA_JOB_QUARANTINED&&j->attached&&guard.mmu_owner==&loan);
  CHECK(execute_downs==d+1&&execute_ups==u+1);
  CHECK(remesa_gem_private_release(&loan)==-EBUSY&&loan.live);
  for(unsigned i=0;i<3;i++)CHECK(s->bo[j->ids[i]&31].holds==1&&s->bo[j->ids[i]&31].quarantine);
  CHECK(remesa_gem_memory_close(s)==-EBUSY);
  return; /* Joined child exits with retained modeled fault; no fake recovery. */
 }else{
  CHECK(!error&&loan.execution_retired&&loan.execution_started&&loan.execution_attempted);
  CHECK(j->state==REMESA_JOB_ACKNOWLEDGED&&!j->quarantined&&j->attached);
  CHECK(!loan.owner->root_active&&!backend.active&&!observation.bound&&source_phase==8);
  CHECK(!owner.fixed_service.observer_context&&flush_count==(cpu_only?4u:5u));
  CHECK(execute_downs==d+1&&execute_ups==u+1);
  CHECK(!remesa_gem_private_release(&loan));CHECK(!remesa_gem_job_retire(s,ticket));
 }
 CHECK(!remesa_gem_memory_close(s));CHECK(!gems&&!pins&&!ptes&&!claimed&&!power);
 expected_memory=NULL;expected_guard=NULL;executing=NULL;
}
int main(void)
{
 execute_case(0,0);execute_case(0,1);execute_case(1,0);
 for(unsigned f=2;f<=9;f++){
  pid_t child=fork();CHECK(child>=0);if(!child){execute_case(f,0);_exit(0);}
  int status;CHECK(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
 }
 printf("PASS %u private execution parent checks plus8 joined fault models; THREE-resource model, no GPU\n",checks);
 return 0;
}
