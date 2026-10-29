/* SPDX-License-Identifier: GPL-2.0-only */
/* Separate single-operation qualification entry. Normal FAST/LAB unchanged. */
#include "remesa_private_probe.h"
#include "remesa_private_probe_uapi.h"
#include "remesa_private_probe_request.h"
#include "remesa_stream_trusted.h"
#include "remesa_stream_grammar.h"
#include "remesa_depth1280_profile.h"
#define gears_stream_profile_asset private_probe_profile
#include "stream_profile_asset.h"
#undef gears_stream_profile_asset
static bool private_probe_enabled;
module_param_named(remesa_private_probe_enabled,private_probe_enabled,bool,0600);
struct remesa_private_probe {
 struct remesa_gem_memory *memory;
 struct remesa_memory_snapshot snapshot;
 struct remesa_memory_view views[11];
 struct sgx535_gma500_owner owner;
 struct remesa_borrowed_owner borrower;
 struct remesa_gem_private loan;
 struct remesa_twod_reservation guard;
 struct sgx535_gma500_fixed_backend backend;
 struct remesa_stream_observation observation;
 bool images_authenticated,source_claimed,execution_consumed;
};
static struct remesa_private_probe *private_probe_retained;
static bool private_probe_consumed;
static int private_probe_verify(void *opaque,struct drm_device *dev,
 struct remesa_memory_ticket ticket,const struct remesa_memory_view *v,unsigned count,u64 end)
{
 struct remesa_private_probe *p=opaque;
 if(!p->images_authenticated||dev!=p->snapshot.dev||end!=p->snapshot.mmu_end||
    count!=11||v!=p->views||memcmp(&ticket,&p->snapshot.ticket,sizeof(ticket)))return -EACCES;
 return remesa_gem_job_check_snapshot(p->memory,&p->snapshot,v,count);
}
static int private_probe_images(struct remesa_private_probe *p)
{
 unsigned i;u8 digest[32];void *masked;int error;
 for(i=0;i<5;i++){
  const void *bytes=p->views[i].cpu;u64 length=p->views[i].binding.bytes;
  if(i==SGX535_BO_PDS){
   masked=vmalloc(length);if(!masked)return -ENOMEM;
   memcpy(masked,bytes,length);
   error=remesa_stream_mask_sources(private_probe_profile.batches,
           private_probe_profile.count,masked,length,p->views[i].binding.va);
   if(!error)error=remesa_depth1280_sha256(masked,length,digest);
   vfree(masked);if(error)return error;
  }else{
   error=remesa_depth1280_sha256(bytes,length,digest);if(error)return error;
  }
  if(memcmp(digest,private_probe_profile.images[i],sizeof(digest)))return -EKEYREJECTED;
 }
 error=remesa_stream_validate_geometry(private_probe_profile.batches,
         private_probe_profile.count,p->views[SGX535_BO_VERTEX_TA].cpu,
         p->views[SGX535_BO_VERTEX_TA].binding.bytes);
 if(!error)p->images_authenticated=true;
 return error;
}
/* Only pre-attempt cleanup. Once source bind consumed execution, even an
 * unissued error requires retained source/root/owner state, not guessed close. */
static int private_probe_cancel(struct remesa_private_probe *p)
{
 int error;
 if(p->execution_consumed||p->loan.execution_attempted)return -EBUSY;
 if(p->source_claimed){
  error=remesa_stream_source_release(p);if(error)return error;p->source_claimed=false;
 }
 if(p->loan.live){error=remesa_gem_private_release(&p->loan);if(error)return error;}
 if(p->guard.held){error=remesa_twod_release(p->guard.device,&p->guard);if(error)return error;}
 if(p->borrower.live){error=remesa_stream_remesa_borrowed_owner_deinit(&p->borrower);if(error)return error;}
 if(p->snapshot.ticket.operation){
  error=remesa_gem_job_cancel(p->memory,p->snapshot.ticket);if(error)return error;
 }
 return 0;
}
int remesa_private_probe_ioctl(struct drm_device *dev,void *data,struct drm_file *file)
{
 struct remesa_private_probe_request *r=data;struct remesa_private_probe *p;
 struct remesa_memory_binding uses[11];struct remesa_gem_private_rule rules[11];
 struct remesa_gem_memory *s;struct drm_psb_private *dev_priv;unsigned i;
 struct retained_job *job;int error,cleanup;
 BUILD_BUG_ON(sizeof(*r)!=112);BUILD_BUG_ON(SGX535_BO_COUNT!=11);
 if(!dev||!r||!file||!capable(CAP_SYS_ADMIN))return -EPERM;
 if(!memory_enabled||!private_probe_enabled)return -EOPNOTSUPP;
 if(!remesa_private_probe_request_valid(r))return -EINVAL;
 mutex_lock(&endpoint_lock);s=endpoint;
 if(!s||s->dev!=dev||s->file!=file||s->context!=r->context){error=-EACCES;goto done;}
 if(private_probe_retained||private_probe_consumed){error=-EBUSY;goto done;}
 dev_priv=dev->dev_private;
 if(!dev_priv||!dev_priv->sgx_reg||!gma_power_is_on(dev)){error=-ENODEV;goto done;}
 r->core=PSB_RSGX32(PSB_CR_CORE_ID);r->revision=PSB_RSGX32(PSB_CR_CORE_REVISION);
 if(r->core!=0x01130000u||r->revision!=0x00010201u){error=-ENODEV;goto outcome_no_context;}
 /* Derive every binding from actual file-owned GEM metadata, then require the
  * exact retained kernel profile before preparing any job or root. */
 mutex_lock(&s->lock);error=0;
 if(s->executor||s->stream_bound||s->ta_cache_domains){error=-EBUSY;goto bindings_done;}
 if(s->root.end+1!=0xe0000000ULL){error=-EACCES;goto bindings_done;}
 for(i=0;i<8;i++)if(s->jobs[i].live){error=-EBUSY;goto bindings_done;}
 for(i=0;i<11;i++){
  u32 id=r->ids[i];struct backing *b=&s->bo[id&31];
  if(!id||!b->live||b->generation!=id>>5){error=-ENOENT;break;}
  if(b->va.start!=private_probe_profile.bindings[i].va||
     b->gem.size!=private_probe_profile.bindings[i].bytes){error=-EACCES;break;}
  uses[i]=(struct remesa_memory_binding){id,b->va.start,b->gem.size};
 }
 bindings_done:mutex_unlock(&s->lock);if(error)goto outcome_no_context;
 p=kzalloc(sizeof(*p),GFP_KERNEL);if(!p){error=-ENOMEM;goto outcome_no_context;}
 p->memory=s;private_probe_retained=p;
 error=remesa_gem_job_prepare(s,uses,11,&private_probe_profile.commands,
                  sizeof(private_probe_profile.commands),&p->snapshot.ticket);
 if(error)goto failed;
 r->operation=p->snapshot.ticket.operation;r->stages=REMESA_PRIVATE_PROBE_PREPARED;
 error=remesa_gem_job_snapshot(s,p->snapshot.ticket,&p->snapshot,p->views,11);
 if(error!=11){error=error<0?error:-EPROTO;goto failed;}
 error=private_probe_images(p);if(error)goto failed;
 /* Separate one-shot LAB initialization, never inserted into FAST frames. */
 for(i=5;i<10;i++)memset(p->views[i].cpu,0,p->views[i].binding.bytes);
 for(i=0;i<p->views[10].binding.bytes/sizeof(u32);i++)((u32*)p->views[10].cpu)[i]=0x00ffffff;
 error=remesa_stream_remesa_borrowed_owner_init(&p->borrower,&p->owner,dev,
    p->snapshot.ticket,p->views,11,p->snapshot.mmu_end,&private_probe_profile.commands,
    private_probe_verify,p);if(error)goto failed;
 error=remesa_twod_reserve(dev_priv,&p->guard);if(error)goto failed;
 for(i=0;i<11;i++){
  unsigned access=i<4?REMESA_MMU_GPU_READ:3u;
  if(p->owner.descriptors[i].domain==SGX535_DOMAIN_LOCAL)access=0;
  rules[i]=(struct remesa_gem_private_rule){r->ids[i],access,access?1u:0u};
 }
 error=remesa_gem_private_prepare(s,p->snapshot.ticket,rules,11,&p->guard,&p->loan);
 if(error)goto failed;
 r->stages|=REMESA_PRIVATE_PROBE_ISOLATED;
 error=remesa_stream_source_claim(dev,p);if(error)goto failed;
 p->source_claimed=true;private_probe_consumed=true;
 error=remesa_gem_private_execute(&p->loan,&p->owner,&p->backend,&p->observation,p,
              p->views,11,msecs_to_jiffies(r->budget_ms),100000);
 p->execution_consumed=p->loan.execution_attempted;
 mutex_lock(&s->lock);job=ticket_job(s,p->snapshot.ticket);r->cookie=job?job->cookie:0;mutex_unlock(&s->lock);
 if(p->loan.execution_started)r->stages|=REMESA_PRIVATE_PROBE_STARTED;
 r->phase=p->owner.session.phase;r->ledger=p->owner.session.observed_events;
 if(error)goto failed;
 if(!p->loan.execution_retired){error=-EPROTO;goto failed;}
 error=remesa_stream_remesa_borrowed_owner_deinit(&p->borrower);if(error)goto failed;
 error=remesa_stream_source_settled(p);if(error)goto failed;
 error=remesa_stream_source_release(p);if(error)goto failed;
 p->source_claimed=false;
 error=remesa_gem_private_release(&p->loan);if(error)goto failed;
 error=remesa_twod_release(dev_priv,&p->guard);if(error)goto failed;
 error=remesa_gem_job_retire(s,p->snapshot.ticket);if(error)goto failed;
 r->stages|=REMESA_PRIVATE_PROBE_RETIRED;
 private_probe_retained=NULL;kfree(p);goto outcome_no_context;
 failed:
 if(!p->execution_consumed&&!p->loan.execution_attempted){
  cleanup=private_probe_cancel(p);
  if(!cleanup){private_probe_retained=NULL;kfree(p);}
 }
 outcome_no_context:r->error=error;r->retained=private_probe_retained!=NULL;error=0;
 done:mutex_unlock(&endpoint_lock);return error;
}
