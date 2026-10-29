/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual eleven-role borrower, contract, service and source lifecycle.
 * Backend execution and event samples are MODELED; no GPU/MMIO/readback. */
#include "remesa_stream_contract_aliases.h"
#define REMESA_STREAM_SOURCE_TEST
#include "remesa_stream_source.c"
#include "source_legacy_stubs.inc"
#include "remesa_borrowed_owner.h"
static unsigned checks,stage_calls,samples_seen,verifies;
static const char *scenario;
static struct remesa_memory_view roles[SGX535_BO_COUNT];
static struct page *page_storage[SGX535_BO_COUNT];
static struct remesa_memory_ticket ticket={77,1,0,1};
static struct sgx535_fixed_ops ops;
static struct sgx535_fixed_status_source status_source;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"failure line%d: %s\n",__LINE__,#x);abort();}}while(0)
static int verify(void *lender,struct drm_device *d,struct remesa_memory_ticket t,
 const struct remesa_memory_view *v,unsigned n,u64 ceiling)
{
 verifies++;
 if(lender!=roles||!d||n!=SGX535_BO_COUNT||ceiling!=0x80000000||
    memcmp(&t,&ticket,sizeof t))return -EACCES;
 for(unsigned i=0;i<n;i++)if(v[i].pages!=roles[i].pages||v[i].cpu!=roles[i].cpu||
    memcmp(&v[i].binding,&roles[i].binding,sizeof(v[i].binding)))return -EACCES;
 return 0; /* Metadata fixture only; no physical PTE ownership attested. */
}
static void tap_begin(void *c,struct sgx535_fixed_service *s,
 const struct sgx535_fixed_ops *o,const struct sgx535_fixed_status_source *src,void *operation_context)
{remesa_route_service(c,s,o==&ops&&src==&status_source&&operation_context==c);}
static void tap_before(void *b,struct sgx535_fixed_service *s,u32 q,u32 a,u32 d,int exclusive)
{remesa_route_before(b,s,q,a,d,exclusive);}
static void tap_after(void *b,struct sgx535_fixed_service *s){remesa_route_after(b,s);}
static void tap_terminal(void *b,struct sgx535_fixed_service *s,int e){remesa_route_terminal(b,s,e);}
static const struct sgx535_fixed_service_observer observer={tap_begin,tap_before,tap_after,tap_terminal};
static int model_stage(void *context,enum sgx535_fixed_stage stage,const void *payload,
 size_t count,struct sgx535_fixed_observation *out)
{
 struct sgx535_gma500_fixed_backend *b=context;
 CHECK(b->active&&b->power_held&&stage==b->last_stage+1&&out);stage_calls++;
 /* Simulated hardware work still requires the ACTUAL source-write gate. */
 if(stage>=SGX535_FIXED_INIT_WRITES&&!remesa_route_source_write(b))return -EAGAIN;
 switch(stage){
 case SGX535_FIXED_PUBLISH_CPU:CHECK(!payload&&!count);b->owner->user_images_flushed=true;break;
 case SGX535_FIXED_PUBLISH_TRANSLATIONS:CHECK(b->owner->user_images_flushed);break;
 case SGX535_FIXED_TA_LOAD:
  CHECK(count==29&&payload);out->load_flags=0x1f;out->status2=15;out->initend=0x400000;break;
 case SGX535_FIXED_USE_RESERVE:CHECK(!b->use_reserved&&count==1);b->use_reserved=true;break;
 case SGX535_FIXED_TA_FIRE:
  CHECK(count==1&&payload&&b->use_reserved);b->fire_possible=true;remesa_route_issued(b);break;
 case SGX535_FIXED_USE_RELEASE:
  CHECK(b->use_reserved&&b->owner->session.phase==SGX535_PHASE_SCENE_COMPLETED);
  b->use_reserved=false;break;
 default:break;
 }
 b->last_stage=stage;return 0;
}
static int model_sample(void *context,u32 sequence,u32 *a,u32 *d,int *exclusive)
{
 struct sgx535_gma500_fixed_backend *b=context;
 CHECK(b->active&&b->power_held&&b->fire_possible&&sequence==ticket.operation);
 samples_seen++;*d=0;*exclusive=1;
 *a=samples_seen==1?(1u<<13):((1u<<18)|1u);
 if(!strcmp(scenario,"unknown")&&samples_seen==1)*a|=1u<<27;
 if(!strcmp(scenario,"missing")&&samples_seen>1)*a=0;
 if(!strcmp(scenario,"wrongtask"))current=&other_task;
 remesa_route_source_pending(b->owner->dev,*a,*d);
 remesa_route_sample(b,!strcmp(scenario,"wrongseq")?sequence+1:sequence,*a,*d,1);
 return 0; /* Explicit synthetic status inputs, not hardware receipts. */
}
static void reader(struct drm_device *d){(void)d;}
int main(int argc,char **argv)
{
 struct sgx535_source_facts facts={.sample_valid=1};
 struct drm_device dev={&facts};struct module mod={0};int lease_token;
 struct sgx535_frozen_cpu_view images[SGX535_BO_COUNT]={0};
 struct sgx535_frozen_command_pairs commands;
 scenario=argc>1?argv[1]:"normal";
 CHECK(SGX535_BO_COUNT==11);
 for(unsigned i=0;i<SGX535_BO_COUNT;i++){
  struct sgx535_frozen_requirement r;CHECK(!sgx535_frozen_get_requirement(i,&r));
  CHECK(r.size==fixture_bytes[i]);
  roles[i].binding=(struct remesa_memory_binding){32+i,fixture_va[i],r.size};
  roles[i].page_count=r.size/4096;roles[i].pages=calloc(roles[i].page_count,sizeof(*roles[i].pages));
  page_storage[i]=calloc(roles[i].page_count,sizeof(struct page));CHECK(roles[i].pages&&page_storage[i]);
  for(unsigned k=0;k<roles[i].page_count;k++){
   roles[i].pages[k]=&page_storage[i][k];roles[i].pages[k]->pfn=100+i*10000+k;
  }
  /* Only initializer's user images are touched; large TA pools are metadata. */
  roles[i].cpu=calloc(1,i<=SGX535_BO_COLOR?r.size:1);CHECK(roles[i].cpu);
  images[i]=(struct sgx535_frozen_cpu_view){roles[i].cpu,r.size};
 }
 CHECK(!sgx535_frozen_initialize_user_images(images));
 CHECK(!sgx535_frozen_extract_commands(&images[SGX535_BO_CONTROL],&commands));
 remesa_route_source_attach(&dev,&mod,reader);remesa_route_source_startup(&dev,&facts);
 remesa_route_source_reset_begin(&dev);remesa_route_source_reset_assert(&dev,0x7f);
 remesa_route_source_reset_end(&dev,&facts);
 CHECK(!remesa_stream_source_claim(&dev,&lease_token));unsigned prior_legacy=legacy_calls;
 ops.run=model_stage;status_source.sample_and_ack=model_sample;
 for(unsigned frame=0;frame<2;frame++){
  struct sgx535_gma500_owner owner={0};struct remesa_borrowed_owner borrower={0};
  struct sgx535_gma500_fixed_backend b={0};struct remesa_stream_observation o={0};
  ticket.operation=frame+1;ticket.generation=frame+1;samples_seen=0;
  CHECK(!remesa_borrowed_owner_init(&borrower,&owner,&dev,ticket,roles,SGX535_BO_COUNT,
                                   0x80000000,&commands,verify,roles));
  CHECK(owner.borrowed_backing&&owner.initialized&&owner.fixed_service.session==&owner.session);
  CHECK(!remesa_stream_source_bind(&lease_token,&o,&b,ticket.operation,ticket.operation));
  CHECK(remesa_route_source_begin(&b,&dev));CHECK(remesa_route_source_boundary(&b,&facts));
  b.owner=&owner;b.active=true;b.power_held=true;remesa_route_admit(&b);
  owner.fixed_service.observer=&observer;owner.fixed_service.observer_context=&b;
  int result=sgx535_fixed_service_run_once(&owner.fixed_service,&ops,&status_source,
                                        &b,0x00010201,ticket.operation,100,4);
  if(strcmp(scenario,"normal")&&strcmp(scenario,"latepending")&&strcmp(scenario,"lateafterunbind")){
   CHECK(result||o.evidence.invalid);
   CHECK(remesa_stream_source_finish_nohash(&lease_token,&o));
   CHECK(remesa_borrowed_owner_deinit(&borrower)==-EBUSY&&borrower.live);
   CHECK(legacy_calls==prior_legacy);
   printf("PASS %u checks; actual11-role source/service rejected %s; modeled backend/status, GPU0\n",checks,scenario);
   return 0; /* Process exits retaining failed model state, no invented recovery. */
  }
  CHECK(!result&&owner.session.phase==SGX535_PHASE_RETIRED&&owner.session.observed_events==7);
  CHECK(owner.session.sequence==ticket.operation&&sgx535_fixed_service_may_release(&owner.fixed_service));
  CHECK(!b.use_reserved&&o.evidence.state==SGX535_CAP_TERMINAL&&!o.evidence.invalid);
  if(!strcmp(scenario,"latepending")){
   remesa_route_source_pending(&dev,1,0);CHECK(remesa_stream_source_finish_nohash(&lease_token,&o));
   CHECK(remesa_borrowed_owner_deinit(&borrower)==-EBUSY);
   printf("PASS %u checks; latepending retained after actual service retirement; GPU0\n",checks);return 0;
  }
  CHECK(!remesa_stream_source_finish_nohash(&lease_token,&o));
  CHECK(o.finished&&o.evidence.state==SGX535_CAP_CLOSED&&!o.evidence.color_present);
  CHECK(!sgx535_capsule_success(&o.evidence)); /* nohash is NOT LAB approval */
  b.active=false;b.power_held=false;CHECK(!remesa_stream_source_unbind(&lease_token,&o));
  if(!strcmp(scenario,"lateafterunbind")){
   remesa_route_source_pending(&dev,1,0);CHECK(remesa_stream_source_settled(&lease_token));
   CHECK(remesa_borrowed_owner_deinit(&borrower)==-EBUSY);
   printf("PASS %u checks; late event after unbind rejected by actual source settling; GPU0\n",checks);return 0;
  }
  CHECK(remesa_borrowed_owner_deinit(&borrower)==-EBUSY); /* observer still attached */
  owner.fixed_service.observer=NULL;owner.fixed_service.observer_context=NULL;
  CHECK(!remesa_borrowed_owner_deinit(&borrower));CHECK(!remesa_stream_source_settled(&lease_token));
 }
 CHECK(legacy_calls==prior_legacy&&verifies==4);
 CHECK(!remesa_stream_source_release(&lease_token));
 CHECK(remesa_stream_source_claim(&dev,&lease_token)); /* original no-rearm rule */
 for(unsigned i=0;i<SGX535_BO_COUNT;i++){free(roles[i].cpu);free(roles[i].pages);free(page_storage[i]);}
 printf("PASS %u checks; actual11-role source/service two operations, modeled backend/status, GPU0\n",checks);
 return 0;
}
