/* SPDX-License-Identifier: GPL-2.0-only. Real held-job code, CPU MMU callbacks. */
#define REMESA_GEM_PRIVATE_TEST_MAIN prior_private_capacity_fixture
#include "test_gem_private.c"
static unsigned lifecycle_count,lifecycle_stages;
static int lifecycle_validate(void *opaque,const void *commands,unsigned bytes,
 const struct remesa_memory_binding *uses,unsigned count)
{
 struct remesa_gem_memory *s=opaque;struct retained_job *j=&s->jobs[0];
 CHECK(s->lock.held&&j->state==REMESA_JOB_PREPARED&&count==lifecycle_count&&commands==j->commands&&bytes==4);
 for(unsigned i=0;i<count;i++)CHECK(uses[i].id==j->ids[i]&&s->bo[uses[i].id&31].holds==1);
 lifecycle_stages|=1;return 0;
}
static int lifecycle_start(void *opaque,struct remesa_memory_ticket t,const void *commands,unsigned bytes,u64 *cookie)
{
 struct remesa_gem_memory *s=opaque;struct retained_job *j=ticket_job(s,t);
 CHECK(s->lock.held&&j&&j->state==REMESA_JOB_ISSUED&&commands==j->commands&&bytes==4);
 lifecycle_stages|=2;*cookie=t.operation+100;return 0;
}
static int lifecycle_poll(void *opaque,struct remesa_memory_ticket t,u64 cookie,unsigned budget,struct remesa_job_completion *out)
{
 struct remesa_gem_memory *s=opaque;CHECK(s->lock.held&&budget==100&&cookie==t.operation+100);
 lifecycle_stages|=4;*out=(struct remesa_job_completion){t,cookie,REMESA_JOB_LIFECYCLE_ALL};return 0;
}
static int lifecycle_visible(void *opaque,struct remesa_memory_ticket t,u64 cookie)
{
 struct remesa_gem_memory *s=opaque;CHECK(s->lock.held&&ticket_job(s,t)->state==REMESA_JOB_COMPLETE&&cookie==t.operation+100);
 lifecycle_stages|=8;return 0;
}
static int lifecycle_ack(void *opaque,struct remesa_memory_ticket t,u64 cookie)
{
 struct remesa_gem_memory *s=opaque;CHECK(s->lock.held&&ticket_job(s,t)->state==REMESA_JOB_VISIBLE&&cookie==t.operation+100);
 lifecycle_stages|=16;return 0;
}
/* Only this CPU test installs a modeled executor; production remains gated. */
static const struct remesa_job_executor lifecycle_executor={lifecycle_validate,lifecycle_start,lifecycle_poll,lifecycle_visible,lifecycle_ack};
static void capacity_case(unsigned count,bool cpu_only,bool quarantine)
{
 struct test_mmu mmu={.has_clflush=true,.pd={.driver=&driver}};
 struct drm_psb_private priv={&mmu,(void*)1,{0x28000000,32,0x80000000}};
 struct drm_device dev={&priv};struct remesa_gem_memory *s=NULL;
 struct remesa_twod_reservation guard={.device=&priv,.held=true};
 struct remesa_memory_binding uses[REMESA_GEM_JOB_BINDINGS+1],bad[REMESA_GEM_JOB_BINDINGS+1];
 struct remesa_gem_private_rule rules[REMESA_GEM_JOB_BINDINGS+1];
 struct remesa_memory_view views[REMESA_GEM_JOB_BINDINGS],saved_views[REMESA_GEM_JOB_BINDINGS];
 struct remesa_memory_ticket ticket,saved_ticket;struct remesa_memory_snapshot snapshot;
 struct remesa_gem_private loan={0},empty={0};u32 command=0;
 CHECK(count>=16&&count<=REMESA_GEM_JOB_BINDINGS);
 mode=0;capacity_gpu_count=count-cpu_only;
 CHECK(!remesa_gem_memory_open(&dev,&file,77,&s));expected_memory=s;expected_guard=&guard;
 for(unsigned i=0;i<count;i++){
  u32 id=create(s,32,32,UINT64_C(0x40000000)+(count-1-i)*UINT64_C(0x01000000));
  struct backing *b=&s->bo[id&31];uses[i]=(struct remesa_memory_binding){id,b->va.start,b->gem.size};
  rules[i]=(struct remesa_gem_private_rule){id,i%3+1,i!=0};
 }
 if(cpu_only)rules[0].access=rules[0].cached=0;
 memset(&ticket,0xa5,sizeof ticket);saved_ticket=ticket;
 CHECK(remesa_gem_job_prepare(s,uses,REMESA_GEM_JOB_BINDINGS+1,&command,4,&ticket)==-EINVAL&&!memcmp(&ticket,&saved_ticket,sizeof ticket));
 memcpy(bad,uses,count*sizeof *uses);bad[count-1].id=bad[count-2].id;
 bad[count-1].va=bad[count-2].va;
 CHECK(remesa_gem_job_prepare(s,bad,count,&command,4,&ticket)==-EINVAL&&!memcmp(&ticket,&saved_ticket,sizeof ticket));
 memcpy(bad,uses,count*sizeof *uses);bad[count-1].va+=4096;
 CHECK(remesa_gem_job_prepare(s,bad,count,&command,4,&ticket)==-EINVAL&&!memcmp(&ticket,&saved_ticket,sizeof ticket));
 for(unsigned i=0;i<count;i++)CHECK(!s->bo[uses[i].id&31].holds);
 CHECK(!remesa_gem_job_prepare(s,uses,count,&command,4,&ticket));
 struct retained_job *j=ticket_job(s,ticket);CHECK(j&&j->count==count);
 for(unsigned i=0;i<count;i++)CHECK(j->ids[i]==uses[i].id&&s->bo[uses[i].id&31].holds==1);
 memset(views,0xa5,sizeof views);memcpy(saved_views,views,sizeof views);unsigned before=vmap_calls;
 CHECK(remesa_gem_job_views(s,ticket,views,count-1)==-ENOSPC&&!memcmp(views,saved_views,sizeof views)&&vmap_calls==before);
 fail_vmap_call=vmap_calls+count;
 CHECK(remesa_gem_job_views(s,ticket,views,count)==-ENOMEM&&!memcmp(views,saved_views,sizeof views));
 for(unsigned i=0;i<count;i++)CHECK(!j->cpu_views[i]&&!s->bo[uses[i].id&31].cached_job_view&&s->bo[uses[i].id&31].holds==1);
 fail_vmap_call=0;
 CHECK(remesa_gem_job_snapshot(s,ticket,&snapshot,views,count)==(int)count);
 CHECK(!remesa_gem_job_check_snapshot(s,&snapshot,views,count));
 for(unsigned i=0;i<count;i++)CHECK(views[i].binding.id==uses[i].id&&views[i].cpu&&views[i].cpu==j->cpu_views[i]);
 void *last=views[count-1].cpu;views[count-1].cpu=(void*)1;
 CHECK(remesa_gem_job_check_snapshot(s,&snapshot,views,count)==-EACCES);views[count-1].cpu=last;
 unsigned prior=prepares;rules[count-1].access=4;
 CHECK(remesa_gem_private_prepare(s,ticket,rules,count,&guard,&loan)==-EINVAL&&!memcmp(&loan,&empty,sizeof loan)&&prepares==prior);
 rules[count-1].access=(count-1)%3+1;
 CHECK(remesa_gem_private_prepare(s,ticket,rules,REMESA_GEM_JOB_BINDINGS+1,&guard,&loan)==-EINVAL&&!memcmp(&loan,&empty,sizeof loan));
 CHECK(!remesa_gem_private_prepare(s,ticket,rules,count,&guard,&loan)&&loan.owner->count==capacity_gpu_count);
 for(unsigned i=0;i<count;i++)CHECK(loan.rules[i].id==rules[i].id&&loan.rules[i].access==rules[i].access);
 CHECK(remesa_gem_job_cancel(s,ticket)==-EBUSY);
 mutex_lock(&s->lock);CHECK(!remesa_gem_private_validate_locked(&loan,views,count));
 unsigned prior_flush=flushes,prior_wmb=test_wmb,prior_bound=bound_checks;
 views[count-1].cpu=(void*)1;
 CHECK(remesa_gem_private_publish_locked(&loan,views,count)==-EACCES&&flushes==prior_flush&&test_wmb==prior_wmb&&bound_checks==prior_bound);
 views[count-1].cpu=last;publishing=&loan;publish_index=0;publish_wmb=test_wmb;test_flush_hook=publication_flush;
 CHECK(!remesa_gem_private_publish_locked(&loan,views,count));test_flush_hook=NULL;
 CHECK(flushes==prior_flush+capacity_gpu_count&&test_wmb==prior_wmb+1&&publish_index==count);
 CHECK(remesa_gem_private_publish_locked(&loan,views,count)==-EALREADY);mutex_unlock(&s->lock);
 CHECK(remesa_gem_job_issue(s,ticket)==-EOPNOTSUPP&&j->state==REMESA_JOB_PREPARED&&!j->cookie);
 if(quarantine){
  CHECK(!remesa_gem_job_quarantine(s,ticket)&&j->state==REMESA_JOB_QUARANTINED);
  CHECK(remesa_gem_job_cancel(s,ticket)==-EBUSY&&remesa_gem_memory_close(s)==-EBUSY);
  for(unsigned i=0;i<count;i++)CHECK(s->bo[uses[i].id&31].holds==1&&s->bo[uses[i].id&31].pinned);
  return; /* Child exits with intentional quarantined ownership; no GPU. */
 }
 CHECK(!remesa_gem_private_release(&loan)&&!memcmp(&loan,&empty,sizeof loan));
 for(unsigned i=0;i<count;i++)CHECK(s->bo[uses[i].id&31].holds==1);
 if(!cpu_only&&count>=17){
  lifecycle_count=count;lifecycle_stages=0;s->executor=&lifecycle_executor;s->executor_context=s;
  CHECK(!remesa_gem_job_issue(s,ticket)&&lifecycle_stages==3);
  CHECK(remesa_gem_job_retire(s,ticket)==-EBUSY&&remesa_gem_job_cancel(s,ticket)==-EBUSY);
  CHECK(!remesa_gem_job_wait(s,ticket,100)&&lifecycle_stages==7);
  CHECK(remesa_gem_job_retire(s,ticket)==-EBUSY);
  CHECK(!remesa_gem_job_publish(s,ticket)&&lifecycle_stages==15);
  CHECK(remesa_gem_job_retire(s,ticket)==-EBUSY);
  for(unsigned i=0;i<count;i++)CHECK(s->bo[uses[i].id&31].holds==1);
  CHECK(!remesa_gem_job_acknowledge(s,ticket)&&lifecycle_stages==31);
  for(unsigned i=0;i<count;i++)CHECK(s->bo[uses[i].id&31].holds==1);
  CHECK(!remesa_gem_job_retire(s,ticket));s->executor=NULL;s->executor_context=NULL;
 }else CHECK(!remesa_gem_job_cancel(s,ticket));
 for(unsigned i=0;i<count;i++)CHECK(!s->bo[uses[i].id&31].holds);
 CHECK(!remesa_gem_memory_close(s)&&!gems&&!pins&&!ptes&&!claimed&&!power);
 expected_memory=NULL;expected_guard=NULL;capacity_gpu_count=0;
}
int main(void)
{
 CHECK(!prior_private_capacity_fixture());
 capacity_case(16,false,false);capacity_case(17,false,false);capacity_case(32,false,false);capacity_case(32,true,false);
 pid_t child=fork();CHECK(child>=0);
 if(!child){capacity_case(32,false,true);_exit(0);}
 int status;CHECK(waitpid(child,&status,0)==child&&WIFEXITED(status)&&!WEXITSTATUS(status));
 puts("PASS retained capacity16/17/32: full holds/views/rules, sorted permissions, CPU-only sidecar, last-view rollback/tamper, rejection33, publication before no native issue, complete modeled lifecycle/retirement, cancellation and quarantine; GPU0");return 0;
}
