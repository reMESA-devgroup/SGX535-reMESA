/* SPDX-License-Identifier: MIT
 * HOST TEST ONLY: real kernel implementation is compiled against injected API
 * faults. These allocations/PTEs are not Linux GEM objects or device mappings. */
#ifndef REMESA_GEM_SHIM_H
#define REMESA_GEM_SHIM_H
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
typedef uint64_t u64;typedef uint32_t u32;typedef uint8_t u8;
typedef struct {int value;} atomic_t;
static inline int atomic_read(const atomic_t*p){return p->value;}
static inline void atomic_set(atomic_t*p,int v){p->value=v;}
struct address_space {int unused;};
static unsigned color_revocations;
static inline void unmap_mapping_range(struct address_space*m,u64 offset,u64 bytes,int cows){assert(m&&offset==0x08000000ULL&&bytes==4096000&&cows==1);++color_revocations;}

#define PAGE_SIZE 4096u
#define PAGE_SHIFT 12
#define CAP_SYS_ADMIN 0
#define GFP_KERNEL 0
#define VM_MAP 0
#define PAGE_KERNEL 0
#define X86_FEATURE_CLFLUSH 0
#define PSB_MMU_CACHED_MEMORY 0
#define max_t(t,a,b) ((t)(a)>(t)(b)?(t)(a):(t)(b))
#define min_t(t,a,b) ((t)(a)<(t)(b)?(t)(a):(t)(b))
#define IS_ERR(p) ((intptr_t)(p)<0 && (intptr_t)(p)>-4096)
#define PTR_ERR(p) ((int)(intptr_t)(p))
struct mutex {int held;};
#define DEFINE_MUTEX(name) struct mutex name={0}
static void mutex_init(struct mutex *m){m->held=0;}
static void mutex_lock(struct mutex *m){assert(!m->held);m->held=1;}
static void mutex_unlock(struct mutex *m){assert(m->held);m->held=0;}
struct page {unsigned char *bytes;unsigned long pfn;};
struct drm_gem_object {size_t size;unsigned char *bytes;unsigned long pfn;};
struct psb_mmu_pd {int hw_context;};
struct test_mmu {bool has_clflush;struct psb_mmu_pd pd;};
struct drm_psb_private {struct test_mmu *mmu;void *sgx_reg;struct {u32 gatt_start,gatt_pages,mmu_gatt_start;} gtt;};
struct drm_device {struct drm_psb_private *dev_private;};struct drm_file {int n;};
struct resource {u64 start,end;const char *name;struct resource *child,*sibling;};
static bool admin=true;
static bool capable(int cap){(void)cap;return admin;}
static int fail_init,fail_pages,fail_insert,fail_vmap,fail_alloc,power,claimed;
static unsigned gems,pins,ptes,inserts,flushes,device_refs,module_refs;
static unsigned vmap_calls,vunmap_calls,fail_vmap_call;
static unsigned long next_pfn=100;
#define THIS_MODULE NULL
#define module_param_named(a,b,c,d)
static void __module_get(void *m){(void)m;++module_refs;}
static void module_put(void *m){(void)m;assert(module_refs);--module_refs;}
static void drm_dev_get(struct drm_device *d){(void)d;++device_refs;}
static void drm_dev_put(struct drm_device *d){(void)d;assert(device_refs);--device_refs;}
static unsigned long page_to_pfn(struct page *p){return p->pfn;}
static struct {u64 va;unsigned long pfn;bool live;} test_pt[4096];
static void *kzalloc(size_t n,int flags){(void)flags;if(fail_alloc)return NULL;return calloc(1,n);}
static void *kmemdup(const void *p,size_t n,int flags){void *copy=kzalloc(n,flags);if(copy)memcpy(copy,p,n);return copy;}
static void kfree(void *p){free(p);}
static bool static_cpu_has(int v){(void)v;return true;}
static bool gma_power_begin(struct drm_device *d,bool wake){(void)d;assert(!wake);++power;return true;}
static void gma_power_end(struct drm_device *d){(void)d;assert(power>0);--power;}
static int sgx535_gma500_exclusive_claim(struct drm_device *d){(void)d;if(claimed)return -EBUSY;claimed=1;return 0;}
static void sgx535_gma500_exclusive_release(struct drm_device *d){(void)d;assert(claimed);claimed=0;}
static struct psb_mmu_pd *psb_mmu_get_default_pd(struct test_mmu *m){return &m->pd;}
static int drm_gem_object_init(struct drm_device *d,struct drm_gem_object *g,size_t n)
{(void)d;if(fail_init)return -ENOMEM;g->bytes=calloc(1,n);if(!g->bytes)return -ENOMEM;g->size=n;g->pfn=next_pfn;next_pfn+=n/PAGE_SIZE;++gems;return 0;}
static void drm_gem_object_release(struct drm_gem_object *g){assert(gems);--gems;free(g->bytes);g->bytes=NULL;}
static struct page **drm_gem_get_pages(struct drm_gem_object *g)
{unsigned n=g->size/PAGE_SIZE;if(fail_pages)return (void *)(intptr_t)-ENOMEM;struct page **p=calloc(n,sizeof(*p));assert(p);for(unsigned i=0;i<n;i++){p[i]=malloc(sizeof(**p));assert(p[i]);p[i]->bytes=g->bytes+i*PAGE_SIZE;p[i]->pfn=g->pfn+i;}++pins;return p;}
static void drm_gem_put_pages(struct drm_gem_object *g,struct page **p,bool dirty,bool accessed)
{(void)dirty;(void)accessed;assert(pins);--pins;for(unsigned i=0;i<g->size/PAGE_SIZE;i++)free(p[i]);free(p);}
static void clear_highpage(struct page *p){memset(p->bytes,0,PAGE_SIZE);}
static void *vmap(struct page **p,unsigned n,int flags,int protection){(void)n;(void)flags;(void)protection;++vmap_calls;return fail_vmap||(fail_vmap_call&&vmap_calls==fail_vmap_call)?NULL:p[0]->bytes;}
static void vunmap(void *p){assert(p);++vunmap_calls;}
static struct resource *test_root;
static int request_resource(struct resource *r,struct resource *b)
{for(struct resource *p=r->child;p;p=p->sibling)if(b->start<=p->end && p->start<=b->end)return -EBUSY;b->sibling=r->child;r->child=b;test_root=r;return 0;}
static void release_resource(struct resource *b)
{struct resource **p=&test_root->child;while(*p && *p!=b)p=&(*p)->sibling;assert(*p==b);*p=b->sibling;b->sibling=NULL;}
static int psb_mmu_insert_pages(struct psb_mmu_pd *pd,struct page **p,u64 va,unsigned n,int a,int b,int flags)
{(void)pd;(void)p;(void)va;(void)a;(void)b;(void)flags;assert(n==1);++inserts;if(fail_insert && inserts==(unsigned)fail_insert)return -ENOMEM;for(unsigned i=0;i<4096;i++)if(!test_pt[i].live){test_pt[i].va=va;test_pt[i].pfn=page_to_pfn(*p);test_pt[i].live=true;++ptes;return 0;}return -ENOMEM;}
static void psb_mmu_remove_pages(struct psb_mmu_pd *pd,u64 va,unsigned n,int a,int b)
{(void)pd;(void)va;(void)a;(void)b;assert(ptes>=n);for(unsigned j=0;j<n;j++)for(unsigned i=0;i<4096;i++)if(test_pt[i].live && test_pt[i].va==va+(u64)j*PAGE_SIZE){test_pt[i].live=false;--ptes;break;}}
static int (*test_pfn_hook)(u32,unsigned long*);
static int psb_mmu_virtual_to_pfn(struct psb_mmu_pd *pd,u32 va,unsigned long *pfn)
{(void)pd;if(test_pfn_hook)return test_pfn_hook(va,pfn);for(unsigned i=0;i<4096;i++)if(test_pt[i].live && test_pt[i].va==va){*pfn=test_pt[i].pfn;return 0;}return -EINVAL;}
/* Provider integration shim; the actual batched implementation is tested
 * separately against the original mmu function with a real PT-shaped model. */
static unsigned batch_verify_calls;
static int psb_mmu_verify_pages(struct psb_mmu_pd *pd,u64 address,struct page **pages,u32 count){
 unsigned i;++batch_verify_calls;
 if(!pd||!pages||!count||address>0xffffffffULL||(address&(PAGE_SIZE-1))||
    (u64)count>((0x100000000ULL-address)>>PAGE_SHIFT))return -EINVAL;
 for(i=0;i<count;i++){
  unsigned long pfn;
  if(!pages[i]||psb_mmu_virtual_to_pfn(pd,(u32)(address+((u64)i<<PAGE_SHIFT)),&pfn)||pfn!=page_to_pfn(pages[i]))return -EINVAL;
 }
 return 0;
}
static void (*test_flush_hook)(struct page**,unsigned);
static unsigned test_wmb,test_rmb;
static void drm_clflush_pages(struct page **p,unsigned n){assert(p && n);++flushes;if(test_flush_hook)test_flush_hook(p,n);}
static void dma_wmb(void){test_wmb++;}
static void dma_rmb(void){test_rmb++;}
#endif
