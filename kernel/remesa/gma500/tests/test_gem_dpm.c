/* SPDX-License-Identifier: GPL-2.0-only. Kernel zero provenance/aliases, GPU0. */
#define REMESA_GEM_PRIVATE_TEST_MAIN prior_dpm_private_fixture
#include "test_gem_private.c"
static u32 pool(struct remesa_gem_memory *s,u64 bytes,u64 va)
{
 struct remesa_memory_request r={.version=1,.action=REMESA_MEM_CREATE,.context=77,.bytes=bytes};
 CHECK(!remesa_gem_memory_request(s,&file,&r));u32 id=r.id;
 r=(struct remesa_memory_request){.version=1,.action=REMESA_MEM_PIN,.context=77,.id=id};
 CHECK(!remesa_gem_memory_request(s,&file,&r));r.action=REMESA_MEM_MAP;r.va=va;
 CHECK(!remesa_gem_memory_request(s,&file,&r));return id;
}
static void dpm_case(unsigned fault)
{
 struct test_mmu mmu={.has_clflush=true,.pd={.driver=&driver}};
 struct drm_psb_private priv={&mmu,(void*)1,{0x28000000,32,0x80000000}};
 struct drm_device dev={&priv};struct remesa_gem_memory *s=NULL;
 struct remesa_twod_reservation guard={.device=&priv,.held=true};
 struct remesa_memory_ticket ticket;struct remesa_memory_binding uses[3];
 struct remesa_memory_view views[3],saved_views[3];struct remesa_memory_snapshot snapshot,saved_snapshot;
 struct remesa_gem_private loan={0};u32 command=0;
 CHECK(!remesa_gem_memory_open(&dev,&file,77,&s));expected_memory=s;expected_guard=&guard;
 unsigned zeroed_before=test_zeroed_pages;
 u32 ids[3]={pool(s,4096,0x20080000),pool(s,fault==6?4096:33554432,0x40001000),pool(s,33554432,fault==3?0x30101000:0x30100000)};
 for(unsigned i=0;i<3;i++){
  struct backing *b=&s->bo[ids[i]&31];CHECK(b->initialized&&b->zero_pristine);
  uses[i]=(struct remesa_memory_binding){ids[i],b->va.start,b->gem.size};
 }
 struct backing *table=&s->bo[ids[1]&31],*parameter=&s->bo[ids[2]&31];
 CHECK(test_zeroed_pages==zeroed_before+1+(fault==6?1:8192)+8192);
 unsigned zeroed=test_zeroed_pages;
 if(fault==1||fault==5){
  struct remesa_memory_request r={.version=1,.action=REMESA_MEM_LEASE,.context=77,.id=ids[1],.flags=fault==1};
  CHECK(!remesa_gem_memory_request(s,&file,&r));
  r.action=REMESA_MEM_UNLEASE;r.flags=0;CHECK(!remesa_gem_memory_request(s,&file,&r));
  CHECK(table->zero_pristine==(fault==5));
 }
 if(fault==4){
  struct remesa_memory_request r={.version=1,.action=REMESA_MEM_UNMAP,.context=77,.id=ids[1]};
  CHECK(!remesa_gem_memory_request(s,&file,&r));r.action=REMESA_MEM_UNPIN;CHECK(!remesa_gem_memory_request(s,&file,&r));
  r.action=REMESA_MEM_PIN;CHECK(!remesa_gem_memory_request(s,&file,&r));r.action=REMESA_MEM_MAP;r.va=0x40001000;
  CHECK(!remesa_gem_memory_request(s,&file,&r)&&!table->zero_pristine&&test_zeroed_pages==zeroed);
 }
 CHECK(!remesa_gem_job_prepare(s,uses,3,&command,4,&ticket));struct retained_job *j=ticket_job(s,ticket);
 if(fault==2)CHECK(remesa_gem_job_views(s,ticket,views,3)==3&&!table->zero_pristine&&!parameter->zero_pristine);
 unsigned maps_before=vmap_calls,flush_before=flushes,wmb_before=test_wmb;
 int error=remesa_gem_job_claim_zero_dpm(s,ticket,ids[1],ids[2]);
 int expected=(fault==1||fault==2||fault==4)?-ENODATA:fault==3?-ERANGE:fault==6?-EACCES:0;
 CHECK(error==expected&&vmap_calls==maps_before&&flushes==flush_before&&test_wmb==wmb_before&&test_zeroed_pages==zeroed);
 if(error){CHECK(!j->dpm_mask);goto cleanup;}
 CHECK(j->dpm_mask==6&&table->zero_pristine&&parameter->zero_pristine);
 CHECK(remesa_gem_job_claim_zero_dpm(s,ticket,ids[1],ids[2])==-EALREADY);
 memset(views,0xa5,sizeof views);memcpy(saved_views,views,sizeof views);memset(&snapshot,0xa5,sizeof snapshot);saved_snapshot=snapshot;
 CHECK(remesa_gem_job_snapshot(s,ticket,&snapshot,views,3)==-EACCES&&!memcmp(views,saved_views,sizeof views)&&!memcmp(&snapshot,&saved_snapshot,sizeof snapshot));
 CHECK(remesa_gem_job_snapshot_selected(s,ticket,&snapshot,views,3,7)==-EACCES&&!memcmp(views,saved_views,sizeof views));
 CHECK(remesa_gem_job_snapshot_selected(s,ticket,&snapshot,views,3,8)==-EINVAL&&!memcmp(views,saved_views,sizeof views));
 fail_vmap_call=vmap_calls+1;
 CHECK(remesa_gem_job_snapshot_selected(s,ticket,&snapshot,views,3,1)==-ENOMEM&&!memcmp(views,saved_views,sizeof views)&&!memcmp(&snapshot,&saved_snapshot,sizeof snapshot));
 CHECK(table->zero_pristine&&parameter->zero_pristine&&!j->cpu_views[0]);fail_vmap_call=0;
 maps_before=vmap_calls;
 CHECK(remesa_gem_job_snapshot_selected(s,ticket,&snapshot,views,3,1)==3&&vmap_calls==maps_before+1);
 CHECK(views[0].cpu&&!views[1].cpu&&!views[2].cpu&&!table->cached_job_view&&!parameter->cached_job_view);
 CHECK(views[1].page_count==8192&&views[2].page_count==8192&&table->zero_pristine&&parameter->zero_pristine);
 CHECK(!s->bo[ids[0]&31].zero_pristine);
 CHECK(remesa_gem_job_check_snapshot(s,&snapshot,views,3)==-EACCES); /* Legacy all-view contract unchanged. */
 const struct remesa_gem_private_rule rules[3]={{ids[0],1,0},{ids[1],3,1},{ids[2],3,1}};
 dpm_model=true;mode=0;CHECK(!remesa_gem_private_prepare(s,ticket,rules,3,&guard,&loan));
 mutex_lock(&s->lock);CHECK(!remesa_gem_private_validate_locked(&loan,views,3));
 struct remesa_memory_view bad[3];memcpy(bad,views,sizeof bad);bad[2].page_count--;
 CHECK(remesa_gem_private_publish_locked(&loan,bad,3)==-EACCES&&table->zero_pristine&&parameter->zero_pristine&&flushes==flush_before);
 CHECK(!remesa_gem_private_publish_locked(&loan,views,3)&&!table->zero_pristine&&!parameter->zero_pristine);
 CHECK(flushes==flush_before+3&&test_wmb==wmb_before+1&&test_zeroed_pages==zeroed);
 mutex_unlock(&s->lock);CHECK(remesa_gem_job_issue(s,ticket)==-EOPNOTSUPP);
 CHECK(!remesa_gem_private_release(&loan));dpm_model=false;
 cleanup:
 CHECK(!remesa_gem_job_cancel(s,ticket));
 CHECK(!remesa_gem_job_prepare(s,uses,3,&command,4,&ticket)&&!ticket_job(s,ticket)->dpm_mask);
 CHECK(!remesa_gem_job_cancel(s,ticket));CHECK(!remesa_gem_memory_close(s));
 CHECK(!gems&&!pins&&!ptes&&!claimed&&!power);expected_memory=NULL;expected_guard=NULL;
}
static int model_validate(void *p,const void *c,unsigned bytes,const struct remesa_memory_binding *b,unsigned n)
{(void)p;(void)c;(void)bytes;(void)b;CHECK(n==3);return 0;}
static int model_start(void *p,struct remesa_memory_ticket t,const void *c,unsigned bytes,u64 *cookie)
{
 struct remesa_gem_memory *s=p;(void)c;(void)bytes;struct retained_job *j=ticket_job(s,t);
 for(unsigned i=0;i<j->count;i++)CHECK(!s->bo[j->ids[i]&31].zero_pristine);
 *cookie=1;return -EIO; /* Possible issue: no certainty/retry/free inferred. */
}
static int model_poll(void *p,struct remesa_memory_ticket t,u64 c,unsigned b,struct remesa_job_completion *out)
{(void)p;(void)t;(void)c;(void)b;(void)out;return -EOPNOTSUPP;}
static int model_finish(void *p,struct remesa_memory_ticket t,u64 c)
{(void)p;(void)t;(void)c;return -EOPNOTSUPP;}
static void issue_consumes_zero(void)
{
 struct test_mmu mmu={.has_clflush=true,.pd={.driver=&driver}};
 struct drm_psb_private priv={&mmu,(void*)1,{0x28000000,32,0x80000000}};
 struct drm_device dev={&priv};struct remesa_gem_memory *s=NULL;struct remesa_memory_binding uses[3];
 struct remesa_memory_ticket ticket;u32 command=0;
 const struct remesa_job_executor executor={model_validate,model_start,model_poll,model_finish,model_finish};
 CHECK(!remesa_gem_memory_open(&dev,&file,77,&s));
 for(unsigned i=0;i<3;i++){u32 id=pool(s,4096,0x40000000+i*4096);struct backing *b=&s->bo[id&31];CHECK(b->zero_pristine);uses[i]=(struct remesa_memory_binding){id,b->va.start,b->gem.size};}
 CHECK(!remesa_gem_job_prepare(s,uses,3,&command,4,&ticket));s->executor=&executor;s->executor_context=s;
 CHECK(remesa_gem_job_issue(s,ticket)==-EIO&&remesa_gem_memory_close(s)==-EBUSY);
 for(unsigned i=0;i<3;i++)CHECK(!s->bo[uses[i].id&31].zero_pristine&&s->bo[uses[i].id&31].holds==1&&s->bo[uses[i].id&31].pinned);
}
int main(void)
{
 for(unsigned fault=0;fault<7;fault++)dpm_case(fault);
 pid_t child=fork();CHECK(child>=0);if(!child){issue_consumes_zero();_exit(0);}
 int status;CHECK(waitpid(child,&status,0)==child&&WIFEXITED(status)&&!WEXITSTATUS(status));
 puts("PASS cold DPM zero origin: real64MiB modeled backing, no extra clear/scan/map on claim, writable lease/view/repin invalidation, readonly lease, atomic shape refusal, selected views/rollback, full private publication and no native issue; GPU0");return 0;
}
