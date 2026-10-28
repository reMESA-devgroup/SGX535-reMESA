/* SPDX-License-Identifier: GPL-2.0-only */
/* Include after 2D lifecycle in native GEM TU. Endpoint lock precedes its
 * memory mutex; all userspace topology operations use that same outer lock. */
#include "remesa_twod_lab.h"
#include "remesa_twod_lab_uapi.h"
#include "remesa_twod_native.h"
#ifdef __KERNEL__
#include <linux/ktime.h>
#endif
static bool twod_lab_enabled;
module_param_named(remesa_twod_lab_enabled,twod_lab_enabled,bool,0600);
struct remesa_twod_lab_session {
 struct remesa_twod_job job;
 struct remesa_twod_native native;
 struct remesa_memory_ticket ticket;
};
/* Only nonNULL after uncertainty; retained with the quarantined GEM endpoint.
 * Never freed by FD death or a runtime enable/disable toggle. */
static struct remesa_twod_lab_session *twod_lab_retained;
static int twod_lab_command(struct remesa_gem_memory *s,
 const struct remesa_twod_lab_request *r,struct remesa_gem_twod_command *c,
 struct remesa_memory_binding uses[2],unsigned *count)
{
 const struct remesa_twod_lab_surface *surface[2]={&r->source,&r->destination};
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
 c->schema=REMESA_GEM_TWOD_COPY_SCHEMA;
 c->copy.request_base=(u32)(c->copy.source.gpu_address&0xf0000000ULL);
 c->copy.source_x=r->source_x;c->copy.source_y=r->source_y;
 c->copy.destination_x=r->destination_x;c->copy.destination_y=r->destination_y;
 c->copy.width=r->width;c->copy.height=r->height;
 error=remesa_twod_copy_prepare(&c->copy,&plan);
 if(error){error=-error;goto done;}
 *count=r->source.id==r->destination.id?1:2;
 done:mutex_unlock(&s->lock);return error;
}
int remesa_twod_lab_ioctl(struct drm_device *dev,void *data,struct drm_file *file)
{
 struct remesa_twod_lab_request *r=data;
 struct remesa_gem_twod_command command={0};struct remesa_memory_binding uses[2];
 struct remesa_twod_lab_session *session;struct remesa_gem_memory *s;
 enum remesa_job_state state;u64 start;unsigned i,count=0;int error;
 if(!dev||!r||!file||!capable(CAP_SYS_ADMIN))return -EPERM;
 if(!memory_enabled||!twod_lab_enabled)return -EOPNOTSUPP;
 if(r->version!=REMESA_TWOD_LAB_VERSION||r->flags||r->reserved||r->response_reserved||
    !r->context||!r->budget_ms||r->budget_ms>10000||r->operation||r->cookie||
    r->stages||r->retained||r->core||r->revision||r->error)return -EINVAL;
 for(i=0;i<4;i++)if(r->phase_ns[i])return -EINVAL;
 mutex_lock(&endpoint_lock);s=endpoint;
 if(!s||s->dev!=dev||s->file!=file||s->context!=r->context){error=-EACCES;goto done;}
 if(twod_lab_retained){error=-EBUSY;goto done;}
 error=twod_lab_command(s,r,&command,uses,&count);if(error)goto done;
 session=kzalloc(sizeof(*session),GFP_KERNEL);if(!session){error=-ENOMEM;goto done;}
 error=remesa_gem_job_prepare(s,uses,count,&command,sizeof(command),&session->ticket);
 if(error){kfree(session);goto done;}
 /* The heap context is published BEFORE possible start; every uncertain
  * return can safely retain callback pointers beyond this syscall/FD. */
 twod_lab_retained=session;r->operation=session->ticket.operation;r->stages=REMESA_TWOD_LAB_PREPARED;
 start=ktime_get_ns();
 error=remesa_gem_twod_issue(s,session->ticket,&remesa_twod_native_executor,
  &session->native,&session->job);
 r->phase_ns[0]=ktime_get_ns()-start;
 r->cookie=session->job.cookie;r->core=session->native.core;r->revision=session->native.revision;
 if(error)goto outcome;
 r->stages|=REMESA_TWOD_LAB_STARTED;
 start=ktime_get_ns();error=remesa_gem_twod_wait(&session->job,r->budget_ms);
 r->phase_ns[1]=ktime_get_ns()-start;if(error)goto outcome;
 r->stages|=REMESA_TWOD_LAB_COMPLETE;
 start=ktime_get_ns();error=remesa_gem_twod_publish(&session->job);
 r->phase_ns[2]=ktime_get_ns()-start;if(error)goto outcome;
 r->stages|=REMESA_TWOD_LAB_VISIBLE;
 start=ktime_get_ns();error=remesa_gem_twod_retire(&session->job);
 r->phase_ns[3]=ktime_get_ns()-start;if(error)goto outcome;
 r->stages|=REMESA_TWOD_LAB_RETIRED;
 twod_lab_retained=NULL;kfree(session);
 outcome:
 if(error){
  /* Only a known unissued PREPARED job may be cancelled. Faulted/attached
   * or possibly issued jobs and their context remain quarantined. */
  if(!session->job.live&&!remesa_gem_job_state(s,session->ticket,&state)&&
     state==REMESA_JOB_PREPARED&&!remesa_gem_job_cancel(s,session->ticket)){
   twod_lab_retained=NULL;kfree(session);
  }
 }
 r->error=error;r->retained=twod_lab_retained!=NULL;
 error=0; /* Explicit structured result, never an ioctl-success GPU oracle. */
 done:mutex_unlock(&endpoint_lock);return error;
}
