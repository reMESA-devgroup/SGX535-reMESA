/* SPDX-License-Identifier: GPL-2.0-only */
/* Real retained GEM service with modeled MMU allocation callbacks. No GPU. */
#define main prior_twod_test_main
#include "test_gem_twod.c"
#undef main
#include "../remesa_gem_private.h"
#include "../remesa_twod_reservation.h"
struct psb_mmu_driver {unsigned identity;};
struct remesa_mmu_private {unsigned count,blocked;struct remesa_mmu_allowed_binding bindings[16];};
static struct psb_mmu_driver driver;
static struct remesa_gem_memory *expected_memory;
static unsigned prepares,releases,mode;
static struct remesa_twod_reservation *expected_guard;
int remesa_twod_mmu_guard_acquire(struct drm_psb_private *p,
 struct remesa_twod_reservation *r,const void *owner)
{
 CHECK(expected_memory->lock.held&&p==expected_memory->dev->dev_private&&r==expected_guard);
 if(mode==5)return -EBUSY;
 CHECK(r->held&&r->device==p&&!r->mmu_owner);r->mmu_owner=owner;return 0;
}
int remesa_twod_mmu_guard_release(struct drm_psb_private *p,
 struct remesa_twod_reservation *r,const void *owner)
{
 CHECK(expected_memory->lock.held&&p==expected_memory->dev->dev_private&&r==expected_guard);
 CHECK(r->held&&r->mmu_owner==owner);
 if(mode==6)return -EIO;
 r->mmu_owner=NULL;return 0;
}

int remesa_mmu_private_prepare(struct psb_mmu_driver *d,
 const struct remesa_mmu_allowed_binding *b,u32 n,struct remesa_mmu_private **out)
{
 CHECK(d==&driver&&n==3&&!*out&&expected_memory->lock.held);prepares++;
 struct retained_job *j=&expected_memory->jobs[0];
 CHECK(j->attached&&j->state==REMESA_JOB_PREPARED&&expected_memory->executor_context);
 CHECK(expected_guard->mmu_owner==expected_memory->executor_context);
 for(unsigned i=0;i<n;i++){
  CHECK(b[i].address==(u64)0x40000000+i*0x01000000);
  CHECK(b[i].page_count&&b[i].pages&&b[i].pages[0]);
  if(i)CHECK(b[i-1].address+(u64)b[i-1].page_count*4096<=b[i].address);
  unsigned found=0;
  for(unsigned k=0;k<j->count;k++){
   struct backing *bo=&expected_memory->bo[j->ids[k]&31];
   if(bo->va.start==b[i].address){
    CHECK(bo->pages==b[i].pages&&bo->holds==1&&bo->inserted==b[i].page_count);
    CHECK(b[i].access==k+1&&b[i].cached==(k!=0));found++;
   }
  }
  CHECK(found==1);
 }
 if(mode==1)return -ENOMEM;
 if(mode==3)return 0; /* impossible success must never become admission */
 *out=calloc(1,sizeof(**out));CHECK(*out);(*out)->count=n;
 memcpy((*out)->bindings,b,n*sizeof(*b));
 if(mode==2){(*out)->blocked=1;return -EUCLEAN;}
 return 0;
}
int remesa_mmu_private_release(struct remesa_mmu_private **out)
{
 CHECK(out&&expected_memory->lock.held&&expected_guard->mmu_owner==expected_memory->executor_context);releases++;
 if(*out&&(*out)->blocked)return -EBUSY;
 free(*out);*out=NULL;return 0;
}
static unsigned bound_checks;
int remesa_mmu_private_validate_bound_locked(struct remesa_mmu_private *owner,
 const struct remesa_mmu_allowed_binding *b,u32 count)
{
 CHECK(expected_memory->lock.held&&owner&&count==owner->count);bound_checks++;
 for(unsigned i=0;i<count;i++){
  CHECK(b[i].address==owner->bindings[i].address);
  CHECK(b[i].pages==owner->bindings[i].pages&&b[i].page_count==owner->bindings[i].page_count);
  CHECK(b[i].access==owner->bindings[i].access&&b[i].cached==owner->bindings[i].cached);
 }
 return 0;
}
#include "../remesa_gem_private.inc"
static void one_case(unsigned width,unsigned height,unsigned fault)
{
 struct test_mmu mmu={.has_clflush=true,.pd={.driver=&driver}};
 struct drm_psb_private priv={&mmu,(void*)1,{0x28000000,32,0x80000000}};
 struct drm_device dev={&priv};struct remesa_gem_memory *s=NULL;
 struct remesa_twod_reservation guard={.device=&priv,.held=true};
 struct remesa_memory_binding uses[3];struct remesa_gem_private_rule rules[3];
 struct remesa_memory_ticket ticket;struct remesa_gem_private loan={0},zero={0};
 uint32_t command=0;mode=fault;unsigned start_prepares=prepares;
 CHECK(!remesa_gem_memory_open(&dev,&file,77,&s));expected_memory=s;expected_guard=&guard;
 const u64 addresses[3]={0x42000000,0x40000000,0x41000000};
 for(unsigned i=0;i<3;i++){
  u32 id=create(s,width,height,addresses[i]);struct backing *b=&s->bo[id&31];
  uses[i]=(struct remesa_memory_binding){id,b->va.start,b->gem.size};
  rules[i]=(struct remesa_gem_private_rule){id,i+1,i!=0};
 }
 CHECK(!remesa_gem_job_prepare(s,uses,3,&command,sizeof(command),&ticket));
 struct retained_job *j=ticket_job(s,ticket);CHECK(j&&j->count==3);
 for(unsigned test=0;test<8;test++){
  struct remesa_gem_private_rule bad[3];memcpy(bad,rules,sizeof(bad));
  struct remesa_memory_ticket t=ticket;unsigned n=3;int expected=0;
  switch(test){
   case 0:t.generation++;expected=-ENOENT;break;
   case 1:bad[0].id=rules[1].id;expected=-EACCES;break;
   case 2:bad[1].access=0;expected=-EINVAL;break;
   case 3:bad[1].access=4;expected=-EINVAL;break;
   case 4:bad[2].cached=2;expected=-EINVAL;break;
   case 5:n=2;expected=-EACCES;break;
   case 6:s->stream_bound=true;expected=-EBUSY;break;
   case 7:s->bo[rules[0].id&31].initialized=false;expected=-ENODATA;break;
  }
  CHECK(remesa_gem_private_prepare(s,t,bad,n,&guard,&loan)==expected);
  CHECK(!memcmp(&loan,&zero,sizeof(loan))&&!j->attached&&!s->executor);
  CHECK(prepares==start_prepares);s->stream_bound=false;s->bo[rules[0].id&31].initialized=true;
 }
 unsigned oldmaps=vmap_calls,oldflush=flushes,oldw=test_wmb,oldr=test_rmb;
 if(fault==4)access_bad=1;
 int error=remesa_gem_private_prepare(s,ticket,rules,3,&guard,&loan);
 if(fault==4){
  CHECK(error==-EACCES&&!loan.live&&!j->attached&&j->quarantined&&j->state==REMESA_JOB_QUARANTINED);
  CHECK(prepares==start_prepares&&remesa_gem_memory_close(s)==-EBUSY);return;
 }
 if(fault==5){
  CHECK(error==-EBUSY&&!loan.live&&!j->attached&&!s->executor&&!j->quarantined&&!guard.mmu_owner);
  CHECK(prepares==start_prepares);goto cleanup;
 }
 CHECK(error==(fault==1?-ENOMEM:fault==2?-EUCLEAN:fault==3?-EPROTO:0));
 CHECK(vmap_calls==oldmaps&&flushes==oldflush&&test_wmb==oldw&&test_rmb==oldr);
 CHECK(prepares==start_prepares+1);
 if(fault==6){
  CHECK(loan.live&&j->attached&&guard.mmu_owner==&loan);
  CHECK(remesa_gem_private_release(&loan)==-EIO&&loan.live&&!loan.owner&&loan.guard==&guard);
  CHECK(guard.mmu_owner==&loan&&j->attached&&remesa_gem_memory_close(s)==-EBUSY);
  return;
 }
 if(fault==1){
  CHECK(!loan.live&&!j->attached&&!s->executor&&!guard.mmu_owner&&j->state==REMESA_JOB_PREPARED);
 }else{
  CHECK(loan.live&&loan.memory==s&&j->attached&&s->executor_context==&loan);
  CHECK(remesa_gem_job_cancel(s,ticket)==-EBUSY);
  CHECK(remesa_gem_memory_close(s)==-EBUSY);
  if(!fault){
   struct remesa_memory_view views[3];
   for(unsigned i=0;i<3;i++){
    struct backing *bo=&s->bo[j->ids[i]&31];
    views[i]=(struct remesa_memory_view){uses[i],bo->pages,bo->inserted,NULL};
   }
   unsigned old_bound=bound_checks;
   rules[0].access=3; /* original caller metadata no longer authoritative */
   mutex_lock(&s->lock);
   CHECK(!remesa_gem_private_validate_locked(&loan,views,3));
   CHECK(bound_checks==old_bound+1);
   for(unsigned negative=0;negative<5;negative++){
    struct remesa_memory_view bad[3];memcpy(bad,views,sizeof bad);
    if(negative==0)bad[0].binding.id++;
    if(negative==1)bad[0].binding.va+=4096;
    if(negative==2)bad[0].page_count++;
    if(negative==3)bad[0].pages=views[1].pages;
    if(negative==4)bad[0].cpu=(void*)1;
    CHECK(remesa_gem_private_validate_locked(&loan,bad,3)==-EACCES);
    CHECK(bound_checks==old_bound+1&&loan.live&&j->attached);
   }
   CHECK(remesa_gem_private_validate_locked(&loan,views,2)==-EACCES);
   CHECK(vmap_calls==oldmaps&&flushes==oldflush&&test_wmb==oldw&&test_rmb==oldr);
   mutex_unlock(&s->lock);
   CHECK(remesa_gem_job_issue(s,ticket)==-EOPNOTSUPP);
   CHECK(j->state==REMESA_JOB_PREPARED&&!j->cookie&&!j->quarantined);
   CHECK(remesa_gem_private_prepare(s,ticket,rules,3,&guard,&loan)==-EBUSY);
   loan.owner->blocked=1;
   CHECK(remesa_gem_private_release(&loan)==-EBUSY&&loan.live&&j->attached);
   for(unsigned i=0;i<3;i++)CHECK(s->bo[j->ids[i]&31].holds==1);
   loan.owner->blocked=0;
   CHECK(!remesa_gem_private_release(&loan)&&!memcmp(&loan,&zero,sizeof loan));
   CHECK(!j->attached&&j->state==REMESA_JOB_PREPARED&&!s->executor&&!guard.mmu_owner);
  }else{
   CHECK(j->quarantined&&j->state==REMESA_JOB_QUARANTINED);
   for(unsigned i=0;i<3;i++)CHECK(s->bo[j->ids[i]&31].holds==1&&s->bo[j->ids[i]&31].quarantine);
   if(fault==2)CHECK(remesa_gem_private_release(&loan)==-EBUSY&&loan.live);
   /* Child process exits with retained fault state; no fake retirement. */
   return;
  }
 }
 cleanup:CHECK(!remesa_gem_job_cancel(s,ticket));
 for(unsigned i=0;i<3;i++)CHECK(!s->bo[uses[i].id&31].holds);
 CHECK(!remesa_gem_memory_close(s));
 CHECK(!gems&&!pins&&!ptes&&!claimed&&!power);
 expected_memory=NULL;expected_guard=NULL;
}
int main(void)
{
 one_case(1280,800,0);one_case(640,640,0);one_case(641,479,0);one_case(32,32,1);
 for(unsigned fault=2;fault<=6;fault++){
  pid_t child=fork();CHECK(child>=0);
  if(!child){one_case(32,32,fault);_exit(0);}
  int status;CHECK(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
 }
 CHECK(!gems&&!pins&&!ptes&&!claimed&&!power);
 printf("PASS %u GEM/private-loan integration checks; MMU callbacks modeled; no GPU\n",checks);
 return 0;
}
