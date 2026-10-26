/* Actual provider policy, synthetic backing metadata. CPU only, NO GPU/MMU qualification. */
#define REMESA_MEMORY_TEST
#include "../module/remesa_gem_memory.c"
#include <stdio.h>
static struct remesa_gem_memory s;
static struct remesa_memory_request r;
static unsigned char bytes[4096];
static struct page p={bytes,100};static struct page *pages[]={&p};
static int request(unsigned action,unsigned slot,unsigned flags){
 memset(&r,0,sizeof(r));r.version=1;r.context=s.context;r.id=32+slot;r.action=action;r.flags=flags;
 if(action==REMESA_MEM_WRITE){r.length=4;r.data[0]=99;}
 return act(&s,&r);
}
int main(void){
 unsigned i;assert(remesa_gem_stream_seal_ta_pools(NULL)==-EINVAL);
 mutex_init(&s.lock);s.context=7;
 assert(remesa_gem_stream_seal_ta_pools(&s)==-EACCES);
 s.stream_bound=true;assert(remesa_gem_stream_seal_ta_pools(&s)==-ENOENT);
 for(i=0;i<3;i++){
  struct backing *b=&s.bo[i];b->live=b->pinned=b->reserved=true;b->generation=1;
  b->gem.size=i<2?33554432:4096;b->va.start=i==0?0x407ef000:i==1?0x30100000:0x40001000;
  b->inserted=i<2?8192:1;b->pages=pages;
 }
 s.jobs[0].live=true;assert(remesa_gem_stream_seal_ta_pools(&s)==-EBUSY);s.jobs[0].live=false;
 for(i=0;i<2;i++){
  struct backing *b=&s.bo[i];b->lease=true;assert(remesa_gem_stream_seal_ta_pools(&s)==-EBUSY);b->lease=false;
  b->holds=1;assert(remesa_gem_stream_seal_ta_pools(&s)==-EBUSY);b->holds=0;
  b->quarantine=true;assert(remesa_gem_stream_seal_ta_pools(&s)==-EBUSY);b->quarantine=false;
  b->pinned=false;assert(remesa_gem_stream_seal_ta_pools(&s)==-EBUSY);b->pinned=true;
  b->inserted--;assert(remesa_gem_stream_seal_ta_pools(&s)==-EBUSY);b->inserted++;
 }
 s.bo[1].va.start++;assert(remesa_gem_stream_seal_ta_pools(&s)==-ENOENT);s.bo[1].va.start--;
 s.bo[1].gem.size-=4096;assert(remesa_gem_stream_seal_ta_pools(&s)==-ENOENT);s.bo[1].gem.size+=4096;
 assert(!remesa_gem_stream_seal_ta_pools(&s)&&s.ta_pools_sealed);
 for(i=0;i<2;i++){
  assert(request(REMESA_MEM_LEASE,i,1)==-EPERM);assert(request(REMESA_MEM_WRITE,i,0)==-EPERM);
  assert(!request(REMESA_MEM_LEASE,i,0));assert(!request(REMESA_MEM_READ,i,0));
  assert(request(REMESA_MEM_WRITE,i,0)==-EPERM);assert(!request(REMESA_MEM_UNLEASE,i,0));
 }
 assert(!bytes[0]);assert(!request(REMESA_MEM_LEASE,2,1));assert(!request(REMESA_MEM_WRITE,2,0));assert(bytes[0]==99);assert(!request(REMESA_MEM_UNLEASE,2,0));
 s.jobs[0].live=true;assert(remesa_gem_stream_release(&s)==-EBUSY&&s.ta_pools_sealed);s.jobs[0].live=false;
 assert(!remesa_gem_stream_release(&s)&&!s.ta_pools_sealed);
 assert(!request(REMESA_MEM_LEASE,0,1));assert(!request(REMESA_MEM_UNLEASE,0,0));
 puts("PASS actual TA seal: exact addresses/size/state, no active writes/holds/jobs, sealed WRITE and WRITELEASE denied, READ allowed, other BO writable, failed release retains seal; CPU metadata only GPU0");return 0;
}
