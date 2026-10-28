/* SPDX-License-Identifier: GPL-2.0-only */
#define REMESA_MEMORY_TEST
#include "remesa_gem_memory.c"
#include "remesa_twod.c"
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
static void *kvcalloc(size_t n,size_t size,int flags)
{if(n&&size>SIZE_MAX/n)return NULL;return kzalloc(n*size,flags);}
static void kvfree(void *p){free(p);}
static void sort(void *p,size_t n,size_t size,int(*cmp)(const void*,const void*),void *swap)
{assert(!swap);qsort(p,n,size,cmp);}
static int access_bad;
/* The exact raw-PTE implementation is exercised separately by the MMU model. */
int psb_mmu_verify_cached_rw_pages(struct psb_mmu_pd *pd,u64 va,struct page **pages,u32 n)
{return access_bad?-EACCES:psb_mmu_verify_pages(pd,va,pages,n);}
#include "../remesa_gem_twod.inc"
static unsigned checks;
#ifdef REMESA_TWOD_VENDOR_STREAM
static int vendor_test;
#endif
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"failure line%d: %s\n",__LINE__,#x);abort();}}while(0)
static struct drm_file file={1};
static int validate(struct remesa_gem_memory *s,struct remesa_memory_ticket t,struct remesa_twod_copy_plan *p)
{int e;mutex_lock(&s->lock);e=remesa_gem_twod_validate_locked(s,t,p);mutex_unlock(&s->lock);return e;}
static u32 create(struct remesa_gem_memory *s,unsigned width,unsigned height,u64 va)
{
 struct remesa_memory_request r={.version=1,.action=REMESA_MEM_CREATE,.context=77,.bytes=(((u64)width*height*4+4095)/4096)*4096};
 CHECK(!remesa_gem_memory_request(s,&file,&r));u32 id=r.id;
 r=(struct remesa_memory_request){.version=1,.action=REMESA_MEM_PIN,.context=77,.id=id};CHECK(!remesa_gem_memory_request(s,&file,&r));
 r.action=REMESA_MEM_MAP;r.va=va;CHECK(!remesa_gem_memory_request(s,&file,&r));return id;
}
static void negative(struct remesa_gem_memory *s,struct remesa_memory_ticket t,int error)
{
 struct remesa_twod_copy_plan p,old;enum remesa_job_state state;
 memset(&p,0xa5,sizeof p);old=p;unsigned f=flushes,w=test_wmb,r=test_rmb;
 CHECK(validate(s,t,&p)==error);CHECK(!memcmp(&p,&old,sizeof p));
 CHECK(flushes==f&&test_wmb==w&&test_rmb==r);
 if(error==-ENOENT)CHECK(remesa_gem_job_state(s,t,&state)==-ENOENT);
 else CHECK(!remesa_gem_job_state(s,t,&state)&&state==REMESA_JOB_PREPARED);
}
static void one(unsigned width,unsigned height,int same)
{
 struct test_mmu mmu={true,{0}};struct drm_psb_private priv={&mmu,(void*)1,{0x28000000,32,0x80000000}};struct drm_device dev={&priv};
 struct remesa_gem_memory *s=NULL;struct remesa_memory_ticket ticket;
 struct remesa_gem_twod_command command={.schema=REMESA_GEM_TWOD_COPY_SCHEMA};
 struct remesa_memory_binding uses[2];struct remesa_twod_copy_plan plan,expected;
 CHECK(!remesa_gem_memory_open(&dev,&file,77,&s));
 u32 src=create(s,width,height,0x40001000),dst=same?src:create(s,width,height,0x41000000);
 struct backing *a=&s->bo[src&31],*b=&s->bo[dst&31];
 uses[0]=(struct remesa_memory_binding){src,a->va.start,a->gem.size};uses[1]=(struct remesa_memory_binding){dst,b->va.start,b->gem.size};
 command.copy=(struct remesa_twod_copy){.source={src,a->va.start,a->gem.size,width,height,width*4,1},.destination={dst,b->va.start,b->gem.size,width,height,width*4,1},.request_base=0x40000000,.width=width,.height=height};
 if(same){command.copy.width=17;command.copy.height=19;command.copy.destination_x=1;command.copy.destination_y=1;}
#ifdef REMESA_TWOD_VENDOR_STREAM
 if(vendor_test){command.schema=REMESA_GEM_TWOD_VENDOR_SCHEMA;CHECK(!remesa_twod_copy_prepare_vendor(&command.copy,&expected));}
 else
#endif
 CHECK(!remesa_twod_copy_prepare(&command.copy,&expected));
 CHECK(!remesa_gem_job_prepare(s,uses,same?1:2,&command,sizeof command,&ticket));
 /* Prove the kernel uses its immutable copied command, not caller metadata. */
 command.schema=0;command.copy.source.allocation_id=0;
 unsigned oldflush=flushes,oldw=test_wmb,oldr=test_rmb;
 CHECK(!validate(s,ticket,&plan));CHECK(!memcmp(plan.command,expected.command,40));
 CHECK(plan.dwords==expected.dwords);
 CHECK(plan.source_id==src&&plan.destination_id==dst);
 CHECK(flushes==oldflush&&test_wmb==oldw&&test_rmb==oldr);
 CHECK(remesa_gem_job_issue(s,ticket)==-EOPNOTSUPP); /* no new executor installed */
 if(width==32&&!same){
  struct retained_job *j=ticket_job(s,ticket);struct remesa_gem_twod_command *owned=j->commands,saved=*owned;
  owned->schema=0;negative(s,ticket,-EINVAL);*owned=saved;
  owned->reserved=1;negative(s,ticket,-EINVAL);*owned=saved;
  owned->copy.source.allocation_id|=1ULL<<40;negative(s,ticket,-EACCES);*owned=saved;
  owned->copy.destination.gpu_address+=4096;negative(s,ticket,-EACCES);*owned=saved;
  owned->copy.source.bytes-=4096;negative(s,ticket,-EACCES);*owned=saved;
  owned->copy.source_x=32;negative(s,ticket,-EINVAL);*owned=saved;
  owned->copy.width=8;negative(s,ticket,-EOPNOTSUPP);*owned=saved;
  owned->copy.reserved=1;negative(s,ticket,-EINVAL);*owned=saved;
  fail_alloc=1;negative(s,ticket,-ENOMEM);fail_alloc=0;
  s->stream_bound=true;negative(s,ticket,-EBUSY);s->stream_bound=false;
  s->ta_cache_domains=true;negative(s,ticket,-EBUSY);s->ta_cache_domains=false;
  j->command_bytes--;negative(s,ticket,-EINVAL);j->command_bytes++;
  a->initialized=false;negative(s,ticket,-ENODATA);a->initialized=true;
  struct remesa_memory_ticket wrong=ticket;wrong.generation++;negative(s,wrong,-ENOENT);
  /* Faults stay in child CPU models; no device backing is discarded as if
   * it had retired. Parent joins each process and retains its valid fixture. */
  for(unsigned mode=0;mode<3;mode++){
   pid_t child=fork();CHECK(child>=0);
   if(!child){
    unsigned at;for(at=0;at<4096;at++)if(test_pt[at].live&&test_pt[at].va==a->va.start)break;
    CHECK(at<4096);
    if(!mode){
     a->pages[0]->pfn=b->pages[0]->pfn;test_pt[at].pfn=a->pages[0]->pfn;
     negative(s,ticket,-EACCES);CHECK(a->holds==1&&b->holds==1);
    }else{
     struct remesa_twod_copy_plan unchanged,previous;enum remesa_job_state state;
     memset(&unchanged,0xa5,sizeof unchanged);previous=unchanged;
     if(mode==1)test_pt[at].pfn^=1;else access_bad=1;
     CHECK(validate(s,ticket,&unchanged)==-EIO);CHECK(!memcmp(&unchanged,&previous,sizeof unchanged));
     CHECK(!remesa_gem_job_state(s,ticket,&state)&&state==REMESA_JOB_QUARANTINED);
     CHECK(a->holds==1&&b->holds==1);CHECK(remesa_gem_job_cancel(s,ticket)==-EBUSY);
     CHECK(remesa_gem_memory_close(s)==-EBUSY);
    }
    _exit(0);
   }
   int status;CHECK(waitpid(child,&status,0)==child);CHECK(WIFEXITED(status)&&!WEXITSTATUS(status));
  }
 }
 CHECK(!remesa_gem_job_cancel(s,ticket));
 CHECK(!remesa_gem_memory_close(s));
 CHECK(!gems&&!pins&&!ptes&&!claimed&&!power&&!device_refs&&!module_refs);
}
int main(void)
{
 one(1280,800,0);one(640,640,0);one(641,479,0);one(32,32,0);one(32,32,1);
#ifdef REMESA_TWOD_VENDOR_STREAM
 vendor_test=1;
 one(1280,800,0);one(640,640,0);one(641,479,0);one(32,32,0);one(32,32,1);
#endif
 printf("PASS %u GEM-backed 2D preflight checks; no issue/cache/device operations\n",checks);return 0;
}
