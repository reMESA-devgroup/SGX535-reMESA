/* SPDX-License-Identifier: MIT */
#define REMESA_MEMORY_TEST
#include "remesa_gem_memory.c"
#include <stdio.h>
static unsigned flushed,ordered;
static struct page **published_pages[16];static unsigned published_counts[16];
static void traced_flush(struct page **p,unsigned n)
{assert(flushed<16);published_pages[flushed]=p;published_counts[flushed++]=n;drm_clflush_pages(p,n);}
static void traced_order(void){assert(flushed);ordered++;dma_wmb();}
#define drm_clflush_pages traced_flush
#define dma_wmb traced_order
#include "../remesa_gem_publish_inputs.inc"
#undef drm_clflush_pages
#undef dma_wmb
static unsigned checks,starts;
#define CHECK(x) do{checks++;assert(x);}while(0)
static struct drm_file file={1};
static struct remesa_gem_memory *memory;
static struct remesa_memory_snapshot snapshot;
static struct remesa_memory_view views[11];
static int validate(void *p,const void *c,unsigned n,const struct remesa_memory_binding *b,unsigned count)
{(void)p;(void)c;(void)b;CHECK(n==8&&count==11);return remesa_gem_job_publish_inputs_locked(memory,&snapshot,views,count);}
static int start(void *p,struct remesa_memory_ticket t,const void *c,unsigned n,u64 *cookie)
{(void)p;(void)c;(void)n;CHECK(flushed==11&&ordered==1);starts++;*cookie=t.operation;return 0;}
static int poll(void *p,struct remesa_memory_ticket t,u64 cookie,unsigned n,struct remesa_job_completion *out)
{(void)p;(void)n;*out=(struct remesa_job_completion){t,cookie,7};return 0;}
static int visible(void *p,struct remesa_memory_ticket t,u64 c){(void)p;(void)t;(void)c;return 0;}
static const struct remesa_job_executor executor={validate,start,poll,visible,visible};
static int other_validate(void *p,const void *c,unsigned n,const struct remesa_memory_binding *b,unsigned count)
{(void)p;(void)c;(void)b;CHECK(n==8&&count==1);return 0;}
static int other_start(void *p,struct remesa_memory_ticket t,const void *c,unsigned n,u64 *cookie)
{(void)p;(void)c;(void)n;*cookie=t.operation;return 0;}
static const struct remesa_job_executor other_executor={other_validate,other_start,poll,visible,visible};
static void action(unsigned a,u32 id)
{struct remesa_memory_request r={.version=1,.context=77,.action=a,.id=id};CHECK(!remesa_gem_memory_request(memory,&file,&r));}
static int publish_inputs(const struct remesa_memory_snapshot *s,const struct remesa_memory_view *v,unsigned count)
{int e;mutex_lock(&memory->lock);e=remesa_gem_job_publish_inputs_locked(memory,s,v,count);mutex_unlock(&memory->lock);return e;}
int main(int argc,char **argv)
{
 CHECK(argc==2);unsigned mode=(unsigned)atoi(argv[1]);CHECK(mode<=1);
 struct test_mmu mmu={true,{0}};struct drm_psb_private priv={&mmu,(void *)1,{0x28000000,32,0x80000000}};
 struct drm_device dev={&priv};CHECK(!remesa_gem_memory_open(&dev,&file,77,&memory));
 struct remesa_memory_binding bindings[11];u64 va=0x40000000;
 for(unsigned i=0;i<11;i++){
  u64 bytes=(i==5||i==6)?4096000:(u64)(i+1)*4096;
  struct remesa_memory_request r={.version=1,.context=77,.action=REMESA_MEM_CREATE,.bytes=bytes};
  CHECK(!remesa_gem_memory_request(memory,&file,&r));u32 id=r.id;action(REMESA_MEM_PIN,id);
  r.action=REMESA_MEM_MAP;r.bytes=0;r.va=va;CHECK(!remesa_gem_memory_request(memory,&file,&r));
  bindings[i]=(struct remesa_memory_binding){id,va,bytes};va+=bytes;
 }
 unsigned char commands[8]={0};struct remesa_memory_ticket ticket;
 CHECK(!remesa_gem_job_prepare(memory,bindings,11,commands,8,&ticket));
 CHECK(remesa_gem_job_snapshot(memory,ticket,&snapshot,views,11)==11);
 CHECK(remesa_gem_job_issue(memory,ticket)==-EOPNOTSUPP);CHECK(!flushed&&!ordered);
 for(unsigned bad=0;bad<9;bad++){
  struct remesa_memory_snapshot s=snapshot;struct remesa_memory_view v[11];memcpy(v,views,sizeof(v));unsigned count=11;
  if(bad==0)s.power_held=false;
  if(bad==1)s.exclusive_held=false;
  if(bad==2)s.mmu_end--;
  if(bad==3)s.commands=commands;
  if(bad==4)v[10].page_count--;
  if(bad==5)v[10].binding.bytes--;
  if(bad==6)v[10].cpu=(void *)1;
  if(bad==7)count--;
  if(bad==8)s.ticket.operation++;
  CHECK(publish_inputs(&s,v,count)<0);CHECK(!flushed&&!ordered);
 }
 if(mode){
  /* Fault on the last role/page; all-role revalidation must precede any flush. */
  for(unsigned i=0;i<4096;i++)if(test_pt[i].live&&test_pt[i].va==va-4096){test_pt[i].pfn++;break;}
  CHECK(publish_inputs(&snapshot,views,11)==-EIO);CHECK(!flushed&&!ordered);
  enum remesa_job_state state;CHECK(!remesa_gem_job_state(memory,ticket,&state)&&state==REMESA_JOB_QUARANTINED);
  for(unsigned i=0;i<11;i++)CHECK(memory->bo[bindings[i].id&31].holds==1&&memory->bo[bindings[i].id&31].quarantine);
  printf("PASS CPU_ONLY late-role PTE quarantine preserves11holds and0publication (%u checks)\n",checks);return 0;
 }
 /* A separate, valid injected operation blocks publication while issued
  * or visible. These are simulated lifecycle receipts, not hardware events. */
 struct remesa_memory_request extra={.version=1,.context=77,.action=REMESA_MEM_CREATE,.bytes=4096};
 CHECK(!remesa_gem_memory_request(memory,&file,&extra));u32 other_id=extra.id;
 action(REMESA_MEM_PIN,other_id);extra.action=REMESA_MEM_MAP;extra.bytes=0;extra.va=va;
 CHECK(!remesa_gem_memory_request(memory,&file,&extra));
 struct remesa_memory_binding one={other_id,va,4096};struct remesa_memory_ticket other;
 CHECK(!remesa_gem_job_prepare(memory,&one,1,commands,8,&other));
 memory->executor=&other_executor;CHECK(!remesa_gem_job_issue(memory,other));
 CHECK(publish_inputs(&snapshot,views,11)==-EBUSY);CHECK(!flushed&&!ordered);
 CHECK(!remesa_gem_job_wait(memory,other,1));CHECK(!remesa_gem_job_publish(memory,other));
 CHECK(publish_inputs(&snapshot,views,11)==-EBUSY);CHECK(!flushed&&!ordered);
 CHECK(!remesa_gem_job_acknowledge(memory,other));CHECK(!remesa_gem_job_retire(memory,other));
 /* Ordinary memory SYNC correctly refuses held resources. */
 struct remesa_memory_request r={.version=1,.context=77,.action=REMESA_MEM_SYNC,.id=bindings[0].id};
 CHECK(remesa_gem_memory_request(memory,&file,&r)==-EBUSY);
 memory->executor=&executor;CHECK(!remesa_gem_job_issue(memory,ticket));CHECK(starts==1);
 for(unsigned i=0;i<11;i++)CHECK(published_pages[i]==views[i].pages&&published_counts[i]==views[i].page_count);
 CHECK(published_counts[5]==1000&&published_counts[6]==1000);
 CHECK(publish_inputs(&snapshot,views,11)==-EBUSY);CHECK(flushed==11&&ordered==1);
 CHECK(!remesa_gem_job_wait(memory,ticket,1));CHECK(!remesa_gem_job_publish(memory,ticket));
 CHECK(publish_inputs(&snapshot,views,11)==-EBUSY);CHECK(flushed==11&&ordered==1);
 CHECK(!remesa_gem_job_acknowledge(memory,ticket));CHECK(!remesa_gem_job_retire(memory,ticket));
 CHECK(publish_inputs(&snapshot,views,11)==-ENOENT);
 CHECK(!remesa_gem_memory_close(memory));CHECK(!gems&&!pins&&!ptes&&!power&&!claimed);
 printf("PASS CPU_ONLY11heldinputs, two1000pagebuffers, identity/PTE validation before publication/issue, complete lifecycle and release (%u checks)\n",checks);
 return 0;
}
