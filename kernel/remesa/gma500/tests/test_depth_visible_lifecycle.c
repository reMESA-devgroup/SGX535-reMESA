/* SPDX-License-Identifier: MIT */
/* CPU integration only: ORIGINAL GEM + sequence + read-only capture endpoint.
 * No production controller or executor. Synthetic bytes are NOT GPU evidence. */
#define REMESA_MEMORY_TEST
#include "remesa_gem_memory.c"
#include "remesa_sequence.c"
#include "../remesa_gem_visible_readback.inc"
#include "../remesa_gem_visible_ioctl.inc"
#include <stdio.h>
#define IMAGE_BYTES 4096000u
static unsigned mode,starts,polls,publishes,acks;
static struct drm_file owner={1};
static int validate(void *p,const void *c,unsigned n,const struct remesa_memory_binding *b,unsigned count)
{(void)p;(void)c;assert(n==8&&count==1&&b[0].bytes==IMAGE_BYTES);return mode==2?-EINVAL:0;}
static int start(void *p,struct remesa_memory_ticket t,const void *c,unsigned n,u64 *cookie)
{(void)p;(void)c;(void)n;starts++;*cookie=t.operation+1;return mode==3?-EIO:0;}
static int poll_job(void *p,struct remesa_memory_ticket t,u64 cookie,unsigned budget,struct remesa_job_completion *out)
{(void)p;assert(budget==1);polls++;*out=(struct remesa_job_completion){t,cookie,7};if(mode==5)out->ticket.operation++;return mode==4?-ETIMEDOUT:0;}
static int publish(void *p,struct remesa_memory_ticket t,u64 cookie)
{(void)p;(void)t;(void)cookie;publishes++;return mode==6?-EIO:0;}
static int ack(void *p,struct remesa_memory_ticket t,u64 cookie)
{(void)p;(void)t;(void)cookie;acks++;return mode==10?-EIO:0;}
static const struct remesa_job_executor executor={validate,start,poll_job,publish,ack};
static int action(struct remesa_gem_memory *s,unsigned a,u32 id)
{struct remesa_memory_request r={.version=1,.context=77,.action=a,.id=id};return remesa_gem_memory_request(s,&owner,&r);}
static struct remesa_visible_readback_request request(struct remesa_sequence_fence f,u32 id,u64 offset)
{return (struct remesa_visible_readback_request){.version=1,.id=id,.context=f.ticket.context,.operation=f.ticket.operation,.slot=f.ticket.slot,.generation=f.ticket.generation,.length=4096,.offset=offset};}
static void state(struct remesa_sequence *s,struct remesa_sequence_fence f,enum remesa_sequence_state expected)
{enum remesa_sequence_state actual;int e;assert(!remesa_sequence_query(s,f,&actual,&e)&&actual==expected);}
int main(int argc,char **argv)
{
 assert(argc==3);mode=(unsigned)atoi(argv[1]);assert(mode<=12);
 struct test_mmu mmu={true,{0}};struct drm_psb_private priv={&mmu,(void *)1,{0x28000000,32,0x80000000}};
 struct drm_device dev={&priv};struct remesa_gem_memory *memory=NULL;
 assert(!remesa_gem_memory_open(&dev,&owner,77,&memory));endpoint=memory;
 struct remesa_memory_request alloc={.version=1,.context=77,.action=REMESA_MEM_CREATE,.bytes=IMAGE_BYTES};
 assert(!remesa_gem_memory_request(memory,&owner,&alloc));u32 id=alloc.id;
 assert(!action(memory,REMESA_MEM_PIN,id));alloc.bytes=0;alloc.action=REMESA_MEM_MAP;alloc.va=0x40000000;
 assert(!remesa_gem_memory_request(memory,&owner,&alloc));
 struct backing *bo=&memory->bo[id&31];
 for(size_t i=0;i<IMAGE_BYTES;i++)bo->gem.bytes[i]=(unsigned char)((i>>12)^i);
 struct remesa_sequence sequence;struct remesa_sequence_fence fence;
 struct remesa_memory_binding binding={id,0x40000000,IMAGE_BYTES};unsigned char commands[8]={0};
 assert(!remesa_sequence_init(&sequence,memory));
 fail_alloc=1;assert(remesa_sequence_prepare(&sequence,&binding,1,commands,8,&fence)==-ENOMEM);
 assert(!bo->holds&&!sequence.serial);fail_alloc=0;
 assert(!remesa_sequence_prepare(&sequence,&binding,1,commands,8,&fence));
 state(&sequence,fence,REMESA_SEQUENCE_PREPARED);
 struct remesa_visible_readback_request r=request(fence,id,0),saved=r;
 memory_enabled=true;
 assert(remesa_gem_visible_readback_ioctl(&dev,&r,&owner)==-EBUSY&&!memcmp(&r,&saved,sizeof r));
 if(mode==1){assert(!remesa_sequence_cancel(&sequence,fence));goto clean;}
 /* There is no default executor, including at the split-phase entry. */
 assert(remesa_sequence_issue(&sequence,fence)==-EOPNOTSUPP&&!starts);
 if(mode==12){
  /* Original synchronous convenience path retires before returning. */
  memory->executor=&executor;
  assert(!remesa_sequence_execute(&sequence,fence,1,NULL,NULL));
  state(&sequence,fence,REMESA_SEQUENCE_RETIRED);
  assert(remesa_gem_visible_readback_ioctl(&dev,&r,&owner)==-ENOENT);
  goto clean;
 }
 memory->executor=&executor;
 int e=remesa_sequence_issue(&sequence,fence);
 if(mode==2){assert(e==-EINVAL&&!starts);state(&sequence,fence,REMESA_SEQUENCE_PREPARED);assert(!remesa_sequence_cancel(&sequence,fence));goto clean;}
 if(mode==3){assert(e==-EIO);goto quarantine;}
 assert(!e&&starts==1);
 assert(remesa_sequence_issue(&sequence,fence)==-EBUSY&&starts==1);
 assert(remesa_sequence_cancel(&sequence,fence)==-EBUSY);
 assert(remesa_gem_visible_readback_ioctl(&dev,&r,&owner)==-EBUSY);
 e=remesa_sequence_wait(&sequence,fence,1);
 if(mode==4||mode==5){assert(e==(mode==4?-ETIMEDOUT:-EPROTO));goto quarantine;}
 assert(!e);state(&sequence,fence,REMESA_SEQUENCE_COMPLETE);
 assert(remesa_gem_visible_readback_ioctl(&dev,&r,&owner)==-EBUSY);
 e=remesa_sequence_publish(&sequence,fence);
 if(mode==6){assert(e==-EIO);goto quarantine;}
 assert(!e);state(&sequence,fence,REMESA_SEQUENCE_VISIBLE);
 assert(!acks&&bo->holds==1);
 /* Actual endpoint capture into independently owned retained CPU storage. */
 unsigned char *capture=malloc(IMAGE_BYTES);assert(capture);memset(capture,0xcc,IMAGE_BYTES);
 for(unsigned page=0;page<1000;page++) {
  r=request(fence,id,(u64)page*4096);saved=r;
  if(page==500&&mode==7)test_pt[999].pfn++;
  if(page==500&&mode==8)fail_vmap=1;
  if(page==500&&mode==9){
   assert(remesa_sequence_close(&sequence)==-EBUSY&&remesa_gem_memory_close(memory)==-EBUSY);
   assert(capture[page*4096]==0xcc&&bo->holds==1&&!acks);
   /* Interrupted reader keeps the entire job held; resume same job, no reissue. */
  }
  e=remesa_gem_visible_readback_ioctl(&dev,&r,&owner);
  if(page==500&&(mode==7||mode==8)) {
   assert(e==(mode==7?-EIO:-ENOMEM)&&!memcmp(&r,&saved,sizeof r));
   assert(capture[page*4096]==0xcc&&bo->holds==1&&!acks);
   if(mode==7){free(capture);goto quarantine;}
   state(&sequence,fence,REMESA_SEQUENCE_VISIBLE);fail_vmap=0;
   assert(!remesa_gem_visible_readback_ioctl(&dev,&r,&owner));
  }else assert(!e);
  memcpy(capture+(size_t)page*4096,r.data,4096);
  assert(bo->holds==1&&!acks&&starts==1);
 }
 assert(!memcmp(capture,bo->gem.bytes,IMAGE_BYTES));
 assert(action(memory,REMESA_MEM_LEASE,id)==-EBUSY);
 if(!mode){FILE *f=fopen(argv[2],"wb");assert(f&&fwrite(capture,1,IMAGE_BYTES,f)==IMAGE_BYTES&&!fclose(f));}
 free(capture);
 e=remesa_sequence_acknowledge(&sequence,fence);
 if(mode==10){assert(e==-EIO);goto quarantine;}
 assert(!e);state(&sequence,fence,REMESA_SEQUENCE_ACKNOWLEDGED);
 if(mode==11){
  /* Model borrowed alias lifetime: attached backing cannot retire. */
  ticket_job(memory,fence.ticket)->attached=true;
  assert(remesa_sequence_retire(&sequence,fence)==-EBUSY&&bo->holds==1);
  ticket_job(memory,fence.ticket)->attached=false;
 }
 assert(!remesa_sequence_retire(&sequence,fence));
 r=request(fence,id,0);assert(remesa_gem_visible_readback_ioctl(&dev,&r,&owner)==-ENOENT);
 assert(starts==1&&polls==1&&publishes==1&&acks==1);
clean:
 assert(!bo->holds&&!remesa_sequence_close(&sequence));endpoint=NULL;
 assert(!remesa_gem_memory_close(memory));assert(!gems&&!pins&&!ptes&&!power&&!module_refs&&!device_refs);
 printf("PASS CPU_ONLY lifecycle mode%u: safe rollback or retained capture then retirement; zero GPU\n",mode);return 0;
quarantine:
 if(mode!=7)state(&sequence,fence,REMESA_SEQUENCE_QUARANTINED);
 /* Readback faults update authoritative GEM state; sequence notices on next
  * transition. No retry, cancellation, detach or release is permitted. */
 assert(remesa_sequence_acknowledge(&sequence,fence)==-EBUSY);
 assert(remesa_sequence_cancel(&sequence,fence)==-EBUSY);
 assert(remesa_sequence_retire(&sequence,fence)==-EBUSY);
 assert(remesa_sequence_close(&sequence)==-EBUSY&&remesa_gem_memory_close(memory)==-EBUSY&&bo->holds==1);
 r=request(fence,id,0);saved=r;
 assert(remesa_gem_visible_readback_ioctl(&dev,&r,&owner)==-EIO&&!memcmp(&r,&saved,sizeof r));
 printf("PASS CPU_ONLY lifecycle mode%u: uncertain operation quarantined, backing retained, zero GPU\n",mode);return 0;
}
