/* SPDX-License-Identifier: MIT */
/* Native GEM service compiled with injected kernel APIs; NO hardware. */
#define REMESA_MEMORY_TEST
#include "remesa_gem_memory.c"
#include "../remesa_gem_visible_readback.inc"
#include <stdio.h>
static struct drm_file owner={1};
static unsigned starts,polls,publishes,acks,mode;
static int validate(void *p,const void *c,unsigned n,const struct remesa_memory_binding *b,unsigned count)
{(void)p;(void)c;assert(n==8&&count==1&&b[0].bytes==4096000);return 0;}
static int start(void *p,struct remesa_memory_ticket t,const void *c,unsigned n,u64 *cookie)
{(void)p;(void)c;(void)n;++starts;*cookie=t.operation+1;return 0;}
static int poll(void *p,struct remesa_memory_ticket t,u64 cookie,unsigned budget,struct remesa_job_completion *out)
{(void)p;assert(budget==1);++polls;*out=(struct remesa_job_completion){t,cookie,7};return 0;}
static int publish(void *p,struct remesa_memory_ticket t,u64 cookie)
{(void)p;(void)t;(void)cookie;++publishes;return mode==1?-EIO:0;}
static int ack(void *p,struct remesa_memory_ticket t,u64 cookie)
{(void)p;(void)t;(void)cookie;++acks;return 0;}
static const struct remesa_job_executor executor={validate,start,poll,publish,ack};
static int action(struct remesa_gem_memory *s,unsigned a,u32 id)
{struct remesa_memory_request r={.version=1,.context=77,.action=a,.id=id};return remesa_gem_memory_request(s,&owner,&r);}
int main(int argc,char **argv)
{
 assert(argc==2);mode=(unsigned)atoi(argv[1]);assert(mode<=3);
 struct test_mmu mmu={true,{0}};struct drm_psb_private priv={&mmu,(void *)1,{0x28000000,32,0x80000000}};
 struct drm_device dev={&priv};struct remesa_gem_memory *s=NULL;
 assert(!remesa_gem_memory_open(&dev,&owner,77,&s));
 struct remesa_memory_request r={.version=1,.context=77,.action=REMESA_MEM_CREATE,.bytes=4096000};
 assert(!remesa_gem_memory_request(s,&owner,&r));u32 id=r.id;
 assert(!action(s,REMESA_MEM_PIN,id));r.bytes=0;r.action=REMESA_MEM_MAP;r.va=0x40000000;
 assert(!remesa_gem_memory_request(s,&owner,&r));
 struct backing *bo=&s->bo[id&31];
 /* Synthetic producer pattern: explicitly not a GPU result. */
 for(size_t i=0;i<4096000;i++)bo->gem.bytes[i]=(unsigned char)((i>>12)^i);
 struct remesa_memory_binding binding={id,0x40000000,4096000};
 struct remesa_memory_ticket t;unsigned char commands[8]={0},out[4096],saved[4096];
 memset(out,0xcc,sizeof(out));memcpy(saved,out,sizeof(out));
 assert(!remesa_gem_job_prepare(s,&binding,1,commands,8,&t));
 assert(remesa_gem_job_copy_visible(s,t,id,0,4096,out)==-EBUSY&&!memcmp(out,saved,4096));
 struct remesa_memory_view view;
 if(mode!=3)assert(remesa_gem_job_views(s,t,&view,1)==1);
 s->executor=&executor;assert(!remesa_gem_job_issue(s,t));
 assert(remesa_gem_job_copy_visible(s,t,id,0,4096,out)==-EBUSY);
 assert(!remesa_gem_job_wait(s,t,1));
 assert(remesa_gem_job_copy_visible(s,t,id,0,4096,out)==-EBUSY);
 int e=remesa_gem_job_publish(s,t);
 if(mode==1){assert(e==-EIO);goto held;}
 assert(!e&&bo->holds==1);
 assert(remesa_gem_job_copy_visible(s,t,id+1,0,1,out)==-ENOENT);
 assert(remesa_gem_job_copy_visible(s,t,id,4096000,1,out)==-EINVAL);
 assert(remesa_gem_job_copy_visible(s,t,id,~(u64)0,1,out)==-EINVAL);
 assert(remesa_gem_job_copy_visible(s,t,id,0,4097,out)==-EINVAL);
 assert(!memcmp(out,saved,4096));
 if(mode==2) {
  test_pt[999].pfn++;assert(remesa_gem_job_copy_visible(s,t,id,0,4096,out)==-EIO);
  assert(!memcmp(out,saved,4096));goto held;
 }
 if(mode==3) {
  fail_vmap=1;assert(remesa_gem_job_copy_visible(s,t,id,0,4096,out)==-ENOMEM);
  assert(bo->holds==1&&!memcmp(out,saved,4096));fail_vmap=0;
 }
 /* Existing held view (mode0) and temporary-map path (mode3). */
 for(unsigned page=0;page<1000;page++) {
  assert(!remesa_gem_job_copy_visible(s,t,id,(u64)page*4096,4096,out));
  assert(!memcmp(out,bo->gem.bytes+(size_t)page*4096,4096)&&bo->holds==1);
 }
 assert(!remesa_gem_job_copy_visible(s,t,id,4095,4096,out));
 assert(!memcmp(out,bo->gem.bytes+4095,4096));
 assert(action(s,REMESA_MEM_LEASE,id)==-EBUSY&&remesa_gem_memory_close(s)==-EBUSY);
 assert(!remesa_gem_job_acknowledge(s,t));assert(!remesa_gem_job_copy_visible(s,t,id,4096000-1,1,out));
 assert(!remesa_gem_job_retire(s,t));assert(remesa_gem_job_copy_visible(s,t,id,0,1,out)==-ENOENT);
 assert(!remesa_gem_memory_close(s));assert(!gems&&!pins&&!ptes&&!power&&!module_refs&&!device_refs);
 assert(starts==1&&polls==1&&publishes==1&&acks==1);
 printf("PASS CPU_ONLY visible readback mode%u:1000pages1280x800, held chunks, cross-page, state/identity/bounds refusal; synthetic executor\n",mode);return 0;
held:
 assert(remesa_gem_job_copy_visible(s,t,id,0,4096,out)==-EIO);
 assert(remesa_gem_job_retire(s,t)==-EBUSY&&remesa_gem_memory_close(s)==-EBUSY&&bo->holds==1);
 puts("PASS CPU_ONLY readback failure: quarantine and hold retained; no GPU or recovery claim");return 0;
}
