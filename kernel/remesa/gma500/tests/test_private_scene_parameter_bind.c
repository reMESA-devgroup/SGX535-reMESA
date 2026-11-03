/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual memory/seals/binder. A validated retired scene and decoded TA/PARAM
 * plan are injected, not executed. No GPU/source/MMIO callback is permitted. */
#define main old_input_fixture
#include "test_private_inputs.c"
#undef main
#include "remesa/ta_references.h"
enum {REMESA_SCENE_JOB_PREPARED=1,REMESA_SCENE_JOB_HELD,REMESA_SCENE_JOB_ACKNOWLEDGED};
#define REMESA_GEM_SCENE_STATE_MAX (2u*REMESA_TA_REFERENCE_MAX_DRAWS+3u)
/* Only consumed scene-plan fields are modeled; full module builds use the real
 * remesa_gem_scene_job.h. All endpoint, ticket, BO and hold checks are real. */
struct remesa_gem_scene_job {
 struct remesa_gem_memory *memory;unsigned state,count;
 struct remesa_memory_snapshot snapshot;struct remesa_gem_private loan;
 struct remesa_memory_view views[32];
 struct {struct remesa_memory_ticket ticket;u32 draw_count,reference_count;
  struct {u32 id,byte_offset,kind,draw;} references[REMESA_TA_REFERENCE_MAX];} ta;
 struct {struct remesa_memory_ticket ticket;u32 count;
  struct {u32 pds_id,pds_offset,source_id,source_offset,source_bytes,use_id,use_offset;}
   uploads[REMESA_GEM_SCENE_STATE_MAX];} states;
};
#define REMESA_SCENE_PARAMETER_BIND_TEST
#include "remesa_private_scene_parameter_bind.inc"
static u32 scene_ids[18];
static struct remesa_memory_binding scene_uses[18];
static struct remesa_gem_scene_job scene;
static struct remesa_memory_ticket ticket;
static int bind(unsigned draw,struct remesa_parameter_patch_policy *out)
{return remesa_private_session_bind_scene_parameters(memory,&token,&scene,draw,0x44800000,out);}
int main(void)
{
 CHECK(!old_input_fixture());
 CHECK(!remesa_gem_memory_open(&dev,&file,77,&memory));endpoint=memory;
 for(unsigned i=0;i<18;i++){
  u64 bytes=i==15||i==16?33554432:4096;
  u64 va=i==15?0x30100000:i==16?0x50000000:0x20001000+i*4096;
  struct remesa_memory_request r={.version=1,.action=REMESA_MEM_CREATE,.context=77,.bytes=bytes};
  CHECK(!remesa_gem_memory_request(memory,&file,&r));scene_ids[i]=r.id;
  CHECK(!request(REMESA_MEM_PIN,r.id,0,0,NULL,0));r.action=REMESA_MEM_MAP;r.va=va;r.bytes=0;
  CHECK(!remesa_gem_memory_request(memory,&file,&r));scene_uses[i]=(struct remesa_memory_binding){r.id,va,bytes};
 }
 struct remesa_gem_memory *out=NULL;u32 command=0;
 CHECK(!remesa_private_session_acquire(&dev,&file,77,&token,&out));
 CHECK(!remesa_gem_job_prepare(memory,scene_uses,18,&command,4,&ticket));
 CHECK(!remesa_gem_job_claim_zero_dpm(memory,ticket,scene_ids[16],scene_ids[15]));
 CHECK(!remesa_private_session_seal_dpm(memory,&token,ticket,scene_ids[16],scene_ids[15]));
 CHECK(!remesa_private_session_seal_scene_inputs(memory,&token,ticket,0x1fff));
 struct retained_job *j=ticket_job(memory,ticket);CHECK(j);
 /* Explicit injected prior retirement and validator output. */
 j->state=REMESA_JOB_ACKNOWLEDGED;j->cookie=j->operation;j->attached=true;j->private_scene_owner=&token;
 memory->private_source_held=true;memory->private_dpm_ready=true;memory->private_dpm_retired_operation=ticket.operation;
 scene.memory=memory;scene.state=REMESA_SCENE_JOB_ACKNOWLEDGED;scene.count=18;
 scene.snapshot.ticket=scene.loan.ticket=scene.ta.ticket=scene.states.ticket=ticket;scene.loan.memory=memory;
 for(unsigned i=0;i<18;i++)scene.views[i].binding=scene_uses[i];
 scene.ta.draw_count=scene.ta.reference_count=scene.states.count=2;
 for(unsigned i=0;i<2;i++){
  unsigned id=scene_ids[10+i];scene.ta.references[i].id=id;scene.ta.references[i].draw=i+1;
  scene.ta.references[i].kind=REMESA_TA_REF_DRAW_PARAMETERS;scene.ta.references[i].byte_offset=192;
  scene.states.uploads[i].pds_id=scene.states.uploads[i].source_id=scene.states.uploads[i].use_id=id;
  scene.states.uploads[i].pds_offset=192;scene.states.uploads[i].source_bytes=160;scene.states.uploads[i].use_offset=160;
 }
 struct remesa_gem_scene_job saved=scene;
 struct remesa_parameter_patch_policy policy,untouched;memset(&untouched,0xa5,sizeof untouched);
 unsigned maps=vmap_calls,flushed=flushes,zeroed=test_zeroed_pages;
 for(unsigned draw=1;draw<=2;draw++){
  CHECK(!bind(draw,&policy)&&remesa_parameter_window_valid(&policy.window));
  CHECK(policy.window.owner==77&&policy.window.id==scene_ids[9+draw]&&policy.window.generation==1&&
   policy.window.gpu_va==scene_uses[9+draw].va&&policy.window.coefficient_dwords==36&&
   policy.window.effective_source_dwords==37&&policy.window.transfer_dwords==40&&
   policy.window.use_offset==160&&policy.window.pds_offset==192&&policy.window.ta_offset==256);
 }
 for(unsigned fault=0;fault<25;fault++){
  struct backing *b=&memory->bo[scene_ids[10]&31],bo=*b;
  switch(fault){
   case 0:scene.state=REMESA_SCENE_JOB_PREPARED;break;case 1:scene.states.ticket.generation++;break;
   case 2:scene.ta.ticket.operation++;break;case 3:scene.snapshot.ticket.context++;break;
   case 4:memory->private_dpm_ready=false;break;case 5:memory->private_dpm_retired_operation++;break;
   case 6:j->cookie++;break;case 7:j->attached=false;break;case 8:j->private_scene_owner=&other;break;
   case 9:scene.ta.draw_count=0;break;case 10:scene.ta.references[0].kind=0;break;
   case 11:scene.ta.references[1].draw=1;break;case 12:scene.states.uploads[0].pds_offset++;break;
   case 13:scene.states.uploads[0].source_id=scene_ids[9];break;
   case 14:scene.states.uploads[0].source_bytes=156;break;case 15:scene.states.uploads[0].use_offset++;break;
   case 16:scene.views[10].binding.va++;break;case 17:scene.views[10].binding.bytes++;break;
   case 18:b->generation++;break;case 19:b->holds++;break;case 20:b->lease=true;break;
   case 21:memory->private_source_poisoned=true;break;case 22:memory->private_device_lost=true;break;
   case 23:scene.states.count=REMESA_GEM_SCENE_STATE_MAX+1;break;
   case 24:scene.ta.reference_count=REMESA_TA_REFERENCE_MAX+1;break;
  }
  memcpy(&policy,&untouched,sizeof policy);CHECK(bind(1,&policy)!=0&&!memcmp(&policy,&untouched,sizeof policy));
  *b=bo;scene=saved;j->cookie=j->operation;j->attached=true;j->private_scene_owner=&token;
  memory->private_dpm_ready=true;memory->private_dpm_retired_operation=ticket.operation;
  memory->private_source_poisoned=false;memory->private_device_lost=false;
 }
 CHECK(bind(0,&policy)==-EINVAL&&bind(3,&policy)==-EPROTO);
 CHECK(bind(1,(void *)memory)==-EINVAL&&bind(1,(void *)&scene)==-EINVAL);
 CHECK(remesa_private_session_bind_scene_parameters((void *)1,&token,&scene,1,0x44800000,&policy)==-EACCES);
 CHECK(remesa_private_session_bind_scene_parameters(memory,&other,&scene,1,0x44800000,&policy)==-EACCES);
 CHECK(remesa_private_session_bind_scene_parameters(memory,&token,&scene,1,0x7f800000,&policy)==-EINVAL);
 /* Two draws can reuse the same decoded PARAM source. */
 scene.ta.references[1].id=scene.ta.references[0].id;scene.states.uploads[1]=scene.states.uploads[0];
 CHECK(!bind(2,&policy)&&policy.window.id==scene_ids[10]);
 scene.states.uploads[1].use_offset++;memcpy(&policy,&untouched,sizeof policy);
 CHECK(bind(1,&policy)==-EPROTO&&!memcmp(&policy,&untouched,sizeof policy));scene=saved;
 CHECK(vmap_calls==maps&&flushes==flushed&&test_zeroed_pages==zeroed);
 memory->private_source_held=false;j->attached=false;j->cookie=0;j->state=REMESA_JOB_PREPARED;
 CHECK(!remesa_gem_job_cancel(memory,ticket)&&!remesa_private_session_release(memory,&token));
 CHECK(!remesa_gem_memory_close(memory));endpoint=memory=NULL;
 CHECK(!gems&&!pins&&!ptes&&!device_refs&&!module_refs&&!power&&!claimed);
 printf("PASS %u actual-core PARAM binding/identity/retirement/plan checks, injected decoder outputs; GPU0\n",checks);return 0;
}
