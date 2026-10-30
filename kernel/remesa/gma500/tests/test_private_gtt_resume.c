/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual GTT init/restore/insert bodies, modeled PCI, resources and MMIO. GPU0. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define PAGE_SHIFT 12
#define PAGE_SIZE 4096UL
#define PAGE_MASK (~(PAGE_SIZE-1))
#define PSB_GMCH_CTRL 1
#define PSB_BSM 2
#define PSB_PGETBL_CTL 3
#define PSB_GTT_RESOURCE 0
#define PSB_GATT_RESOURCE 1
#define _PSB_GMCH_ENABLED 4
#define _PSB_PGETBL_ENABLED 1
#define PSB_MMU_CACHED_MEMORY 1
#define PSB_MMU_RO_MEMORY 2
#define PSB_MMU_WO_MEMORY 4
#define PSB_PTE_VALID 1
#define PSB_PTE_CACHED 2
#define PSB_PTE_RO 4
#define PSB_PTE_WO 8
#define IORESOURCE_MEM 1
#define __iomem
#define dev_dbg(...) ((void)0)
#define dev_err(...) ((void)0)
#define DRM_DEBUG_DRIVER(...) ((void)0)
#define WARN_ON(c) assert(!(c))
#define BUG_ON(c) assert(!(c))
#define container_of(p,t,m) ((t *)((char *)(p)-offsetof(t,m)))
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint64_t u64;
typedef uint64_t resource_size_t;
struct resource {resource_size_t start,end;struct resource *child,*sibling;const char *name;unsigned flags;};
struct page {unsigned long pfn;};
struct psb_gtt {u32 gatt_start,mmu_gatt_start,gtt_start,gtt_phys_start;unsigned gtt_pages,gatt_pages;unsigned long stolen_size,vram_stolen_size;int sem;};
struct drm_psb_private {
 struct psb_gtt gtt;u32 *gtt_map;void *vram_addr;struct resource *gtt_mem;
 struct page *scratch_page;u32 stolen_base,pge_ctl;u16 gmch_ctrl;
 int gtt_initialized,gtt_mutex,mmap_mutex;unsigned long vram_stolen_size;
};
struct pci_dev {struct resource resource[2];};
struct drm_device {struct drm_psb_private *dev_private;struct pci_dev *pdev;void *dev;};
struct gtt_range {struct resource resource;struct page **pages;int npage,roll;bool stolen;};
static struct drm_psb_private private;
static struct pci_dev pci;
static struct drm_device device={&private,&pci,NULL};
static struct page scratch={0x123},backing[2]={{0x234},{0x345}};
static struct page *pages[2]={&backing[0],&backing[1]};
static struct gtt_range range;
static u32 table[4096];
static u32 hardware_pge,hardware_bsm;
static unsigned map_writes,map_reads,unmaps,insert_calls,bad_insert,reg_writes,read_words,read_dwords,write_words;
static int word_read_error,bsm_error,word_write_error,insert_error;
static bool live,locked;
static void mutex_init(int *m){*m=0;}
static void init_rwsem(int *m){*m=0;}
static void mutex_lock(int *m){assert(m==&private.gtt_mutex&&!locked);locked=true;}
static void mutex_unlock(int *m){assert(m==&private.gtt_mutex&&locked);locked=false;}
static unsigned long page_to_pfn(struct page *p){assert(p);return p->pfn;}
static resource_size_t pci_resource_start(struct pci_dev *p,unsigned which)
{assert(p==&pci&&which<2);return p->resource[which].start;}
static resource_size_t pci_resource_len(struct pci_dev *p,unsigned which)
{assert(p==&pci&&which<2);return p->resource[which].end-p->resource[which].start+1;}
static int pcibios_err_to_errno(int err){return err>0?-EIO:err;}
static int pci_read_config_word(struct pci_dev *p,unsigned reg,u16 *v)
{assert(p==&pci&&reg==PSB_GMCH_CTRL);read_words++;*v=0x10;return word_read_error;}
static int pci_write_config_word(struct pci_dev *p,unsigned reg,u16 v)
{assert(p==&pci&&reg==PSB_GMCH_CTRL);(void)v;write_words++;return word_write_error;}
static int pci_read_config_dword(struct pci_dev *p,unsigned reg,u32 *v)
{assert(p==&pci&&reg==PSB_BSM);read_dwords++;*v=hardware_bsm;return bsm_error;}
static u32 read_reg(unsigned reg){assert(reg==PSB_PGETBL_CTL);return hardware_pge;}
#define PSB_RVDC32(r) read_reg(r)
#define PSB_WVDC32(v,r) do{assert((r)==PSB_PGETBL_CTL);(void)(v);reg_writes++;}while(0)
static void *ioremap(unsigned long start,unsigned long size)
{(void)start;assert(size<=sizeof(table));live=true;return table;}
static void *ioremap_wc(unsigned long start,unsigned long size)
{(void)start;(void)size;return &pci;}
static void iounmap(void *p){unmaps++;if(p==table)live=false;}
static void iowrite32(u32 v,u32 *p)
{assert(live&&p>=table&&p<table+4096);*p=v;map_writes++;}
static u32 ioread32(u32 *p)
{assert(live&&p>=table&&p<table+4096);map_reads++;return *p;}
static void set_pages_array_wc(struct page **p,int n){(void)p;(void)n;assert(0);}
int psb_gtt_init(struct drm_device *,int);
static int psb_gtt_insert(struct drm_device *,struct gtt_range *,int);
#include "gtt_actual.inc"
/* A missing mapping is recorded as the failing access it would cause. Do not
 * manufacture a host NULL access to demonstrate the retained driver's bug. */
static int psb_gtt_insert(struct drm_device *d,struct gtt_range *r,int resume)
{
 insert_calls++;
 if(!live||!private.gtt_map){bad_insert++;return -EFAULT;}
 if(insert_error)return insert_error;
 return actual_gtt_insert(d,r,resume);
}
static void fixture(void)
{
 assert(!locked);memset(table,0,sizeof(table));live=true;
 pci=(struct pci_dev){.resource={{.start=0x100000,.end=0x103fff},{.start=0x40000000,.end=0x40ffffff}}};
 range=(struct gtt_range){.resource={.start=0x40010000,.end=0x40011fff},.pages=pages,.npage=2,.roll=1};
 pci.resource[1].child=&range.resource;
 private=(struct drm_psb_private){.gtt={.gatt_start=0x40000000,.mmu_gatt_start=0xe0000000,.gtt_start=0x100000,.gtt_phys_start=0x1100000,.gtt_pages=4,.gatt_pages=4096,.stolen_size=4*PAGE_SIZE},.gtt_map=table,.vram_addr=&pci,.gtt_mem=&pci.resource[1],.scratch_page=&scratch,.stolen_base=0x10fb000,.pge_ctl=0x1100000,.gmch_ctrl=0x20,.gtt_initialized=1,.vram_stolen_size=4*PAGE_SIZE};
 hardware_pge=private.pge_ctl;hardware_bsm=private.stolen_base;
 scratch.pfn=0x123;backing[0].pfn=0x234;pages[0]=&backing[0];
 map_writes=map_reads=unmaps=insert_calls=bad_insert=reg_writes=read_words=read_dwords=write_words=0;
 word_read_error=bsm_error=word_write_error=insert_error=0;
}
static void expect_denial(int error)
{
 struct drm_psb_private before=private;
 assert(psb_gtt_restore(&device)==error);
 assert(!locked&&!unmaps&&!insert_calls&&!map_writes);
 assert(!memcmp(&before,&private,sizeof(private)));
}
int main(int argc,char **argv)
{
 /* Kept actual cold-init dependencies are deliberately available in this model. */
 (void)ioremap;(void)ioremap_wc;(void)mutex_init;(void)init_rwsem;
 (void)pcibios_err_to_errno;
 fixture();pci.resource[0].end+=4*PAGE_SIZE;hardware_bsm-=PAGE_SIZE;
#if CHECKED_GTT
 expect_denial(-EINVAL);
#else
 assert(!psb_gtt_restore(&device));assert(unmaps&&bad_insert==1&&!private.gtt_map);
#endif
 /* The old && accepts either size changing alone and rewrites cached layout. */
 fixture();pci.resource[0].end+=4*PAGE_SIZE;
#if CHECKED_GTT
 expect_denial(-EINVAL);
#else
 assert(!psb_gtt_restore(&device)&&private.gtt.gtt_pages==8&&map_writes);
#endif
 fixture();hardware_bsm-=PAGE_SIZE;
#if CHECKED_GTT
 expect_denial(-EINVAL);
#else
 assert(!psb_gtt_restore(&device)&&private.gtt.stolen_size==5*PAGE_SIZE&&map_writes);
#endif
 fixture();private.vram_addr=NULL;
#if CHECKED_GTT
 expect_denial(-ENODEV);
#else
 assert(!psb_gtt_restore(&device)&&unmaps&&bad_insert==1);
#endif
 fixture();word_write_error=0x88;
#if CHECKED_GTT
 expect_denial(-EIO);
#else
 assert(!psb_gtt_restore(&device)&&map_writes&&insert_calls==1);
#endif
#if CHECKED_GTT
 fixture();word_read_error=0x88;expect_denial(-EIO);assert(!write_words&&!reg_writes);
 fixture();bsm_error=0x88;expect_denial(-EIO);assert(!write_words&&!reg_writes);
 fixture();hardware_pge+=PAGE_SIZE;expect_denial(-EINVAL);
 fixture();pci.resource[0].start+=PAGE_SIZE;pci.resource[0].end+=PAGE_SIZE;expect_denial(-EINVAL);
 fixture();pci.resource[0].start+=0x100000000ULL;pci.resource[0].end+=0x100000000ULL;expect_denial(-EINVAL);
 fixture();pci.resource[1].end-=PAGE_SIZE;expect_denial(-EINVAL);
 fixture();range.roll=-1;expect_denial(-EINVAL);
 fixture();range.npage=0;expect_denial(-EINVAL);
 fixture();range.resource.end+=PAGE_SIZE;expect_denial(-EINVAL);
 fixture();range.resource.start-=1;expect_denial(-EINVAL);
 fixture();range.resource.start=pci.resource[1].end+1;range.resource.end=range.resource.start+2*PAGE_SIZE-1;expect_denial(-EINVAL);
 fixture();pci.resource[1].end+=0x1000000;private.gtt.gatt_pages*=2;
 range.resource.start=pci.resource[1].start+0x1000000;range.resource.end=range.resource.start+2*PAGE_SIZE-1;expect_denial(-EINVAL);
 fixture();backing[0].pfn=0x100000;expect_denial(-ERANGE);
 fixture();pages[0]=NULL;expect_denial(-ERANGE);
 fixture();scratch.pfn=0x100000;expect_denial(-ERANGE);
 fixture();private.gtt_mem=NULL;expect_denial(-ENODEV);
 fixture();private.gtt_map=NULL;expect_denial(-ENODEV);
 fixture();insert_error=-EIO;assert(psb_gtt_restore(&device)==-EIO&&!locked&&!unmaps);
#else
 (void)expect_denial;
#endif
 fixture();assert(!psb_gtt_restore(&device));assert(!unmaps&&!bad_insert&&insert_calls==1&&map_writes==6&&!locked);
 assert(table[0]==((hardware_bsm>>PAGE_SHIFT)<<PAGE_SHIFT|PSB_PTE_VALID|PSB_PTE_CACHED));
 assert(table[16]==(backing[1].pfn<<PAGE_SHIFT|PSB_PTE_VALID|PSB_PTE_CACHED));
#if CHECKED_GTT
 assert(private.gmch_ctrl==0x20&&private.pge_ctl==0x1100000);
#endif
 FILE *out=NULL;
 if(argc==2){out=fopen(argv[1],"wb");assert(out);assert(fwrite(table,1,sizeof(table),out)==sizeof(table));}
 /* Exercise the retained scratch extent too, not just stolen+range writes. */
 fixture();pci.resource[0].end+=4*PAGE_SIZE;private.gtt.gtt_pages=8;
 assert(!psb_gtt_restore(&device)&&map_writes==10&&!unmaps&&!locked);
 assert(table[4]==(scratch.pfn<<PAGE_SHIFT|PSB_PTE_VALID|PSB_PTE_CACHED));
 if(out){assert(fwrite(table,1,sizeof(table),out)==sizeof(table));assert(!fclose(out));}
 fixture();pci.resource[1].child=NULL;
 assert(!psb_gtt_restore(&device)&&!insert_calls&&map_writes==4&&!locked);
 puts(CHECKED_GTT?"PRIVATE_GTT_RESUME_MODEL_PASS GPU0":"NATIVE_GTT_RESUME_COUNTEREXAMPLES_CONFIRMED NOT safety pass GPU0");
 return 0;
}
