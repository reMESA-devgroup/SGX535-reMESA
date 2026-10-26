/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_MEMORY_TEST
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#endif
#include "remesa_retained_submit.h"
#ifdef REMESA_LAB_DEPTH1280_PROFILE
#include "remesa_depth1280_capture.h"
#endif
#ifndef REMESA_MEMORY_TEST
static bool retained_lab_enabled;
module_param_named(remesa_retained_lab_enabled,retained_lab_enabled,bool,0600);
MODULE_PARM_DESC(remesa_retained_lab_enabled,"Isolated single-attempt retained GEM laboratory; default off");
bool remesa_retained_lab_admitted(void) {return READ_ONCE(retained_lab_enabled);}
#elif defined(REMESA_RETAINED_IOCTL_TEST)
bool remesa_retained_lab_admitted(void) {return ioctl_test_gate;}
#else
bool remesa_retained_lab_admitted(void) {return false;}
#endif
static int detach_session(void *opaque,struct remesa_memory_ticket ticket)
{
 struct remesa_retained_submit *s=opaque;
 int e;
 (void)ticket; /* Sequence owns this sole session and exact ticket. */
 #if !defined(REMESA_RETAINED_SUBMIT_TEST) || defined(REMESA_RETAINED_IOCTL_TEST)
 e=remesa_retained_session_snapshots(s->session,&s->snapshots);if(e)return e;
#endif
 e=remesa_retained_session_detach(s->session);
 if(!e)s->session=NULL;
 return e;
}
int remesa_retained_submit_init(struct remesa_retained_submit *s,
 struct remesa_gem_memory *memory,remesa_retained_images images,void *profile,
 u32 revision,u32 ticks,u32 samples)
{
 int e;
 if(!s || !memory || !images || !profile || revision!=0x00010201 ||
    !ticks || ticks>10000 || !samples || samples>300000)return -EINVAL;
 /* Caller supplies fresh storage; reinitializing a live object is prohibited. */
 memset(s,0,sizeof(*s));mutex_init(&s->lock);
 e=remesa_sequence_init(&s->sequence,memory);if(e)return e;
 s->images=images;s->profile=profile;s->revision=revision;
 s->ticks=ticks;s->samples=samples;s->initialized=true;return 0;
}
int remesa_retained_submit_once(struct remesa_retained_submit *s,
 const struct remesa_memory_binding *bindings,unsigned count,
 const void *commands,unsigned bytes,unsigned budget,struct remesa_retained_result *out)
{
 int e,cleanup,query_error;
 if(!s || !out)return -EINVAL;
 memset(out,0,sizeof(*out));out->state=REMESA_SEQUENCE_FREE;
 if(!s->initialized || !bindings || count!=REMESA_ADAPTER_ROLES || !commands || bytes!=264 ||
    !budget || budget>10000)return out->error=-EINVAL;
 mutex_lock(&s->lock);
 if(s->closing || s->attempted){e=-EBUSY;goto done;}
#ifndef REMESA_RETAINED_SUBMIT_TEST
 /* Remains unavailable until exact profile, observer admission and physical
  * visibility have independent qualification. Never enables adapter start. */
 if(!remesa_retained_lab_admitted()){e=-EOPNOTSUPP;goto done;}
#else
 if(!s->test_admission){e=-EOPNOTSUPP;goto done;}
#endif
 s->attempted=true;out->admitted=true;
 e=remesa_sequence_prepare(&s->sequence,bindings,count,commands,bytes,&out->fence);
 if(e)goto done;
 out->state=REMESA_SEQUENCE_PREPARED;s->fence=out->fence;
 e=remesa_retained_session_create(s->sequence.memory,out->fence.ticket,
  s->images,s->profile,s->revision,s->ticks,s->samples,&s->session);
 if(e){
  /* Creation may quarantine/retain a partially initialized owner. Cancellation
   * is safe only when the authoritative service explicitly permits it. */
  cleanup=remesa_sequence_cancel(&s->sequence,out->fence);
  if(cleanup)e=cleanup;
  goto query;
 }
#ifdef REMESA_LAB_DEPTH1280_PROFILE
 e=remesa_sequence_issue(&s->sequence,out->fence);if(e)goto query;
 e=remesa_sequence_wait(&s->sequence,out->fence,budget);if(e)goto query;
 e=remesa_sequence_publish(&s->sequence,out->fence);if(e)goto query;
 e=remesa_depth1280_capture_visible(s->sequence.memory,out->fence.ticket);if(e)goto query;
 e=remesa_sequence_acknowledge(&s->sequence,out->fence);if(e)goto query;
 e=detach_session(s,out->fence.ticket);if(e)goto query;
 e=remesa_sequence_retire(&s->sequence,out->fence);
#else
 e=remesa_sequence_execute(&s->sequence,out->fence,budget,detach_session,s);
#endif
 /* Never repeat execute, detach uncertain issued work, or cancel after the
  * possible-issuance boundary. A clean pre-issue rejection may be unwound. */
query:
 query_error=remesa_sequence_query(&s->sequence,out->fence,&out->state,&cleanup);
 if(query_error && !e)e=query_error;
 out->attached=s->session!=NULL;
done:out->error=e;mutex_unlock(&s->lock);return e;
}
int remesa_retained_submit_close(struct remesa_retained_submit *s)
{
 int e;
 if(!s || !s->initialized)return -EINVAL;
 mutex_lock(&s->lock);s->closing=true;
 if(s->session){
  enum remesa_sequence_state state;int error;
  e=remesa_sequence_query(&s->sequence,s->fence,&state,&error);if(e)goto done;
  if(state==REMESA_SEQUENCE_ACKNOWLEDGED){
   e=detach_session(s,s->fence.ticket);if(e)goto done;
   e=remesa_sequence_retire(&s->sequence,s->fence);if(e)goto done;
  }else {e=remesa_retained_session_detach(s->session);if(e)goto done;s->session=NULL;}
 }
 e=remesa_sequence_close(&s->sequence);
 if(!e)s->initialized=false;
done:mutex_unlock(&s->lock);return e;
}
#ifdef REMESA_RETAINED_SUBMIT_TEST
int remesa_retained_submit_test_admit(struct remesa_retained_submit *s)
{if(!s || !s->initialized || s->attempted || s->closing)return -EINVAL;
 s->test_admission=true;return 0;}
#endif
