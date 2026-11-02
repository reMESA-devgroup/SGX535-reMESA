/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual private publication/validation and memory requests. MMU callbacks,
 * cache instructions and completion are modeled; no GPU or warm admission. */
#define REMESA_GEM_PRIVATE_TEST_MAIN inherited_private_main
#include "test_gem_private.c"
#include "remesa_private_session.inc"
#include "remesa_private_session_dpm.inc"

static unsigned token;
static u32 cache_pool(struct remesa_gem_memory *s,u64 bytes,u64 va)
{
 struct remesa_memory_request r={.version=1,.action=REMESA_MEM_CREATE,.context=77,.bytes=bytes};
 CHECK(!remesa_gem_memory_request(s,&file,&r));u32 id=r.id;
 r=(struct remesa_memory_request){.version=1,.action=REMESA_MEM_PIN,.context=77,.id=id};
 CHECK(!remesa_gem_memory_request(s,&file,&r));r.action=REMESA_MEM_MAP;r.va=va;
 CHECK(!remesa_gem_memory_request(s,&file,&r));return id;
}
static int cpu_request(struct remesa_gem_memory *s,unsigned action,u32 id)
{
 struct remesa_memory_request r={.version=1,.action=action,.context=77,.id=id};
 return remesa_gem_memory_request(s,&file,&r);
}
int main(void)
{
 CHECK(!inherited_private_main());mode=0;
 struct test_mmu mmu={.has_clflush=true,.pd={.driver=&driver}};
 struct drm_psb_private priv={&mmu,(void*)1,{0x28000000,32,0xe0000000}};
 struct drm_device dev={&priv};struct remesa_gem_memory *s=NULL,*leased=NULL;
 struct remesa_twod_reservation guard={.device=&priv,.held=true};
 struct remesa_memory_binding uses[3];struct remesa_gem_private_rule rules[3];
 struct remesa_memory_snapshot snapshot;struct remesa_memory_view views[3];
 struct remesa_gem_private loan={0};struct remesa_memory_ticket t;u32 command=0;
 CHECK(!remesa_gem_memory_open(&dev,&file,77,&s));endpoint=s;
 expected_memory=s;expected_guard=&guard;dpm_model=true;
 u32 ids[3]={cache_pool(s,4096,0x20080000),cache_pool(s,33554432,0x50000000),cache_pool(s,33554432,0x30100000)};
 CHECK(!remesa_private_session_acquire(&dev,&file,77,&token,&leased)&&leased==s);
 for(unsigned i=0;i<3;i++){
  struct backing *b=&s->bo[ids[i]&31];
  uses[i]=(struct remesa_memory_binding){ids[i],b->va.start,b->gem.size};
  rules[i]=(struct remesa_gem_private_rule){ids[i],i?3u:1u,i?1u:0u};
 }
 struct backing *table=&s->bo[ids[1]&31],*parameter=&s->bo[ids[2]&31];
 unsigned zeroed=test_zeroed_pages;
 for(unsigned pass=0;pass<4;pass++){
  unsigned before=flushes;
  CHECK(!remesa_gem_job_prepare(s,uses,3,&command,4,&t));
  if(!pass){
   CHECK(!remesa_gem_job_claim_zero_dpm(s,t,ids[1],ids[2]));
   CHECK(!remesa_private_session_seal_dpm(s,&token,t,ids[1],ids[2]));
  }else{
   /* Cache publication alone is not scene admission: cold provenance was
    * consumed, and no warm claim is fabricated for these generic test jobs. */
   CHECK(remesa_gem_job_claim_zero_dpm(s,t,ids[1],ids[2])==-ENODATA);
   mutex_lock(&s->lock);CHECK(remesa_private_session_dpm_locked(s,t)==-EACCES);mutex_unlock(&s->lock);
  }
  CHECK(private_dpm_domain(s,table)&&private_dpm_domain(s,parameter));
  CHECK(!private_dpm_domain(s,&s->bo[ids[0]&31]));
  table->generation++;CHECK(!private_dpm_domain(s,table));table->generation--;
  CHECK(remesa_gem_job_snapshot_selected(s,t,&snapshot,views,3,1)==3);
  CHECK(!remesa_gem_private_prepare(s,t,rules,3,&guard,&loan));
  mutex_lock(&s->lock);
  struct remesa_memory_view bad[3];memcpy(bad,views,sizeof bad);bad[2].page_count--;
  CHECK(remesa_gem_private_publish_locked(&loan,bad,3)==-EACCES&&flushes==before);
  CHECK(!loan.host_published);
  bound_error=-EIO;
  CHECK(remesa_gem_private_publish_locked(&loan,views,3)==-EIO&&flushes==before&&!loan.host_published);
  bound_error=0;
  unsigned w=test_wmb;
  CHECK(!remesa_gem_private_publish_locked(&loan,views,3)&&loan.host_published&&test_wmb==w+1);
  /* Initial upload flushes both pools; after CPU read, only that pool;
   * untouched device ownership flushes neither; both CPU reads flush both. */
  unsigned expected=pass==0||pass==3?3:pass==1?2:1;
  CHECK(flushes==before+expected);
  CHECK(!table->ta_cpu_cached&&!table->ta_cpu_dirty&&table->ta_gpu_dirty);
  CHECK(!parameter->ta_cpu_cached&&!parameter->ta_cpu_dirty&&parameter->ta_gpu_dirty);
  CHECK(!table->zero_pristine&&!parameter->zero_pristine);
  CHECK(remesa_gem_private_publish_locked(&loan,views,3)==-EALREADY&&flushes==before+expected);
  /* Actual general executor calls this only after authenticated retirement.
   * Here the callback boundary is modeled, never presented as GPU evidence. */
  private_dpm_complete(s,table);private_dpm_complete(s,parameter);
  CHECK(flushes==before+expected);
  mutex_unlock(&s->lock);
  CHECK(cpu_request(s,REMESA_MEM_LEASE,ids[1])==-EBUSY);
  CHECK(!remesa_gem_private_release(&loan));CHECK(!remesa_gem_job_cancel(s,t));
  if(pass==0){
   unsigned f=flushes,r=test_rmb;
   CHECK(!cpu_request(s,REMESA_MEM_LEASE,ids[1]));
   CHECK(flushes==f+1&&test_rmb==r+1&&table->ta_cpu_cached&&!table->ta_gpu_dirty);
   CHECK(parameter->ta_gpu_dirty&&!parameter->ta_cpu_cached);
   CHECK(!cpu_request(s,REMESA_MEM_UNLEASE,ids[1])&&flushes==f+1);
  }
  if(pass==2){
   unsigned f=flushes,r=test_rmb;
   CHECK(!cpu_request(s,REMESA_MEM_LEASE,ids[1]));
   CHECK(!cpu_request(s,REMESA_MEM_LEASE,ids[2]));
   CHECK(flushes==f+2&&test_rmb==r+2);
   CHECK(!cpu_request(s,REMESA_MEM_UNLEASE,ids[1]));
   CHECK(!cpu_request(s,REMESA_MEM_UNLEASE,ids[2]));
   CHECK(table->ta_cpu_cached&&parameter->ta_cpu_cached&&!table->ta_gpu_dirty&&!parameter->ta_gpu_dirty);
  }
  CHECK(test_zeroed_pages==zeroed);
 }
 unsigned f=flushes,r=test_rmb;
 u32 saved=s->private_dpm_ids[1];s->private_dpm_ids[1]^=32;
 CHECK(remesa_private_session_release(s,&token)==-EIO&&flushes==f&&test_rmb==r);
 CHECK(s->private_dpm_ids[0]==ids[1]);s->private_dpm_ids[1]=saved;
 s->private_source_held=true;
 CHECK(remesa_private_session_release(s,&token)==-EBUSY&&flushes==f);s->private_source_held=false;
 CHECK(!remesa_private_session_release(s,&token));
 CHECK(flushes==f+2&&test_rmb==r+2&&!s->private_dpm_ids[0]&&!s->private_dpm_ids[1]);
 CHECK(!table->ta_gpu_dirty&&!parameter->ta_gpu_dirty&&!private_dpm_domain(s,table));
 CHECK(!cpu_request(s,REMESA_MEM_LEASE,ids[1])&&flushes==f+2);
 CHECK(!cpu_request(s,REMESA_MEM_UNLEASE,ids[1]));
 CHECK(test_zeroed_pages==zeroed&&!remesa_gem_memory_close(s));endpoint=NULL;
 CHECK(!gems&&!pins&&!ptes&&!claimed&&!power&&!device_refs&&!module_refs);
 printf("PASS %u actual publication/cache-domain checks; pool flushes initial2/read1/untouched0/both-read2, unseal2; GPU0\n",checks);
 return 0;
}
