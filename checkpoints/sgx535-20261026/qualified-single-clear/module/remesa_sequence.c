/* SPDX-License-Identifier: GPL-2.0-only */
#include "remesa_sequence.h"
#ifndef REMESA_MEMORY_TEST
#include <linux/errno.h>
#include <linux/string.h>
#endif
static bool terminal(enum remesa_sequence_state s)
{return s==REMESA_SEQUENCE_FREE || s==REMESA_SEQUENCE_RETIRED || s==REMESA_SEQUENCE_CANCELLED;}
static struct remesa_sequence_record *record(struct remesa_sequence *s,struct remesa_sequence_fence f)
{
 struct remesa_sequence_record *r;
 if(f.slot>=REMESA_SEQUENCE_DEPTH || !f.serial || !f.generation)return NULL;
 r=&s->records[f.slot];
 if(r->state==REMESA_SEQUENCE_FREE || r->fence.serial!=f.serial || r->fence.generation!=f.generation ||
    r->fence.ticket.context!=f.ticket.context || r->fence.ticket.operation!=f.ticket.operation ||
    r->fence.ticket.slot!=f.ticket.slot || r->fence.ticket.generation!=f.ticket.generation)return NULL;
 return r;
}
int remesa_sequence_init(struct remesa_sequence *s,struct remesa_gem_memory *m)
{
 if(!s || !m)return -EINVAL;
 memset(s,0,sizeof(*s));mutex_init(&s->lock);s->memory=m;return 0;
}
int remesa_sequence_prepare(struct remesa_sequence *s,const struct remesa_memory_binding *b,
                            unsigned n,const void *c,unsigned bytes,struct remesa_sequence_fence *out)
{
 unsigned i;int e=-ENOSPC;struct remesa_sequence_record *r;struct remesa_memory_ticket t;
 if(!s || !out)return -EINVAL;
 mutex_lock(&s->lock);
 if(s->closing || s->failed){e=-ESHUTDOWN;goto done;}
 if(s->serial==~(u64)0){e=-EOVERFLOW;goto done;}
 for(i=0;i<REMESA_SEQUENCE_DEPTH;i++)if(terminal(s->records[i].state) && s->records[i].fence.generation!=~(u32)0)break;
 if(i==REMESA_SEQUENCE_DEPTH)goto done;
 e=remesa_gem_job_prepare(s->memory,b,n,c,bytes,&t);if(e)goto done;
 r=&s->records[i];r->fence.ticket=t;r->fence.serial=++s->serial;
 r->fence.slot=i;r->fence.generation++;r->state=REMESA_SEQUENCE_PREPARED;r->error=0;
 *out=r->fence;
done:mutex_unlock(&s->lock);return e;
}
static int issue_locked(struct remesa_sequence *s,struct remesa_sequence_fence f)
{
 struct remesa_sequence_record *r;unsigned i;int e=-ENOENT;enum remesa_job_state js;
 if(!s)return -EINVAL;
 r=record(s,f);if(!r)goto done;
 if(s->closing || s->failed){e=-ESHUTDOWN;goto done;}
 if(r->state!=REMESA_SEQUENCE_PREPARED){e=-EBUSY;goto done;}
 for(i=0;i<REMESA_SEQUENCE_DEPTH;i++)if(!terminal(s->records[i].state) &&
     s->records[i].fence.serial<f.serial){e=-EBUSY;goto done;}
 e=remesa_gem_job_issue(s->memory,f.ticket);
 if(!e)r->state=REMESA_SEQUENCE_ISSUED;
 else if(!remesa_gem_job_state(s->memory,f.ticket,&js) && js==REMESA_JOB_QUARANTINED){
  r->state=REMESA_SEQUENCE_QUARANTINED;r->error=e;s->failed=true;
 }
done:return e;
}
static int transition_locked(struct remesa_sequence *s,struct remesa_sequence_fence f,
                      enum remesa_sequence_state before,enum remesa_sequence_state after,unsigned budget)
{
 struct remesa_sequence_record *r;int e=-ENOENT;enum remesa_job_state js;
 if(!s)return -EINVAL;
 r=record(s,f);if(!r)goto done;
 if(r->state!=before){e=-EBUSY;goto done;}
 /* Closing admission does not prevent safe completion of previously issued work. */
 switch(before){
 case REMESA_SEQUENCE_ISSUED:e=remesa_gem_job_wait(s->memory,f.ticket,budget);break;
 case REMESA_SEQUENCE_COMPLETE:e=remesa_gem_job_publish(s->memory,f.ticket);break;
 case REMESA_SEQUENCE_VISIBLE:e=remesa_gem_job_acknowledge(s->memory,f.ticket);break;
 case REMESA_SEQUENCE_ACKNOWLEDGED:e=remesa_gem_job_retire(s->memory,f.ticket);break;
 default:e=-EINVAL;break;
 }
 if(!e)r->state=after;
 else if(!remesa_gem_job_state(s->memory,f.ticket,&js) && js==REMESA_JOB_QUARANTINED){
  r->state=REMESA_SEQUENCE_QUARANTINED;r->error=e;s->failed=true;
 }
done:return e;
}
int remesa_sequence_issue(struct remesa_sequence *s,struct remesa_sequence_fence f)
{
 int e;if(!s)return -EINVAL;mutex_lock(&s->lock);
 e=issue_locked(s,f);mutex_unlock(&s->lock);return e;
}
static int transition(struct remesa_sequence *s,struct remesa_sequence_fence f,
 enum remesa_sequence_state before,enum remesa_sequence_state after,unsigned budget)
{
 int e;if(!s)return -EINVAL;mutex_lock(&s->lock);
 e=transition_locked(s,f,before,after,budget);mutex_unlock(&s->lock);return e;
}
int remesa_sequence_execute(struct remesa_sequence *s,struct remesa_sequence_fence f,
 unsigned budget,int (*detach)(void *,struct remesa_memory_ticket),void *context)
{
 struct remesa_sequence_record *r;enum remesa_job_state js;int e;
 /* A malformed wait budget must not first consume an execution attempt. */
 if(!s || !budget || budget>10000 || (!detach && context))return -EINVAL;
 mutex_lock(&s->lock);
 e=issue_locked(s,f);if(e)goto done;
 e=transition_locked(s,f,REMESA_SEQUENCE_ISSUED,REMESA_SEQUENCE_COMPLETE,budget);
 if(e)goto done;
 e=transition_locked(s,f,REMESA_SEQUENCE_COMPLETE,REMESA_SEQUENCE_VISIBLE,0);
 if(e)goto done;
 e=transition_locked(s,f,REMESA_SEQUENCE_VISIBLE,REMESA_SEQUENCE_ACKNOWLEDGED,0);
 if(e)goto done;
 /* Borrowers release aliases/adapter references before backing is retired.
  * Detach failure retains the acknowledged job; never retry the GPU operation. */
 if(detach){
  e=detach(context,f.ticket);
  if(e){
   r=record(s,f);
   if(r){
    r->error=e;
    if(!remesa_gem_job_state(s->memory,f.ticket,&js) && js==REMESA_JOB_QUARANTINED)
     r->state=REMESA_SEQUENCE_QUARANTINED;
   }
   s->failed=true;goto done;
  }
 }
 e=transition_locked(s,f,REMESA_SEQUENCE_ACKNOWLEDGED,REMESA_SEQUENCE_RETIRED,0);
 if(e){r=record(s,f);if(r){r->error=e;s->failed=true;}}
done:mutex_unlock(&s->lock);return e;
}
int remesa_sequence_wait(struct remesa_sequence *s,struct remesa_sequence_fence f,unsigned budget)
{if(!budget)return -EINVAL;return transition(s,f,REMESA_SEQUENCE_ISSUED,REMESA_SEQUENCE_COMPLETE,budget);}
int remesa_sequence_publish(struct remesa_sequence *s,struct remesa_sequence_fence f)
{return transition(s,f,REMESA_SEQUENCE_COMPLETE,REMESA_SEQUENCE_VISIBLE,0);}
int remesa_sequence_acknowledge(struct remesa_sequence *s,struct remesa_sequence_fence f)
{return transition(s,f,REMESA_SEQUENCE_VISIBLE,REMESA_SEQUENCE_ACKNOWLEDGED,0);}
int remesa_sequence_retire(struct remesa_sequence *s,struct remesa_sequence_fence f)
{return transition(s,f,REMESA_SEQUENCE_ACKNOWLEDGED,REMESA_SEQUENCE_RETIRED,0);}
int remesa_sequence_cancel(struct remesa_sequence *s,struct remesa_sequence_fence f)
{
 struct remesa_sequence_record *r;int e=-ENOENT;
 if(!s)return -EINVAL;
 mutex_lock(&s->lock);r=record(s,f);if(!r)goto done;
 if(r->state!=REMESA_SEQUENCE_PREPARED){e=-EBUSY;goto done;}
 e=remesa_gem_job_cancel(s->memory,f.ticket);if(!e)r->state=REMESA_SEQUENCE_CANCELLED;
done:mutex_unlock(&s->lock);return e;
}
int remesa_sequence_query(struct remesa_sequence *s,struct remesa_sequence_fence f,
                          enum remesa_sequence_state *state,int *error)
{
 struct remesa_sequence_record *r;int e=-ENOENT;
 if(!s || !state || !error)return -EINVAL;
 mutex_lock(&s->lock);r=record(s,f);
 if(r){*state=r->state;*error=r->error;e=0;}mutex_unlock(&s->lock);return e;
}
int remesa_sequence_close(struct remesa_sequence *s)
{
 unsigned i;int e=0,k;
 if(!s)return -EINVAL;
 mutex_lock(&s->lock);s->closing=true;
 for(i=0;i<REMESA_SEQUENCE_DEPTH;i++){
  struct remesa_sequence_record *r=&s->records[i];
  if(r->state==REMESA_SEQUENCE_PREPARED){
   k=remesa_gem_job_cancel(s->memory,r->fence.ticket);
   if(k){e=k;s->failed=true;}else r->state=REMESA_SEQUENCE_CANCELLED;
  }else if(!terminal(r->state))e=-EBUSY;
 }
 mutex_unlock(&s->lock);return e;
}
