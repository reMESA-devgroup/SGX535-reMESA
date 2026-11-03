/* SPDX-License-Identifier: GPL-2.0-only */
/* Joined actual fill endpoint + normal backing owner. Job execution and
 * receipts modeled; separate tests cover real native/GEM retirement code. */
#define REMESA_GENERAL_SCENE 1
#define REMESA_MATRIX_CONTROLLER_FIXTURE
#include "test_matrix_submit_handler.c"
#include "../remesa_twod_native.h"
static unsigned endpoint_mode,starts,cancels;
static enum remesa_job_state endpoint_state;
static struct remesa_gem_twod_command endpoint_command;
int remesa_gem_job_prepare(struct remesa_gem_memory *s,const struct remesa_memory_binding *uses,
 unsigned count,const void *command,unsigned bytes,struct remesa_memory_ticket *out)
{
 CHECK(endpoint_lock.held&&s==endpoint&&count==1&&uses[0].id==44&&bytes==sizeof endpoint_command);
 if(endpoint_mode==1||endpoint_mode==8)return -ENOMEM;
 endpoint_command=*(const struct remesa_gem_twod_command *)command;
 *out=(struct remesa_memory_ticket){.context=77,.operation=1,.generation=1};
 s->jobs[0].live=true;s->bo[12].holds=1;endpoint_state=REMESA_JOB_PREPARED;return 0;
}
int remesa_gem_job_state(struct remesa_gem_memory *s,struct remesa_memory_ticket t,enum remesa_job_state *out)
{CHECK(s==endpoint&&t.operation==1);if(!s->jobs[0].live)return -ENOENT;*out=endpoint_state;return 0;}
int remesa_gem_job_cancel(struct remesa_gem_memory *s,struct remesa_memory_ticket t)
{CHECK(s==endpoint&&t.operation==1&&endpoint_state==REMESA_JOB_PREPARED);cancels++;s->jobs[0].live=false;s->bo[12].holds=0;return 0;}
const struct remesa_twod_native_ops remesa_twod_native_executor={0};
int remesa_gem_twod_issue(struct remesa_gem_memory *s,struct remesa_memory_ticket t,
 const struct remesa_twod_native_ops *ops,void *context,struct remesa_twod_job *run)
{
 CHECK(endpoint_lock.held&&s==endpoint&&ops==&remesa_twod_native_executor&&context);
 if(endpoint_mode==2)return -ENODATA;
 run->live=true;run->memory=s;run->launch.ticket=t;run->launch.dev=s->dev;run->launch.pd=s->pd;run->cookie=101;
 CHECK(!remesa_twod_fill_prepare(&endpoint_command.fill,&run->launch.plan));
 struct remesa_twod_native *native=context;native->core=0x01130000;native->revision=0x10201;
 s->executor=context;endpoint_state=REMESA_JOB_ISSUED;starts++;
 return endpoint_mode==3?-EIO:0;
}
int remesa_gem_twod_wait(struct remesa_twod_job *run,unsigned budget)
{CHECK(run->live&&budget==1000);endpoint_state=REMESA_JOB_COMPLETE;return 0;}
int remesa_gem_twod_publish(struct remesa_twod_job *run)
{CHECK(run->live&&endpoint_state==REMESA_JOB_COMPLETE);endpoint_state=REMESA_JOB_VISIBLE;return 0;}
int remesa_gem_twod_retire(struct remesa_twod_job *run)
{
 struct remesa_gem_memory *s=run->memory;
 struct remesa_twod_retirement proof={.dev=s->dev,.pd=s->pd,.ticket=run->launch.ticket,.cookie=run->cookie,
  .destination_id=run->launch.plan.destination_id,.packet_dwords=7,.core=0x01130000,.revision=0x10201,
  .native_retired=endpoint_mode!=4,.gem_retired=endpoint_mode!=4};
 if(endpoint_mode==5)proof.cookie++;
 s->executor=NULL;s->jobs[0].live=false;s->bo[12].holds=0;
 memset(run,0,sizeof(*run));run->retirement=proof;return 0;
}
#include "../remesa_twod_copy.inc"
#define twod_fill_command endpoint_actual_fill_command
#include "../remesa_twod_fill.inc"
#undef twod_fill_command
int main(int argc,char **argv)
{
 CHECK(argc==2);endpoint_mode=(unsigned)strtoul(argv[1],NULL,10);CHECK(endpoint_mode<9);
 struct drm_psb_private priv={(void*)1};struct drm_device dev={&priv};struct drm_file file={1};
 struct remesa_gem_memory memory={.dev=&dev,.file=&file,.context=77,.pd=(void*)1};endpoint=&memory;
 memory.bo[12]=(struct backing){.live=true,.pinned=true,.reserved=true,.initialized=true,.generation=1,
  .va={0x40001000,0x403e8fff},.gem={4096000}};
 matrix_submit_enabled=endpoint_mode!=6;twod_fill_enabled=true;
 struct remesa_twod_fill_request r={.version=1,.context=77,.budget_ms=1000,
  .destination={44,1280,800,5120},.width=1280,.height=800};
 if(endpoint_mode==7)fault=CLAIM;
 if(endpoint_mode==8)fault=SOURCE_END;
 int error=remesa_twod_fill_ioctl(&dev,&r,&file);
 if(endpoint_mode==7){
  CHECK(error==-EIO&&!memory.private_session&&!matrix_fill_owner&&!starts&&allocs==frees);
 }else if(endpoint_mode==8){
  CHECK(error==-EOWNERDEAD&&memory.private_session&&matrix_fill_owner&&!starts&&memory.private_source_held&&memory.private_source_poisoned);
 }else if(endpoint_mode==1){
  CHECK(error==-ENOMEM&&!memory.private_session&&!matrix_fill_owner&&!twod_copy_retained&&allocs==frees&&!starts);
 }else if(endpoint_mode==2){
  CHECK(!error&&r.error==-ENODATA&&!r.retained&&r.stages==1&&cancels==1&&!starts);
  CHECK(!memory.private_session&&!matrix_fill_owner&&!twod_copy_retained&&allocs==frees);
 }else if(endpoint_mode>=3&&endpoint_mode<=5){
  CHECK(!error&&r.error==(endpoint_mode==3?-EIO:-EPROTO)&&r.retained);
  CHECK(twod_copy_retained&&matrix_fill_owner&&matrix_fill_owner->failed&&memory.private_session&&!memory.private_matrix_constructing);
 }else{
  CHECK(!error&&!r.error&&!r.retained&&r.stages==31&&r.operation==1&&r.cookie==101&&starts==1);
  CHECK(!twod_copy_retained&&!memory.jobs[0].live&&!memory.executor);
  if(endpoint_mode==6)CHECK(!memory.private_session&&!matrix_fill_owner&&allocs==frees);
  else{
   CHECK(memory.private_session&&matrix_fill_owner&&memory.private_matrix_constructing&&!matrix_fill_owner->active);
   twod_copy_enabled=true;struct remesa_twod_copy_request copy={.version=1,.context=77,.budget_ms=1000};
   CHECK(remesa_twod_copy_ioctl(&dev,&copy,&file)==-EBUSY&&starts==1);
  }
 }
 CHECK(!claims&&!matrix_executes&&!hashes&&fill_claims==(endpoint_mode==6?0U:1U));
 printf("PASS joined fill endpoint mode%u: backing/construction/error routing, source claims0 GPU0\n",endpoint_mode);
 return 0;
}
