/* SPDX-License-Identifier: GPL-2.0-only */
/* Normal typed copy: no LAB timing, readback or diagnostics.
 * Include after 2D lifecycle in native GEM TU. Endpoint lock precedes its
 * memory mutex; all userspace topology operations use that same outer lock. */
#include "remesa_twod_copy.h"
#include "remesa_twod_copy_uapi.h"
#include "remesa_twod_native.h"
static bool twod_copy_enabled;
module_param_named(remesa_twod_copy_enabled,twod_copy_enabled,bool,0600);
struct remesa_twod_copy_session {
 struct remesa_twod_job job;
 struct remesa_twod_native native;
 struct remesa_memory_ticket ticket;
};
/* Only nonNULL after uncertainty; retained with the quarantined GEM endpoint.
 * Never freed by FD death or a runtime enable/disable toggle. */
static struct remesa_twod_copy_session *twod_copy_retained;
static int twod_copy_command(struct remesa_gem_memory *s,
 const struct remesa_twod_copy_request *r,struct remesa_gem_twod_command *c,
 struct remesa_memory_binding uses[2],unsigned *count)
{
 const struct remesa_twod_copy_surface *surface[2]={&r->source,&r->destination};
 struct remesa_twod_surface *output[2]={&c->copy.source,&c->copy.destination};
 struct remesa_twod_copy_plan plan;unsigned i;int error=0;
 mutex_lock(&s->lock);
 if(s->executor||s->stream_bound||s->ta_cache_domains){error=-EBUSY;goto done;}
 for(i=0;i<2;i++){
  u32 id=surface[i]->id;struct backing *b=&s->bo[id&31];
  if(!id||!b->live||b->generation!=id>>5){error=-ENOENT;goto done;}
  *output[i]=(struct remesa_twod_surface){id,b->va.start,b->gem.size,
   surface[i]->width,surface[i]->height,surface[i]->stride,REMESA_TWOD_ARGB8888};
  uses[i]=(struct remesa_memory_binding){id,b->va.start,b->gem.size};
 }
 c->schema=REMESA_GEM_TWOD_VENDOR_SCHEMA;
 c->copy.request_base=(u32)(c->copy.source.gpu_address&0xf0000000ULL);
 c->copy.source_x=r->source_x;c->copy.source_y=r->source_y;
 c->copy.destination_x=r->destination_x;c->copy.destination_y=r->destination_y;
 c->copy.width=r->width;c->copy.height=r->height;
 error=remesa_twod_copy_prepare_vendor(&c->copy,&plan);
 if(error){error=-error;goto done;}
 *count=r->source.id==r->destination.id?1:2;
 done:mutex_unlock(&s->lock);return error;
}
int remesa_twod_copy_ioctl(struct drm_device *dev,void *data,struct drm_file *file)
{
 struct remesa_twod_copy_request *r=data;
 struct remesa_gem_twod_command command={0};struct remesa_memory_binding uses[2];
 struct remesa_twod_copy_session *session;struct remesa_gem_memory *s;
 enum remesa_job_state state;unsigned count=0;int error;
 if(!dev||!r||!file||!capable(CAP_SYS_ADMIN))return -EPERM;
 if(!memory_enabled||!twod_copy_enabled)return -EOPNOTSUPP;
 if(r->version!=REMESA_TWOD_COPY_VERSION||r->flags||r->reserved||r->response_reserved||
    !r->context||!r->budget_ms||r->budget_ms>10000||r->operation||r->cookie||
    r->stages||r->retained||r->core||r->revision||r->error)return -EINVAL;
 mutex_lock(&endpoint_lock);s=endpoint;
 if(!s||s->dev!=dev||s->file!=file||s->context!=r->context){error=-EACCES;goto done;}
#ifdef REMESA_GENERAL_SCENE
 if(s->private_session){error=-EBUSY;goto done;}
#endif
 if(twod_copy_retained){error=-EBUSY;goto done;}
 error=twod_copy_command(s,r,&command,uses,&count);if(error)goto done;
 session=kzalloc(sizeof(*session),GFP_KERNEL);if(!session){error=-ENOMEM;goto done;}
 error=remesa_gem_job_prepare(s,uses,count,&command,sizeof(command),&session->ticket);
 if(error){kfree(session);goto done;}
 /* The heap context is published BEFORE possible start; every uncertain
  * return can safely retain callback pointers beyond this syscall/FD. */
 twod_copy_retained=session;r->operation=session->ticket.operation;r->stages=REMESA_TWOD_COPY_PREPARED;
 error=remesa_gem_twod_issue(s,session->ticket,&remesa_twod_native_executor,
  &session->native,&session->job);

 r->cookie=session->job.cookie;r->core=session->native.core;r->revision=session->native.revision;
 if(error)goto outcome;
 r->stages|=REMESA_TWOD_COPY_STARTED;
 error=remesa_gem_twod_wait(&session->job,r->budget_ms);
 if(error)goto outcome;
 r->stages|=REMESA_TWOD_COPY_COMPLETE;
 error=remesa_gem_twod_publish(&session->job);
 if(error)goto outcome;
 r->stages|=REMESA_TWOD_COPY_VISIBLE;
 error=remesa_gem_twod_retire(&session->job);
 if(error)goto outcome;
 r->stages|=REMESA_TWOD_COPY_RETIRED;
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
