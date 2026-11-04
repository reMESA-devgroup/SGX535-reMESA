/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual GEM issue adapter, modeled upstream validated plan and primitives.
 * Existing retained/PTE tests separately cover that upstream validator. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
typedef uint32_t u32;typedef uint64_t u64;
#include "remesa_gem_twod_job.h"
struct drm_device {unsigned id;};struct drm_file {unsigned id;};
struct page {unsigned id;};struct psb_mmu_pd {unsigned id;};
struct mutex {bool held;};
struct backing {struct {u64 start;} va;struct {u64 size;} gem;struct page **pages;unsigned inserted;};
struct retained_job {unsigned count,ids[2],state;bool attached;u64 cookie;};
struct remesa_gem_memory {
 struct mutex lock;struct drm_device *dev;struct drm_file *file;struct psb_mmu_pd *pd;
 const void *private_session,*executor;void *executor_context;
 bool private_source_held,private_source_poisoned,private_device_lost,private_matrix_constructing;
 u32 private_matrix_color,private_matrix_depth;struct backing bo[32];
};
static unsigned mode,flushes,barriers,starts;
static struct retained_job job;
static struct remesa_twod_copy_plan plan;
static const unsigned twod_reserved_executor;
static void mutex_lock(struct mutex *m){assert(!m->held);m->held=true;}
static void mutex_unlock(struct mutex *m){assert(m->held);m->held=false;}
static struct retained_job *ticket_job(struct remesa_gem_memory *s,struct remesa_memory_ticket t)
{assert(s->lock.held&&t.operation==1);return &job;}
int remesa_gem_twod_validate_locked(struct remesa_gem_memory *s,struct remesa_memory_ticket t,struct remesa_twod_copy_plan *out)
{assert(s->lock.held&&t.context==77&&t.operation==1);*out=plan;return mode==11?-EACCES:0;}
static void drm_clflush_pages(struct page **pages,unsigned n){assert(pages&&n==1);flushes++;}
static void dma_wmb(void){barriers++;}
static int job_fault(struct remesa_gem_memory *s,struct retained_job *j,int error)
{assert(s->lock.held&&j==&job);j->state=REMESA_JOB_QUARANTINED;return error;}
#include "owned_gem_issue_actual.inc"
static unsigned token,alien;
static int start(void *context,const struct remesa_twod_launch *l,u64 *cookie)
{
 struct remesa_gem_memory *s=context;starts++;
 assert(s->lock.held&&l->dev==s->dev&&l->pd==s->pd&&l->retirement&&l->count==job.count);
 assert(l->source_owner==(mode==1?NULL:&token));
 assert(job.attached&&job.state==REMESA_JOB_ISSUED&&s->executor==&twod_reserved_executor);
 *cookie=mode==15?0:7;return mode==14?-EIO:0;
}
static int poll(void *c,const struct remesa_twod_launch *l,u64 q,unsigned n,struct remesa_twod_completion *o)
{(void)c;(void)l;(void)q;(void)n;(void)o;abort();}
static int finish(void *c,const struct remesa_twod_launch *l,u64 q){(void)c;(void)l;(void)q;abort();}
int main(int argc,char **argv)
{
 assert(argc==2);mode=strtoul(argv[1],NULL,10);assert(mode<17);
 struct drm_device dev={1};struct drm_file file={1};struct psb_mmu_pd pd={1};struct page page={1},*pages[]={&page};
 struct remesa_gem_memory s={.dev=&dev,.file=&file,.pd=&pd,.private_session=&token,.private_source_held=true,.private_matrix_color=33};
 s.bo[1]=(struct backing){.va={0x40001000},.gem={4096},.pages=pages,.inserted=1};s.bo[2]=s.bo[1];
 job=(struct retained_job){.count=1,.ids={33},.state=REMESA_JOB_PREPARED};
 plan=(struct remesa_twod_copy_plan){.destination_id=33,.dwords=7};
 struct remesa_twod_job run={0};struct remesa_twod_native_ops ops={start,poll,finish,finish};
 switch(mode){
 case 1:s.private_matrix_color=0;job.count=2;job.ids[1]=34;plan.dwords=10;break;
 case 2:s.private_session=NULL;break;case 3:s.private_source_held=false;break;
 case 4:s.private_source_poisoned=true;break;case 5:s.private_device_lost=true;break;
 case 6:s.file=NULL;break;case 7:s.private_matrix_constructing=true;break;
 case 8:job.count=2;job.ids[1]=34;break;case 9:plan.dwords=9;break;
 case 10:plan.destination_id++;break;case 12:run.live=true;break;
 case 13:ops.start=NULL;break;case 16:run.launch.source_owner=&alien;break;
 default:break;
 }
 struct remesa_twod_job before=run;
 int error=remesa_gem_twod_issue(&s,(struct remesa_memory_ticket){77,1,0,1},&ops,&s,&run);
 if(mode==0||mode==1||mode==16){
  assert(!error&&starts==1&&flushes==job.count&&barriers==1&&run.live&&run.cookie==7&&run.launch.retirement==&run.retirement);
 }else if(mode==14||mode==15){
  assert(error==(mode==14?-EIO:-EPROTO)&&starts==1&&run.live&&job.state==REMESA_JOB_QUARANTINED);
 }else{
  assert(error==(mode==12?-EBUSY:mode==13?-EINVAL:-EACCES));
  assert(!starts&&!flushes&&!barriers&&!s.executor&&!job.attached&&job.state==REMESA_JOB_PREPARED);
  assert(!memcmp(&run,&before,sizeof run));
 }
 puts("PASS actual owned GEM launch authority; GPU0");return 0;
}
