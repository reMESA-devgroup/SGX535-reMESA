/* SPDX-License-Identifier: GPL-2.0-only. After existing scene helpers. */
#include "remesa_gem_scene_job.h"
int remesa_gem_scene_job_cancel(struct remesa_gem_scene_job *c)
{
 int error;
 if(!c||!c->memory||c->state==REMESA_SCENE_JOB_EMPTY)return -EINVAL;
 if(c->loan.execution_attempted||c->loan.execution_started||c->loan.execution_retired||c->service.started)return -EBUSY;
 if(c->loan.live){
  error=remesa_gem_private_release(&c->loan);
  if(error){c->state=REMESA_SCENE_JOB_HELD;return error;}
 }
 error=remesa_gem_job_cancel(c->memory,c->snapshot.ticket);
 if(error){c->state=REMESA_SCENE_JOB_HELD;return error;}
 memset(c,0,sizeof(*c));return 0;
}
int remesa_gem_scene_job_prepare(struct remesa_gem_memory *s,
 const struct remesa_memory_binding *bindings,unsigned count,const void *commands,unsigned bytes,
 const struct remesa_gem_scene_sections *sections,const struct remesa_gem_private_rule *rules,
 struct remesa_twod_reservation *guard,struct remesa_gem_scene_job *c)
{
 struct remesa_gem_scene_action_command action;struct retained_job *j;
 u32 mask=0;unsigned i;int error,cleanup;
 if(!s||!bindings||!commands||!bytes||!sections||!rules||!guard||!c||
    !count||count>REMESA_GEM_JOB_BINDINGS)return -EINVAL;
 if(c->memory||c->state!=REMESA_SCENE_JOB_EMPTY||c->loan.live)return -EBUSY;
 error=remesa_gem_job_prepare(s,bindings,count,commands,bytes,&c->snapshot.ticket);
 if(error)return error;
 c->memory=s;c->state=REMESA_SCENE_JOB_PREPARED;c->count=count;c->sections=*sections;
 /* Read only the authoritative copied section to choose the two existing
  * zero-origin claims. The action preparer validates its complete contract. */
 mutex_lock(&s->lock);j=ticket_job(s,c->snapshot.ticket);
 if(!j){error=-ENOENT;goto read_done;}
 if((sections->actions&3)||sections->actions>j->command_bytes||
    sizeof(action)>j->command_bytes-sections->actions){error=-EINVAL;goto read_done;}
 memcpy(&action,(const u8 *)j->commands+sections->actions,sizeof(action));error=0;
 read_done:mutex_unlock(&s->lock);if(error)goto failed;
 error=remesa_gem_job_claim_zero_dpm(s,c->snapshot.ticket,action.table_id,action.parameter_id);if(error)goto failed;
 error=remesa_gem_scene_actions_prepare(s,c->snapshot.ticket,sections->actions,&c->actions);if(error)goto failed;
 error=remesa_gem_scene_controls_prepare(s,c->snapshot.ticket,sections->actions,sections->controls,&c->controls);if(error)goto failed;
 error=remesa_gem_scene_use_prepare(s,c->snapshot.ticket,sections->actions,sections->controls,sections->use,&c->use);if(error)goto failed;
 /* GPU-writable scratch/COLOR/Z are retained without exported CPU aliases.
  * READ inputs are mapped once by the existing selected-snapshot owner. */
 for(i=0;i<count;i++)if(rules[i].access==REMESA_MMU_GPU_READ)mask|=(u32)1<<i;
 error=remesa_gem_job_snapshot_selected(s,c->snapshot.ticket,&c->snapshot,c->views,count,mask);
 if(error!=(int)count){if(error>=0)error=-EPROTO;goto failed;}
 error=remesa_gem_private_prepare(s,c->snapshot.ticket,rules,count,guard,&c->loan);if(error)goto failed;
 return 0;
 failed:
 cleanup=remesa_gem_scene_job_cancel(c);
 /* A retained partial private owner or failed cancellation must survive the
  * original error. Caller checks HELD and never frees/reissues that context. */
 if(cleanup)c->state=REMESA_SCENE_JOB_HELD;
 return error;
}

int remesa_gem_scene_job_bind_service_locked(struct remesa_gem_scene_job *c)
{
 struct retained_job *j;unsigned i;int error;
 if(!c||!c->memory||c->state!=REMESA_SCENE_JOB_PREPARED||!c->loan.live||
    c->loan.memory!=c->memory||c->loan.execution_started||c->loan.host_published)return -EINVAL;
 if(c->service.session||c->service.started)return -EALREADY;
 error=remesa_gem_scene_event_check_locked(&c->loan,c->views,c->count,&c->actions,&c->controls,&c->use,c->sections.vertices,
  &c->ta,&c->states,&c->vertices,&c->fragments,&c->background,&c->event);if(error)return error;
 j=ticket_job(c->memory,c->snapshot.ticket);
 if(!j||j->state!=REMESA_JOB_PREPARED||j->commands!=c->snapshot.commands||j->command_bytes!=c->snapshot.command_bytes)return -EACCES;
 c->service_plan.binding_count=c->count;
 c->service_plan.load_count=c->actions.load_count;c->service_plan.ta_count=c->actions.ta_count;c->service_plan.raster_count=c->actions.raster_count;
 memcpy(c->service_plan.commands.ta,c->controls.ta,sizeof(c->controls.ta));
 memcpy(c->service_plan.commands.raster,c->controls.raster,sizeof(c->controls.raster));
 memcpy(c->service_plan.load,c->actions.load,sizeof(c->actions.load));
 memcpy(c->service_plan.ta,c->actions.ta,sizeof(c->actions.ta));
 memcpy(c->service_plan.raster,c->actions.raster,sizeof(c->actions.raster));
 c->service_plan.use.mask=c->use.bank_mask;
 for(i=0;i<16;i++){c->service_plan.use.banks[i].offset=c->use.banks[i].offset;c->service_plan.use.banks[i].word=c->use.banks[i].word;}
 /* Successful real general-loan/closure validation supplies BOS_VALIDATED.
  * The fixed eleven-role request/bos admission is not called or fabricated. */
 c->session=(struct sgx535_frozen_session){.phase=SGX535_PHASE_BOS_VALIDATED};
#ifdef __KERNEL__
 error=remesa_stream_sgx535_frozen_session_advance(&c->session,SGX535_PHASE_HOST_IMAGE_FINAL);
#else
 error=sgx535_frozen_session_advance(&c->session,SGX535_PHASE_HOST_IMAGE_FINAL);
#endif
 if(error)return -EPROTO;
 return remesa_scene_service_bind(&c->service,&c->session,&c->service_plan)?-EINVAL:0;
}
