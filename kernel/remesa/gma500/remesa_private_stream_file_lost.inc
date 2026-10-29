/* SPDX-License-Identifier: GPL-2.0-only */
/* Only software quarantine and identity removal after postclose or unplug. Deliberately
 * bypass ops.close/ops.quarantine, whose callbacks may depend on live hardware. */
#include "remesa_private_file_lost.h"
void remesa_private_stream_file_lost(struct drm_device *dev,struct drm_file *file)
{
 if(!dev||!file)return;
 mutex_lock(&stream_lock);
 if(owner_device==dev&&owner==file){
  if(active){active->state=STREAM_QUARANTINED;permanently_quarantined=true;}
  owner=NULL;owner_device=NULL;
 }
 remesa_private_trusted_file_lost(dev,file);
 mutex_unlock(&stream_lock);
 remesa_private_memory_file_lost(dev,file);
}
void remesa_private_stream_device_lost(struct drm_device *dev)
{
 if(!dev)return;
 mutex_lock(&stream_lock);
 if(owner_device==dev){
  if(active){active->state=STREAM_QUARANTINED;permanently_quarantined=true;}
  owner=NULL;owner_device=NULL;
 }
 remesa_private_trusted_device_lost(dev);
 mutex_unlock(&stream_lock);
 remesa_private_memory_device_lost(dev);
}
