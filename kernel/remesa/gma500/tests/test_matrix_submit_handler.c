/* SPDX-License-Identifier: GPL-2.0-only. Actual new controller; memory,
 * source and execute dependencies modeled. Separate tests cover real core. */
#define main inherited_scene_controller_tests
#include "test_scene_probe_handler.c"
#undef main
static void *general_session;
static unsigned matrix_executes;
#include "../remesa_gem_twod_job.h"
/* Typed fill command construction has its own real retained-core tests. */
#include "remesa_twod_fill_uapi.h"
static int twod_fill_command(struct remesa_gem_memory *s,const struct remesa_twod_fill_request *r,
 struct remesa_gem_twod_command *command,struct remesa_memory_binding uses[2],unsigned *count)
{
 (void)command;CHECK(endpoint_lock.held&&s==endpoint);
 struct backing *b=&s->bo[r->destination.id&31];
 if(!b->live||b->generation!=r->destination.id>>5)return -ENOENT;
 if(s->executor||s->stream_bound||s->ta_cache_domains)return -EBUSY;
 uses[0]=(struct remesa_memory_binding){r->destination.id,b->va.start,b->gem.size};*count=1;return 0;
}
int remesa_gem_scene_job_execute_matrix(struct remesa_gem_scene_job *c,
 struct sgx535_gma500_fixed_backend *b,struct remesa_stream_observation *o,const void *owner,unsigned ticks,unsigned samples)
{
 matrix_executes++;
 if(fault==CASE_COUNT){c->loan.execution_attempted=true;return -EOPNOTSUPP;}
 return remesa_gem_scene_job_execute(c,b,o,owner,ticks,samples);
}
#include "../remesa_matrix_submit.inc"
#ifndef REMESA_MATRIX_CONTROLLER_FIXTURE
int main(int argc,char **argv)
{
 CHECK(argc==2);fault=(unsigned)strtoul(argv[1],NULL,10);
 struct drm_psb_private priv={(void*)1};struct drm_device dev={&priv};struct drm_file file={1};
 struct remesa_gem_memory memory={.dev=&dev,.file=&file,.context=77,.root={0,0xdfffffffULL}};
 struct remesa_scene_probe_binding bindings[18]={0};
 struct remesa_matrix_submit_request r={.version=1,.budget_ms=1000,.context=77};
 struct remesa_scene_packet *p=&r.packet;*p=(struct remesa_scene_packet){.owner=77,.serial=1,.version=1,.binding_count=18,.use_count=11,.window_count=2};
 for(unsigned i=0;i<18;i++){
  p->bindings[i].id=32+i;p->bindings[i].access=i<12||i==17?1:3;
  bindings[i].address=0x40000000u+i*4096u;bindings[i].bytes=4096;bindings[i].access=p->bindings[i].access;
  memory.bo[i]=(struct backing){.live=true,.pinned=true,.reserved=true,.initialized=true,.zero_pristine=true,
   .generation=1,.va={bindings[i].address,bindings[i].address+4095},.gem={4096}};
  images[i]=(void *)(uintptr_t)(i+1);
 }
 memory.bo[12].zero_pristine=false; /* real COLOR fill need not be a cold allocation */
 p->actions=(struct remesa_scene_packet_actions){1,32,1280,800,46,47,48,0};
 p->controls=(struct remesa_scene_packet_controls){2,80,1280,800,1280,1280,4,0x3f800000,44,45,32,0,104,42,0,68,43,0,148,1};
 for(unsigned i=0;i<11;i++)p->uses[i]=(struct remesa_scene_packet_use){32+i,0,8,4,0,0};
 p->windows[0]=(struct remesa_scene_packet_window){33,0,6,34,0,48,3,0};
 p->windows[1]=(struct remesa_scene_packet_window){35,0,6,36,0,144,3,0};
 endpoint=&memory;expected_count=18;expected_bindings=bindings;expected_reads=0;matrix_submit_enabled=true;
 switch(fault){
 case OK:break;case OK640:p->actions.width=p->actions.height=p->controls.width=p->controls.height=640;break;
 case CAP:administrator=false;break;case DISABLED:matrix_submit_enabled=false;break;
 case MEMDISABLED:memory_enabled=false;break;case INVALID:r.budget_ms=0;break;
 case POWER:powered=false;break;
 case ALLOC:case BACKING:case LOST:case CORE:case REV:case PDS:break;
 case GEN:memory.bo[0].generation++;break;case LEASE:memory.bo[0].lease=true;break;
 case HOLD:memory.bo[0].holds=1;break;case PIN:memory.bo[0].pinned=false;break;
 case RESERVED:memory.bo[0].reserved=false;break;case QUAR:memory.bo[0].quarantine=true;break;
 case VMA:memory.bo[0].color_vmas=1;break;case ZERO:memory.bo[12].initialized=false;break;
 case CPU_ALIAS:memory.bo[12].cached_job_view=(void*)1;break;case STREAM:memory.stream_bound=true;break;
 case EXECUTOR:memory.executor=(void*)1;break;case TA:memory.ta_cache_domains=true;break;
 case CEILING:memory.root.end--;break;case LIVE:memory.jobs[0].live=true;break;
 case RESERVE:case CLAIM:case PREPARE:case PREPARE_HELD:case EARLY_EXEC:case EXEC:case NO_RECEIPT:
 case LOAN_DROP:case GUARD_DROP:case JOB_DROP:case SOURCE_END:case BACKING_END:case LOST_AFTER_CLAIM:
 case CASE_COUNT:break;
 default:CHECK(0);
 }
 struct remesa_matrix_submit_request before=r;
 int error=remesa_matrix_submit_ioctl(&dev,&r,&file);
 CHECK(!memcmp(&r,&before,offsetof(struct remesa_matrix_submit_request,operation)));
 if(fault==OK||fault==OK640){
  CHECK(!error&&!r.error&&!r.retained&&r.stages==15&&r.operation==91&&r.cookie==91&&r.phase==9&&r.ledger==7);
  CHECK(matrix_executes==1&&scene_probe_consumed&&!scene_probe_retained&&allocs==frees);
  CHECK(!memory.private_session&&!memory.private_source_held&&!memory.jobs[0].live&&!memory.executor);
 }else CHECK(error||r.error);
 bool retained=fault==LOST||fault==PREPARE_HELD||fault==EXEC||fault==NO_RECEIPT||fault==LOAN_DROP||
  fault==GUARD_DROP||fault==JOB_DROP||fault==SOURCE_END||fault==BACKING_END||fault==LOST_AFTER_CLAIM||fault==CASE_COUNT;
 CHECK((scene_probe_retained!=NULL)==retained);
 if(retained)CHECK(r.retained&&memory.private_session==scene_probe_retained&&allocs>frees);
 else CHECK(!r.retained&&allocs==frees);
 CHECK(!hashes&&!scene_probe_enabled&&!scene_probe_trace_protocol);
 if(fault==CASE_COUNT)CHECK(matrix_executes==1&&!r.cookie&&r.stages==1&&r.error==-EOPNOTSUPP);
 if(scene_probe_consumed){
  struct remesa_matrix_submit_request retry=before;unsigned reads_before=reads,exec_before=matrix_executes;
  CHECK(remesa_matrix_submit_ioctl(&dev,&retry,&file)==-EBUSY&&reads==reads_before&&matrix_executes==exec_before);
 }
 printf("PASS normal matrix controller case%u retained=%u executes=%u hashes0 LABdisabled GPU0\n",fault,r.retained,matrix_executes);
 return 0;
}
#endif
