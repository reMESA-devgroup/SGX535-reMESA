/* SPDX-License-Identifier: GPL-2.0-only */
/* Same TU as endpoint; no unmap/unpin/free, PM, source/backend or MMU calls. */
#include "remesa_private_file_lost.h"
static void remesa_private_memory_lost(struct drm_device *dev,struct drm_file *file,bool all)
{
 struct remesa_gem_memory *s;unsigned i;
 if(!dev||(!all&&!file))return;
 mutex_lock(&endpoint_lock);s=endpoint;
 if(s&&s->dev==dev&&(all||s->file==file)){
  mutex_lock(&s->lock);
  s->private_device_lost=true;s->file=NULL;
  s->private_matrix_constructing=false;
  for(i=0;i<8;i++)if(s->jobs[i].live){
   s->jobs[i].quarantined=true;s->jobs[i].state=REMESA_JOB_QUARANTINED;
  }
  for(i=0;i<OBJECTS;i++)if(s->bo[i].live)s->bo[i].quarantine=true;
  mutex_unlock(&s->lock);
 }
 mutex_unlock(&endpoint_lock);
}
void remesa_private_memory_file_lost(struct drm_device *dev,struct drm_file *file)
{remesa_private_memory_lost(dev,file,false);}
void remesa_private_memory_device_lost(struct drm_device *dev)
{remesa_private_memory_lost(dev,NULL,true);}
bool remesa_private_memory_device_retained(struct drm_device *dev)
{
 struct remesa_gem_memory *s;bool retained=false;
 if(!dev)return false;
 mutex_lock(&endpoint_lock);s=endpoint;
 if(s&&s->dev==dev){
  mutex_lock(&s->lock);retained=s->private_device_lost||s->private_session;mutex_unlock(&s->lock);
 }
 mutex_unlock(&endpoint_lock);return retained;
}
bool remesa_private_memory_rebind_blocked(void)
{
 struct remesa_gem_memory *s;bool retained=false;
 /* The retained backend has one global endpoint, not independent per-PCI
  * contexts. Refuse every new binding before it can write device registers. */
 mutex_lock(&endpoint_lock);s=endpoint;
 if(s){mutex_lock(&s->lock);retained=s->private_device_lost||s->private_session;mutex_unlock(&s->lock);}
 mutex_unlock(&endpoint_lock);return retained;
}
