/* SPDX-License-Identifier: GPL-2.0-only */
/* Same TU as endpoint; no unmap/unpin/free, PM, source/backend or MMU calls. */
#include "remesa_private_file_lost.h"
void remesa_private_memory_file_lost(struct drm_device *dev,struct drm_file *file)
{
 struct remesa_gem_memory *s;unsigned i;
 if(!dev||!file)return;
 mutex_lock(&endpoint_lock);s=endpoint;
 if(s&&s->dev==dev&&s->file==file){
  mutex_lock(&s->lock);
  s->private_device_lost=true;s->file=NULL;
  for(i=0;i<8;i++)if(s->jobs[i].live){
   s->jobs[i].quarantined=true;s->jobs[i].state=REMESA_JOB_QUARANTINED;
  }
  for(i=0;i<OBJECTS;i++)if(s->bo[i].live)s->bo[i].quarantine=true;
  mutex_unlock(&s->lock);
 }
 mutex_unlock(&endpoint_lock);
}
