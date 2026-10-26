/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/capability.h>
#include "remesa_stream_bridge.h"
#include "remesa_gem_memory.h"
#include "remesa_stream_trusted.h"
static DEFINE_MUTEX(stream_lock);
static bool remesa_stream_enabled;
module_param(remesa_stream_enabled,bool,0600);
MODULE_PARM_DESC(remesa_stream_enabled,"Private continuous ABI; requires independently installed qualified stream backend");
static const struct remesa_stream_ops *installed;
static void *installed_backend;
static struct remesa_stream *active;
static struct drm_file *owner;
static struct drm_device *owner_device;
static bool permanently_quarantined;
int remesa_stream_backend_install(const struct remesa_stream_ops *ops,void *backend)
{
 int ret=0;
 mutex_lock(&stream_lock);
 if(installed||active||!ops||!ops->claim||!ops->freeze||!ops->execute||!ops->settle||!ops->seal||!ops->quarantine||!ops->close) ret=-EINVAL;
 else { installed=ops; installed_backend=backend; }
 mutex_unlock(&stream_lock); return ret;
}
int remesa_stream_ioctl(struct drm_device *dev,void *data,struct drm_file *file)
{
 struct remesa_stream_request *r=data; int ret;
 if(!capable(CAP_SYS_ADMIN)||!remesa_stream_enabled) return -EPERM;
 if(!r||r->version!=REMESA_STREAM_VERSION)return -EINVAL;
 mutex_lock(&stream_lock);
 if(permanently_quarantined) { ret=-EIO; goto out; }
 /* No implementation may silently fall back to the one-shot observer. */
 if(!installed) { ret=-EOPNOTSUPP; goto out; }
 if(r->action==REMESA_STREAM_OPEN||r->action==REMESA_STREAM_OPEN_FAST_BLAKE3||r->action==REMESA_STREAM_OPEN_FAST_NOHASH) {
  if(active) { ret=-EBUSY; goto out; }
  ret=remesa_stream_trusted_owner(dev,file);
  if(ret)goto out;
  active=kzalloc(sizeof(*active),GFP_KERNEL);
  if(!active) { ret=-ENOMEM; goto out; }
  ret=remesa_stream_open(active,r,installed,installed_backend);
  if(ret) { kfree(active); active=NULL; goto out; }
  owner=file; owner_device=dev;
 } else {
  if(!active||owner!=file||owner_device!=dev) { ret=-EACCES; goto out; }
  ret=remesa_stream_action(active,r);
  if(active->state==STREAM_QUARANTINED) permanently_quarantined=true;
  if(!ret&&active->state==STREAM_CLOSED) {
   kfree(active); active=NULL; owner=NULL; owner_device=NULL;
  }
 }
 out: mutex_unlock(&stream_lock); return ret;
}
void remesa_stream_postclose(struct drm_device *dev,struct drm_file *file)
{
 mutex_lock(&stream_lock);
 if(active&&owner==file&&owner_device==dev) {
  if(active->state==STREAM_IDLE||active->state==STREAM_STAGING) {
   if(!active->ops.close(active->backend)) { kfree(active); active=NULL; owner=NULL; owner_device=NULL; }
  }
  if(active) {
   /* File disappearance never permits rearming a possibly issued producer. */
   active->ops.quarantine(active->backend); active->state=STREAM_QUARANTINED;
   permanently_quarantined=true;
  }
 }
 mutex_unlock(&stream_lock);
 remesa_gem_memory_postclose(dev,file);
}
