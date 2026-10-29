/* SPDX-License-Identifier: GPL-2.0-only */
/* Included after the original destructor, renamed _live. List ownership is
 * independent of the GEM kref: a lost zero-kref allocation is quarantined,
 * never resurrected, released, retried or returned to userspace. */
#include "remesa_private_gem_lifetime.h"
static DEFINE_MUTEX(private_gem_lock);
static LIST_HEAD(private_gem_objects);
static void remesa_private_gem_quarantine(struct gtt_range *range)
{
 mutex_lock(&private_gem_lock);
 if(!WARN_ON(!range->private_tracked))range->private_orphaned=true;
 mutex_unlock(&private_gem_lock);
}
void remesa_private_gem_track(struct drm_gem_object *object)
{
 struct gtt_range *range=container_of(object,struct gtt_range,gem);
 mutex_lock(&private_gem_lock);
 if(WARN_ON(range->private_tracked)){mutex_unlock(&private_gem_lock);return;}
 drm_dev_get(object->dev);__module_get(THIS_MODULE);
 range->private_tracked=true;
 list_add_tail(&range->private_lifetime_link,&private_gem_objects);
 mutex_unlock(&private_gem_lock);
}
void psb_gem_free_object(struct drm_gem_object *object)
{
 struct gtt_range *range=container_of(object,struct gtt_range,gem);
 struct drm_device *dev=object->dev;int index;
 if(!drm_dev_enter(dev,&index)){
  remesa_private_gem_quarantine(range);return;
 }
 mutex_lock(&private_gem_lock);
 if(WARN_ON(!range->private_tracked||range->private_orphaned)){
  mutex_unlock(&private_gem_lock);drm_dev_exit(index);return;
 }
 mutex_unlock(&private_gem_lock);
 /* Native unpin returns void and retains in_gart on idle failure. Never
  * release GEM backing before checking that the mmap pin actually dropped.
  * No tracking lock is held across GTT/MMU/IRQ work. */
 if(range->mmapping){
  int before=range->in_gart;
  if(before<=0){remesa_private_gem_quarantine(range);drm_dev_exit(index);return;}
  psb_gtt_unpin(range);
  if(range->in_gart!=before-1){
   remesa_private_gem_quarantine(range);drm_dev_exit(index);return;
  }
  range->mmapping=false;
 }
 if(range->in_gart&&!range->stolen){
  remesa_private_gem_quarantine(range);drm_dev_exit(index);return;
 }
 mutex_lock(&private_gem_lock);
 list_del_init(&range->private_lifetime_link);range->private_tracked=false;
 mutex_unlock(&private_gem_lock);
 /* Original destruction may free range. Only the saved dev and SRCU index
  * are used afterwards; the tracking references survive the entire call. */
 remesa_private_gem_free_object_live(object);
 drm_dev_exit(index);drm_dev_put(dev);module_put(THIS_MODULE);
}
bool remesa_private_gem_device_retained(struct drm_device *dev)
{
 struct list_head *entry;bool retained=false;
 if(!dev)return false;
 mutex_lock(&private_gem_lock);
 list_for_each(entry,&private_gem_objects){
  struct gtt_range *range=list_entry(entry,struct gtt_range,private_lifetime_link);
  if(range->gem.dev==dev&&(range->private_orphaned||drm_dev_is_unplugged(dev))){retained=true;break;}
 }
 mutex_unlock(&private_gem_lock);return retained;
}
bool remesa_private_gem_rebind_blocked(void)
{
 struct list_head *entry;bool retained=false;
 mutex_lock(&private_gem_lock);
 list_for_each(entry,&private_gem_objects){
  struct gtt_range *range=list_entry(entry,struct gtt_range,private_lifetime_link);
  if(range->private_orphaned||drm_dev_is_unplugged(range->gem.dev)){retained=true;break;}
 }
 mutex_unlock(&private_gem_lock);return retained;
}
