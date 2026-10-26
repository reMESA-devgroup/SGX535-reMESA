/* Actual GEM code, synthetic mapped TA metadata; no hardware or real vmap.
 * Real heap-backed fake GEM pages make ownership/free paths observable. */
#define REMESA_MEMORY_TEST
#include "../module/remesa_gem_memory.c"
#include <stdio.h>
static struct remesa_gem_memory s;static struct drm_file file={1};static u32 id;
static int req(unsigned action,unsigned flags,unsigned offset,void*out){struct remesa_memory_request r={.version=1,.context=19,.action=action,.id=id,.flags=flags};if(action==REMESA_MEM_READ||action==REMESA_MEM_WRITE){r.offset=offset;r.length=4096;}int e=remesa_gem_memory_request(&s,&file,&r);if(!e&&out&&action==REMESA_MEM_READ)memcpy(out,r.data,4096);return e;}
static struct backing*setup(bool domain,bool cached){memset(&s,0,sizeof(s));s.context=19;s.file=&file;mutex_init(&s.lock);struct remesa_memory_request r={.version=1,.context=19,.action=REMESA_MEM_CREATE,.bytes=33554432};assert(!remesa_gem_memory_request(&s,&file,&r));id=r.id;assert(!req(REMESA_MEM_PIN,0,0,NULL));struct backing*b=&s.bo[id&31];b->reserved=true;b->va.start=0x407ef000;b->va.end=b->va.start+b->gem.size-1;b->inserted=8192;s.stream_bound=s.ta_pools_sealed=true;s.ta_cache_domains=domain;if(cached){b->cached_job_view=vmap(b->pages,8192,VM_MAP,PAGE_KERNEL);assert(b->cached_job_view);}return b;}
static void teardown(struct backing*b,bool via_unpin){assert(!b->lease);b->reserved=false;b->inserted=0;s.stream_bound=false;s.ta_cache_domains=false;unsigned old=vunmap_calls;if(via_unpin){assert(!req(REMESA_MEM_UNPIN,0,0,NULL));assert(vunmap_calls==old+1&&!b->cached_job_view);assert(!req(REMESA_MEM_DESTROY,0,0,NULL));assert(vunmap_calls==old+1);}else{discard(&s,b);assert(vunmap_calls==old+1&&!b->cached_job_view&&!b->live);}assert(!gems&&!pins);}
int main(void){
 struct backing*b=setup(true,true);unsigned maps=vmap_calls,unmaps=vunmap_calls,flush0=flushes,rmb=test_rmb;unsigned char out[4096];memset(b->cached_job_view,0x7a,4096);b->ta_gpu_dirty=true;
 fail_vmap=true;assert(!req(REMESA_MEM_LEASE,0,0,NULL));assert(vmap_calls==maps&&b->cpu==b->cached_job_view&&b->lease_borrowed&&b->lease&&!b->ta_gpu_dirty&&b->ta_cpu_cached&&flushes==flush0+1&&test_rmb==rmb+1);assert(!req(REMESA_MEM_READ,0,0,out));for(unsigned i=0;i<4096;i++)assert(out[i]==0x7a);
 assert(req(REMESA_MEM_LEASE,0,0,NULL)==-EBUSY);assert(req(REMESA_MEM_UNPIN,0,0,NULL)==-EBUSY);assert(req(REMESA_MEM_DESTROY,0,0,NULL)==-EBUSY);s.stream_bound=false;assert(remesa_gem_memory_close(&s)==-EBUSY&&b->lease_borrowed&&b->cached_job_view);s.stream_bound=true;
 assert(!req(REMESA_MEM_UNLEASE,0,0,NULL)&&vunmap_calls==unmaps&&!b->cpu&&!b->lease&&!b->lease_borrowed&&b->cached_job_view);assert(req(REMESA_MEM_UNLEASE,0,0,NULL)==-EINVAL&&vunmap_calls==unmaps);
 assert(req(REMESA_MEM_LEASE,1,0,NULL)==-EPERM&&!b->lease&&!b->lease_borrowed&&vmap_calls==maps);assert(req(REMESA_MEM_WRITE,0,0,NULL)==-EPERM);
 b->holds=1;assert(req(REMESA_MEM_LEASE,0,0,NULL)==-EBUSY);b->holds=0;b->quarantine=true;assert(req(REMESA_MEM_LEASE,0,0,NULL)==-EIO);b->quarantine=false;id+=32;assert(req(REMESA_MEM_LEASE,0,0,NULL)==-ENOENT);id-=32;assert(vmap_calls==maps&&!b->lease_borrowed);fail_vmap=false;
 teardown(b,true);
 /* CREATE slot reuse must erase a stale borrowed flag. */
 s.bo[0].lease_borrowed=true;struct remesa_memory_request r={.version=1,.context=19,.action=REMESA_MEM_CREATE,.bytes=4096};assert(!remesa_gem_memory_request(&s,&file,&r));assert(!s.bo[r.id&31].lease_borrowed);id=r.id;assert(!req(REMESA_MEM_DESTROY,0,0,NULL));assert(!gems&&!pins);
 /* Legacy mode preserves distinct lease vmap, and unlease frees only it. */
 b=setup(false,true);maps=vmap_calls;unmaps=vunmap_calls;assert(!req(REMESA_MEM_LEASE,0,0,NULL)&&vmap_calls==maps+1&&!b->lease_borrowed);assert(!req(REMESA_MEM_UNLEASE,0,0,NULL)&&vunmap_calls==unmaps+1&&b->cached_job_view);teardown(b,false);
 /* No cached map and non-TA READ preserve ENOMEM, with no fake lease. */
 b=setup(true,false);b->ta_gpu_dirty=true;fail_vmap=true;assert(req(REMESA_MEM_LEASE,0,0,NULL)==-ENOMEM&&!b->lease&&!b->lease_borrowed&&!b->cpu&&b->ta_gpu_dirty);fail_vmap=false;b->cached_job_view=vmap(b->pages,8192,VM_MAP,PAGE_KERNEL);assert(b->cached_job_view);b->va.start=0x50000000;fail_vmap=true;assert(req(REMESA_MEM_LEASE,0,0,NULL)==-ENOMEM&&!b->lease_borrowed);fail_vmap=false;b->va.start=0x407ef000;teardown(b,false);
 /* FD death borrows no ownership of cached_job_view. Postclose must neither
  * unmap it nor erase uncertainty when stream teardown remains blocked. */
 b=setup(true,true);assert(!req(REMESA_MEM_LEASE,0,0,NULL)&&b->lease_borrowed);unmaps=vunmap_calls;void*cached=b->cached_job_view;b->quarantine=true;b->ta_gpu_dirty=true;endpoint=&s;remesa_gem_memory_postclose(NULL,&file);assert(endpoint==&s&&s.file==NULL&&s.stream_bound&&b->cached_job_view==cached&&!b->cpu&&!b->lease&&!b->lease_borrowed&&vunmap_calls==unmaps&&b->quarantine&&b->ta_gpu_dirty);endpoint=NULL;
 /* No GPU exists in this synthetic fixture: free its own modeled allocation
  * directly, not through an attempted recovery of uncertain driver work. */
 b->quarantine=false;s.file=&file;teardown(b,false);
 puts("PASS actual GEM READTA cached lease under exhausted vmap budget; readonly/fallback/generation/holds/quarantine; UNLEASE/postclose no borrowed vunmap; unpin/discard free once; CREATE flag reset; CPU metadata/vmap mock GPU0");return 0;
}
