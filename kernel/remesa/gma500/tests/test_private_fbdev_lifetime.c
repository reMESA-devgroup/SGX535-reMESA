/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual private fb mapping/ops/detach and original destroy/config bodies.
 * Reduced metadata, modeled fb/VFS/GEM/PFN/kernel APIs; pthread entry join.
 * This does not execute a kernel fb file or map physical memory. */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
typedef uint64_t u64;typedef unsigned vm_fault_t;
#define PAGE_SHIFT 12u
#define PAGE_SIZE 4096u
#define VM_FAULT_SIGBUS 2u
#define VM_FAULT_ERROR 2u
#define unlikely(value) (value)
#define PFN_DEV 1u
#define VM_IO 1u
#define VM_MIXEDMAP 2u
#define VM_DONTEXPAND 4u
#define VM_DONTDUMP 8u
#define container_of(ptr,type,member) ((type *)((char *)(ptr)-offsetof(type,member)))
struct drm_device;struct drm_fb_helper;struct fb_info;struct vm_fault;struct vm_area_struct;
struct device {int id;};struct pci_dev {struct device dev;};
struct drm_psb_private {uint32_t stolen_base;unsigned long vram_stolen_size;
 struct drm_fb_helper *fb_helper;bool remesa_private_fbdev_unregistered,rpm_enabled;};
struct drm_device {bool unplugged;struct drm_psb_private *dev_private;struct pci_dev *pdev;};
struct drm_gem_object {struct drm_device *dev;u64 size;unsigned refs;};
struct gtt_range {struct drm_gem_object gem;unsigned offset;bool stolen,private_tracked;};
#define to_gtt_range(obj) container_of(obj,struct gtt_range,gem)
struct drm_framebuffer {struct drm_device *dev;struct drm_gem_object *obj[1];unsigned refs;};
struct work {int id;};
struct drm_fb_helper {struct drm_device *dev;struct drm_framebuffer *fb;struct fb_info *fbdev;
 struct work resume_work,dirty_work;};
struct fb_info {void *par;unsigned count;};
struct vm_operations_struct {vm_fault_t (*fault)(struct vm_fault *);
 void (*open)(struct vm_area_struct *),(*close)(struct vm_area_struct *);};
struct vm_area_struct {unsigned long vm_start,vm_end,vm_pgoff,vm_page_prot,vm_flags;
 void *vm_private_data;const struct vm_operations_struct *vm_ops;};
struct vm_fault {struct vm_area_struct *vma;unsigned long address,pgoff;};
struct fb_var_screeninfo {int dummy;};struct fb_cmap {int dummy;};
struct fb_fillrect {int dummy;};struct fb_copyarea {int dummy;};struct fb_image {int dummy;};
struct drm_crtc {struct drm_device *dev;};
struct drm_file {int dummy;};
struct drm_mode_set {struct drm_crtc *crtc;};struct drm_modeset_acquire_ctx {int dummy;};
static pthread_mutex_t state=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static _Thread_local unsigned entered;
static unsigned readers,calls,inserts,gem_gets,gem_puts,orphans,cancels,unregisters;
static unsigned helper_finis,info_frees,fb_frees,helper_frees,fb_cleanups,config_calls,power_calls;
static unsigned cursor_calls;
static unsigned long last_pfn,last_address;
static bool block_call,call_started,resume_call,remove_started,remove_done;
static int live_result;
static int initial_error,helper_init_error;static bool initial_published;
static unsigned error_unplugs;
static struct drm_psb_private private;
static struct drm_device device,other;
static struct pci_dev pci;
static struct gtt_range range;
static struct drm_framebuffer fb;
static struct drm_fb_helper helper;
static struct fb_info info;
static bool drm_dev_enter(struct drm_device *dev,int *index)
{
 assert(!pthread_mutex_lock(&state));bool ok=!dev->unplugged;
 if(ok){readers++;entered++;*index=43;}
 assert(!pthread_mutex_unlock(&state));return ok;
}
static void drm_dev_exit(int index)
{
 assert(index==43&&entered);assert(!pthread_mutex_lock(&state));
 entered--;assert(readers);readers--;assert(!pthread_cond_broadcast(&changed));
 assert(!pthread_mutex_unlock(&state));
}
static void drm_gem_object_get(struct drm_gem_object *obj)
{assert(obj==&range.gem&&obj->refs);obj->refs++;gem_gets++;}
static void drm_gem_object_put(struct drm_gem_object *obj)
{assert(obj==&range.gem&&obj->refs);obj->refs--;gem_puts++;if(!obj->refs&&obj->dev->unplugged)orphans++;}
static unsigned long pgprot_noncached(unsigned long p){return p|16;}
static unsigned long __pfn_to_pfn_t(unsigned long pfn,unsigned flags)
{assert(flags==PFN_DEV);return pfn;}
static vm_fault_t vmf_insert_mixed(struct vm_area_struct *vma,unsigned long address,unsigned long pfn)
{assert(entered&&address>=vma->vm_start&&address<vma->vm_end);inserts++;last_pfn=pfn;last_address=address;return 0;}
static int vma_pages(struct vm_area_struct *vma){return (vma->vm_end-vma->vm_start)>>PAGE_SHIFT;}
#include "baseline_fb_fault.inc"
static const struct vm_operations_struct baseline_fb_vm_ops={.fault=baseline_fb_fault};
#include "baseline_fb_mmap.inc"
#include "remesa_private_fbdev_map.inc"
static int callback(void)
{
 assert(entered);assert(!pthread_mutex_lock(&state));calls++;call_started=true;
 assert(!pthread_cond_broadcast(&changed));
 while(block_call&&!resume_call)assert(!pthread_cond_wait(&changed,&state));
 assert(!pthread_mutex_unlock(&state));return live_result;
}
static int drm_fb_helper_check_var(struct fb_var_screeninfo *v,struct fb_info *i){(void)v;(void)i;return callback();}
static int drm_fb_helper_set_par(struct fb_info *i){(void)i;return callback();}
static int drm_fb_helper_setcmap(struct fb_cmap *m,struct fb_info *i){(void)m;(void)i;return callback();}
static int drm_fb_helper_blank(int blank,struct fb_info *i){(void)blank;(void)i;return callback();}
static int drm_fb_helper_pan_display(struct fb_var_screeninfo *v,struct fb_info *i){(void)v;(void)i;return callback();}
static int psbfb_pan(struct fb_var_screeninfo *v,struct fb_info *i){(void)v;(void)i;return callback();}
static int drm_fb_helper_debug_enter(struct fb_info *i){(void)i;return callback();}
static int drm_fb_helper_debug_leave(struct fb_info *i){(void)i;return callback();}
static int drm_fb_helper_ioctl(struct fb_info *i,unsigned cmd,unsigned long arg){(void)i;(void)cmd;(void)arg;return callback();}
static int psbfb_setcolreg(unsigned r,unsigned red,unsigned green,unsigned blue,unsigned t,struct fb_info *i)
{(void)r;(void)red;(void)green;(void)blue;(void)t;(void)i;return callback();}
static int psbfb_sync(struct fb_info *i){(void)i;return callback();}
static void drm_fb_helper_cfb_fillrect(struct fb_info *i,const struct fb_fillrect *d){(void)i;(void)d;(void)callback();}
static void drm_fb_helper_cfb_copyarea(struct fb_info *i,const struct fb_copyarea *d){(void)i;(void)d;(void)callback();}
static void psbfb_copyarea(struct fb_info *i,const struct fb_copyarea *d){(void)i;(void)d;(void)callback();}
static void drm_fb_helper_cfb_imageblit(struct fb_info *i,const struct fb_image *d){(void)i;(void)d;(void)callback();}
#include "remesa_private_fbdev_ops.inc"
static void cancel_work_sync(struct work *w)
{assert(!entered&&(w==&helper.resume_work||w==&helper.dirty_work));cancels++;}
static void drm_fb_helper_unregister_fbi(struct drm_fb_helper *h)
{assert(h==&helper&&info.count&&!entered);unregisters++;if(!--info.count)remesa_private_fb_destroy_info(&info);}
static void drm_fb_helper_fini(struct drm_fb_helper *h)
{assert(h==&helper&&!entered);helper_finis++;if(h->fbdev)info_frees++;h->fbdev=NULL;}
static void kfree(void *p)
{if(p==&fb)fb_frees++;else {assert(p==&helper);helper_frees++;}}
static void drm_framebuffer_put(struct drm_framebuffer *f)
{
 assert(f==&fb&&f->refs);if(--f->refs)return;
 /* Modeled drm_gem_fb_destroy: the real constructor already selects it. */
 drm_gem_object_put(f->obj[0]);fb_cleanups++;kfree(f);
}
#include "fb_destroy_actual.inc"
#include "remesa_private_fbdev_detach.inc"
#define GFP_KERNEL 1
#define dev_err(...) ((void)0)
static const int psb_fb_helper_funcs=0;
static int atomic_read(unsigned *value){return (int)*value;}
static void *kzalloc(size_t size,int flags)
{assert(size==sizeof(helper)&&flags==GFP_KERNEL);memset(&helper,0,sizeof helper);return &helper;}
static void drm_fb_helper_prepare(struct drm_device *dev,struct drm_fb_helper *h,const int *funcs)
{assert(h==&helper&&funcs==&psb_fb_helper_funcs);h->dev=dev;}
static int drm_fb_helper_init(struct drm_device *d,struct drm_fb_helper *h)
{assert(d==&device&&h==&helper);return helper_init_error;}
static void drm_helper_disable_unused_functions(struct drm_device *d){assert(d==&device&&!d->unplugged);}
static int drm_fb_helper_initial_config(struct drm_fb_helper *h,int bpp)
{assert(h==&helper&&bpp==32);h->fbdev=&info;h->fb=&fb;info.par=h;info.count=initial_published?2:0;return initial_error;}
static void drm_dev_unplug(struct drm_device *dev)
{assert(!dev->unplugged&&!readers);dev->unplugged=true;error_unplugs++;}
#include "fb_init_actual.inc"
static int drm_crtc_helper_set_config(struct drm_mode_set *set,struct drm_modeset_acquire_ctx *ctx)
{(void)ctx;assert(set->crtc->dev==&device&&entered);config_calls++;return live_result;}
static void pm_runtime_forbid(struct device *d){assert(d==&pci.dev&&entered);power_calls++;}
static void pm_runtime_allow(struct device *d){assert(d==&pci.dev&&entered);power_calls++;}
#include "config_native_actual.inc"
static int remesa_private_fb_cursor_set_live(struct drm_crtc *crtc,struct drm_file *file,
 uint32_t handle,uint32_t width,uint32_t height)
{(void)file;(void)handle;(void)width;(void)height;assert(crtc->dev==&device&&entered);cursor_calls++;return live_result;}
static int remesa_private_fb_cursor_move_live(struct drm_crtc *crtc,int x,int y)
{(void)x;(void)y;assert(crtc->dev==&device&&entered);cursor_calls++;return live_result;}
#include "remesa_private_fbdev_config.inc"
static struct vm_area_struct vma;
static void fixture(void)
{
 assert(!readers&&!entered);
 private=(struct drm_psb_private){.stolen_base=0x30000000,.vram_stolen_size=1024*1024,.fb_helper=&helper};
 device=(struct drm_device){false,&private,&pci};other=device;
 range=(struct gtt_range){.gem={&device,8192,1},.offset=8192,.stolen=true,.private_tracked=true};
 fb=(struct drm_framebuffer){.dev=&device,.obj={&range.gem},.refs=1};
 helper=(struct drm_fb_helper){.dev=&device,.fb=&fb,.fbdev=&info};info=(struct fb_info){&helper,1};
 vma=(struct vm_area_struct){.vm_start=0x100000,.vm_end=0x102000};
 calls=inserts=gem_gets=gem_puts=orphans=cancels=unregisters=helper_finis=info_frees=fb_frees=helper_frees=fb_cleanups=config_calls=power_calls=0;
 block_call=call_started=resume_call=remove_started=remove_done=false;live_result=17;
 initial_error=helper_init_error=0;initial_published=false;error_unplugs=0;
 cursor_calls=0;
}
static void test_ops(bool live)
{
 struct fb_var_screeninfo var={0};struct fb_cmap map={0};
 struct fb_fillrect rect={0};struct fb_copyarea area={0};struct fb_image image={0};
 int expected=live?17:-ENODEV;unsigned before=calls;
 assert(remesa_fb_check_var(&var,&info)==expected&&remesa_fb_set_par(&info)==expected);
 assert(remesa_fb_setcmap(&map,&info)==expected&&remesa_fb_blank(0,&info)==expected);
 assert(remesa_fb_pan(&var,&info)==expected&&remesa_fb_roll(&var,&info)==expected);
 assert(remesa_fb_debug_enter(&info)==expected&&remesa_fb_debug_leave(&info)==expected);
 assert(remesa_fb_ioctl(&info,0,0)==expected&&remesa_fb_color(0,0,0,0,0,&info)==expected);
 assert(remesa_fb_sync(&info)==expected);
 remesa_fb_fill(&info,&rect);remesa_fb_copy(&info,&area);remesa_fb_native_copy(&info,&area);remesa_fb_image(&info,&image);
 assert(calls==before+(live?15:0)&&!readers&&!entered);
}
static void *callback_worker(void *unused){(void)unused;assert(remesa_fb_sync(&info)==17);return NULL;}
static void *remove_worker(void *unused)
{
 (void)unused;assert(!pthread_mutex_lock(&state));device.unplugged=true;remove_started=true;
 assert(!pthread_cond_broadcast(&changed));while(readers)assert(!pthread_cond_wait(&changed,&state));
 assert(!pthread_mutex_unlock(&state));remesa_private_fbdev_detach(&device);
 assert(!pthread_mutex_lock(&state));remove_done=true;assert(!pthread_cond_broadcast(&changed));
 assert(!pthread_mutex_unlock(&state));return NULL;
}
int main(void)
{
 fixture();range.gem.size=4096;int entry;
 assert(drm_dev_enter(&device,&entry));assert(!baseline_fb_mmap(&info,&vma));
 struct vm_fault old_fault={&vma,vma.vm_start,0};assert(!baseline_fb_fault(&old_fault));drm_dev_exit(entry);
 assert(inserts==2&&!gem_gets&&last_pfn==((0x30002000u>>12)+1));
 fixture();range.gem.size=4096;assert(remesa_private_fb_mmap(&info,&vma)==-EINVAL&&!gem_gets&&!vma.vm_private_data);
 fixture();vma.vm_pgoff=1;assert(remesa_private_fb_mmap(&info,&vma)==-EINVAL&&!gem_gets);
 fixture();range.offset=private.vram_stolen_size;assert(remesa_private_fb_mmap(&info,&vma)==-EINVAL);
 fixture();private.stolen_base=0xfffff000;range.offset=0;assert(remesa_private_fb_mmap(&info,&vma)==-EINVAL);
 fixture();private.stolen_base=0xfffff000;range.offset=0;range.gem.size=4096;vma.vm_end=vma.vm_start+4096;
 assert(!remesa_private_fb_mmap(&info,&vma));struct vm_fault edge={&vma,vma.vm_start,0};
 assert(!vma.vm_ops->fault(&edge)&&last_pfn==0xfffff);vma.vm_ops->close(&vma);
 fixture();range.stolen=false;assert(remesa_private_fb_mmap(&info,&vma)==-EINVAL);
 fixture();range.private_tracked=false;assert(remesa_private_fb_mmap(&info,&vma)==-EINVAL);
 fixture();range.gem.dev=&other;assert(remesa_private_fb_mmap(&info,&vma)==-EINVAL);
 fixture();assert(!remesa_private_fb_mmap(&info,&vma)&&range.gem.refs==2&&vma.vm_private_data==&range.gem);
 helper.fb=NULL;struct vm_fault fault={&vma,vma.vm_start+4096,1};
 assert(!vma.vm_ops->fault(&fault)&&inserts==1&&last_pfn==((0x30002000u>>12)+1)&&last_address==fault.address);
 struct vm_area_struct split=vma;split.vm_start+=4096;split.vm_pgoff=1;split.vm_ops->open(&split);
 assert(range.gem.refs==3);fault=(struct vm_fault){&split,split.vm_start,1};
 assert(!split.vm_ops->fault(&fault)&&inserts==2);fault.pgoff=0;
 assert(split.vm_ops->fault(&fault)==VM_FAULT_SIGBUS&&inserts==2);
 fault.pgoff=2;assert(split.vm_ops->fault(&fault)==VM_FAULT_SIGBUS&&inserts==2);
 split.vm_ops->close(&split);vma.vm_ops->close(&vma);assert(range.gem.refs==1&&gem_gets==2&&gem_puts==2);
 fixture();test_ops(true);device.unplugged=true;device.dev_private=NULL;test_ops(false);
 assert(remesa_private_fb_mmap(&info,&vma)==-ENODEV&&!gem_gets);
 fixture();struct drm_crtc crtc={&device};struct drm_mode_set set={&crtc};
 assert(gma_crtc_set_config(&set,NULL)==17&&config_calls==1&&!power_calls);
 private.rpm_enabled=true;assert(gma_crtc_set_config(&set,NULL)==17&&power_calls==2);
 assert(gma_crtc_cursor_set(&crtc,NULL,0,0,0)==17&&gma_crtc_cursor_move(&crtc,1,2)==17&&cursor_calls==2);
 device.unplugged=true;device.dev_private=NULL;
 assert(gma_crtc_set_config(&set,NULL)==-ENODEV&&config_calls==2&&power_calls==2);
 assert(gma_crtc_cursor_set(&crtc,NULL,0,0,0)==-ENODEV&&gma_crtc_cursor_move(&crtc,1,2)==-ENODEV&&cursor_calls==2);
 /* Open fb file holds helper/info across unregister; VMA owns GEM separately. */
 fixture();info.count=2;assert(!remesa_private_fb_mmap(&info,&vma));
 block_call=true;pthread_t worker,remover;assert(!pthread_create(&worker,NULL,callback_worker,NULL));
 assert(!pthread_mutex_lock(&state));while(!call_started)assert(!pthread_cond_wait(&changed,&state));
 assert(!pthread_mutex_unlock(&state));assert(!pthread_create(&remover,NULL,remove_worker,NULL));
 assert(!pthread_mutex_lock(&state));while(!remove_started)assert(!pthread_cond_wait(&changed,&state));
 assert(readers==1&&!remove_done&&!unregisters);resume_call=true;
 assert(!pthread_cond_broadcast(&changed));assert(!pthread_mutex_unlock(&state));
 assert(!pthread_join(worker,NULL));assert(!pthread_join(remover,NULL));
 assert(info.count==1&&unregisters==1&&cancels==2&&!helper_finis&&!info_frees&&!fb_frees&&!helper_frees);
 assert(private.fb_helper==&helper&&private.remesa_private_fbdev_unregistered);
 unsigned before=inserts;fault=(struct vm_fault){&vma,vma.vm_start,0};
 assert(vma.vm_ops->fault(&fault)==VM_FAULT_SIGBUS&&inserts==before);
 remesa_private_fbdev_detach(&device);assert(unregisters==1&&cancels==2);
 vma.vm_ops->close(&vma);assert(range.gem.refs==1);
 assert(!--info.count);remesa_private_fb_destroy_info(&info);
 assert(!private.fb_helper&&helper_finis==1&&info_frees==1&&fb_frees==1&&helper_frees==1&&fb_cleanups==1&&orphans==1);
 /* No open files: core final put may synchronously free helper during detach. */
 fixture();device.unplugged=true;remesa_private_fbdev_detach(&device);
 assert(!info.count&&!private.fb_helper&&unregisters==1&&helper_frees==1&&info_frees==1);
 remesa_private_fbdev_detach(&device);assert(unregisters==1);
 /* A KMS plane's FB reference outlives the last fb_info file. */
 fixture();device.unplugged=true;fb.refs=2;remesa_private_fbdev_detach(&device);
 assert(helper_frees==1&&info_frees==1&&!fb_frees&&fb.refs==1&&range.gem.refs==1);
 drm_framebuffer_put(&fb);assert(fb_frees==1&&orphans==1);
 /* Registration failure before/after publication follows different owners. */
 fixture();helper_init_error=-ENOMEM;assert(psb_fbdev_init(&device)==-ENOMEM);
 assert(!private.fb_helper&&helper_frees==1&&!helper_finis&&!error_unplugs);
 fixture();initial_error=-ENOMEM;assert(psb_fbdev_init(&device)==-ENOMEM);
 assert(!private.fb_helper&&helper_finis==1&&info_frees==1&&helper_frees==1&&!error_unplugs&&!unregisters);
 fixture();initial_error=-ENOMEM;initial_published=true;
 assert(psb_fbdev_init(&device)==-ENOMEM&&error_unplugs==1&&device.unplugged);
 assert(private.fb_helper==&helper&&info.count==1&&unregisters==1&&!helper_finis&&!info_frees&&!helper_frees);
 assert(!--info.count);remesa_private_fb_destroy_info(&info);
 assert(!private.fb_helper&&helper_finis==1&&info_frees==1&&helper_frees==1&&fb_frees==1&&orphans==1);
 puts("PASS actual fbdev fragments: bounded/single-page GEM-owned VMA, split/fork refs, postloss faults/ops/config denied, entry join, delayed final-file destroy and synchronous destroy; fb/VFS/GEM APIs modeled, GPU0.");
}
