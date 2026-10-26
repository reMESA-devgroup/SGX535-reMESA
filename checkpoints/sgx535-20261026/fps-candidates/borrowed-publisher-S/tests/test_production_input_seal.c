/* Actual provider/seal and pageguard, synthetic backing metadata. NO GPU/MMU proof. */
#define REMESA_MEMORY_TEST
#include "../module/remesa_gem_memory.c"
#include "../module/remesa_stream_trusted.h"
#include "../module/stream_profile_asset.h"
#include "../module/remesa_stream_grammar.h"
#include "../module/remesa_canonical_inputs.h"
#include "../module/remesa_pds_write_guard.h"
#include <stdio.h>
static struct remesa_gem_memory s;
static unsigned char pds[131072],pagebuf[4096],oracle[131072];
static struct remesa_memory_request r;
static int guard(u64 off,const void*p,unsigned n){return remesa_canonical_pds_page(gears_stream_profile_asset.batches,12,remesa_canonical_inputs[0],off,p,n);}
static int request(unsigned actn,unsigned role,unsigned flags,unsigned off,unsigned n,const void*data){memset(&r,0,sizeof(r));r.version=1;r.context=s.context;r.id=32+role;r.action=actn;r.flags=flags;r.offset=off;r.length=n;if(data&&n<=4096)memcpy(r.data,data,n);return act(&s,&r);}
int main(void){
 struct remesa_memory_ticket t={.context=7,.slot=0,.operation=1,.generation=1};
 mutex_init(&s.lock);s.context=7;s.stream_bound=true;s.jobs[0].live=true;s.jobs[0].state=REMESA_JOB_PREPARED;s.jobs[0].operation=s.jobs[0].generation=1;s.jobs[0].count=11;
 for(unsigned i=0;i<11;i++){struct backing*b=&s.bo[i];b->live=b->pinned=b->reserved=true;b->generation=1;b->holds=1;b->gem.size=gears_stream_profile_asset.bindings[i].bytes;b->va.start=gears_stream_profile_asset.bindings[i].va;s.jobs[0].ids[i]=32+i;}
 s.bo[4].lease=true;assert(remesa_gem_stream_seal_inputs(&s,t,guard)==-EBUSY&&!s.pds_write_guard&&!s.bo[0].input_seal);s.bo[4].lease=false;
 assert(!remesa_gem_stream_seal_inputs(&s,t,guard));assert(s.bo[0].input_seal==1);for(unsigned i=1;i<5;i++)assert(s.bo[i].input_seal==2);assert(remesa_gem_stream_seal_inputs(&s,t,guard)==-EACCES);
 assert(remesa_gem_stream_release(&s)==-EBUSY&&s.pds_write_guard);s.jobs[0].live=false;for(unsigned i=0;i<11;i++)s.bo[i].holds=0;
 for(unsigned i=1;i<5;i++){assert(request(REMESA_MEM_LEASE,i,1,0,0,NULL)==-EPERM);assert(request(REMESA_MEM_WRITE,i,0,0,4,pagebuf)==-EPERM);}
 memcpy(pds,remesa_canonical_inputs[0],sizeof(pds));s.bo[0].cpu=pds;s.bo[0].lease=s.bo[0].write=true;
 unsigned off=24576;memcpy(pagebuf,pds+off,4096);unsigned hole=25600+68-off;pagebuf[hole+2]=128;pagebuf[hole+3]=63;
 assert(!request(REMESA_MEM_WRITE,0,0,off,4096,pagebuf)&&pds[off+hole+3]==63);
 assert(request(REMESA_MEM_WRITE,0,0,off+1,4096,pagebuf)==-EINVAL);assert(request(REMESA_MEM_WRITE,0,0,off,4,pagebuf)==-EINVAL);
 pagebuf[0]^=1;assert(request(REMESA_MEM_WRITE,0,0,off,4096,pagebuf)==-EKEYREJECTED&&pds[off]==remesa_canonical_inputs[0][off]);pagebuf[0]^=1;
 pagebuf[hole+2]=192;pagebuf[hole+3]=127;assert(request(REMESA_MEM_WRITE,0,0,off,4096,pagebuf)==-ERANGE&&pds[off+hole+3]==63);
 assert(remesa_gem_stream_release(&s)==-EBUSY&&s.pds_write_guard);s.bo[0].lease=false;s.bo[0].cpu=NULL;assert(!remesa_gem_stream_release(&s)&&!s.pds_write_guard);for(unsigned i=0;i<5;i++)assert(!s.bo[i].input_seal);
 /* Differential oracle: every admitted typed float is also accepted by the
  * existing full masker; outside holes exact canonical bytes are required. */
 unsigned cases=0;
 for(unsigned i=0;i<12;i++)for(unsigned field=0;field<19;field++){
  const struct remesa_stream_batch*b=&gears_stream_profile_asset.batches[i];unsigned at=b->source_offset+(field<16?68+field*4:4+(b->normal_variant?8:9)*16+(field-16)*4);unsigned base=at&~4095u;
  memcpy(oracle,remesa_canonical_inputs[0],sizeof(oracle));oracle[at+2]=128;oracle[at+3]=63;
  assert(!guard(base,oracle+base,4096));assert(!remesa_stream_mask_sources(gears_stream_profile_asset.batches,12,oracle,sizeof(oracle),0x20000000));assert(!memcmp(oracle,remesa_canonical_inputs[0],sizeof(oracle)));
  oracle[at+2]=192;oracle[at+3]=127;assert(guard(base,oracle+base,4096)==-ERANGE);assert(remesa_stream_mask_sources(gears_stream_profile_asset.batches,12,oracle,sizeof(oracle),0x20000000)==-ERANGE);cases++;
 }
 for(unsigned off2=0;off2<131072;off2+=4096){memcpy(pagebuf,remesa_canonical_inputs[0]+off2,4096);assert(!guard(off2,pagebuf,4096));pagebuf[0]^=1;assert(guard(off2,pagebuf,4096)==-EKEYREJECTED);}
 printf("PASS production input seal: atomic five-BO seal, static WRITE/WRITELEASE denied, PDS page-only transaction and bounds/NaN/immutable rejects, busy release keeps seal, idle release clears; %u typed-hole cases differential against original masker; synthetic metadata/GPU0\n",cases);
}
