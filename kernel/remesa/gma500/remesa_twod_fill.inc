/* SPDX-License-Identifier: GPL-2.0-only */
/* Include immediately after the normal copy endpoint: the same retained
 * session slot excludes copy/fill overlap and survives uncertain FD death. */
#include "remesa_twod_fill.h"
#include "remesa_twod_fill_uapi.h"
static bool twod_fill_enabled;
module_param_named(remesa_twod_fill_enabled,twod_fill_enabled,bool,0600);
static int twod_fill_command(struct remesa_gem_memory *s,
 const struct remesa_twod_fill_request *r,struct remesa_gem_twod_command *c,
 struct remesa_memory_binding uses[2],unsigned *count)
{
 struct remesa_twod_copy_plan plan;u32 id=r->destination.id;int error=0;
 struct backing *b;
 mutex_lock(&s->lock);
 if(s->executor||s->stream_bound||s->ta_cache_domains){error=-EBUSY;goto done;}
 b=&s->bo[id&31];
 if(!id||!b->live||b->generation!=id>>5){error=-ENOENT;goto done;}
 c->schema=REMESA_GEM_TWOD_FILL_SCHEMA;
 c->fill.destination=(struct remesa_twod_surface){id,b->va.start,b->gem.size,
  r->destination.width,r->destination.height,r->destination.stride,REMESA_TWOD_ARGB8888};
 c->fill.request_base=(u32)(b->va.start&0xf0000000ULL);
 c->fill.destination_x=r->destination_x;c->fill.destination_y=r->destination_y;
 c->fill.width=r->width;c->fill.height=r->height;c->fill.color=r->color;
 error=remesa_twod_fill_prepare(&c->fill,&plan);
 if(error){error=-error;goto done;}
 uses[0]=(struct remesa_memory_binding){id,b->va.start,b->gem.size};*count=1;
 done:mutex_unlock(&s->lock);return error;
}
int remesa_twod_fill_ioctl(struct drm_device *dev,void *data,struct drm_file *file)
{
 struct remesa_twod_fill_request *r=data;
 struct remesa_gem_twod_command command={0};struct remesa_memory_binding uses[2];
 struct remesa_twod_copy_session *session;struct remesa_gem_memory *s;
 enum remesa_job_state state;unsigned count=0;int error;
 if(!dev||!r||!file||!capable(CAP_SYS_ADMIN))return -EPERM;
 if(!memory_enabled||!twod_fill_enabled)return -EOPNOTSUPP;
 if(r->version!=REMESA_TWOD_FILL_VERSION||r->flags||r->reserved[0]||r->reserved[1]||r->response_reserved||
    !r->context||!r->budget_ms||r->budget_ms>10000||r->operation||r->cookie||
    r->stages||r->retained||r->core||r->revision||r->error)return -EINVAL;
 mutex_lock(&endpoint_lock);s=endpoint;
 if(!s||s->dev!=dev||s->file!=file||s->context!=r->context){error=-EACCES;goto done;}
 if(twod_copy_retained){error=-EBUSY;goto done;}
 error=twod_fill_command(s,r,&command,uses,&count);if(error)goto done;
 session=kzalloc(sizeof(*session),GFP_KERNEL);if(!session){error=-ENOMEM;goto done;}
 error=remesa_gem_job_prepare(s,uses,count,&command,sizeof(command),&session->ticket);
 if(error){kfree(session);goto done;}
 /* The heap context is published BEFORE possible start; every uncertain
  * return can safely retain callback pointers beyond this syscall/FD. */
 twod_copy_retained=session;r->operation=session->ticket.operation;r->stages=REMESA_TWOD_FILL_PREPARED;
 error=remesa_gem_twod_issue(s,session->ticket,&remesa_twod_native_executor,
  &session->native,&session->job);

 r->cookie=session->job.cookie;r->core=session->native.core;r->revision=session->native.revision;
 if(error)goto outcome;
 r->stages|=REMESA_TWOD_FILL_STARTED;
 error=remesa_gem_twod_wait(&session->job,r->budget_ms);
 if(error)goto outcome;
 r->stages|=REMESA_TWOD_FILL_COMPLETE;
 error=remesa_gem_twod_publish(&session->job);
 if(error)goto outcome;
 r->stages|=REMESA_TWOD_FILL_VISIBLE;
 error=remesa_gem_twod_retire(&session->job);
 if(error)goto outcome;
 r->stages|=REMESA_TWOD_FILL_RETIRED;
 twod_copy_retained=NULL;kfree(session);
 outcome:
 if(error){
  /* Only a known unissued PREPARED job may be cancelled. Faulted/attached
   * or possibly issued jobs and their context remain quarantined. */
  if(!session->job.live&&!remesa_gem_job_state(s,session->ticket,&state)&&
     state==REMESA_JOB_PREPARED&&!remesa_gem_job_cancel(s,session->ticket)){
   twod_copy_retained=NULL;kfree(session);
  }
 }
 r->error=error;r->retained=twod_copy_retained!=NULL;
 error=0; /* Explicit structured result, never an ioctl-success GPU oracle. */
 done:mutex_unlock(&endpoint_lock);return error;
}
