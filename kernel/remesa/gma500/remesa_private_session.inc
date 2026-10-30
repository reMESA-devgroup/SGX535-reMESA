/* SPDX-License-Identifier: GPL-2.0-only */
/* Same TU as the private endpoint. Lock order: endpoint_lock then s->lock. */
#include "remesa_private_session.h"
int remesa_private_session_acquire(struct drm_device *dev,struct drm_file *file,
 u64 context,const void *owner,struct remesa_gem_memory **out)
{
 struct remesa_gem_memory *s;unsigned i;int error=-EACCES;
 if(!out)return -EINVAL;
 *out=NULL;
 if(!dev||!file||!context||!owner)return -EINVAL;
 mutex_lock(&endpoint_lock);s=endpoint;
 if(!s||s->dev!=dev||s->file!=file||s->context!=context)goto done;
 mutex_lock(&s->lock);
 if(s->private_device_lost){error=-ENODEV;goto unlock;}
 if(s->private_session||s->stream_bound||s->executor){error=-EBUSY;goto unlock;}
#if !defined(REMESA_MEMORY_TEST) || defined(REMESA_RETAINED_IOCTL_TEST)
 if(retained_entry.initialized||retained_entry.attempted){error=-EBUSY;goto unlock;}
#endif
 for(i=0;i<8;i++)if(s->jobs[i].live){error=-EBUSY;goto unlock;}
 for(i=0;i<OBJECTS;i++)if(s->bo[i].lease||s->bo[i].holds||s->bo[i].quarantine||
                         atomic_read(&s->bo[i].color_vmas)){
  error=-EBUSY;goto unlock;
 }
 s->private_session=owner;*out=s;error=0;
 unlock:mutex_unlock(&s->lock);
 done:mutex_unlock(&endpoint_lock);return error;
}
int remesa_private_session_release(struct remesa_gem_memory *s,const void *owner)
{
 unsigned i;int error=-EACCES;
 if(!s||!owner)return -EINVAL;
 mutex_lock(&endpoint_lock);
 /* Compare identity before dereferencing an arbitrary/stale endpoint. */
 if(endpoint!=s)goto done;
 mutex_lock(&s->lock);
 if(s->private_device_lost||!s->file){error=-ENODEV;goto unlock;}
 if(s->private_session!=owner)goto unlock;
 if(s->stream_bound||s->executor){error=-EBUSY;goto unlock;}
 for(i=0;i<8;i++)if(s->jobs[i].live){error=-EBUSY;goto unlock;}
 for(i=0;i<OBJECTS;i++)if(s->bo[i].lease||s->bo[i].holds||s->bo[i].quarantine||
                         atomic_read(&s->bo[i].color_vmas)){
  error=-EBUSY;goto unlock;
 }
 s->private_session=NULL;error=0;
 unlock:mutex_unlock(&s->lock);
 done:mutex_unlock(&endpoint_lock);return error;
}
