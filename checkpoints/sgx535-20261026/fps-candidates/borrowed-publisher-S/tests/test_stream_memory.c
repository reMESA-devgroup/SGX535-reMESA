/* HOST injection test. Actual provider source, synthetic executor, NO GPU. */
#define REMESA_MEMORY_TEST
#include "../module/remesa_gem_memory.c"
#include <stdio.h>
static struct drm_file owner={1},foreign={2};
static struct remesa_memory_request r;
static unsigned calls;
static int validate(void *c,const void *p,unsigned n,const struct remesa_memory_binding *v,unsigned count) {(void)c;(void)p;(void)n;(void)v;(void)count;calls++;return 0;}
static int start(void *c,struct remesa_memory_ticket t,const void *p,unsigned n,u64 *cookie) {(void)c;(void)p;(void)n;calls++;*cookie=t.operation;return 0;}
static int poll(void *c,struct remesa_memory_ticket t,u64 cookie,unsigned n,struct remesa_job_completion *out) {(void)c;(void)n;calls++;*out=(struct remesa_job_completion){t,cookie,7};return 0;}
static int visible(void *c,struct remesa_memory_ticket t,u64 cookie) {(void)c;(void)t;(void)cookie;calls++;return 0;}
static int ack(void *c,struct remesa_memory_ticket t,u64 cookie) {(void)c;(void)t;(void)cookie;calls++;return 0;}
static const struct remesa_job_executor ops={validate,start,poll,visible,ack};
static void req(unsigned action,u64 context,u32 id) {memset(&r,0,sizeof(r));r.version=1;r.action=action;r.context=context;r.id=id;}
int main(void) {
 struct test_mmu mmu={true,{0}};struct drm_psb_private priv={&mmu,(void*)1,{0x28000000,32,0x80000000}};
 struct drm_device dev={&priv};struct remesa_gem_memory *s=NULL,*bad=NULL;struct remesa_memory_ticket ticket;
 struct remesa_memory_binding use;unsigned char commands[8]={0};u64 context;u32 id;
 memory_enabled=true;req(REMESA_MEM_OPEN,0,0);assert(!remesa_gem_memory_ioctl(&dev,&r,&owner));context=r.context;
 req(REMESA_MEM_CREATE,context,0);r.bytes=8192;assert(!remesa_gem_memory_ioctl(&dev,&r,&owner));id=r.id;
 req(REMESA_MEM_PIN,context,id);assert(!remesa_gem_memory_ioctl(&dev,&r,&owner));
 req(REMESA_MEM_MAP,context,id);r.va=0x40000000;assert(!remesa_gem_memory_ioctl(&dev,&r,&owner));
 assert(remesa_gem_stream_claim(&dev,&foreign,context,&bad)==-EACCES);
 assert(remesa_gem_stream_claim(&dev,&owner,context+1,&bad)==-EACCES);
 assert(!remesa_gem_stream_claim(&dev,&owner,context,&s));assert(s && s->stream_bound);
 assert(remesa_gem_stream_claim(&dev,&owner,context,&bad)==-EBUSY);
 req(REMESA_MEM_CLOSE,context,0);assert(remesa_gem_memory_ioctl(&dev,&r,&owner)==-EBUSY);
 req(REMESA_MEM_UNMAP,context,id);assert(remesa_gem_memory_ioctl(&dev,&r,&owner)==-EBUSY);
 req(REMESA_MEM_CREATE,context,0);r.bytes=4096;assert(remesa_gem_memory_ioctl(&dev,&r,&owner)==-EBUSY);
 req(REMESA_MEM_LEASE,context,id);r.flags=1;assert(!remesa_gem_memory_ioctl(&dev,&r,&owner));
 use=(struct remesa_memory_binding){id,0x40000000,8192};
 assert(remesa_gem_job_prepare(s,&use,1,commands,8,&ticket)==-EBUSY);
 req(REMESA_MEM_UNLEASE,context,id);assert(!remesa_gem_memory_ioctl(&dev,&r,&owner));
 assert(!remesa_gem_job_prepare(s,&use,1,commands,8,&ticket));assert(s->bo[id&31].holds==1);
 req(REMESA_MEM_LEASE,context,id);assert(remesa_gem_memory_ioctl(&dev,&r,&owner)==-EBUSY);
 assert(remesa_gem_stream_release(s)==-EBUSY);
 assert(!remesa_gem_stream_attach(s,ticket,&ops,s));
 assert(remesa_gem_stream_detach(s,ticket,s)==-EBUSY);
 assert(!remesa_gem_job_issue(s,ticket));assert(remesa_gem_job_cancel(s,ticket)==-EBUSY);
 assert(!remesa_gem_job_wait(s,ticket,10));assert(!remesa_gem_job_publish(s,ticket));
 assert(!remesa_gem_job_acknowledge(s,ticket));assert(remesa_gem_job_retire(s,ticket)==-EBUSY);
 assert(remesa_gem_stream_detach(s,ticket,&dev)==-EBUSY);
 assert(!remesa_gem_stream_detach(s,ticket,s));assert(!remesa_gem_job_retire(s,ticket));
 assert(!s->bo[id&31].holds && calls==5);assert(!remesa_gem_stream_release(s));
 req(REMESA_MEM_CLOSE,context,0);assert(!remesa_gem_memory_ioctl(&dev,&r,&owner));
 assert(!gems&&!pins&&!ptes&&!power&&!claimed&&!device_refs&&!module_refs);
 puts("PASS: stream owner/file/context lease, topology freeze, CPU exclusion, real provider holds, executor attach/detach, acknowledge-before-retire and final reference release; synthetic executor, NO hardware");return 0;
}
