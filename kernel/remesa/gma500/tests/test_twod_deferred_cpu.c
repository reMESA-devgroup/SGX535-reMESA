/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual memory-service CPU-boundary tests; no physical GPU/cache oracle. */
#ifndef REMESA_TWOD_DEFER_CPU_INVALIDATE
#error "This fixture requires the separately staged deferred CPU candidate"
#endif
int twod_lifecycle_regression(void);
#define REMESA_TWOD_JOB_TEST_MAIN twod_lifecycle_regression
#include "test_gem_twod_job.c"
#undef REMESA_TWOD_JOB_TEST_MAIN

static struct backing *pending_backing;
static bool pending_dma;
static unsigned char gpu_value;
static void invalidate_model(struct page **pages,unsigned n)
{
 if(pending_dma&&pages==pending_backing->pages){
  CHECK(n==pending_backing->gem.size/PAGE_SIZE);
  memset(pages[0]->bytes,gpu_value,pending_backing->gem.size);
  pending_dma=false;
 }
}
static struct remesa_memory_request cpu_request(unsigned action,u32 id)
{return (struct remesa_memory_request){.version=1,.action=action,.context=77,.id=id};}
static void boundary(unsigned mode)
{
 struct test_mmu mmu={true,{0}};
 struct drm_psb_private priv={&mmu,(void*)1,{0x28000000,32,0x80000000}};
 struct drm_device dev={&priv};struct remesa_gem_memory *s=NULL;
 CHECK(!remesa_gem_memory_open(&dev,&file,77,&s));
 u32 src=create(s,32,32,0x40001000),dst=create(s,32,32,0x41000000);
 struct backing *a=&s->bo[src&31],*b=&s->bo[dst&31];
 const struct remesa_memory_binding uses[2]={{src,a->va.start,a->gem.size},{dst,b->va.start,b->gem.size}};
 struct remesa_gem_twod_command cmd={.schema=REMESA_GEM_TWOD_COPY_SCHEMA};
 cmd.copy=(struct remesa_twod_copy){.source={src,a->va.start,a->gem.size,32,32,128,1},
  .destination={dst,b->va.start,b->gem.size,32,32,128,1},.request_base=0x40000000,.width=32,.height=32};
 struct remesa_twod_job run={0};struct remesa_memory_ticket ticket;
 struct native_model model={.memory=s};unsigned before=flushes;
 unsigned post=0;twod_defer_cpu_invalidation=true;
 pending_backing=b;test_flush_hook=invalidate_model;
 for(unsigned i=0;i<16;i++){
  CHECK(!remesa_gem_job_prepare(s,uses,2,&cmd,sizeof(cmd),&ticket));
  model.stage=0;model.initial_flush=flushes;
  CHECK(!remesa_gem_twod_issue(s,ticket,&model_ops,&model,&run));
  CHECK(!remesa_gem_twod_wait(&run,10));
  gpu_value=(unsigned char)(0x40+i);pending_dma=true;
  if(mode==8)twod_defer_cpu_invalidation=!(i&1);
  if(mode==9)twod_defer_cpu_invalidation=(i&1)!=0;
  if(!twod_defer_cpu_invalidation)post++;
  CHECK(!remesa_gem_twod_publish(&run));
  CHECK(b->twod_cpu_invalidate_pending==twod_defer_cpu_invalidation&&
        !a->twod_cpu_invalidate_pending&&pending_dma==twod_defer_cpu_invalidation);
  struct remesa_memory_request denied=cpu_request(REMESA_MEM_LEASE,dst);
  unsigned at=flushes;
  CHECK(remesa_gem_memory_request(s,&file,&denied)==-EBUSY&&flushes==at&&
        pending_dma==twod_defer_cpu_invalidation);
  CHECK(!remesa_gem_twod_retire(&run));
 }
 CHECK(flushes==before+32+post&&b->twod_cpu_invalidate_pending==pending_dma);
 unsigned expected_cpu_flush=pending_dma?1:0;
 if(mode==7)twod_defer_cpu_invalidation=false; /* pending state survives disable */
 before=flushes;struct remesa_memory_request r=cpu_request(REMESA_MEM_LEASE,dst);
 if(mode==2){r.action=REMESA_MEM_SYNC;CHECK(!remesa_gem_memory_request(s,&file,&r));}
 else if(mode==3){
  struct remesa_memory_view views[2];
  CHECK(!remesa_gem_job_prepare(s,uses,2,&cmd,sizeof(cmd),&ticket));
  CHECK(remesa_gem_job_views(s,ticket,views,2)==2);
  CHECK(((unsigned char*)views[1].cpu)[0]==gpu_value);
  CHECK(remesa_gem_job_views(s,ticket,views,2)==2&&flushes==before+1);
  /* No stale internal CPU alias may survive into the next 2D interval,
   * including conservative mode followed by a runtime policy change. */
  unsigned starts=model.starts;
  twod_defer_cpu_invalidation=false;
  CHECK(remesa_gem_twod_issue(s,ticket,&model_ops,&model,&run)==-EBUSY);
  CHECK(model.starts==starts&&!run.live&&flushes==before+1);
  CHECK(!remesa_gem_job_cancel(s,ticket));
 }else if(mode==4){
  r.action=REMESA_MEM_UNMAP;CHECK(!remesa_gem_memory_request(s,&file,&r));
  r.action=REMESA_MEM_UNPIN;CHECK(!remesa_gem_memory_request(s,&file,&r));
 }else if(mode==6){
  CHECK(!remesa_gem_memory_close(s));s=NULL;
 }else{
  r.flags=mode==1;
  if(mode==5){fail_vmap=1;CHECK(remesa_gem_memory_request(s,&file,&r)==-ENOMEM);fail_vmap=0;}
  CHECK(!remesa_gem_memory_request(s,&file,&r));
  r.action=REMESA_MEM_READ;r.flags=0;r.length=4;
  CHECK(!remesa_gem_memory_request(s,&file,&r));
  for(unsigned i=0;i<4;i++)CHECK(r.data[i]==gpu_value);
  if(mode==1){r.action=REMESA_MEM_WRITE;r.offset=4;memset(r.data,0x91,4);
   CHECK(!remesa_gem_memory_request(s,&file,&r));
   CHECK(b->pages[0]->bytes[0]==gpu_value&&b->pages[0]->bytes[4]==0x91);}
  r=cpu_request(REMESA_MEM_UNLEASE,dst);CHECK(!remesa_gem_memory_request(s,&file,&r));
  r.action=REMESA_MEM_LEASE;CHECK(!remesa_gem_memory_request(s,&file,&r));
  r.action=REMESA_MEM_UNLEASE;CHECK(!remesa_gem_memory_request(s,&file,&r));
 }
 CHECK(flushes==before+expected_cpu_flush&&!pending_dma);
 if(s){CHECK(!b->twod_cpu_invalidate_pending);CHECK(!remesa_gem_memory_close(s));}
 test_flush_hook=NULL;pending_backing=NULL;
 CHECK(!gems&&!pins&&!ptes&&!claimed&&!power&&!device_refs&&!module_refs);
}
int main(void)
{
 twod_defer_cpu_invalidation=true;
 CHECK(!twod_lifecycle_regression());
 for(unsigned i=0;i<10;i++)boundary(i);
 printf("PASS deferred CPU invalidation:160 retired modeled operations, mandatory prefire flush, held-job refusal, read/write leases, SYNC, cached internal views, unpin/close, failed-vmap and runtime toggle boundaries; GPU0 (%uchecks)\n",checks);
 return 0;
}
