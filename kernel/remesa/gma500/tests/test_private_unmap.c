/* SPDX-License-Identifier: GPL-2.0-only */
/* Exact staged unmap, modeled failed-removal/PTE APIs. No GEM or GPU. */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
typedef uint64_t u64;
#define PAGE_SHIFT 12
struct page {unsigned long pfn;};
struct resource {u64 start;};
struct backing {bool lease,quarantine,reserved;unsigned holds,inserted,color_vmas;
 struct resource va;struct page **pages;};
struct remesa_gem_memory {void *pd;};
static unsigned releases,removes,mode;
static int atomic_read(unsigned *n){return *n;}
static void psb_mmu_remove_pages(void *pd,u64 va,unsigned count,unsigned a,unsigned b)
{(void)pd;(void)va;(void)a;(void)b;assert(count==1);removes++;}
static int psb_mmu_virtual_to_pfn(void *pd,u64 va,unsigned long *pfn)
{(void)pd;(void)va;*pfn=mode==1?0x222:0x333;return mode==0?-EINVAL:mode==3?-EIO:0;}
static void release_resource(struct resource *r){(void)r;releases++;}
#include "private_unmap_actual.inc"
int main(void)
{
 for(mode=0;mode<4;mode++){
  struct remesa_gem_memory s={0};struct page original={0x222};struct page *pages[]={&original};
  struct backing b={.inserted=1,.reserved=true,.va={0x40000000},.pages=pages};
  removes=releases=0;int error=unmap(&s,&b);assert(removes==1);
  if(!mode)assert(!error&&!b.inserted&&!b.reserved&&!b.quarantine&&releases==1);
  else assert(error==-EIO&&b.inserted==1&&b.reserved&&b.quarantine&&!releases);
 }
 puts("PASS staged unmap: absent mapping releases; original/foreign PTE or lookup error retain VA/backing; modeled CPU APIs, GPU0");
}
