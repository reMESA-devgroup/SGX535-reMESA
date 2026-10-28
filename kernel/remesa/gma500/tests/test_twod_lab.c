/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual GEM/preflight/lifecycle/LAB endpoint, injected native ops: CPU only. */
#define main preflight_test_main
#include "test_gem_twod.c"
#undef main
#include "../remesa_gem_twod_job.inc"
#include "../remesa_twod_native.h"
static unsigned lab_mode,starts,polls,visible_calls,retires;
static u64 time_ns;
static u64 ktime_get_ns(void){return time_ns+=100;}
static int lab_start(void *opaque,const struct remesa_twod_launch *l,u64 *cookie)
{
 struct remesa_twod_native *n=opaque;struct retained_job *j=ticket_job(endpoint,l->ticket);
 CHECK(endpoint_lock.held&&endpoint->lock.held&&j&&j->attached&&j->state==REMESA_JOB_ISSUED);
#ifdef REMESA_TWOD_LAB_VENDOR_STREAM
 CHECK(l->plan.dwords==9&&l->plan.command[3]==0xa0&&l->plan.command[35]==0xf0&&l->plan.command[39]==0);
#else
 CHECK(l->plan.dwords==10&&l->plan.command[3]==0x70);
#endif
 starts++;n->live=true;n->launch=l;n->core=0x01130000;n->revision=0x00010201;
 *cookie=l->ticket.operation+100;n->cookie=*cookie;
 return lab_mode==2?-EIO:0;
}
static int lab_poll(void *opaque,const struct remesa_twod_launch *l,u64 cookie,
 unsigned budget,struct remesa_twod_completion *out)
{
 struct remesa_twod_native *n=opaque;
 CHECK(endpoint_lock.held&&endpoint->lock.held&&n->live&&n->launch==l&&cookie==n->cookie&&budget==10);
 polls++;*out=(struct remesa_twod_completion){l->ticket,cookie,REMESA_TWOD_EVENT_COMPLETE,128,0,0};
 if(lab_mode==4)out->cookie++;
 return lab_mode==3?-ETIMEDOUT:0;
}
static int lab_visible(void *opaque,const struct remesa_twod_launch *l,u64 cookie)
{(void)opaque;CHECK(endpoint_lock.held&&endpoint->lock.held&&l&&cookie);visible_calls++;return lab_mode==5?-EIO:0;}
static int lab_retire(void *opaque,const struct remesa_twod_launch *l,u64 cookie)
{
 struct remesa_twod_native *n=opaque;struct retained_job *j=ticket_job(endpoint,l->ticket);
 CHECK(endpoint_lock.held&&endpoint->lock.held&&j&&j->attached&&cookie==n->cookie);
 for(unsigned i=0;i<j->count;i++)CHECK(endpoint->bo[j->ids[i]&31].holds==1);
 retires++;if(lab_mode==6)return -EIO;memset(n,0,sizeof(*n));return 0;
}
const struct remesa_twod_native_ops remesa_twod_native_executor={lab_start,lab_poll,lab_visible,lab_retire};
#include "../remesa_twod_lab.inc"
static void refused(struct drm_device *dev,struct drm_file *who,struct remesa_twod_lab_request *r,int error)
{
 struct remesa_twod_lab_request before=*r;unsigned old_starts=starts;
 CHECK(remesa_twod_lab_ioctl(dev,r,who)==error&&!memcmp(&before,r,sizeof before));
 CHECK(starts==old_starts&&!twod_lab_retained);
}
static void lab_case(unsigned width,unsigned height,int same,unsigned fault)
{
 struct test_mmu mmu={true,{0}};struct drm_psb_private priv={&mmu,(void*)1,{0x28000000,32,0x80000000}};
 struct drm_device dev={&priv},other_dev={&priv};struct drm_file stranger={2};
 struct remesa_memory_request open={.version=1,.action=REMESA_MEM_OPEN};
 struct remesa_twod_lab_request base={.version=1,.budget_ms=10,.width=width,.height=height};
 starts=polls=visible_calls=retires=0;lab_mode=fault;memory_enabled=true;twod_lab_enabled=true;next_context=76;
 CHECK(!remesa_gem_memory_ioctl(&dev,&open,&file)&&open.context==77);base.context=open.context;
 struct remesa_gem_memory *s=endpoint;
 u32 src=create(s,width,height,0x40001000),dst=same?src:create(s,width,height,0x41000000);
 base.source=(struct remesa_twod_lab_surface){src,width,height,width*4};
 base.destination=(struct remesa_twod_lab_surface){dst,width,height,width*4};
 if(same){base.width=17;base.height=19;base.destination_x=1;base.destination_y=1;}
 struct remesa_twod_lab_request request=base;
 if(!fault&&width==32){
  twod_lab_enabled=false;refused(&dev,&file,&request,-EOPNOTSUPP);twod_lab_enabled=true;
  memory_enabled=false;refused(&dev,&file,&request,-EOPNOTSUPP);memory_enabled=true;
  admin=false;refused(&dev,&file,&request,-EPERM);admin=true;
  refused(&dev,&stranger,&request,-EACCES);refused(&other_dev,&file,&request,-EACCES);
  request.context++;refused(&dev,&file,&request,-EACCES);request=base;
  request.version++;refused(&dev,&file,&request,-EINVAL);request=base;
  request.flags=1;refused(&dev,&file,&request,-EINVAL);request=base;
  request.reserved=1;refused(&dev,&file,&request,-EINVAL);request=base;
  request.response_reserved=1;refused(&dev,&file,&request,-EINVAL);request=base;
  request.phase_ns[3]=1;refused(&dev,&file,&request,-EINVAL);request=base;
  request.operation=1;refused(&dev,&file,&request,-EINVAL);request=base;
  request.budget_ms=10001;refused(&dev,&file,&request,-EINVAL);request=base;
  request.source.id^=32;refused(&dev,&file,&request,-ENOENT);request=base;
  request.width=8;refused(&dev,&file,&request,-EOPNOTSUPP);request=base;
  fail_alloc=1;refused(&dev,&file,&request,-ENOMEM);fail_alloc=0;
  s->stream_bound=true;refused(&dev,&file,&request,-EBUSY);s->stream_bound=false;
 }
 if(fault==1)access_bad=1;
 if(fault==7)s->bo[src&31].initialized=false;
 for(unsigned iteration=0;iteration<(fault?1u:3u);iteration++){
  request=base;
  CHECK(!remesa_twod_lab_ioctl(&dev,&request,&file));
  CHECK(request.operation&&request.phase_ns[0]==100);
  if(fault){
   int expected=fault==1||fault==2||fault==5||fault==6?-EIO:fault==3?-ETIMEDOUT:fault==4?-EPROTO:-ENODATA;
   CHECK(request.error==expected&&!(request.stages&REMESA_TWOD_LAB_RETIRED));
   if(fault==7){
    CHECK(!request.retained&&!twod_lab_retained&&!s->bo[src&31].holds&&!starts);
    s->bo[src&31].initialized=true;break;
   }
   CHECK(request.retained&&twod_lab_retained&&s->bo[src&31].holds==1&&s->bo[dst&31].holds==1);
   struct remesa_twod_lab_session *saved=twod_lab_retained;
   CHECK(ticket_job(s,saved->ticket)->state==REMESA_JOB_QUARANTINED);
   CHECK((fault==1&&!starts&&!saved->job.live)||(fault!=1&&starts==1&&saved->job.live));
   remesa_gem_memory_postclose(&dev,&file);
   CHECK(endpoint==s&&!s->file&&twod_lab_retained==saved);
   CHECK(remesa_twod_lab_ioctl(&dev,&base,&stranger)==-EACCES);
   open=(struct remesa_memory_request){.version=1,.action=REMESA_MEM_OPEN};
   CHECK(remesa_gem_memory_ioctl(&dev,&open,&stranger)==-EBUSY);
   memory_enabled=false;twod_lab_enabled=false;
   CHECK(remesa_twod_lab_ioctl(&dev,&base,&file)==-EOPNOTSUPP&&twod_lab_retained==saved);
   CHECK(module_refs&&device_refs&&power&&claimed&&!endpoint_lock.held&&!s->lock.held);
   return;
  }
  CHECK(!request.error&&!request.retained&&request.stages==REMESA_TWOD_LAB_ALL&&!twod_lab_retained);
  CHECK(request.core==0x01130000&&request.revision==0x00010201&&request.cookie);
  CHECK(!s->bo[src&31].holds&&!s->bo[dst&31].holds&&!s->executor);
  for(unsigned phase=0;phase<4;phase++)CHECK(request.phase_ns[phase]==100);
  CHECK(starts==iteration+1&&polls==starts&&visible_calls==starts&&retires==starts);
 }
 open=(struct remesa_memory_request){.version=1,.action=REMESA_MEM_CLOSE,.context=77};
 CHECK(!remesa_gem_memory_ioctl(&dev,&open,&file)&&!endpoint);
 CHECK(!gems&&!pins&&!ptes&&!claimed&&!power&&!device_refs&&!module_refs);
 memory_enabled=false;twod_lab_enabled=false;
}
int main(void)
{
 _Static_assert(sizeof(struct remesa_twod_lab_request)==152,"fixed LAB ABI layout");
 lab_case(1280,800,0,0);lab_case(640,640,0,0);lab_case(641,479,0,0);lab_case(32,32,1,0);
 for(unsigned fault=1;fault<=7;fault++){
  pid_t child=fork();CHECK(child>=0);if(!child){lab_case(32,32,0,fault);_exit(0);}
  int status;CHECK(waitpid(child,&status,0)==child);CHECK(WIFEXITED(status)&&!WEXITSTATUS(status));
 }
 printf("PASS %u LAB endpoint parent checks plus seven joined retention/cancellation fault models; NO GPU\n",checks);return 0;
}
