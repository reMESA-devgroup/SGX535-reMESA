/* SPDX-License-Identifier: GPL-2.0-only. Real transaction/GEM/service, modeled
 * source/backend/MMU hardware hooks. Failure processes exit with holds intact;
 * there is no invented successful teardown/retirement after a failed issue. */
#define REMESA_SCENE_EXECUTION_TEST 1
#define HZ 100u
struct remesa_stream_observation {bool bound,finished;};
static unsigned execute_fault,execute_phase,execute_cases,execute_stages,execute_samples,execute_flushes;
static struct remesa_gem_scene_job *execute_context;
static void down_write(struct private_test_sem *sem)
{CHECK(!sem->held&&execute_context->memory->lock.held&&execute_phase==1);sem->held=1;}
static void up_write(struct private_test_sem *sem)
{CHECK(sem->held&&execute_context->memory->lock.held);sem->held=0;}
static int remesa_stream_source_bind(const void *who,struct remesa_stream_observation *o,
 struct sgx535_gma500_fixed_backend *b,u64 op,u64 seq)
{
 CHECK(who==execute_context->memory&&!driver.sem.held&&!o->bound&&!b->active);
 CHECK(op==execute_context->loan.ticket.operation&&op==seq);
 if(execute_fault==1)return -EBUSY;
 execute_phase=1;o->bound=true;return 0;
}
int remesa_mmu_private_bind_locked(struct remesa_mmu_private *o)
{CHECK(driver.sem.held&&execute_phase==1&&o==execute_context->loan.owner);o->root_active=1;execute_phase=2;return execute_fault==2?-EIO:0;}
int remesa_mmu_private_restore_locked(struct remesa_mmu_private *o)
{CHECK(driver.sem.held&&execute_phase==6&&o->root_active);if(execute_fault==8)return -EIO;o->root_active=0;execute_phase=7;return 0;}
int remesa_mmu_private_quarantine_locked(struct remesa_mmu_private *o)
{CHECK(driver.sem.held&&o==execute_context->loan.owner);o->blocked=1;return 0;}
static int execute_backend_begin(struct sgx535_gma500_fixed_backend *b,struct remesa_gem_scene_job *c)
{
 CHECK(driver.sem.held&&execute_phase==2&&c==execute_context&&c->service.plan==&c->service_plan);
 CHECK(!remesa_gem_private_validate_locked(&c->loan,c->views,c->count));
 if(execute_fault==3)return -EIO;
 b->general_scene=c;b->private_loan=&c->loan;b->active=true;execute_phase=3;return 0;
}
static int execute_backend_run(void *context,enum sgx535_fixed_stage stage,const void *payload,size_t count,struct sgx535_fixed_observation *out)
{
 struct sgx535_gma500_fixed_backend *b=context;struct remesa_gem_scene_job *c=execute_context;
 struct retained_job *j=ticket_job(c->memory,c->snapshot.ticket);
 CHECK(driver.sem.held&&execute_phase==3&&b->general_scene==c&&stage==++execute_stages);
 CHECK(j->state==REMESA_JOB_ISSUED&&j->cookie==c->loan.ticket.operation&&c->loan.execution_started);
 if(stage==SGX535_FIXED_PUBLISH_CPU){
  CHECK(!payload&&!count);
  if(execute_fault==4)return -EIO;
  CHECK(!remesa_gem_private_publish_locked(&c->loan,c->views,c->count));
 }
 if(stage==SGX535_FIXED_TA_LOAD){out->load_flags=31;out->status2=15;out->initend=0x400000;}
 if(stage==SGX535_FIXED_USE_RELEASE){CHECK(c->session.phase==SGX535_PHASE_SCENE_COMPLETED);execute_phase=4;}
 return 0;
}
static int execute_sample(void *context,u32 sequence,u32 *a,u32 *d,int *owned)
{CHECK(context&&driver.sem.held&&execute_phase==3&&sequence==execute_context->loan.ticket.operation);*owned=1;*d=execute_fault==5?16:0;*a=execute_samples++?((1u<<18)|1):(1u<<13);return 0;}
static const struct sgx535_fixed_ops execute_ops={execute_backend_run};
static const struct sgx535_fixed_status_source execute_status={execute_sample};
static int remesa_stream_source_finish_nohash(const void *who,struct remesa_stream_observation *o)
{
 struct retained_job *j=ticket_job(execute_context->memory,execute_context->snapshot.ticket);
 CHECK(driver.sem.held&&execute_phase==4&&who==execute_context->memory&&o->bound&&j->state==REMESA_JOB_VISIBLE);
 CHECK(execute_context->session.phase==SGX535_PHASE_RETIRED&&execute_context->session.observed_events==7);
 if(execute_fault==6)return -EIO;
 o->finished=true;execute_phase=5;return 0;
}
static int sgx535_gma500_fixed_backend_end(struct sgx535_gma500_fixed_backend *b)
{CHECK(driver.sem.held&&execute_phase==5&&b->active);if(execute_fault==7)return -EIO;b->active=false;b->private_loan=NULL;execute_phase=6;return 0;}
static int remesa_stream_source_unbind(const void *who,struct remesa_stream_observation *o)
{CHECK(driver.sem.held&&execute_phase==7&&who==execute_context->memory&&o->finished&&o->bound);if(execute_fault==9)return -EIO;o->bound=false;execute_phase=8;return 0;}
static int remesa_stream_source_settled(const void *who)
{CHECK(driver.sem.held&&execute_phase==8&&who==execute_context->memory);if(execute_fault==10)return -EIO;execute_phase=9;return 0;}
#define remesa_scene_backend_begin execute_backend_begin
#define remesa_scene_backend_ops execute_ops
#define sgx535_gma500_fixed_backend_status_source execute_status
#include "../remesa_gem_scene_execute.inc"
#undef remesa_scene_backend_begin
#undef remesa_scene_backend_ops
#undef sgx535_gma500_fixed_backend_status_source
static int execute_default_pfn(u32 va,unsigned long *pfn)
{CHECK(!driver.sem.held);unsigned i=va>>12;if(test_pt[i].live&&test_pt[i].va==va){*pfn=test_pt[i].pfn;return 0;}return -EINVAL;}
static void execute_flush(struct page **pages,unsigned count)
{
 struct remesa_gem_scene_job *c=execute_context;struct retained_job *j=ticket_job(c->memory,c->snapshot.ticket);unsigned found=0;
 CHECK(driver.sem.held&&(execute_phase==3||execute_phase==4));
 for(unsigned i=0;i<c->count;i++)if(c->views[i].pages==pages){
  CHECK(count==c->views[i].page_count&&c->memory->bo[j->ids[i]&31].holds==1);
  CHECK(execute_phase==3?c->loan.rules[i].access:(c->loan.rules[i].access&REMESA_MMU_GPU_WRITE));found++;
 }
 CHECK(found==1);execute_flushes++;
}
static void scene_execute_case(struct remesa_gem_scene_job *c)
{
 struct sgx535_gma500_fixed_backend b={0};struct remesa_stream_observation observation={0};
 struct retained_job *j=ticket_job(c->memory,c->snapshot.ticket);execute_context=c;
 execute_phase=execute_stages=execute_samples=execute_flushes=0;
 /* No native run occurred in earlier protocol tests: reset only their binding
  * metadata, then exercise the real executor's binding after its root bind. */
 c->service=(struct remesa_scene_service){0};c->session=(struct sgx535_frozen_session){0};
 test_pfn_hook=execute_default_pfn;test_flush_hook=execute_flush;
 mutex_lock(&c->memory->lock);
 CHECK(!remesa_gem_scene_job_check_snapshot_locked(c));
 void *saved=c->views[9].cpu;c->views[9].cpu=NULL;CHECK(remesa_gem_scene_job_check_snapshot_locked(c)==-EACCES);c->views[9].cpu=saved;
 c->views[1].cpu=saved;CHECK(remesa_gem_scene_job_check_snapshot_locked(c)==-EACCES);c->views[1].cpu=NULL;
 c->snapshot.power_held=false;CHECK(remesa_gem_scene_job_check_snapshot_locked(c)==-EACCES);c->snapshot.power_held=true;
 c->loan.rules[0].id++;CHECK(remesa_gem_scene_job_check_snapshot_locked(c)==-EACCES);c->loan.rules[0].id--;
 mutex_unlock(&c->memory->lock);
 int error=remesa_gem_scene_job_execute(c,&b,&observation,c->memory,10,20);execute_cases++;
 CHECK(!driver.sem.held&&!c->memory->lock.held);
 test_flush_hook=NULL;test_pfn_hook=NULL;
 if(execute_fault){
  CHECK(error);
  if(execute_fault==1)CHECK(!c->loan.execution_attempted&&!j->quarantined&&c->state==REMESA_SCENE_JOB_PREPARED);
  else{
   CHECK(c->state==REMESA_SCENE_JOB_HELD&&j->quarantined&&c->loan.live&&c->loan.execution_attempted&&!c->loan.execution_retired);
   CHECK(remesa_gem_scene_job_cancel(c)==-EBUSY&&remesa_gem_private_release(&c->loan)==-EBUSY);
   for(unsigned i=0;i<c->count;i++)CHECK(c->memory->bo[j->ids[i]&31].holds==1);
  }
  printf("PASS general executor fault%u retained; process exits with modeled holds intact, GPU0\n",execute_fault);exit(0);
 }
 CHECK(!error&&execute_phase==9&&execute_stages==19&&execute_flushes==22);
 CHECK(c->state==REMESA_SCENE_JOB_ACKNOWLEDGED&&c->loan.execution_retired&&!c->loan.owner->root_active&&!observation.bound);
 CHECK(remesa_gem_scene_job_cancel(c)==-EBUSY);
 CHECK(!remesa_gem_scene_job_retire(c)&&!c->memory);
}
