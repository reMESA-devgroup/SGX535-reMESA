/* SPDX-License-Identifier: MIT. Real portable ownership core; CPU model/GPU0. */
#include "remesa/provider.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
struct fixture {
 struct remesa_slot slots[32];struct remesa_registry registry;
 struct remesa_provider provider;struct remesa_use uses[33];
 unsigned calls,releases;enum remesa_dispatch result;unsigned release_retry;
};
static enum remesa_dispatch dispatch(void *data,const struct remesa_job *job)
{
 struct fixture *f=data;f->calls++;
 assert(job->count>=1&&job->count<=32&&job->context==77);
 for(unsigned i=0;i<job->count;i++)assert(f->slots[i].holds==1&&f->slots[i].resource.state==REMESA_IN_FLIGHT&&f->slots[i].resource.operation==job->operation);
 return f->result;
}
static int release(void *data,const struct remesa_job *job)
{
 struct fixture *f=data;f->releases++;assert(job->state==REMESA_FENCE_PUBLISHED);
 for(unsigned i=0;i<job->count;i++)assert(f->slots[i].holds==1&&f->slots[i].resource.state==REMESA_RETIRED);
 return f->release_retry?REMESA_RETRY:REMESA_OK;
}
static struct fixture *setup(void)
{
 struct fixture *f=calloc(1,sizeof(*f));assert(f);f->result=REMESA_ISSUED;
 assert(remesa_registry_init(&f->registry,f->slots,32)==REMESA_OK);
 struct remesa_backend b={dispatch,release,f};assert(remesa_provider_init(&f->provider,&f->registry,77,&b)==REMESA_OK);
 for(unsigned i=0;i<32;i++){
  struct remesa_token t;assert(remesa_registry_allocate(&f->registry,4096,&t)==REMESA_OK);assert(remesa_registry_map(&f->registry,t,0x40000000u+i*4096u,4096)==REMESA_OK);
  f->uses[i]=(struct remesa_use){t,0,4096,i&1?REMESA_ACCESS_READ:REMESA_ACCESS_READ|REMESA_ACCESS_WRITE};
 }
 return f;
}
static void mapped(const struct fixture *f)
{for(unsigned i=0;i<32;i++)assert(f->slots[i].resource.state==REMESA_MAPPED&&!f->slots[i].holds&&!f->slots[i].resource.operation&&!f->slots[i].resource.published);}
static void success(unsigned count)
{
 struct fixture *f=setup();struct remesa_fence fence={0};assert(remesa_provider_submit(&f->provider,f->uses,count,&fence)==REMESA_OK&&f->calls==1);
 struct remesa_job job;assert(remesa_provider_query(&f->provider,fence,&job)==REMESA_OK&&job.count==count&&job.operation==1);
 for(unsigned i=0;i<count;i++)assert(remesa_registry_remove(&f->registry,f->uses[i].token)==REMESA_BUSY);
 assert(remesa_provider_release(&f->provider,fence)==REMESA_BUSY&&!f->releases);
 assert(remesa_provider_complete(&f->provider,fence,78,1,1)==REMESA_INVALID);
 assert(remesa_provider_complete(&f->provider,fence,77,2,1)==REMESA_INVALID);
 assert(remesa_provider_complete(&f->provider,fence,77,1,1)==REMESA_OK);
 for(unsigned i=0;i<count;i++)assert(!f->slots[i].resource.published&&f->slots[i].holds==1);
 assert(remesa_provider_publish(&f->provider,fence,77,2)==REMESA_INVALID);
 assert(remesa_provider_publish(&f->provider,fence,77,1)==REMESA_OK);
 for(unsigned i=0;i<count;i++)assert(f->slots[i].resource.published==((f->uses[i].access&REMESA_ACCESS_WRITE)!=0));
 f->release_retry=1;assert(remesa_provider_release(&f->provider,fence)==REMESA_RETRY);
 for(unsigned i=0;i<count;i++)assert(f->slots[i].holds==1);
 f->release_retry=0;assert(remesa_provider_release(&f->provider,fence)==REMESA_OK);mapped(f);
 assert(remesa_provider_query(&f->provider,fence,&job)==REMESA_INVALID);
 struct remesa_fence second;assert(remesa_provider_submit(&f->provider,f->uses,count,&second)==REMESA_OK&&second.generation!=fence.generation);
 assert(remesa_provider_query(&f->provider,fence,&job)==REMESA_INVALID);
 assert(remesa_provider_complete(&f->provider,second,77,2,1)==REMESA_OK&&remesa_provider_publish(&f->provider,second,77,2)==REMESA_OK&&remesa_provider_release(&f->provider,second)==REMESA_OK);mapped(f);free(f);
}
static void rejected(unsigned at,unsigned fault)
{
 struct fixture *f=setup();struct remesa_fence out={5,123,456},saved=out;int expected=REMESA_INVALID;size_t count=32;
 switch(fault){
 case 0:count=33;break;case 1:f->uses[at].token.generation++;break;
 case 2:f->uses[at].token=f->uses[0].token;break;
 case 3:f->uses[at].offset=4096;f->uses[at].bytes=1;break;
 case 4:f->uses[at].bytes=4097;break;case 5:f->uses[at].access=4;break;
 case 6:f->slots[at].holds=UINT32_MAX;expected=REMESA_BUSY;break;
 default:assert(0);
 }
 assert(remesa_provider_submit(&f->provider,f->uses,count,&out)==expected&&!f->calls&&!memcmp(&out,&saved,sizeof out));
 for(unsigned i=0;i<32;i++)assert(f->slots[i].resource.state==REMESA_MAPPED&&!f->slots[i].resource.operation&&f->slots[i].holds==((fault==6&&i==at)?UINT32_MAX:0));
 assert(f->provider.next_operation==1);free(f);
}
static void failure(unsigned count,unsigned mode)
{
 struct fixture *f=setup();struct remesa_fence out={4,22,33},saved=out;
 f->result=mode==0?REMESA_NOT_ISSUED:mode==1?REMESA_ISSUANCE_AMBIGUOUS:REMESA_ISSUED;
 int result=remesa_provider_submit(&f->provider,f->uses,count,&out);assert(f->calls==1);
 if(mode==0){assert(result==REMESA_UNSUPPORTED&&!memcmp(&out,&saved,sizeof out));mapped(f);free(f);return;}
 if(mode==1)assert(result==REMESA_AMBIGUOUS);
 else{
  assert(result==REMESA_OK);
  if(mode==2){f->slots[count-1].resource.operation++;assert(remesa_provider_complete(&f->provider,out,77,1,1)==REMESA_INVALID);}
  else {assert(remesa_provider_complete(&f->provider,out,77,1,1)==REMESA_OK);assert(remesa_provider_publish(&f->provider,out,77,1)==REMESA_OK);assert(remesa_provider_fault(&f->provider,out,77,1)==REMESA_OK);}
 }
 struct remesa_job job;assert(remesa_provider_query(&f->provider,out,&job)==REMESA_OK&&job.state==REMESA_FENCE_FAILED);
 assert(remesa_provider_release(&f->provider,out)==REMESA_BUSY&&!f->releases);
 for(unsigned i=0;i<count;i++)assert(f->slots[i].holds==1&&f->slots[i].resource.state==REMESA_QUARANTINED&&remesa_registry_remove(&f->registry,f->uses[i].token)==REMESA_BUSY);
 free(f); /* Modeled retained resources only: no kernel/GEM/GPU backing. */
}
#ifdef REMESA_PROVIDER_EXPECT_LEGACY_16
int main(void)
{
 _Static_assert(REMESA_PROVIDER_MAX_USES==16,"preserved original bound");
 struct fixture *f=setup();struct remesa_fence out={4,55,66},saved=out;
 assert(remesa_provider_submit(&f->provider,f->uses,17,&out)==REMESA_INVALID&&!f->calls&&!memcmp(&out,&saved,sizeof out));mapped(f);free(f);
 puts("CONFIRMED original provider16 refuses the valid17-resource scene before dispatch; GPU0");
}
#else
int main(void)
{
 _Static_assert(REMESA_PROVIDER_MAX_USES==32,"full bounded scene graph");
 const unsigned counts[]={1,16,17,32};for(unsigned i=0;i<4;i++)success(counts[i]);
 for(unsigned i=16;i<=31;i+=15)for(unsigned f=0;f<7;f++)rejected(i,f);
 for(unsigned n=0;n<2;n++)for(unsigned f=0;f<4;f++)failure(n?32:17,f);
 puts("PASS portable provider1/16/17/32: complete holds, late-slot validation/hold rollback,33 refusal, completion/publication/ack ordering, release retry, stale fences, all-resource quarantine; GPU0");
}

#endif
