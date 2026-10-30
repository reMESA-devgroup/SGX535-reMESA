/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual LAB controller/envelope and initial canonical comparison; dependencies
 * (GEM/leases/source/borrower/cold execution/retirement) modeled, GPU0. */
#define REMESA_PRIVATE_LAB_HANDLER_TEST
#include "remesa_canonical_inputs.h"
#define main original_probe_fixture_main
#include "test_private_probe_handler.c"
#undef main
#include "remesa_private_session.h"
#define DEFINE_MUTEX(name) struct mutex name={0}
static unsigned quarantines,seal_calls,source_poisons;
void remesa_stream_source_poison(const void *p)
{CHECK(source_held&&source_token==p);source_poisons++;}
int remesa_gem_job_quarantine(struct remesa_gem_memory *m,struct remesa_memory_ticket t)
{CHECK(ticket_job(m,t));quarantines++;return 0;}
int remesa_private_session_acquire(struct drm_device *d,struct drm_file *f,u64 c,
 const void *owner,struct remesa_gem_memory **out)
{
 *out=NULL;if(endpoint->dev!=d||endpoint->file!=f||endpoint->context!=c)return -EACCES;
 if(endpoint->private_session||endpoint->stream_bound)return -EBUSY;
 endpoint->private_session=owner;*out=endpoint;return 0;
}
int remesa_private_session_release(struct remesa_gem_memory *m,const void *who)
{
 CHECK(m==endpoint&&m->private_session==who);
 if(m->jobs[0].live||m->private_source_held||m->private_source_poisoned)return -EBUSY;
 m->private_session=NULL;memset(m->private_input_ids,0,sizeof(m->private_input_ids));return 0;
}
int remesa_private_session_seal_inputs(struct remesa_gem_memory *m,const void *who,
 struct remesa_memory_ticket t,int (*guard)(u64,const void*,unsigned))
{
 CHECK(m==endpoint&&m->private_session==who&&ticket_job(m,t)&&guard);seal_calls++;
 for(unsigned i=0;i<5;i++)m->private_input_ids[i]=32+i;
 return 0;
}
int remesa_private_session_inputs_locked(struct remesa_gem_memory *m,struct remesa_memory_ticket t)
{CHECK(m->lock.held&&ticket_job(m,t));for(unsigned i=0;i<5;i++)CHECK(m->private_input_ids[i]==32+i);return 0;}
int remesa_private_session_source_begin(struct remesa_gem_memory *m,const void *p)
{
 CHECK(!m->jobs[0].live&&m->private_session==p);
 int e=remesa_stream_source_claim(m->dev,p);if(!e)m->private_source_held=true;return e;
}
int remesa_private_session_source_end(struct remesa_gem_memory *m,const void *p)
{
 CHECK(m->private_session==p&&!m->jobs[0].live&&m->private_source_held);
 int e=remesa_stream_source_settled(p);if(!e)e=remesa_stream_source_release(p);
 if(e)m->private_source_poisoned=true;else m->private_source_held=false;return e;
}
int remesa_private_session_execute(struct remesa_gem_memory *m,const void *who,
 struct remesa_gem_private *l,struct sgx535_gma500_owner *o,
 struct sgx535_gma500_fixed_backend *b,struct remesa_stream_observation *obs,
 const struct remesa_memory_view *v,unsigned n,unsigned ticks,unsigned samples)
{
 CHECK(m==endpoint&&m->private_session==who&&m->private_source_held);
 int e=remesa_gem_private_execute(l,o,b,obs,who,v,n,ticks,samples);
 if(e)m->private_source_poisoned=true;
 return e;
}
#include "../remesa_private_session_lab.inc"
static struct remesa_private_session_lab_request packet(unsigned action,u64 sequence)
{return (struct remesa_private_session_lab_request){.version=1,.action=action,.context=77,.session=1,.sequence=sequence,.budget_ms=action==2?1000:0};}
int main(int argc,char **argv)
{
 unsigned scenario=argc>1?strtoul(argv[1],NULL,0):0;CHECK(scenario<24);
 struct drm_file file={0},other={0};struct drm_psb_private priv={.sgx_reg=(void*)1};struct drm_device dev={&priv};
 struct remesa_gem_memory memory={.dev=&dev,.file=&file,.context=77,.root={.end=0xdfffffffULL}};endpoint=&memory;
 struct remesa_private_session_lab_request r={.version=1,.action=1,.context=77,.max_jobs=2};
 private_probe_enabled=true;
 for(unsigned i=0;i<11;i++){
  r.ids[i]=32+i;memory.bo[i].live=true;memory.bo[i].generation=1;
  memory.bo[i].va.start=private_probe_profile.bindings[i].va;memory.bo[i].gem.size=private_probe_profile.bindings[i].bytes;
 }
 switch(scenario){
  case 1:administrator=false;break;case 2:private_probe_enabled=false;break;case 3:r.flags=1;break;
  case 4:memory.file=&other;break;case 5:fault=CORE;break;case 6:fault=PREPARE;break;
  case 7:fault=SNAPSHOT;break;case 8:fault=HASH;break;case 9:fault=MASK;break;
  case 10:fault=GEOMETRY;break;case 11:fault=CLAIM;break;
  case 12:memory.private_session=&other;break;case 13:r.ids[1]=r.ids[0];break;
 }
 int e=remesa_private_session_lab_ioctl(&dev,&r,&file);
 CHECK(!endpoint_lock.held&&!memory.lock.held&&!private_lab_lock.held);
 if(scenario>=1&&scenario<=13){
  CHECK(e<0||r.error<0);CHECK(!executions&&!retirements&&!source_held&&!private_lab_active);
  CHECK(!private_probe_consumed&&allocations==frees);
  printf("PASS LAB open-denial scenario%u; modeled dependencies GPU0\n",scenario);return 0;
 }
 CHECK(!e&&!r.error&&r.state==1&&r.stages==16&&r.session==1);
 CHECK(private_probe_consumed&&source_held&&claims==1&&cancels==1&&seal_calls==1&&hashes==0&&masks==1&&geometries==1);
 if(scenario==21||scenario==22){
  fault=scenario==21?SETTLE:SOURCE_CLOSE;r=packet(3,0);
  CHECK(!remesa_private_session_lab_ioctl(&dev,&r,&file)&&r.error<0&&r.state==2);
  CHECK(private_lab_active&&source_held&&memory.private_session&&memory.private_source_poisoned&&!executions);
 }else{
  const unsigned faults[]={EXEC,EARLY_EXEC,NO_RECEIPT,BORROW_CLOSE,LOAN_CLOSE,GUARD_CLOSE,JOB_CLOSE};
  if(scenario>=14&&scenario<=20)fault=faults[scenario-14];
  r=packet(2,1);e=remesa_private_session_lab_ioctl(&dev,&r,&file);
  if(scenario>=14&&scenario<=20){
   CHECK(!e&&r.error<0&&r.state==2&&r.stages!=15&&quarantines&&private_lab_active&&source_held);
   unsigned before=executions;r=packet(2,1);CHECK(!remesa_private_session_lab_ioctl(&dev,&r,&file)&&r.error==-EIO&&r.state==2&&executions==before);
   r=packet(3,0);CHECK(!remesa_private_session_lab_ioctl(&dev,&r,&file)&&r.error==-EIO&&r.state==2);
  }else{
   CHECK(!e&&!r.error&&r.state==1&&r.stages==15&&r.operation==2&&r.cookie==2&&r.phase==9&&r.ledger==7);
   unsigned before=executions;r=packet(2,1);CHECK(remesa_private_session_lab_ioctl(&dev,&r,&file)==-ESTALE&&executions==before);
   r=packet(2,2);CHECK(!remesa_private_session_lab_ioctl(&dev,&r,&file)&&!r.error&&r.stages==15&&r.operation==3&&r.cookie==3);
   r=packet(2,3);CHECK(remesa_private_session_lab_ioctl(&dev,&r,&file)==-ESTALE&&executions==2);
   CHECK(hashes==0);
   CHECK(masks==1&&geometries==1&&seal_calls==1&&claims==1&&!source_closes&&retirements==2);
   if(scenario==23)memory_enabled=private_probe_enabled=false;
   r=packet(3,2);CHECK(!remesa_private_session_lab_ioctl(&dev,&r,&file)&&!r.error&&r.state==3&&r.stages==32);
   CHECK(!private_lab_active&&!memory.private_session&&!source_held&&source_closes==1&&allocations==frees);
   r=(struct remesa_private_session_lab_request){.version=1,.action=1,.context=77,.max_jobs=1};
   for(unsigned i=0;i<11;i++)r.ids[i]=32+i;
   CHECK(remesa_private_session_lab_ioctl(&dev,&r,&file)<0&&!private_lab_active);
  }
 }
 CHECK(!endpoint_lock.held&&!memory.lock.held&&!private_lab_lock.held);
 printf("PASS LAB controller scenario%u; %u checks; actual envelope/canonical comparison, source/execution MODELED GPU0\n",scenario,checks);return 0;
}
