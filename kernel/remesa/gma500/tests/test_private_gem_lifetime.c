/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual tracker/destructor/constructors with modeled DRM/GTT APIs and real
 * pthread entry joining. This is not kernel SRCU or physical GPU evidence. */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
typedef uint32_t u32;typedef uint64_t u64;
#define container_of(ptr,type,member) ((type *)((char *)(ptr)-offsetof(type,member)))
struct list_head {struct list_head *next,*prev;};
#define LIST_HEAD(name) struct list_head name={&name,&name}
#define list_for_each(pos,head) for((pos)=(head)->next;(pos)!=(head);(pos)=(pos)->next)
#define list_entry(ptr,type,member) container_of(ptr,type,member)
static void list_add_tail(struct list_head *node,struct list_head *head)
{node->next=head;node->prev=head->prev;head->prev->next=node;head->prev=node;}
static void list_del_init(struct list_head *node)
{node->next->prev=node->prev;node->prev->next=node->next;node->next=node->prev=node;}
struct mutex {pthread_mutex_t native;};
#define DEFINE_MUTEX(name) struct mutex name={PTHREAD_MUTEX_INITIALIZER}
static void mutex_lock(struct mutex *m){assert(!pthread_mutex_lock(&m->native));}
static void mutex_unlock(struct mutex *m){assert(!pthread_mutex_unlock(&m->native));}
struct drm_device {bool unplugged,hardware_alive;unsigned refs;void *dev;};
struct mapping {int value;};struct filp {struct mapping *f_mapping;};
struct drm_gem_object {struct drm_device *dev;struct filp *filp;unsigned refs;size_t size;};
struct gtt_range {
 struct drm_gem_object gem;unsigned mmapping,stolen;int in_gart,resource,npage;
 struct list_head private_lifetime_link;bool private_tracked,private_orphaned;
};
struct drm_file {unsigned id;};
static pthread_mutex_t state=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static _Thread_local unsigned entered;
static unsigned readers,module_refs,offsets,releases,gtt_frees,unpins,late_unpins,unloads;
static bool pause_release,release_started,resume_release,remove_started,remove_done;
static bool init_fails,handle_fails,allocate_fails;
static bool pause_unpin,unpin_started,resume_unpin,unpin_fails;
static bool flip_on_nested;
static struct drm_device device,other;
static struct gtt_range range;
static struct mapping mapping;
static struct filp filp={&mapping};
#define THIS_MODULE ((void *)1)
static bool warning(bool value){assert(!value);return value;}
#define WARN_ON(value) warning(!!(value))
static bool drm_dev_enter(struct drm_device *dev,int *index)
{
 assert(!pthread_mutex_lock(&state));
 if(flip_on_nested&&entered)dev->unplugged=true;
 bool ok=!dev->unplugged;
 if(ok){entered++;readers++;*index=77;}
 assert(!pthread_mutex_unlock(&state));return ok;
}
static void drm_dev_exit(int index)
{
 assert(index==77&&entered);assert(!pthread_mutex_lock(&state));
 entered--;assert(readers);readers--;assert(!pthread_cond_broadcast(&changed));
 assert(!pthread_mutex_unlock(&state));
}
static bool drm_dev_is_unplugged(struct drm_device *dev)
{int index;if(!drm_dev_enter(dev,&index))return true;drm_dev_exit(index);return false;}
static void drm_dev_get(struct drm_device *dev){dev->refs++;}
static void drm_dev_put(struct drm_device *dev){assert(dev->refs);dev->refs--;}
static void __module_get(void *m){assert(m==THIS_MODULE);module_refs++;}
static void module_put(void *m){assert(m==THIS_MODULE&&module_refs);module_refs--;}
static void drm_gem_free_mmap_offset(struct drm_gem_object *obj)
{assert(obj==&range.gem&&entered);offsets++;}
static void drm_gem_object_release(struct drm_gem_object *obj)
{
 assert(obj==&range.gem&&entered);assert(!pthread_mutex_lock(&state));
 releases++;release_started=true;assert(!pthread_cond_broadcast(&changed));
 while(pause_release&&!resume_release)assert(!pthread_cond_wait(&changed,&state));
 assert(!pthread_mutex_unlock(&state));
}
static void remesa_private_gtt_unpin_live(struct gtt_range *r)
{
 assert(r==&range&&entered);assert(!pthread_mutex_lock(&state));
 unpin_started=true;assert(!pthread_cond_broadcast(&changed));
 while(pause_unpin&&!resume_unpin)assert(!pthread_cond_wait(&changed,&state));
 unpins++;if(!r->gem.dev->hardware_alive)late_unpins++;
 if(!unpin_fails)r->in_gart--;
 assert(!pthread_mutex_unlock(&state));
}
static int remesa_private_gtt_pin_live(struct gtt_range *r)
{(void)r;assert(0);return -ENOSYS;}
static void remesa_private_gtt_roll_live(struct drm_device *d,struct gtt_range *r,int roll)
{(void)d;(void)r;(void)roll;assert(0);}
#include "remesa_private_gtt_guard.inc"
static void release_resource(int *r){assert(r==&range.resource);}
static void kfree(void *p)
{assert(p==&range);gtt_frees++;memset(&range,0xa5,sizeof range);}
#include "gtt_free_actual.inc"
#include "gem_free_live_actual.inc"
#include "remesa_private_gem_lifetime.inc"
static void drm_gem_object_put(struct drm_gem_object *obj)
{assert(obj->refs);if(!--obj->refs)psb_gem_free_object(obj);}
#define PAGE_SIZE 4096u
#define GFP_KERNEL 1u
#define __GFP_DMA32 2u
#define roundup(value,align) (((value)+(align)-1)&~((align)-1))
#define dev_err(...) ((void)0)
static struct gtt_range *psb_gtt_alloc_range(struct drm_device *dev,int bytes,
 const char *name,int backed,u32 align)
{
 (void)name;assert(align==4096);if(allocate_fails)return NULL;
 range=(struct gtt_range){.gem={.dev=dev,.size=(unsigned)bytes},.stolen=backed,.in_gart=backed};
 return &range;
}
static int drm_gem_object_init(struct drm_device *dev,struct drm_gem_object *obj,u64 bytes)
{assert(dev==&device&&obj==&range.gem);if(init_fails)return -ENOMEM;obj->refs=1;obj->size=bytes;obj->filp=&filp;return 0;}
static void drm_gem_private_object_init(struct drm_device *dev,struct drm_gem_object *obj,u64 bytes)
{assert(dev==&device&&obj==&range.gem);obj->refs=1;obj->size=bytes;obj->filp=NULL;}
static void mapping_set_gfp_mask(struct mapping *m,unsigned flags)
{assert(m==&mapping&&flags==(GFP_KERNEL|__GFP_DMA32));}
static int drm_gem_handle_create(struct drm_file *file,struct drm_gem_object *obj,u32 *handle)
{assert(file&&obj==&range.gem);if(handle_fails)return -ENOMEM;obj->refs++;*handle=1;return 0;}
#include "gem_create_actual.inc"
#include "fb_alloc_actual.inc"
static void fixture(void)
{
 assert(!readers&&!entered&&!module_refs&&private_gem_objects.next==&private_gem_objects);
 device=(struct drm_device){.hardware_alive=true};other=device;
 offsets=releases=gtt_frees=unpins=late_unpins=unloads=0;
 pause_release=release_started=resume_release=remove_started=remove_done=false;
 init_fails=handle_fails=allocate_fails=false;range=(struct gtt_range){0};
 pause_unpin=unpin_started=resume_unpin=unpin_fails=false;
 flip_on_nested=false;
}
static void *free_worker(void *unused)
{(void)unused;drm_gem_object_put(&range.gem);return NULL;}
static void *remove_worker(void *unused)
{
 (void)unused;assert(!pthread_mutex_lock(&state));device.unplugged=true;remove_started=true;
 assert(!pthread_cond_broadcast(&changed));
 while(readers)assert(!pthread_cond_wait(&changed,&state));
 assert(!pthread_mutex_unlock(&state));
 /* Model the retained-unload decision, tested separately with actual unload. */
 bool retain=remesa_private_gem_device_retained(&device);
 assert(!pthread_mutex_lock(&state));if(!retain)device.hardware_alive=false;
 unloads++;remove_done=true;assert(!pthread_cond_broadcast(&changed));
 assert(!pthread_mutex_unlock(&state));return NULL;
}
int main(int argc,char **argv)
{
 struct drm_file file={1};u32 handle;
 if(argc==2){
  bool idle_failure=!strcmp(argv[1],"unpin-failure");
  bool other_pin=!strcmp(argv[1],"other-pin");
  bool invalid_pin=!strcmp(argv[1],"invalid-pin");
  bool nested_loss=!strcmp(argv[1],"nested-loss");
  assert(idle_failure||other_pin||invalid_pin||nested_loss);fixture();
  assert(!psb_gem_create(&file,&device,4096,&handle,0,4096));
  range.mmapping=1;range.in_gart=invalid_pin?0:(other_pin?2:1);
  unpin_fails=idle_failure;flip_on_nested=nested_loss;drm_gem_object_put(&range.gem);
  assert(unpins==((invalid_pin||nested_loss)?0u:1u)&&!late_unpins&&!offsets&&!releases&&!gtt_frees);
  assert(range.private_orphaned&&range.private_tracked);
  assert(range.mmapping==(other_pin?0u:1u)&&range.in_gart==(invalid_pin?0:1));
  assert(module_refs==1&&device.refs==1&&remesa_private_gem_device_retained(&device));
  assert(remesa_private_gem_rebind_blocked());
  puts("PASS failed idle, remaining/invalid pin or nested GTT denial retains backing/tracking/module/device ownership; no GEM release, model only");
  return 0;
 }
 assert(argc==1);
 fixture();assert(!psb_gem_create(&file,&device,4096,&handle,0,4096));
 assert(handle==1&&range.gem.refs==1&&range.private_tracked&&device.refs==1&&module_refs==1);
 assert(!remesa_private_gem_device_retained(&device)&&!remesa_private_gem_rebind_blocked());
 drm_gem_object_put(&range.gem);assert(gtt_frees==1&&releases==1&&!module_refs&&!device.refs);
 fixture();handle_fails=true;assert(psb_gem_create(&file,&device,4096,&handle,0,4096)==-ENOMEM);
 assert(releases==1&&gtt_frees==1&&!module_refs&&!device.refs);
 fixture();init_fails=true;assert(psb_gem_create(&file,&device,4096,&handle,0,4096)==-ENOMEM);
 assert(!releases&&gtt_frees==1&&!module_refs&&!device.refs);
 fixture();allocate_fails=true;assert(psb_gem_create(&file,&device,4096,&handle,0,4096)==-ENOSPC);
 assert(!gtt_frees&&!module_refs&&!device.refs);
 fixture();assert(psbfb_alloc(&device,4096)==&range&&range.private_tracked&&range.stolen);
 assert(device.refs==1&&module_refs==1);drm_gem_object_put(&range.gem);
 assert(gtt_frees==1&&releases==1&&!module_refs&&!device.refs);
 /* Destruction starts before unplug; the read interval must join unpin. */
 fixture();assert(!psb_gem_create(&file,&device,4096,&handle,0,4096));
 range.mmapping=range.in_gart=1;pause_unpin=true;pthread_t free_thread,remove_thread;
 assert(!pthread_create(&free_thread,NULL,free_worker,NULL));
 assert(!pthread_mutex_lock(&state));
 while(!unpin_started)assert(!pthread_cond_wait(&changed,&state));
 assert(!pthread_mutex_unlock(&state));
 assert(!pthread_create(&remove_thread,NULL,remove_worker,NULL));
 assert(!pthread_mutex_lock(&state));
 while(!remove_started)assert(!pthread_cond_wait(&changed,&state));
 assert(readers==2&&!remove_done&&device.hardware_alive&&!unpins);
 resume_unpin=true;assert(!pthread_cond_broadcast(&changed));assert(!pthread_mutex_unlock(&state));
 assert(!pthread_join(free_thread,NULL));assert(!pthread_join(remove_thread,NULL));
 assert(unpins==1&&!late_unpins&&gtt_frees==1&&!module_refs&&!device.refs&&!readers);
 /* A positive-kref object at unplug already retains the device graph. */
 fixture();assert(!psb_gem_create(&file,&device,4096,&handle,0,4096));
 range.mmapping=range.in_gart=1;assert(!remove_worker(NULL));
 assert(device.hardware_alive&&remesa_private_gem_device_retained(&device));
 assert(!remesa_private_gem_device_retained(&other)&&!remesa_private_gem_device_retained(NULL));
 assert(remesa_private_gem_rebind_blocked());
 drm_gem_object_put(&range.gem);
 assert(range.private_orphaned&&range.private_tracked&&!range.gem.refs);
 assert(!offsets&&!releases&&!unpins&&!gtt_frees&&!late_unpins);
 assert(module_refs==1&&device.refs==1&&range.mmapping&&range.in_gart);
 psb_gem_free_object(&range.gem); /* Idempotent retained state, no resurrection. */
 assert(module_refs==1&&device.refs==1&&!gtt_frees&&!unpins);
 puts("PASS actual constructors/tracker/destructors: refs balanced on live/failure paths; preloss unpin joined; postloss zero-kref object and full-device ownership retained with no MMIO/unpin/release. Kernel APIs modeled, GPU0.");
}
