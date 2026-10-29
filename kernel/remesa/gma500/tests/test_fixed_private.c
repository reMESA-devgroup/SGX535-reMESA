/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual private adapter + extracted backend cases; kernel/MMIO callbacks modeled. */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t u32;typedef uint64_t u64;
#include "frozen_fixed_service.h"
#include "../remesa_gem_private.h"
#include "../remesa_twod_reservation.h"
struct page {unsigned n;};
struct psb_mmu_pd {int hw_context;};
struct psb_mmu_driver {struct psb_mmu_pd *pd;};
struct drm_psb_private {struct drm_device *dev;struct psb_mmu_driver *mmu;};
struct drm_device {struct drm_psb_private *dev_private;};
struct sgx535_gma500_bo {
 struct {u64 size;} gem;struct page **pages;void *cpu;
 struct {u64 start,end;} va;u32 page_count,mapped_pages;bool gem_live,va_reserved;
};
struct sgx535_gma500_owner {
 struct drm_device *dev;bool borrowed_backing;
 struct sgx535_frozen_bo descriptors[SGX535_BO_COUNT];
 struct sgx535_gma500_bo objects[SGX535_BO_COUNT];
};
struct sgx535_gma500_fixed_backend {
 struct sgx535_gma500_owner *owner;bool active;
 struct remesa_gem_private *private_loan;
 struct remesa_memory_view private_views[SGX535_BO_COUNT];u64 cpu_flush_ns;
};
static unsigned checks,validated,published,maintained,guarded,default_gets,legacy_flushes,legacy_uploads;
static int validate_error,publish_error,maintain_error,begin_error;
static struct remesa_gem_private *expected_loan;
static struct remesa_memory_view expected_views[SGX535_BO_COUNT];
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"failure line%d: %s\n",__LINE__,#x);assert(x);}}while(0)
int remesa_gem_private_validate_locked(struct remesa_gem_private *l,const struct remesa_memory_view *v,unsigned n)
{
 CHECK(l==expected_loan&&n==SGX535_BO_COUNT);validated++;
 for(unsigned i=0;i<n;i++)CHECK(!memcmp(&v[i],&expected_views[i],sizeof(v[i])));
 return validate_error;
}
int remesa_gem_private_publish_locked(struct remesa_gem_private *l,const struct remesa_memory_view *v,unsigned n)
{
 CHECK(!remesa_gem_private_validate_locked(l,v,n));published++;
 if(publish_error)return publish_error;
 l->host_published=true;return 0;
}
int remesa_mmu_private_maintain_locked(struct remesa_mmu_private *o)
{CHECK(o==expected_loan->owner);maintained++;return maintain_error;}
int sgx535_gma500_fixed_backend_begin_guarded(struct sgx535_gma500_fixed_backend *b,
 struct sgx535_gma500_owner *o,struct remesa_twod_reservation *g,const void *token)
{
 CHECK(g==expected_loan->guard&&token==expected_loan);guarded++;
 if(begin_error)return begin_error;
 b->active=true;b->owner=o;return 0;
}
#include "../remesa_fixed_private.inc"
static u64 ktime_get_ns(void){static u64 n;return ++n;}
static struct psb_mmu_pd *psb_mmu_get_default_pd(struct psb_mmu_driver *m)
{default_gets++;return m->pd;}
static void psb_mmu_flush(struct psb_mmu_driver *m){CHECK(m);legacy_flushes++;}
static int remesa_stream_source_owned(struct sgx535_gma500_fixed_backend *b){(void)b;return 0;}
static int remesa_stream_owner_flush(struct sgx535_gma500_owner *o){CHECK(o);legacy_uploads++;return 0;}
static int sgx535_gma500_owner_flush_user_images(struct sgx535_gma500_owner *o){return remesa_stream_owner_flush(o);}
/* Verbatim first three switch arms from the current staged fixed_run. */
static int actual_cases(struct sgx535_gma500_fixed_backend *backend,
 enum sgx535_fixed_stage stage,const void *payload,size_t count)
{
 struct sgx535_gma500_owner *owner=backend->owner;
 struct drm_psb_private *dev_priv=owner->dev->dev_private;
 int ret=-EINVAL,role;u64 timing_mark;
 switch(stage){
#include "fixed_private_cases_actual.inc"
 default:break;
 }
 return ret;
}
int main(void)
{
 struct psb_mmu_pd pd={0};struct psb_mmu_driver mmu={&pd};
 struct drm_device dev;struct drm_psb_private priv={&dev,&mmu};dev.dev_private=&priv;
 struct remesa_twod_reservation guard={.device=&priv,.held=true};
 struct remesa_gem_private loan={.guard=&guard,.owner=(void*)1,.live=true};
 struct sgx535_gma500_owner owner={.dev=&dev,.borrowed_backing=true};
 struct remesa_memory_view views[SGX535_BO_COUNT]={0};struct page pages[SGX535_BO_COUNT];
 struct page *page_arrays[SGX535_BO_COUNT];unsigned char cpu[SGX535_BO_COUNT];
 struct sgx535_gma500_fixed_backend backend={0};expected_loan=&loan;
 for(unsigned i=0;i<SGX535_BO_COUNT;i++){
  u64 va=0x40000000u+i*0x10000u;page_arrays[i]=&pages[i];
  views[i]=(struct remesa_memory_view){{32+i,va,4096},&page_arrays[i],1,&cpu[i]};
  loan.rules[i]=(struct remesa_gem_private_rule){32+i,i?3u:0u,0};
  owner.descriptors[i]=(struct sgx535_frozen_bo){i,i?SGX535_DOMAIN_MMU:SGX535_DOMAIN_LOCAL,4096,i?va:0,32+i};
  owner.objects[i]=(struct sgx535_gma500_bo){.gem={4096},.pages=&page_arrays[i],.page_count=1};
  if(i){owner.objects[i].va.start=va;owner.objects[i].va.end=va+4095;
   owner.objects[i].va_reserved=true;owner.objects[i].mapped_pages=1;}
 }
 memcpy(expected_views,views,sizeof views);
 for(unsigned fault=0;fault<18;fault++){
  struct sgx535_gma500_owner bad=owner;unsigned n=SGX535_BO_COUNT;
  unsigned before=validated,g=guarded;
  switch(fault){
   case 0:bad.borrowed_backing=false;break;
   case 1:n--;break;
   case 2:bad.descriptors[2].owner_token++;break;
   case 3:bad.descriptors[2].size--;break;
   case 4:bad.objects[2].pages=&page_arrays[1];break;
   case 5:bad.objects[2].cpu=&cpu[2];break;
   case 6:bad.objects[2].gem_live=true;break;
   case 7:bad.objects[2].mapped_pages=0;break;
   case 8:bad.descriptors[0].gpu_va=1;break;
   case 9:bad.objects[0].va_reserved=true;break;
   case 10:bad.descriptors[2].gpu_va++;break;
   case 11:bad.objects[2].va.end++;break;
   case 12:bad.descriptors[2].role--;break;
   case 13:loan.rules[0].access=1;break;
   case 14:views[2].cpu=NULL;break;
   case 15:loan.rules[2].access=0;break;
   case 16:bad.descriptors[2].domain=SGX535_DOMAIN_MMU+1;break;
   case 17:dev.dev_private=NULL;guard.device=NULL;break;
  }
  CHECK(sgx535_gma500_fixed_backend_begin_private(&backend,&bad,&loan,views,n)==-EACCES);
  CHECK(validated==before&&guarded==g&&!backend.active&&!backend.private_loan);
  loan.rules[0].access=0;loan.rules[2].access=3;
  views[2].cpu=&cpu[2];dev.dev_private=&priv;guard.device=&priv;
 }
 validate_error=-ESTALE;CHECK(sgx535_gma500_fixed_backend_begin_private(&backend,&owner,&loan,views,SGX535_BO_COUNT)==-ESTALE);
 CHECK(!backend.private_loan&&!backend.active);validate_error=0;
 begin_error=-EAGAIN;CHECK(sgx535_gma500_fixed_backend_begin_private(&backend,&owner,&loan,views,SGX535_BO_COUNT)==-EAGAIN);
 CHECK(!backend.private_loan&&!backend.active);begin_error=0;
 loan.host_published=true;
 CHECK(sgx535_gma500_fixed_backend_begin_private(&backend,&owner,&loan,views,SGX535_BO_COUNT)==-EALREADY);
 CHECK(!backend.active&&!backend.private_loan);loan.host_published=false;
 CHECK(!sgx535_gma500_fixed_backend_begin_private(&backend,&owner,&loan,views,SGX535_BO_COUNT));
 CHECK(backend.private_loan==&loan&&backend.active&&!memcmp(backend.private_views,views,sizeof views));
 views[2].binding.va++; /* Caller metadata is no longer authoritative. */
 CHECK(actual_cases(&backend,SGX535_FIXED_PUBLISH_TRANSLATIONS,NULL,0)==-ENODATA);
 CHECK(actual_cases(&backend,SGX535_FIXED_DEVICE_MAINTAIN,NULL,0)==-ENODATA&&!maintained);
 for(unsigned stage=1;stage<=3;stage++){
  CHECK(actual_cases(&backend,stage,&cpu,0)==-EINVAL);
  CHECK(actual_cases(&backend,stage,NULL,1)==-EINVAL);
 }
 publish_error=-EIO;CHECK(actual_cases(&backend,SGX535_FIXED_PUBLISH_CPU,NULL,0)==-EIO&&!loan.host_published);publish_error=0;
 CHECK(!actual_cases(&backend,SGX535_FIXED_PUBLISH_CPU,NULL,0)&&loan.host_published);
 CHECK(!actual_cases(&backend,SGX535_FIXED_PUBLISH_TRANSLATIONS,NULL,0));
 maintain_error=-EIO;CHECK(actual_cases(&backend,SGX535_FIXED_DEVICE_MAINTAIN,NULL,0)==-EIO);maintain_error=0;
 CHECK(!actual_cases(&backend,SGX535_FIXED_DEVICE_MAINTAIN,NULL,0));
 CHECK(!default_gets&&!legacy_flushes&&!legacy_uploads&&published==2&&maintained==2);
 validate_error=-EIO;unsigned old=maintained;
 CHECK(actual_cases(&backend,SGX535_FIXED_DEVICE_MAINTAIN,NULL,0)==-EIO&&maintained==old);validate_error=0;
 backend.private_loan=NULL; /* Model normal path independently; no pretend hardware teardown. */
 CHECK(!actual_cases(&backend,SGX535_FIXED_PUBLISH_CPU,NULL,0));
 CHECK(!actual_cases(&backend,SGX535_FIXED_PUBLISH_TRANSLATIONS,NULL,0));
 CHECK(!actual_cases(&backend,SGX535_FIXED_DEVICE_MAINTAIN,NULL,0));
 CHECK(default_gets==2&&legacy_flushes==1&&legacy_uploads==1);
 printf("PASS %u private backend adapter checks; publication/MMU/IRQ callbacks modeled; no GPU\n",checks);
 return 0;
}
