/* SPDX-License-Identifier: MIT */
#define REMESA_MEMORY_TEST
#include "../module/remesa_gem_memory.c"
#include <stdio.h>
static struct drm_file owner={1},foreign={2};
static struct remesa_memory_request request(unsigned action,u32 id)
{struct remesa_memory_request r={0};r.version=1;r.context=77;r.action=action;r.id=id;return r;}
static int action(struct remesa_gem_memory *s,unsigned a,u32 id)
{struct remesa_memory_request r=request(a,id);return remesa_gem_memory_request(s,&owner,&r);}
int main(void)
{
 memory_enabled=true;
 struct test_mmu mmu={true,{0}};struct drm_psb_private priv={&mmu,(void *)1,{0x28000000,32,0x80000000}};
 struct drm_device dev={&priv};struct remesa_gem_memory *s=NULL,*other=NULL;struct remesa_memory_request r;u32 id,old;
 admin=false;assert(remesa_gem_memory_open(&dev,&owner,77,&s)==-EPERM);admin=true;
 fail_alloc=1;assert(remesa_gem_memory_open(&dev,&owner,77,&s)==-ENOMEM);assert(!power && !claimed);fail_alloc=0;
 assert(!remesa_gem_memory_open(&dev,&owner,77,&s));assert(remesa_gem_memory_open(&dev,&foreign,78,&other)==-EBUSY);assert(power==1);
 r=request(REMESA_MEM_CREATE,0);r.bytes=8192;fail_init=1;assert(remesa_gem_memory_request(s,&owner,&r)==-ENOMEM);fail_init=0;
 assert(!remesa_gem_memory_request(s,&owner,&r));id=r.id;assert(gems==1);
 assert(remesa_gem_memory_request(s,&foreign,&r)==-EACCES);
 fail_pages=1;assert(action(s,REMESA_MEM_PIN,id)==-ENOMEM);fail_pages=0;assert(!action(s,REMESA_MEM_PIN,id));
 r=request(REMESA_MEM_MAP,id);r.va=0x28000000;assert(remesa_gem_memory_request(s,&owner,&r)==-EBUSY);
 r.va=0x40000000;inserts=0;fail_insert=2;assert(remesa_gem_memory_request(s,&owner,&r)==-ENOMEM);assert(!ptes);fail_insert=0;
 assert(!remesa_gem_memory_request(s,&owner,&r));assert(ptes==2);
 r=request(REMESA_MEM_INFO,id);assert(!remesa_gem_memory_request(s,&owner,&r));
 struct remesa_memory_info v;memcpy(&v,r.data,sizeof(v));
 struct remesa_memory_binding use={id,0x40000000,8192};
 struct remesa_memory_ticket ticket,stale;
 unsigned char commands[8]={1,2,3,4,5,6,7,8};
 fail_alloc=1;assert(remesa_gem_job_prepare(s,&use,1,commands,8,&ticket)==-ENOMEM);fail_alloc=0;
 assert(!remesa_gem_job_prepare(s,&use,1,commands,8,&ticket));
 commands[0]=99;assert(((unsigned char *)s->jobs[ticket.slot].commands)[0]==1);
 assert(action(s,REMESA_MEM_UNMAP,id)==-EBUSY && action(s,REMESA_MEM_LEASE,id)==-EBUSY);
 assert(remesa_gem_job_issue(s,ticket)==-EOPNOTSUPP);
 stale=ticket;stale.operation++;assert(remesa_gem_job_cancel(s,stale)==-ENOENT);
 assert(!remesa_gem_job_cancel(s,ticket));assert(remesa_gem_job_cancel(s,ticket)==-ENOENT);
 assert(!remesa_gem_job_prepare(s,&use,1,commands,8,&stale));assert(stale.operation>ticket.operation);
 assert(remesa_gem_job_cancel(s,ticket)==-ENOENT);assert(!remesa_gem_job_cancel(s,stale));
 assert(v.objects==1 && v.pinned_objects==1 && v.mapped_pages==2 && v.verified_pages==2 && v.gpu_va==0x40000000);
 r=request(REMESA_MEM_INFO,id);r.reserved=1;assert(remesa_gem_memory_request(s,&owner,&r)==-EINVAL);
 assert(action(s,REMESA_MEM_DESTROY,id)==-EBUSY);assert(action(s,REMESA_MEM_UNPIN,id)==-EBUSY);
 r=request(REMESA_MEM_LEASE,id);r.flags=1;fail_vmap=1;assert(remesa_gem_memory_request(s,&owner,&r)==-ENOMEM);fail_vmap=0;assert(!remesa_gem_memory_request(s,&owner,&r));
 assert(remesa_gem_memory_close(s)==-EBUSY);assert(action(s,REMESA_MEM_UNMAP,id)==-EBUSY);
 r=request(REMESA_MEM_WRITE,id);r.offset=8191;r.length=2;assert(remesa_gem_memory_request(s,&owner,&r)==-EINVAL);
 r.offset=4090;r.length=10;memset(r.data,0x42,10);assert(!remesa_gem_memory_request(s,&owner,&r));
 r=request(REMESA_MEM_READ,id);r.offset=4090;r.length=10;assert(!remesa_gem_memory_request(s,&owner,&r));for(int i=0;i<10;i++)assert(r.data[i]==0x42);
 assert(!action(s,REMESA_MEM_UNLEASE,id));assert(!action(s,REMESA_MEM_SYNC,id));assert(flushes==1);
 assert(!action(s,REMESA_MEM_UNMAP,id));assert(!action(s,REMESA_MEM_UNPIN,id));assert(!action(s,REMESA_MEM_PIN,id));
 r=request(REMESA_MEM_LEASE,id);assert(!remesa_gem_memory_request(s,&owner,&r));
 r=request(REMESA_MEM_READ,id);r.offset=4090;r.length=10;assert(!remesa_gem_memory_request(s,&owner,&r));assert(r.data[0]==0x42);
 assert(action(s,REMESA_MEM_WRITE,id)==-EINVAL);assert(!action(s,REMESA_MEM_UNLEASE,id));
 assert(!action(s,REMESA_MEM_UNPIN,id));assert(!action(s,REMESA_MEM_DESTROY,id));old=id;
 for(unsigned i=0;i<50;i++){r=request(REMESA_MEM_CREATE,0);r.bytes=4096;assert(!remesa_gem_memory_request(s,&owner,&r));id=r.id;assert(id!=old);assert(action(s,REMESA_MEM_PIN,old)==-ENOENT);assert(!action(s,REMESA_MEM_PIN,id));assert(!action(s,REMESA_MEM_UNPIN,id));assert(!action(s,REMESA_MEM_DESTROY,id));old=id;}
 r=request(REMESA_MEM_CREATE,0);r.bytes=MAX_BYTES;assert(!remesa_gem_memory_request(s,&owner,&r));id=r.id;
 r=request(REMESA_MEM_CREATE,0);r.bytes=4096;assert(remesa_gem_memory_request(s,&owner,&r)==-EINVAL);
 r=request(123,id);assert(remesa_gem_memory_request(s,&owner,&r)==-EOPNOTSUPP);
 assert(!remesa_gem_memory_close(s));assert(!gems && !pins && !ptes && !power && !claimed);
 /* Real per-file hooks issue fresh server contexts and reclaim fd leases. */
 r=request(REMESA_MEM_OPEN,0);r.context=0;
 memory_enabled=false;assert(remesa_gem_memory_ioctl(&dev,&r,&owner)==-EOPNOTSUPP);memory_enabled=true;
 assert(!remesa_gem_memory_ioctl(&dev,&r,&owner));u64 context=r.context;assert(context);
 r=request(REMESA_MEM_CREATE,0);r.context=context;r.bytes=4096;
 assert(!remesa_gem_memory_ioctl(&dev,&r,&owner));id=r.id;
 r=request(REMESA_MEM_PIN,id);r.context=context;assert(!remesa_gem_memory_ioctl(&dev,&r,&owner));
 r=request(REMESA_MEM_LEASE,id);r.context=context;assert(!remesa_gem_memory_ioctl(&dev,&r,&owner));
 remesa_gem_memory_postclose(&dev,&foreign);assert(gems==1);
 remesa_gem_memory_postclose(&dev,&owner);assert(!gems && !pins && !ptes && !power);
 r=request(REMESA_MEM_OPEN,0);r.context=0;assert(!remesa_gem_memory_ioctl(&dev,&r,&owner));assert(r.context>context);
 r=request(REMESA_MEM_CREATE,0);r.bytes=4096;r.context=context;assert(remesa_gem_memory_ioctl(&dev,&r,&owner)==-EINVAL);
 remesa_gem_memory_postclose(&dev,&owner);assert(!gems && !pins && !power);
 /* Uncertain jobs retain pages, commands, device and module references. */
 assert(!remesa_gem_memory_open(&dev,&owner,88,&s));
 r=request(REMESA_MEM_CREATE,0);r.context=88;r.bytes=8192;assert(!remesa_gem_memory_request(s,&owner,&r));id=r.id;
 r=request(REMESA_MEM_PIN,id);r.context=88;assert(!remesa_gem_memory_request(s,&owner,&r));
 r=request(REMESA_MEM_MAP,id);r.context=88;r.va=0x40000000;assert(!remesa_gem_memory_request(s,&owner,&r));
 use=(struct remesa_memory_binding){id,0x40000000,8192};
 assert(!remesa_gem_job_prepare(s,&use,1,commands,8,&ticket));
 stale=ticket;stale.context++;assert(remesa_gem_job_quarantine(s,stale)==-ENOENT);
 assert(!remesa_gem_job_quarantine(s,ticket));assert(!remesa_gem_job_quarantine(s,ticket));
 assert(remesa_gem_job_cancel(s,ticket)==-EBUSY);
 for(unsigned pt=0;pt<4096;pt++)if(test_pt[pt].live){test_pt[pt].pfn++;break;}
 r=request(REMESA_MEM_INFO,id);r.context=88;assert(remesa_gem_memory_request(s,&owner,&r)==-EIO);
 assert(s->bo[id&31].quarantine);assert(remesa_gem_memory_close(s)==-EBUSY);
 assert(gems==1 && pins==1 && ptes==2 && device_refs==1 && module_refs==1);
 puts("PASS: real kernel resource source under host API fault injection; partial GEM/PT rollback, GTT exclusion, leases, preserved contents, 50 generations, quota, ownership, teardown and intentional quarantine retention; NO hardware");return 0;
}
