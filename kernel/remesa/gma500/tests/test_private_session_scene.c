/* SPDX-License-Identifier: GPL-2.0-only. Actual source/backing/scene adapter;
 * executor, memory, MMIO and retirement are modeled. GPU0. */
#define REMESA_PRIVATE_SESSION_SOURCE_FIXTURE
#define remesa_gem_job_quarantine inherited_job_quarantine
#include "test_private_session_source.c"
#undef remesa_gem_job_quarantine
static bool quarantine_failed;
static unsigned scene_calls;
static int remesa_gem_job_quarantine(struct remesa_gem_memory *m,struct remesa_memory_ticket t)
{return quarantine_failed?-EUCLEAN:inherited_job_quarantine(m,t);}
enum {REMESA_SCENE_JOB_PREPARED=1,REMESA_SCENE_JOB_HELD,REMESA_SCENE_JOB_ACKNOWLEDGED};
struct remesa_gem_scene_job {
 struct remesa_gem_memory *memory;unsigned state,count;struct remesa_gem_private loan;
};
static int remesa_gem_scene_job_execute(struct remesa_gem_scene_job *j,
 struct sgx535_gma500_fixed_backend *b,struct remesa_stream_observation *o,
 const void *token,unsigned ticks,unsigned samples)
{
 CHECK(rank==1&&endpoint_held&&b&&o&&ticks==10&&samples==20);
 CHECK(source_live&&token==source_token&&j->memory==&s&&j->loan.memory==&s&&s.jobs[0].live);
 CHECK(j->count==18&&!j->loan.execution_attempted&&!o->finished);scene_calls++;
 if(fault==1)return -EACCES;
 j->loan.execution_attempted=true;if(fault==2)return -EIO;
 j->loan.execution_retired=true;j->state=REMESA_SCENE_JOB_ACKNOWLEDGED;o->finished=true;return 0;
}
#define REMESA_PRIVATE_SESSION_SCENE_TEST
#include "remesa_private_session_scene.inc"
static struct remesa_gem_scene_job scene_job(u64 op)
{
 s.jobs[0].live=true;s.bo[0].holds=1;
 return (struct remesa_gem_scene_job){.memory=&s,.state=REMESA_SCENE_JOB_PREPARED,.count=18,
  .loan={.memory=&s,.ticket={op}}};
}
static int unused_guard(u64 address,const void *data,unsigned bytes)
{(void)address;(void)data;(void)bytes;assert(0);return -EINVAL;}
int main(void)
{
 CHECK(!backing_fixture_main());fresh_boot();open_source();CHECK(!job(1));
 CHECK(!remesa_private_session_source_end(&s,&owner)&&!remesa_private_session_release(&s,&owner));
 fresh_boot();open_source();struct sgx535_gma500_fixed_backend b={0};struct remesa_stream_observation o={0};
 struct remesa_gem_scene_job j=scene_job(1);unsigned calls=scene_calls;
#define RUN(m,t,c,back,obs) remesa_private_session_scene_execute(m,t,c,back,obs,10,20)
 CHECK(RUN(NULL,&owner,&j,&b,&o)==-EINVAL);CHECK(RUN(&s,NULL,&j,&b,&o)==-EINVAL);
 CHECK(RUN(&s,&owner,NULL,&b,&o)==-EINVAL);CHECK(RUN(&s,&owner,&j,NULL,&o)==-EINVAL);
 CHECK(RUN(&s,&owner,&j,&b,NULL)==-EINVAL);CHECK(RUN((void *)1,&owner,&j,&b,&o)==-EACCES);
 CHECK(RUN(&s,&otherowner,&j,&b,&o)==-EACCES);
 s.private_source_held=false;CHECK(RUN(&s,&owner,&j,&b,&o)==-EACCES);s.private_source_held=true;
 j.memory=NULL;CHECK(RUN(&s,&owner,&j,&b,&o)==-EACCES);j.memory=&s;
 j.loan.memory=NULL;CHECK(RUN(&s,&owner,&j,&b,&o)==-EACCES);j.loan.memory=&s;
 for(unsigned i=0;i<5;i++){
  s.private_input_ids[i]=32;CHECK(RUN(&s,&owner,&j,&b,&o)==-EOPNOTSUPP);s.private_input_ids[i]=0;
 }
 for(unsigned i=0;i<6;i++){
  s.private_work_ids[i]=32;CHECK(RUN(&s,&owner,&j,&b,&o)==-EOPNOTSUPP);s.private_work_ids[i]=0;
 }
 s.private_work_owned=true;CHECK(RUN(&s,&owner,&j,&b,&o)==-EOPNOTSUPP);s.private_work_owned=false;
 s.pds_write_guard=unused_guard;CHECK(RUN(&s,&owner,&j,&b,&o)==-EOPNOTSUPP);s.pds_write_guard=NULL;
 CHECK(scene_calls==calls&&!s.private_source_poisoned&&!poisons);
 for(u64 op=1;op<=3;op++){
  j=scene_job(op);o=(struct remesa_stream_observation){0};CHECK(!RUN(&s,&owner,&j,&b,&o));
  CHECK(j.state==REMESA_SCENE_JOB_ACKNOWLEDGED&&j.loan.execution_retired&&o.finished);
  CHECK(claims==1&&!releases&&!poisons&&s.private_source_held&&source_live);
  /* Separate modeled job release/retirement, not performed by the adapter. */
  s.jobs[0].live=false;s.bo[0].holds=0;
 }
 CHECK(scene_calls==calls+3&&!remesa_private_session_source_end(&s,&owner));
 CHECK(settles==1&&releases==1&&!source_live&&!remesa_private_session_release(&s,&owner));
 struct remesa_gem_memory *m=NULL;CHECK(!acquire(&m));
 CHECK(remesa_private_session_source_begin(&s,&owner)==-EBUSY&&!s.private_source_held);
 CHECK(!remesa_private_session_release(&s,&owner));
 for(unsigned mode=0;mode<3;mode++){
  fresh_boot();open_source();fault=mode==1?2:1;quarantine_failed=mode==2;
  j=scene_job(1);o=(struct remesa_stream_observation){0};calls=scene_calls;
  CHECK(RUN(&s,&owner,&j,&b,&o)==(fault==1?-EACCES:-EIO));
  CHECK(j.state==REMESA_SCENE_JOB_HELD&&s.private_source_poisoned&&s.private_source_held&&source_live&&poisons==1);
  CHECK(s.jobs[0].quarantined==!quarantine_failed);
  CHECK(RUN(&s,&owner,&j,&b,&o)==-EIO&&scene_calls==calls+1);
  CHECK(remesa_private_session_source_end(&s,&owner)==-EIO&&remesa_private_session_release(&s,&owner)==-EBUSY);
  CHECK(remesa_gem_memory_close(&s)==-EBUSY&&!cleanup);
 }
 for(unsigned mode=0;mode<2;mode++){
  fresh_boot();open_source();j=scene_job(1);o=(struct remesa_stream_observation){0};calls=scene_calls;
  if(mode)s.file=NULL;else remesa_private_memory_device_lost(&dev);
  CHECK(RUN(&s,&owner,&j,&b,&o)==-ENODEV&&scene_calls==calls&&!poisons&&!settles);
 }
 CHECK(!rank&&!endpoint_held);
 printf("PASS %u source/scene adapter checks;3 modeled general jobs/one lease, legacy-seal exclusion, loss/quarantine/no-retry retention; GPU0\n",checks);
 return 0;
}
