/* Actual provider + actual trusted prefire bridge + actual owner_flush body.
 * One CPU/GPU page per BO models data caches; full BO/PFN metadata is fake.
 * No GPU execution/cache hardware claim. Mutex shim rejects recursive locking. */
#define REMESA_MEMORY_TEST
#include "../module/remesa_gem_memory.c"
#define REMESA_LAB_DEPTH1280_PROFILE 1
#include "../module/frozen_kernel_contract.h"
#include <stdio.h>
struct sgx535_gma500_bo {struct drm_gem_object gem;struct page**pages;unsigned page_count,mapped_pages;void*cpu;};
struct sgx535_gma500_owner {bool initialized,device_claimed,user_images_flushed,borrowed_backing;struct {unsigned phase;}session;struct sgx535_gma500_bo objects[11];struct sgx535_frozen_bo descriptors[11];struct sgx535_frozen_cpu_view views[11];u64 mmu_end;};
struct trusted_operation {struct sgx535_gma500_owner owner;struct remesa_memory_ticket ticket;struct remesa_memory_view views[11];bool borrowed_images,completed,visible,acknowledged;};
struct trusted_stream {struct trusted_operation*op;struct remesa_gem_memory*memory;bool claimed,fast_nohash,quarantined;};
static struct trusted_stream stream;
#include "trusted_prefire_actual.inc"
#include "publisher_views_actual.inc"
static struct remesa_gem_memory s;static struct trusted_operation op;static struct remesa_memory_snapshot snapshot;static struct remesa_memory_view views[11];
static unsigned char cpu[11][4096],gpu[11][4096],commands[264];
static const u64 sizes[11]={131072,32768,131072,4096,4096,4096000,122880,33554432,33554432,4096,4096000};
static const u64 vas[11]={0x20000000,0x20080000,0x20028000,0x30000000,0x46000000,0x40001000,0x407d1000,0x407ef000,0x30100000,0x427ef000,0x403e9000};
static unsigned role_flush[11],query_count,provider_checks,visible_calls,ack_calls,starts,contract_calls;static u64 fault_va;static bool contract_fail,start_fail,visible_fail;static unsigned frame;
static int lookup(u32 va,unsigned long*out){query_count++;if(va==fault_va)return -EINVAL;for(unsigned i=0;i<11;i++)if(va>=vas[i]&&va<vas[i]+sizes[i]){*out=s.bo[i].pages[(va-vas[i])/4096]->pfn;return 0;}return -EINVAL;}
static void flush_hook(struct page**p,unsigned n){unsigned role;for(role=0;role<11;role++)if(s.bo[role].pages==p)break;assert(role<11&&n==sizes[role]/4096);role_flush[role]++;if(role==7||role==8){if(s.bo[role].ta_cpu_dirty)memcpy(gpu[role],cpu[role],4096);else if(s.bo[role].ta_gpu_dirty)memcpy(cpu[role],gpu[role],4096);}}
int remesa_stream_sgx535_frozen_validate_publication_pages(const struct sgx535_frozen_bo*b,size_t n,u64 end,const u32*pages,const u32*mapped){contract_calls++;assert(n==11&&end==0xe0000000ULL);for(unsigned i=0;i<11;i++)assert(b[i].role==i&&pages[i]==sizes[i]/4096&&pages[i]==mapped[i]);return contract_fail?-EINVAL:0;}
#include "owner_flush_actual.inc"
static void owner_setup(void){struct sgx535_gma500_owner*o=&op.owner;memset(o,0,sizeof(*o));op.borrowed_images=false;o->initialized=o->device_claimed=o->borrowed_backing=true;o->session.phase=SGX535_PHASE_HOST_IMAGE_FINAL;o->mmu_end=0xe0000000ULL;for(unsigned i=0;i<11;i++){o->objects[i].gem.size=sizes[i];o->objects[i].pages=s.bo[i].pages;o->objects[i].page_count=o->objects[i].mapped_pages=sizes[i]/4096;o->descriptors[i].role=i;op.views[i]=views[i];if(!stream.fast_nohash&&i<=5){o->objects[i].cpu=cpu[i];o->views[i].bytes=cpu[i];o->views[i].length=sizes[i];}}if(stream.fast_nohash)assert(!bind_publisher_images(&stream,&op));}
static int validate(void*p,const void*c,unsigned bytes,const struct remesa_memory_binding*b,unsigned n){(void)p;assert(c&&bytes==264&&b&&n==11);provider_checks++;return remesa_gem_job_check_snapshot_locked(&s,&snapshot,views,11);}
static int start(void*p,struct remesa_memory_ticket t,const void*c,unsigned n,u64*cookie){(void)p;(void)c;assert(n==264&&t.operation==op.ticket.operation);starts++;int e=remesa_stream_owner_flush(&op.owner);if(e)return e;assert(op.owner.user_images_flushed);for(unsigned i=0;i<=5;i++)assert(!op.owner.objects[i].cpu&&!op.owner.views[i].bytes);if(start_fail)return -EIO;for(unsigned i=7;i<=8;i++){assert(!s.ta_cache_domains||(!s.bo[i].ta_cpu_dirty&&!s.bo[i].ta_cpu_cached&&s.bo[i].ta_gpu_dirty));memset(gpu[i],(unsigned char)(frame+i),4096);}*cookie=t.operation;return 0;}
static int poll(void*p,struct remesa_memory_ticket t,u64 cookie,unsigned budget,struct remesa_job_completion*out){(void)p;assert(budget);*out=(struct remesa_job_completion){t,cookie,7};return 0;}
static int visible(void*p,struct remesa_memory_ticket t,u64 cookie){(void)p;assert(t.operation==cookie);visible_calls++;return visible_fail?-EIO:0;}
static int ack(void*p,struct remesa_memory_ticket t,u64 cookie){(void)p;assert(t.operation==cookie);ack_calls++;return 0;}
static const struct remesa_job_executor executor={validate,start,poll,visible,ack};
static void initialize(bool domains){memset(&s,0,sizeof(s));memset(&op,0,sizeof(op));memset(cpu,0,sizeof(cpu));memset(gpu,0,sizeof(gpu));memset(role_flush,0,sizeof(role_flush));s.context=17;s.root.end=0xdfffffff;s.stream_bound=s.ta_pools_sealed=true;s.executor=&executor;mutex_init(&s.lock);for(unsigned i=0;i<11;i++){struct backing*b=&s.bo[i];unsigned pages=sizes[i]/4096;b->live=b->pinned=b->reserved=b->initialized=true;b->generation=1;b->gem.size=sizes[i];b->va.start=vas[i];b->va.end=vas[i]+sizes[i]-1;b->inserted=pages;b->pages=calloc(pages,sizeof(*b->pages));assert(b->pages);for(unsigned j=0;j<pages;j++){b->pages[j]=malloc(sizeof(**b->pages));assert(b->pages[j]);b->pages[j]->bytes=cpu[i];b->pages[j]->pfn=100+i*10000+j;}}test_pfn_hook=lookup;test_flush_hook=flush_hook;stream=(struct trusted_stream){.op=&op,.memory=&s,.claimed=true,.fast_nohash=domains};if(domains)assert(!remesa_gem_stream_enable_ta_domains(&s));}
static void release_pages(void){for(unsigned i=0;i<11;i++){unsigned n=sizes[i]/4096;for(unsigned j=0;j<n;j++)free(s.bo[i].pages[j]);free(s.bo[i].pages);s.bo[i].pages=NULL;}}
static int request(unsigned action,unsigned role,unsigned flags,unsigned char*out){struct remesa_memory_request r={.version=1,.context=17,.action=action,.id=32+role,.flags=flags};if(action==REMESA_MEM_READ||action==REMESA_MEM_WRITE)r.length=4096;int e=act(&s,&r);if(!e&&out&&action==REMESA_MEM_READ)memcpy(out,r.data,4096);return e;}
static void prepare(unsigned seq,bool domains){frame=seq;struct remesa_memory_binding uses[11];for(unsigned i=0;i<11;i++)uses[i]=(struct remesa_memory_binding){32+i,vas[i],sizes[i]};assert(!remesa_gem_job_prepare(&s,uses,11,commands,264,&op.ticket));assert(remesa_gem_job_snapshot(&s,op.ticket,&snapshot,views,11)==11);for(unsigned i=0;i<3;i++){provider_checks++;assert(!remesa_gem_job_check_snapshot(&s,&snapshot,views,11));}if(domains){assert(!remesa_gem_job_ta_policy(&s,op.ticket,seq>3));if(seq<=3){for(unsigned i=7;i<=8;i++){assert(s.bo[i].ta_cpu_dirty&&s.bo[i].ta_cpu_cached);memset(cpu[i],0,4096);}}}owner_setup();}
static void execute_retire(void){assert(!remesa_gem_job_issue(&s,op.ticket));assert(!remesa_gem_job_wait(&s,op.ticket,10));assert(!remesa_gem_job_publish(&s,op.ticket));assert(!remesa_gem_job_acknowledge(&s,op.ticket));assert(!remesa_gem_job_retire(&s,op.ticket));}
static void read_ta(unsigned role){unsigned char first[4096],second[4096];unsigned before=role_flush[role],barrier=test_rmb;assert(s.bo[role].ta_gpu_dirty);assert(!request(REMESA_MEM_LEASE,role,0,NULL));assert(role_flush[role]==before+1&&test_rmb==barrier+1&&!s.bo[role].ta_gpu_dirty&&s.bo[role].ta_cpu_cached);assert(!request(REMESA_MEM_READ,role,0,first)&&!memcmp(first,gpu[role],4096));assert(!request(REMESA_MEM_READ,role,0,second)&&!memcmp(second,first,4096)&&role_flush[role]==before+1);assert(!request(REMESA_MEM_UNLEASE,role,0,NULL));assert(s.bo[role].ta_cpu_cached);assert(!request(REMESA_MEM_SYNC,role,0,NULL));assert(role_flush[role]==before+2&&!s.bo[role].ta_cpu_cached);assert(!request(REMESA_MEM_LEASE,role,0,NULL));assert(!request(REMESA_MEM_READ,role,0,second)&&!memcmp(second,first,4096));assert(!request(REMESA_MEM_UNLEASE,role,0,NULL));assert(s.bo[role].ta_cpu_cached&&role_flush[role]==before+2);}
int main(void){initialize(true);for(unsigned seq=1;seq<=6;seq++){unsigned old7=role_flush[7],old8=role_flush[8];prepare(seq,true);execute_retire();if(seq<=3){assert(role_flush[7]-old7==2&&role_flush[8]-old8==2&&!s.bo[7].ta_gpu_dirty&&!s.bo[8].ta_gpu_dirty);}else{assert(role_flush[7]-old7==(seq==5?1u:0u));assert(role_flush[8]-old8==(seq==6?1u:0u));assert(s.bo[7].ta_gpu_dirty&&s.bo[8].ta_gpu_dirty);}assert(request(REMESA_MEM_LEASE,7,1,NULL)==-EPERM&&request(REMESA_MEM_WRITE,8,0,NULL)==-EPERM);if(seq==4)read_ta(7);if(seq==5)read_ta(8);}assert(provider_checks==24&&visible_calls==6&&ack_calls==6);for(unsigned i=0;i<11;i++)if(i!=7&&i!=8)assert(role_flush[i]==12);s.executor=NULL;unsigned f7=role_flush[7],f8=role_flush[8];assert(!remesa_gem_stream_release(&s));assert(role_flush[7]==f7+1&&role_flush[8]==f8+1&&!s.ta_cache_domains&&!s.bo[7].ta_gpu_dirty);release_pages();
 /* Legacy OPEN1/OPEN7 branch stays full flush, including all TA. */
 for(unsigned mode=1;mode<=7;mode+=6){(void)mode;initialize(false);prepare(1,false);execute_retire();for(unsigned i=0;i<11;i++)assert(role_flush[i]==2);release_pages();}
 /* Existing late PTE failure still prevents any fire/flush. */
 initialize(true);prepare(1,true);fault_va=vas[10]+sizes[10]-4096;unsigned prior=starts;assert(remesa_gem_job_issue(&s,op.ticket)==-EIO&&starts==prior);assert(s.jobs[op.ticket.slot].quarantined&&s.bo[7].holds&&s.bo[7].quarantine);fault_va=0;assert(remesa_gem_stream_release(&s)==-EBUSY);release_pages();
 /* Owner contract failure precedes every maintenance operation. */
 initialize(true);prepare(1,true);contract_fail=true;unsigned before_flush=flushes;assert(remesa_gem_job_issue(&s,op.ticket)==-EINVAL&&flushes==before_flush&&s.bo[7].quarantine&&s.bo[7].ta_gpu_dirty);contract_fail=false;release_pages();
 /* Device visibility failure cannot publish stale CPU bytes or retire. */
 initialize(true);prepare(1,true);assert(!remesa_gem_job_issue(&s,op.ticket));assert(!remesa_gem_job_wait(&s,op.ticket,10));visible_fail=true;assert(remesa_gem_job_publish(&s,op.ticket)==-EIO);assert(s.bo[7].ta_gpu_dirty&&s.bo[7].holds&&s.bo[7].quarantine);assert(request(REMESA_MEM_LEASE,7,0,NULL)==-EBUSY);visible_fail=false;release_pages();
 /* Ambiguous start retains GPU-dirty state and backing quarantine. */
 initialize(true);prepare(1,true);start_fail=true;assert(remesa_gem_job_issue(&s,op.ticket)==-EIO);assert(s.bo[7].ta_gpu_dirty&&s.bo[7].holds&&s.bo[7].quarantine&&remesa_gem_stream_release(&s)==-EBUSY);start_fail=false;release_pages();
 /* Actual attached-job guards prevent release/cancel/lease before consume.
  * Normal detach and retirement remain mandatory after ACK. */
 initialize(true);prepare(1,true);s.executor=NULL;
 assert(!remesa_gem_stream_attach(&s,op.ticket,&executor,&op));
 assert(remesa_gem_job_cancel(&s,op.ticket)==-EBUSY);
 assert(remesa_gem_job_retire(&s,op.ticket)==-EBUSY);
 assert(remesa_gem_stream_detach(&s,op.ticket,&op)==-EBUSY);
 assert(remesa_gem_stream_release(&s)==-EBUSY);
 assert(request(REMESA_MEM_LEASE,5,0,NULL)==-EBUSY);
 for(unsigned i=0;i<11;i++)assert(s.bo[i].holds==1);
 unsigned old_vunmaps=vunmap_calls;
 assert(!remesa_gem_job_issue(&s,op.ticket));assert(!op.borrowed_images);
 for(unsigned i=0;i<6;i++)assert(!op.owner.objects[i].cpu&&op.views[i].cpu==views[i].cpu);
 assert(vunmap_calls==old_vunmaps);
 assert(!remesa_gem_job_wait(&s,op.ticket,10));assert(!remesa_gem_job_publish(&s,op.ticket));assert(!remesa_gem_job_acknowledge(&s,op.ticket));
 assert(!remesa_gem_stream_detach(&s,op.ticket,&op));assert(!remesa_gem_job_retire(&s,op.ticket));
 for(unsigned i=0;i<11;i++)assert(!s.bo[i].holds);
 assert(!remesa_gem_stream_release(&s));release_pages();
 /* Policy API cannot be applied to stale tickets or relabeled pools. */
 initialize(true);prepare(4,true);struct remesa_memory_ticket stale=op.ticket;stale.generation++;assert(remesa_gem_job_ta_policy(&s,stale,true)==-EACCES);assert(remesa_gem_job_ta_policy(&s,op.ticket,true)==-EBUSY);s.bo[8].va.start++;mutex_lock(&s.lock);assert(remesa_gem_job_prefire_cache_locked(&s,op.ticket)==-EIO);mutex_unlock(&s.lock);s.bo[8].va.start--;assert(!remesa_gem_job_cancel(&s,op.ticket));release_pages();
 printf("PASS actual GEM/ownerflush/trustedbridge: 4 providerchecks/job, first3 fullTA, jobs4-6 defer, READ lazy-sync before bytes + next prefire, other9/LAB7 full, end sync, EPERM/PTE/fault quarantine; fake PFN/cache/MMIO data model GPU0 queries=%u\n",query_count);return 0;}
