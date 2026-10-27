/* SPDX-License-Identifier: GPL-2.0-only */
/* CPU integration/fault model. No MMIO, GPU rendering or physical receipt. */
#define main preflight_test_main
#include "test_gem_twod.c"
#undef main
#include "../remesa_gem_twod_job.inc"
struct native_model {
 struct remesa_gem_memory *memory;
 unsigned stage,mode,initial_flush,starts;
};
static int model_start(void *opaque,const struct remesa_twod_launch *l,u64 *cookie)
{
 struct native_model *m=opaque;struct remesa_gem_memory *s=m->memory;
 struct retained_job *j=ticket_job(s,l->ticket);
 CHECK(s->lock.held&&j&&j->state==REMESA_JOB_ISSUED&&j->attached);
 CHECK(l->dev==s->dev&&l->pd==s->pd&&l->count==j->count);
 CHECK(l->count==1||l->count==2);
 CHECK(flushes==m->initial_flush+l->count);
 CHECK(!m->stage);m->stage=1;m->starts++;
 for(unsigned i=0;i<l->count;i++){
  struct backing *b=&s->bo[j->ids[i]&31];
  CHECK(l->views[i].binding.id==j->ids[i]);
  CHECK(l->views[i].binding.va==b->va.start&&l->views[i].binding.bytes==b->gem.size);
  CHECK(l->views[i].pages==b->pages&&l->views[i].page_count==b->inserted);
  CHECK(!l->views[i].cpu&&b->holds==1);
 }
 CHECK(l->plan.dwords==10&&l->plan.command[0]==0&&l->plan.command[1]==0&&l->plan.command[2]==0&&l->plan.command[3]==0x70);
 *cookie=m->mode==2?0:0x123456789ULL;
 return m->mode==1?-EIO:0;
}
static int model_poll(void *opaque,const struct remesa_twod_launch *l,u64 cookie,
 unsigned budget,struct remesa_twod_completion *out)
{
 struct native_model *m=opaque;
 CHECK(m->memory->lock.held&&m->stage==1&&cookie==0x123456789ULL&&budget==10);
 m->stage=2;
 *out=(struct remesa_twod_completion){l->ticket,cookie,REMESA_TWOD_EVENT_COMPLETE,
  REMESA_TWOD_FIFO_EMPTY,0,0};
 switch(m->mode){
 case 3:return -ETIMEDOUT;
 case 4:out->ticket.context++;break;
 case 5:out->ticket.operation++;break;
 case 6:out->ticket.slot++;break;
 case 7:out->ticket.generation++;break;
 case 8:out->cookie++;break;
 case 9:out->event=REMESA_JOB_LIFECYCLE_ALL;break;
 case 10:out->fifo_free--;break;
 case 11:out->blit_status=REMESA_TWOD_BLIT_BUSY;break;
 case 19:out->event|=1;break;
 case 20:out->event=0;break;
 case 21:out->status2=1;break;
 }
 return 0;
}
static int model_visible(void *opaque,const struct remesa_twod_launch *l,u64 cookie)
{
 struct native_model *m=opaque;
 CHECK(m->memory->lock.held&&m->stage==2&&cookie==0x123456789ULL);
 CHECK(ticket_job(m->memory,l->ticket)->state==REMESA_JOB_COMPLETE);
 m->stage=3;return m->mode==12?-EIO:0;
}
static int model_retire(void *opaque,const struct remesa_twod_launch *l,u64 cookie)
{
 struct native_model *m=opaque;struct retained_job *j=ticket_job(m->memory,l->ticket);
 CHECK(m->memory->lock.held&&m->stage==3&&cookie==0x123456789ULL);
 CHECK(j&&j->attached&&j->state==REMESA_JOB_VISIBLE);
 for(unsigned i=0;i<j->count;i++)CHECK(m->memory->bo[j->ids[i]&31].holds==1);
 m->stage=4;return m->mode==13?-EIO:0;
}
static const struct remesa_twod_native_ops model_ops={model_start,model_poll,model_visible,model_retire};
static void quarantined(struct remesa_twod_job *run)
{
 struct remesa_gem_memory *s=run->memory;struct retained_job *j=ticket_job(s,run->launch.ticket);
 CHECK(run->live&&j&&j->attached&&j->quarantined&&j->state==REMESA_JOB_QUARANTINED);
 CHECK(s->executor==&twod_reserved_executor&&s->executor_context==run);
 for(unsigned i=0;i<j->count;i++)CHECK(s->bo[j->ids[i]&31].holds==1&&s->bo[j->ids[i]&31].quarantine);
 CHECK(remesa_gem_job_cancel(s,run->launch.ticket)==-EBUSY);
 CHECK(remesa_gem_job_retire(s,run->launch.ticket)==-EBUSY);
 CHECK(remesa_gem_memory_close(s)==-EBUSY);
 CHECK(remesa_gem_twod_wait(run,10)==-EIO);
 CHECK(remesa_gem_twod_publish(run)==-EIO);
 CHECK(remesa_gem_twod_retire(run)==-EIO);
}
static void lifecycle_case(unsigned width,unsigned height,int same,unsigned mode)
{
 struct test_mmu mmu={true,{0}};struct drm_psb_private priv={&mmu,(void*)1,{0x28000000,32,0x80000000}};
 struct drm_device dev={&priv};struct remesa_gem_memory *s=NULL;
 struct remesa_twod_job run={0},zero={0};struct remesa_memory_ticket ticket;
 struct remesa_gem_twod_command command={.schema=REMESA_GEM_TWOD_COPY_SCHEMA};
 struct remesa_memory_binding uses[2];struct native_model model={0};
 CHECK(!remesa_gem_memory_open(&dev,&file,77,&s));model.memory=s;model.mode=mode;
 u32 src=create(s,width,height,0x40001000),dst=same?src:create(s,width,height,0x41000000);
 struct backing *a=&s->bo[src&31],*b=&s->bo[dst&31];
 uses[0]=(struct remesa_memory_binding){src,a->va.start,a->gem.size};
 uses[1]=(struct remesa_memory_binding){dst,b->va.start,b->gem.size};
 command.copy=(struct remesa_twod_copy){.source={src,a->va.start,a->gem.size,width,height,width*4,1},
  .destination={dst,b->va.start,b->gem.size,width,height,width*4,1},.request_base=0x40000000,.width=width,.height=height};
 if(same){command.copy.width=17;command.copy.height=19;command.copy.destination_x=1;command.copy.destination_y=1;}
 unsigned allocations=gems,maps=vmap_calls,initial_wmb=test_wmb;
 for(unsigned iteration=0;iteration<(mode?1u:3u);iteration++){
  CHECK(!remesa_gem_job_prepare(s,uses,same?1:2,&command,sizeof command,&ticket));
  model.stage=0;model.initial_flush=flushes;
  CHECK(remesa_gem_twod_wait(&run,10)==-EINVAL);
  CHECK(remesa_gem_twod_publish(&run)==-EINVAL);
  CHECK(remesa_gem_twod_retire(&run)==-EINVAL);
  struct remesa_twod_native_ops bad=model_ops;bad.poll=NULL;
  CHECK(remesa_gem_twod_issue(s,ticket,&bad,&model,&run)==-EINVAL);
  CHECK(!memcmp(&run,&zero,sizeof run)&&!model.stage);
  struct remesa_memory_ticket wrong=ticket;wrong.generation++;
  CHECK(remesa_gem_twod_issue(s,wrong,&model_ops,&model,&run)==-ENOENT);
  CHECK(!memcmp(&run,&zero,sizeof run)&&!model.stage);
  int e=remesa_gem_twod_issue(s,ticket,&model_ops,&model,&run);
  if(mode==1||mode==2){CHECK(e==(mode==1?-EIO:-EPROTO));quarantined(&run);return;}
  CHECK(!e&&run.live&&run.cookie==0x123456789ULL);
  CHECK(remesa_gem_twod_issue(s,ticket,&model_ops,&model,&run)==-EBUSY);
  CHECK(remesa_gem_job_cancel(s,ticket)==-EBUSY&&remesa_gem_memory_close(s)==-EBUSY);
  CHECK(remesa_gem_job_issue(s,ticket)==-EBUSY);
  CHECK(remesa_gem_twod_publish(&run)==-EBUSY&&remesa_gem_twod_retire(&run)==-EBUSY);
  CHECK(remesa_gem_twod_wait(&run,0)==-EINVAL&&remesa_gem_twod_wait(&run,10001)==-EINVAL);
  if(mode==14){CHECK(remesa_gem_job_wait(s,ticket,10)==-EOPNOTSUPP);quarantined(&run);return;}
  e=remesa_gem_twod_wait(&run,10);
  if((mode>=3&&mode<=11)||mode>=19){CHECK(e==(mode==3?-ETIMEDOUT:-EPROTO));quarantined(&run);return;}
  CHECK(!e);CHECK(remesa_gem_twod_wait(&run,10)==-EBUSY);
  CHECK(remesa_gem_twod_retire(&run)==-EBUSY);
  if(mode==15){CHECK(remesa_gem_job_publish(s,ticket)==-EOPNOTSUPP);quarantined(&run);return;}
  if(mode==17)access_bad=1;
  unsigned before=flushes;
  e=remesa_gem_twod_publish(&run);
  if(mode==12||mode==17){CHECK(e==-EIO&&flushes==before);quarantined(&run);return;}
  CHECK(!e&&flushes==before+1);CHECK(remesa_gem_twod_publish(&run)==-EBUSY);
  if(mode==16){CHECK(remesa_gem_job_acknowledge(s,ticket)==-EOPNOTSUPP);quarantined(&run);return;}
  if(mode==18)access_bad=1;
  e=remesa_gem_twod_retire(&run);
  if(mode==13||mode==18){CHECK(e==-EIO);quarantined(&run);return;}
  CHECK(!e&&model.stage==4&&!memcmp(&run,&zero,sizeof run));
  CHECK(!a->holds&&!b->holds&&!s->executor&&!s->executor_context);
  CHECK(!ticket_job(s,ticket));
  CHECK(gems==allocations&&vmap_calls==maps);
  CHECK(test_wmb==initial_wmb+iteration+1);
 }
 CHECK(!remesa_gem_memory_close(s));
 CHECK(!gems&&!pins&&!ptes&&!claimed&&!power&&!device_refs&&!module_refs);
}
int main(void)
{
 lifecycle_case(1280,800,0,0);lifecycle_case(640,640,0,0);
 lifecycle_case(641,479,0,0);lifecycle_case(32,32,1,0);
 for(unsigned mode=1;mode<=21;mode++){
  pid_t child=fork();CHECK(child>=0);
  if(!child){lifecycle_case(32,32,0,mode);_exit(0);}
  int status;CHECK(waitpid(child,&status,0)==child);
  CHECK(WIFEXITED(status)&&!WEXITSTATUS(status));
 }
 printf("PASS %u parent lifecycle checks plus 21 joined fault/cross-engine CPU models; no GPU operations\n",checks);
 return 0;
}
