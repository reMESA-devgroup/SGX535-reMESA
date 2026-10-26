/* SPDX-License-Identifier: GPL-2.0-only */
/* Read-only LAB bridge. Same endpoint lock/file ownership and default-disabled
 * memory gate as the service. No executor registration or device operations. */
#include "remesa/visible_readback_uapi.h"
#include "remesa_gem_visible_ioctl.h"
_Static_assert(sizeof(struct remesa_visible_readback_request)==4144,
 "readback wire layout must agree on i386/64-bit");
int remesa_gem_visible_readback_ioctl(struct drm_device *dev,void *data,
 struct drm_file *file)
{
 struct remesa_visible_readback_request *r=data;
 struct remesa_memory_ticket ticket;unsigned i;int e;
 if(!r||!dev||!file||!capable(CAP_SYS_ADMIN))return -EPERM;
 if(!memory_enabled)return -EOPNOTSUPP;
 if(r->version!=REMESA_VISIBLE_READBACK_VERSION||!r->id||!r->context||
    !r->operation||!r->generation||r->slot>=8||!r->length||
    r->length>REMESA_VISIBLE_READBACK_CHUNK||r->reserved)return -EINVAL;
 for(i=0;i<REMESA_VISIBLE_READBACK_CHUNK;i++)if(r->data[i])return -EINVAL;
 /* The DRM ioctl core supplies kernel-owned disjoint request storage. This
  * entry does not accept userspace pointers embedded inside the envelope. */
 ticket=(struct remesa_memory_ticket){.context=r->context,.operation=r->operation,
  .slot=r->slot,.generation=r->generation};
 mutex_lock(&endpoint_lock);
 if(!endpoint)e=-ENODEV;
 else if(endpoint->dev!=dev||endpoint->file!=file||endpoint->context!=r->context)e=-EACCES;
 else e=remesa_gem_job_copy_visible(endpoint,ticket,r->id,r->offset,r->length,r->data);
 mutex_unlock(&endpoint_lock);return e;
}
