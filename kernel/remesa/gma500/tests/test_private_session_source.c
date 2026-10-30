/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual session orchestration; source and qualified cold executor modeled.
 * Does not model GPU results as physical completion or alter LAB admission. */
#define main backing_fixture_main
#include "test_private_session.c"
#undef main
#define REMESA_PRIVATE_SESSION_SOURCE_TEST
struct remesa_memory_ticket {u64 operation;};
struct remesa_gem_private {
 struct remesa_gem_memory *memory;struct remesa_memory_ticket ticket;
 bool execution_attempted,execution_retired;
};
struct sgx535_gma500_owner {unsigned unused;};
struct sgx535_gma500_fixed_backend {unsigned unused;};
struct remesa_stream_observation {bool finished;};
struct remesa_memory_view {unsigned unused;};
static unsigned claims,releases,settles,executes,poisons,fault;
static bool ever_claimed,source_live;
static const void *source_token;
static int remesa_stream_source_claim(struct drm_device *d,const void *who)
{
 CHECK(rank==2&&endpoint_held&&d==&dev&&who==&owner);claims++;
 if(ever_claimed)return -EBUSY;
 ever_claimed=true;source_live=true;source_token=who;return 0;
}
static int remesa_stream_source_settled(const void *who)
{CHECK(rank==2&&source_live&&who==source_token);settles++;return fault==3?-EIO:0;}
static int remesa_stream_source_release(const void *who)
{
 CHECK(rank==2&&source_live&&who==source_token&&settles);releases++;
 if(fault==4)return -EIO;
 source_live=false;return 0;
}
static void remesa_stream_source_poison(const void *who)
{CHECK(endpoint_held&&source_live&&who==source_token);poisons++;}
static int remesa_gem_job_quarantine(struct remesa_gem_memory *m,struct remesa_memory_ticket t)
{
 CHECK(rank==1&&m==&s&&t.operation);mutex_lock(&m->lock);
 m->jobs[0].quarantined=true;m->jobs[0].state=REMESA_JOB_QUARANTINED;
 m->bo[0].quarantine=true;mutex_unlock(&m->lock);return 0;
}
static int remesa_gem_private_execute(struct remesa_gem_private *loan,
 struct sgx535_gma500_owner *o,struct sgx535_gma500_fixed_backend *b,
 struct remesa_stream_observation *observation,const void *who,
 const struct remesa_memory_view *v,unsigned count,unsigned ticks,unsigned samples)
{
 CHECK(rank==1&&endpoint_held&&o&&b&&v&&count==11&&ticks==10&&samples==20);
 CHECK(source_live&&who==source_token&&loan->memory==&s&&s.jobs[0].live);
 mutex_lock(&s.lock);executes++;
 CHECK(!loan->execution_attempted&&!observation->finished);
 if(fault==1){mutex_unlock(&s.lock);return -EACCES;}
 loan->execution_attempted=true;
 if(fault==2){mutex_unlock(&s.lock);return -EIO;}
 loan->execution_retired=true;observation->finished=true;
 mutex_unlock(&s.lock);return 0;
}
#include "remesa_private_session_source.inc"
static void fresh_boot(void)
{
 fixture();ever_claimed=source_live=false;source_token=NULL;
 claims=releases=settles=executes=poisons=fault=0;
}
static void open_source(void)
{
 struct remesa_gem_memory *m=NULL;
 CHECK(!acquire(&m)&&m==&s);
 CHECK(!remesa_private_session_source_begin(&s,&owner));
 CHECK(source_live&&claims==1&&s.private_source_held);
}
static int job(u64 operation)
{
 struct remesa_gem_private loan={.memory=&s,.ticket={operation}};
 struct sgx535_gma500_owner o={0};struct sgx535_gma500_fixed_backend b={0};
 struct remesa_stream_observation obs={0};struct remesa_memory_view views[11]={{0}};
 s.jobs[0].live=true;s.bo[0].holds=1;
 int e=remesa_private_session_execute(&s,&owner,&loan,&o,&b,&obs,views,11,10,20);
 if(!e){CHECK(loan.execution_retired&&obs.finished);s.jobs[0].live=false;s.bo[0].holds=0;}
 return e;
}
int main(void)
{
 CHECK(!backing_fixture_main());fresh_boot();
 CHECK(remesa_private_session_source_begin(&s,&owner)==-EACCES);
 CHECK(remesa_private_session_source_begin((void*)1,&owner)==-EACCES);
 open_source();
 CHECK(remesa_private_session_source_begin(&s,&owner)==-EBUSY&&claims==1);
 CHECK(remesa_private_session_source_end(&s,&otherowner)==-EACCES&&!settles&&!releases);
 CHECK(remesa_private_session_release(&s,&owner)==-EBUSY);
 for(u64 op=1;op<=3;op++){
  CHECK(!job(op));CHECK(source_live&&claims==1&&!releases&&!poisons);
  CHECK(s.private_source_held&&s.private_session==&owner);
 }
 s.bo[0].lease=true;CHECK(remesa_private_session_source_end(&s,&owner)==-EBUSY&&!settles);
 s.bo[0].lease=false;
 CHECK(!remesa_private_session_source_end(&s,&owner)&&!s.private_source_held);
 CHECK(settles==1&&releases==1&&executes==3&&!source_live);
 CHECK(!remesa_private_session_release(&s,&owner));
 struct remesa_gem_memory *m=NULL;CHECK(!acquire(&m));
 CHECK(remesa_private_session_source_begin(&s,&owner)==-EBUSY&&claims==2);
 CHECK(!s.private_source_held&&!remesa_private_session_release(&s,&owner));
 for(unsigned f=1;f<=4;f++){
  fresh_boot();open_source();fault=f;
  if(f<=2){
   CHECK(job(1)<0&&executes==1&&poisons==1&&s.jobs[0].quarantined);
   CHECK(job(2)==-EIO&&executes==1); /* no retry/second cold call */
  }else CHECK(remesa_private_session_source_end(&s,&owner)==-EIO&&poisons==1);
  CHECK(s.private_source_held&&s.private_source_poisoned&&source_live);
  CHECK(remesa_private_session_source_end(&s,&owner)==-EIO);
  CHECK(remesa_private_session_release(&s,&owner)==-EBUSY);
  CHECK(remesa_gem_memory_close(&s)==-EBUSY&&!cleanup);
 }
 fresh_boot();open_source();remesa_private_memory_device_lost(&dev);
 CHECK(job(1)==-ENODEV&&!executes);
 CHECK(remesa_private_session_source_end(&s,&owner)==-ENODEV&&!settles&&!releases);
 CHECK(remesa_private_session_release(&s,&owner)==-ENODEV);
 CHECK(!rank&&!endpoint_held);
 printf("PASS %u source/backing-session checks; three modeled jobs/one lease, no rearm, GPU0\n",checks);
 return 0;
}
