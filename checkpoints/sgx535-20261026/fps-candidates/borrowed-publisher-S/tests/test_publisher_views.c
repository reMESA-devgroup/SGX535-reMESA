/* Actual extracted bind/consume/flush/quarantine/close functions; CPU mocks. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
typedef uint32_t u32;typedef uint64_t u64;
#define SGX535_BO_COUNT 11
#define SGX535_BO_COLOR 5
#define SGX535_PHASE_HOST_IMAGE_FINAL 2
#define VM_MAP 1
#define PAGE_KERNEL 2
#define X86_FEATURE_CLFLUSH 1
struct page{unsigned id;};
struct binding{u64 bytes;};
struct remesa_memory_view{struct binding binding;struct page **pages;u32 page_count;void *cpu;};
struct sgx535_gma500_bo{struct{u64 size;}gem;struct page **pages;void *cpu;u32 page_count,mapped_pages;};
struct sgx535_gma500_owner{struct sgx535_gma500_bo objects[11];struct{void *bytes;u64 length;}views[11];bool borrowed_backing,initialized,device_claimed,user_images_flushed;struct{unsigned phase;}session;unsigned descriptors[11];u64 mmu_end;};
struct trusted_operation{struct remesa_memory_view views[11];struct sgx535_gma500_owner owner;bool borrowed_images,completed,visible,acknowledged;struct{u64 operation;}ticket;int observation;};
struct trusted_stream{struct trusted_operation *op;bool fast_nohash,claimed,quarantined,inputs_sealed;void *memory;};
static struct trusted_stream stream;
static struct trusted_operation operation;
static struct page physical_pages[11],*pages[11][1];static unsigned char provider_cpu[11][8],private_cpu[6][8];
static unsigned vmaps,vunmaps,cache_calls,barriers,source_poisoned,job_quarantined,source_released,provider_released;
static int fail_map=-1,prefire_error,publication_error;static bool provider_holds,clflush=true;
static void *vmap(struct page **p,unsigned n,int flags,int protection){assert(provider_holds&&p&&n&&flags==VM_MAP&&protection==PAGE_KERNEL);unsigned i=vmaps++;assert(i<6);return (int)i==fail_map?NULL:private_cpu[i];}
static void vunmap(void *p){assert(p);for(unsigned i=0;i<11;i++)assert(p!=provider_cpu[i]);++vunmaps;}
static bool static_cpu_has(int f){assert(f==X86_FEATURE_CLFLUSH);return clflush;}
static int remesa_stream_sgx535_frozen_validate_publication_pages(const void *d,unsigned n,u64 end,const u32 *counts,const u32 *mapped){(void)d;(void)end;assert(n==11);for(unsigned i=0;i<n;i++)assert(counts[i]&&mapped[i]);return publication_error;}
static int remesa_stream_trusted_prefire_cache(struct sgx535_gma500_owner *o,bool *handled){assert(o==&operation.owner&&handled);++cache_calls;*handled=stream.fast_nohash;if(!provider_holds)return -EACCES;return prefire_error;}
static void drm_clflush_pages(struct page **p,unsigned n){assert(p&&n&&provider_holds);++cache_calls;}
static void dma_wmb(void){assert(provider_holds);++barriers;}
static void remesa_stream_source_poison(void *s){assert(s==&stream);++source_poisoned;}
static void remesa_gem_job_quarantine(void *m,__typeof__(operation.ticket)t){assert(m==stream.memory&&t.operation);++job_quarantined;}
static int remesa_stream_source_release(void *s){assert(s==&stream);++source_released;return 0;}
static int remesa_gem_stream_release(void *m){assert(m==stream.memory);++provider_released;return 0;}
#include "publisher_views_actual.inc"
#include "publisher_flush_actual.inc"
#include "publisher_failure_actual.inc"
static void setup(bool fast){
 memset(&operation,0,sizeof(operation));stream=(struct trusted_stream){.op=&operation,.fast_nohash=fast,.claimed=true,.memory=&physical_pages,.inputs_sealed=true};
 provider_holds=clflush=true;fail_map=-1;prefire_error=publication_error=0;vmaps=vunmaps=cache_calls=barriers=source_poisoned=job_quarantined=source_released=provider_released=0;
 operation.ticket.operation=1;operation.owner.initialized=operation.owner.borrowed_backing=operation.owner.device_claimed=true;operation.owner.session.phase=SGX535_PHASE_HOST_IMAGE_FINAL;
 for(unsigned i=0;i<11;i++){pages[i][0]=&physical_pages[i];operation.views[i]=(struct remesa_memory_view){.binding={4096},.pages=pages[i],.page_count=1,.cpu=provider_cpu[i]};operation.owner.objects[i]=(struct sgx535_gma500_bo){.gem={4096},.pages=pages[i],.page_count=1,.mapped_pages=1};}
}
static void empty_owner(void){for(unsigned i=0;i<6;i++)assert(!operation.owner.objects[i].cpu&&!operation.owner.views[i].bytes&&!operation.owner.views[i].length);}
static void intact_provider(void){for(unsigned i=0;i<11;i++)assert(operation.views[i].cpu==provider_cpu[i]);assert(!vunmaps);}
static void loan_intact(void){for(unsigned i=0;i<6;i++)assert(operation.owner.objects[i].cpu&&operation.owner.views[i].bytes&&operation.owner.views[i].length);assert(operation.borrowed_images);intact_provider();}
int main(void){
 for(unsigned frame=0;frame<120;frame++){
  setup(true);assert(!bind_publisher_images(&stream,&operation));loan_intact();assert(!vmaps);
  assert(!remesa_stream_owner_flush(&operation.owner));empty_owner();intact_provider();assert(!operation.borrowed_images&&operation.owner.user_images_flushed&&cache_calls==1&&barriers==1);
  bool handled=false;assert(remesa_stream_trusted_consume_images(&operation.owner,&handled)==-EACCES&&handled);assert(remesa_stream_owner_flush(&operation.owner)==-EINVAL);intact_provider();
 }
 /* LAB uses exactly its six independent mappings and six frees. */
 setup(false);assert(!bind_publisher_images(&stream,&operation));assert(vmaps==6&&!operation.borrowed_images);for(unsigned i=0;i<6;i++)assert(operation.owner.objects[i].cpu==private_cpu[i]);assert(!remesa_stream_owner_flush(&operation.owner));empty_owner();assert(vunmaps==6&&cache_calls==12&&barriers==1);
 /* LAB allocation failure preserves every already-created alias for quarantine;
  * no freeing is inferred safe on an ambiguous partially prepared operation. */
 for(unsigned fail=0;fail<6;fail++){
  setup(false);fail_map=fail;assert(bind_publisher_images(&stream,&operation)==-ENOMEM);assert(vmaps==fail+1&&!vunmaps&&!operation.borrowed_images);
  for(unsigned i=0;i<6;i++)assert(operation.owner.objects[i].cpu==(i<fail?private_cpu[i]:NULL));
  quarantine(&stream);assert(source_poisoned==1&&job_quarantined==1&&close_stream(&stream)==-EBUSY&&!source_released&&!provider_released&&!vunmaps);
 }
 /* A bad FAST role never exposes even an earlier partial loan. */
 for(unsigned role=0;role<6;role++)for(unsigned kind=0;kind<6;kind++){
  setup(true);switch(kind){case 0:operation.views[role].cpu=NULL;break;case 1:operation.views[role].pages=NULL;break;case 2:operation.owner.objects[role].pages=pages[(role+1)%11];break;case 3:operation.owner.objects[role].page_count=2;break;case 4:operation.owner.objects[role].gem.size=8192;break;case 5:operation.owner.views[role].length=1;break;}
  assert(bind_publisher_images(&stream,&operation)==-EACCES&&!vmaps&&!vunmaps&&!operation.borrowed_images);
  for(unsigned i=0;i<6;i++)assert(!operation.owner.objects[i].cpu&&!operation.owner.views[i].bytes);
 }
 /* Every stale pointer/size is refused transactionally before any revocation. */
 for(unsigned role=0;role<6;role++)for(unsigned kind=0;kind<6;kind++){
  setup(true);assert(!bind_publisher_images(&stream,&operation));switch(kind){case 0:operation.owner.objects[role].cpu=private_cpu[role];break;case 1:operation.owner.views[role].bytes=private_cpu[role];break;case 2:operation.owner.views[role].length=1;break;case 3:operation.owner.objects[role].pages=pages[(role+1)%11];break;case 4:operation.owner.objects[role].page_count=2;break;case 5:operation.owner.objects[role].gem.size=8192;break;}
  bool handled=false;assert(remesa_stream_trusted_consume_images(&operation.owner,&handled)==-EACCES&&handled);loan_intact();
 }
 /* Errors before successful consume retain all provider loans under quarantine;
  * no CPU alias is revoked prematurely and no provider mapping is freed. */
 for(unsigned kind=0;kind<5;kind++){
  setup(true);assert(!bind_publisher_images(&stream,&operation));
  if(kind==0)provider_holds=false;
  if(kind==1)prefire_error=-EIO;
  if(kind==2)publication_error=-EINVAL;
  if(kind==3)clflush=false;
  if(kind==4)operation.owner.views[4].length=1;
  assert(remesa_stream_owner_flush(&operation.owner)<0);loan_intact();quarantine(&stream);assert(close_stream(&stream)==-EBUSY&&!source_released&&!provider_released);loan_intact();
 }
 /* An abort before prefire cannot release lender backing; neither may a
  * hypothetical late completed operation with an unconsumed loan. */
 setup(true);assert(!bind_publisher_images(&stream,&operation));assert(close_stream(&stream)==-EBUSY);quarantine(&stream);bool handled=false;assert(remesa_stream_trusted_consume_images(&operation.owner,&handled)==-EACCES);loan_intact();assert(!source_released&&!provider_released);
 setup(true);assert(!bind_publisher_images(&stream,&operation));operation.completed=true;operation.owner.session.phase=9;assert(remesa_stream_trusted_consume_images(&operation.owner,&handled)==-EACCES);assert(close_stream(&stream)==-EBUSY);loan_intact();
 setup(true);assert(!bind_publisher_images(&stream,&operation));struct sgx535_gma500_owner foreign=operation.owner;assert(remesa_stream_trusted_consume_images(&foreign,&handled)==-EACCES);loan_intact();assert(remesa_stream_trusted_consume_images(&operation.owner,NULL)==-EINVAL);loan_intact();
 /* Only the normal caller may finish/detach/retire provider holds after consume;
  * this test models that terminal state without claiming GPU completion. */
 setup(true);assert(!bind_publisher_images(&stream,&operation));assert(!remesa_stream_owner_flush(&operation.owner));empty_owner();stream.op=NULL;assert(!close_stream(&stream));assert(source_released==1&&provider_released==1&&!stream.claimed&&!stream.memory&&!stream.inputs_sealed);intact_provider();
 puts("PASS actual borrowed publisher helpers/ownerflush/quarantine/close:120 FAST zero vmap/vunmap, LAB6maps/6frees,6partial failures,36bind/36consume mismatches,holds/cache/CLFLUSH/publication failures,abort/latecompleted/foreign/null/doubleconsume,heldprovider untilnormalclose; CPU mocks GPU0");return 0;
}
