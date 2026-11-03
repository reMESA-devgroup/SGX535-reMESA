/* SPDX-License-Identifier: GPL-2.0-only. After existing scene helpers. */
#include "remesa_gem_scene_job.h"
#ifdef __KERNEL__
#include "remesa_matrix_draw_domain.h"
#else
#include "remesa/matrix_draw_domain.h"
#endif
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
static int scene_job_prepare_owned(struct remesa_gem_memory *s,
 const struct remesa_memory_binding *bindings,unsigned count,const void *commands,unsigned bytes,
 const struct remesa_gem_scene_sections *sections,const struct remesa_gem_private_rule *rules,
 struct remesa_twod_reservation *guard,struct remesa_gem_scene_job *c,const void *session_owner)
{
 struct remesa_gem_scene_action_command action;struct retained_job *j;
 u32 mask=0;unsigned i;int error,cleanup;
 if(!s||!bindings||!commands||!bytes||!sections||!rules||!guard||!c||
    !count||count>REMESA_GEM_JOB_BINDINGS)return -EINVAL;
 if(c->memory||c->state!=REMESA_SCENE_JOB_EMPTY||c->loan.live)return -EBUSY;
#ifndef REMESA_PRIVATE_DPM_CACHE
 if(session_owner)return -EOPNOTSUPP;
#endif
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
#ifdef REMESA_PRIVATE_DPM_CACHE
 if(session_owner)j->private_scene_owner=session_owner;
#endif
 read_done:mutex_unlock(&s->lock);if(error)goto failed;
 error=remesa_gem_job_claim_zero_dpm(s,c->snapshot.ticket,action.table_id,action.parameter_id);
#ifdef REMESA_PRIVATE_DPM_CACHE
 if(error==-ENODATA&&session_owner)error=remesa_private_session_claim_warm_dpm(s,session_owner,c->snapshot.ticket,action.table_id,action.parameter_id);
#endif
 if(error)goto failed;
#ifdef REMESA_PRIVATE_DPM_CACHE
 if(session_owner){error=scene_region_initialize(s,c->snapshot.ticket,sections->actions,session_owner);if(error)goto failed;}
#endif
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
int remesa_gem_scene_job_prepare(struct remesa_gem_memory *s,
 const struct remesa_memory_binding *b,unsigned n,const void *commands,unsigned bytes,
 const struct remesa_gem_scene_sections *sections,const struct remesa_gem_private_rule *rules,
 struct remesa_twod_reservation *guard,struct remesa_gem_scene_job *c)
{return scene_job_prepare_owned(s,b,n,commands,bytes,sections,rules,guard,c,NULL);}

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

static const u8 *scene_pds_image_view(const struct remesa_gem_scene_job *c,u32 id,u32 offset,u32 bytes)
{
 unsigned i;
 for(i=0;i<c->count&&c->views[i].binding.id!=id;i++);
 if(i==c->count||!c->views[i].cpu||c->loan.rules[i].access!=REMESA_MMU_GPU_READ||
    c->loan.rules[i].cached!=1||offset>c->views[i].binding.bytes||
    bytes>c->views[i].binding.bytes-offset)return NULL;
 return (const u8 *)c->views[i].cpu+offset;
}
int remesa_gem_scene_job_pds_images_check_locked(struct remesa_gem_scene_job *c)
{
 struct retained_job *j;unsigned n,r;int error;const u8 *image;
 if(!c||!c->memory||c->state!=REMESA_SCENE_JOB_PREPARED||!c->loan.live||
    c->loan.memory!=c->memory||!c->count||c->count>REMESA_GEM_JOB_BINDINGS||
    c->loan.execution_started||c->loan.host_published||c->service.started)return -EINVAL;
 if(c->loan.ticket.generation!=c->snapshot.ticket.generation||
    c->loan.ticket.operation!=c->snapshot.ticket.operation)return -EACCES;
 j=ticket_job(c->memory,c->snapshot.ticket);
 if(!j||j->state!=REMESA_JOB_PREPARED||j->commands!=c->snapshot.commands||
    j->command_bytes!=c->snapshot.command_bytes)return -EACCES;
 error=remesa_gem_scene_event_check_locked(&c->loan,c->views,c->count,&c->actions,&c->controls,&c->use,c->sections.vertices,
  &c->ta,&c->states,&c->vertices,&c->fragments,&c->background,&c->event);if(error)return error;
 error=scene_state_images_build_checked(c->views,c->count,&c->states);if(error)return error;
 for(n=0;n<c->vertices.count;n++){
  struct remesa_vertex_fetch decoded;
  for(r=0;r<c->ta.reference_count;r++)if(c->ta.references[r].draw==n&&
     c->ta.references[r].kind==(n?REMESA_TA_REF_DRAW_VERTEX:REMESA_TA_REF_PREFIX_VERTEX))break;
  if(r==c->ta.reference_count)return -EPROTO;
  image=scene_pds_image_view(c,c->ta.references[r].id,c->ta.references[r].byte_offset,64);
  if(!image)return -EACCES;
  error=remesa_vertex_fetch_pds_image(image,64,c->vertices.draws[n].stride_bytes/4,n?1:0,&decoded);
  if(error)return -error;
 }
 for(n=0;n<c->fragments.count;n++){
  struct remesa_fragment_task decoded;
  image=scene_pds_image_view(c,c->fragments.draws[n].primary_id,c->fragments.draws[n].primary_offset,
   c->fragments.draws[n].primary_bytes);if(!image)return -EACCES;
  error=remesa_fragment_primary_image(image,c->fragments.draws[n].primary_bytes,n?1:0,&decoded);
  if(error)return -error;
 }
 {
  struct remesa_background_task decoded;
  image=scene_pds_image_view(c,c->background.primary_id,c->background.primary_offset,64);
  if(!image)return -EACCES;
  error=remesa_background_primary_image(image,64,c->controls.command.width,c->controls.command.height,&decoded);
  if(error)return -error;
 }
 {
  struct remesa_event_fetch decoded;
  image=scene_pds_image_view(c,c->event.event_id,c->event.event_offset,148);if(!image)return -EACCES;
  error=remesa_event_fetch_image(image,148,&decoded);if(error)return -error;
 }
 return 0;
}

int remesa_gem_scene_job_matrix_values_check_locked(struct remesa_gem_scene_job *c)
{
 unsigned r,n,parameter,seen=0;const u8 *coefficients,*vertices;int error;
 error=remesa_gem_scene_job_pds_images_check_locked(c);if(error)return error;
 if(c->ta.draw_count!=1||c->ta.reference_count!=9||c->states.count!=5||
    c->ta.references[0].kind!=REMESA_TA_REF_INITIAL_STATE||c->vertices.count!=2||c->fragments.count!=2||
    c->vertices.draws[1].use_bytes!=344||c->vertices.draws[1].stride_bytes!=48||
    c->controls.command.schema!=2||c->controls.command.depth_finalize!=1||
    c->controls.command.depth_cpp!=4||c->controls.command.clear_depth_bits!=0x3f800000U)return -EOPNOTSUPP;
 parameter=c->states.count;
 for(r=0;r<c->ta.reference_count;r++)if(c->ta.references[r].draw==1&&
    c->ta.references[r].kind==REMESA_TA_REF_DRAW_PARAMETERS){
  for(n=0;n<c->states.count;n++)if(c->states.uploads[n].pds_id==c->ta.references[r].id&&
     c->states.uploads[n].pds_offset==c->ta.references[r].byte_offset){
   if(parameter!=c->states.count)return -EPROTO;
   parameter=n;
  }
 }
 if(parameter==c->states.count||c->states.uploads[parameter].source_bytes!=160)return -EOPNOTSUPP;
 coefficients=scene_pds_image_view(c,c->states.uploads[parameter].source_id,
  c->states.uploads[parameter].source_offset,160);
 vertices=scene_pds_image_view(c,c->vertices.draws[1].vertex_id,c->vertices.draws[1].vertex_offset,
  c->vertices.draws[1].vertex_bytes);
 if(!coefficients||!vertices)return -EACCES;
 error=remesa_matrix_draw_values(vertices,c->vertices.draws[1].vertex_bytes,
  c->vertices.draws[1].vertex_bytes/48,48,coefficients+4,144);
 if(error)return -error;
 for(r=0;r<c->ta.reference_count;r++){
  unsigned kind=c->ta.references[r].kind,bit;
  const u8 *state;
  if(kind!=REMESA_TA_REF_PREFIX_STATE&&kind!=REMESA_TA_REF_DRAW_STATE)continue;
  bit=kind==REMESA_TA_REF_DRAW_STATE?2:1;
  if(seen&bit)return -EPROTO;
  seen|=bit;
  for(n=0;n<c->states.count;n++)if(c->states.uploads[n].pds_id==c->ta.references[r].id&&
     c->states.uploads[n].pds_offset==c->ta.references[r].byte_offset)break;
  if(n==c->states.count)return -EPROTO;
  state=scene_pds_image_view(c,c->states.uploads[n].source_id,c->states.uploads[n].source_offset,
   c->states.uploads[n].source_bytes);if(!state)return -EACCES;
  if(bit==2)error=remesa_matrix_draw_state_values(state,c->states.uploads[n].source_bytes,
   c->controls.command.width,c->controls.command.height);
  else{
   const u8 *prefix=scene_pds_image_view(c,c->vertices.draws[0].vertex_id,c->vertices.draws[0].vertex_offset,
    c->vertices.draws[0].vertex_bytes);
   const u8 *indices=scene_pds_image_view(c,c->vertices.draws[0].index_id,c->vertices.draws[0].index_offset,
    c->vertices.draws[0].index_bytes);
   error=remesa_matrix_prefix_values(state,c->states.uploads[n].source_bytes,prefix,c->vertices.draws[0].vertex_bytes,
    indices,c->vertices.draws[0].index_bytes,c->controls.command.width,c->controls.command.height);
  }
  if(error)return -error;
 }
 return seen==3?0:-EPROTO;
}

/* Default-PD authentication is deliberately before the MMU writer interval.
 * The original all-view checker remains unchanged. Writable targets must have
 * no borrowed CPU alias in this selected-view contract. */
int remesa_gem_scene_job_check_snapshot_locked(struct remesa_gem_scene_job *c)
{
 struct remesa_gem_memory *s;struct retained_job *j;
 struct remesa_memory_binding uses[REMESA_GEM_JOB_BINDINGS];unsigned i;int error;
 if(!c||!c->memory||c->state!=REMESA_SCENE_JOB_PREPARED||!c->loan.live||
    c->loan.memory!=c->memory||!c->count||c->count>REMESA_GEM_JOB_BINDINGS)return -EINVAL;
 s=c->memory;
#ifdef __KERNEL__
 lockdep_assert_held(&s->lock);
#endif
 if(c->loan.ticket.generation!=c->snapshot.ticket.generation||
    c->loan.ticket.operation!=c->snapshot.ticket.operation)return -EACCES;
 j=ticket_job(s,c->snapshot.ticket);if(!j)return -ENOENT;
 if(j->quarantined)return -EIO;
 if(j->state!=REMESA_JOB_PREPARED||j->cookie||!j->attached||
    s->executor!=&private_reserved_executor||s->executor_context!=&c->loan)return -EBUSY;
 if(c->snapshot.dev!=s->dev||c->snapshot.mmu_end!=s->root.end+1||
    c->snapshot.commands!=j->commands||c->snapshot.command_bytes!=j->command_bytes||
    !c->snapshot.exclusive_held||!c->snapshot.power_held||c->count!=j->count)return -EACCES;
 error=verify_job_bindings(s,j,uses);if(error)return job_fault(s,j,error);
 for(i=0;i<c->count;i++){
  const struct remesa_memory_view *v=&c->views[i];struct backing *b=&s->bo[j->ids[i]&31];
  if(v->binding.id!=uses[i].id||v->binding.va!=uses[i].va||v->binding.bytes!=uses[i].bytes||
     v->pages!=b->pages||v->page_count!=b->gem.size/PAGE_SIZE||c->loan.rules[i].id!=j->ids[i])return -EACCES;
  if(c->loan.rules[i].access==REMESA_MMU_GPU_READ){
   if(!v->cpu||v->cpu!=j->cpu_views[i]||v->cpu!=b->cached_job_view)return -EACCES;
  }else if(v->cpu)return -EACCES;
 }
 return 0;
}
int remesa_gem_scene_job_retire(struct remesa_gem_scene_job *c)
{
 int error;
 if(!c||!c->memory||c->state!=REMESA_SCENE_JOB_ACKNOWLEDGED||!c->loan.execution_retired)return -EINVAL;
 error=remesa_gem_private_release(&c->loan);
 if(!error)error=remesa_gem_job_retire(c->memory,c->snapshot.ticket);
 if(error){c->state=REMESA_SCENE_JOB_HELD;return error;}
 memset(c,0,sizeof(*c));return 0;
}
