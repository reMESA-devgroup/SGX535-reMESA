/* SPDX-License-Identifier: MIT */
#include "remesa_twod.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
extern int oracle_copy(const struct remesa_twod_copy *,uint32_t *);
static unsigned checks,cases;
#define CHECK(x) do{checks++;assert(x);}while(0)
static struct remesa_twod_copy request(unsigned w,unsigned h)
{
 struct remesa_twod_copy r={.request_base=0x40000000,.width=w,.height=h};
 r.source=(struct remesa_twod_surface){1,0x40001000,(uint64_t)w*h*4,w,h,w*4,1};
 r.destination=r.source;r.destination.allocation_id=2;r.destination.gpu_address=0x42000000;return r;
}
static void good(struct remesa_twod_copy *r)
{
 struct remesa_twod_copy saved=*r;struct remesa_twod_copy_plan p;uint32_t original[10];
 CHECK(!remesa_twod_copy_prepare(r,&p));CHECK(!oracle_copy(r,original));
 CHECK(!memcmp(&saved,r,sizeof saved));CHECK(p.dwords==10);
 CHECK(!memcmp(p.command,original,40));
 CHECK(p.source_id==r->source.allocation_id&&p.destination_id==r->destination.allocation_id);
 CHECK(p.source_first>=r->source.gpu_address&&p.source_end<=r->source.gpu_address+r->source.bytes);
 CHECK(p.destination_first>=r->destination.gpu_address&&p.destination_end<=r->destination.gpu_address+r->destination.bytes);
 cases++;
}
static void bad(struct remesa_twod_copy r,int expected)
{
 struct remesa_twod_copy saved=r;struct remesa_twod_copy_plan p,old;
 memset(&p,0xa5,sizeof p);old=p;
 CHECK(remesa_twod_copy_prepare(&r,&p)==expected);CHECK(!memcmp(&p,&old,sizeof p));CHECK(!memcmp(&r,&saved,sizeof r));
}
int main(void)
{
 const unsigned dims[][2]={{1280,800},{640,640},{641,479},{1,1},{4095,4095}};
 for(unsigned k=0;k<5;k++){
  struct remesa_twod_copy r=request(dims[k][0],dims[k][1]);
  /* Keep huge surfaces in distinct windows of the same256MiB aperture. */
  if(k==4)r.destination.gpu_address=0x48000000;
  good(&r);
 }
 for(unsigned x=0;x<4;x++)for(unsigned y=0;y<4;y++){
  struct remesa_twod_copy r=request(1280,800);r.destination=r.source;
  r.width=31;r.height=17;r.source_x=(x&1)?10:0;r.destination_x=(x&2)?10:0;
  r.source_y=(y&1)?10:0;r.destination_y=(y&2)?10:0;good(&r);
 }
 for(unsigned pad=0;pad<64;pad+=4){
  struct remesa_twod_copy r=request(641,479);r.source.stride+=pad;r.source.bytes=(uint64_t)r.source.stride*479;
  r.destination.stride+=64-pad;r.destination.bytes=(uint64_t)r.destination.stride*479;good(&r);
 }
 struct remesa_twod_copy r=request(32,32);
 r.request_base=0xf0000000;r.source.gpu_address=0xfffff000;r.destination.gpu_address=0xffffc000;good(&r);
 r=request(1280,800);r.width=0;bad(r,EINVAL);
 r=request(1280,800);r.height=0;bad(r,EINVAL);
 r=request(1280,800);r.width=8;bad(r,ENOTSUP);
 r=request(1280,800);r.height=8;bad(r,ENOTSUP);
 r=request(1280,800);r.width=4096;bad(r,ENOTSUP);
 r=request(1280,800);r.source_x=UINT_MAX;bad(r,EINVAL);
 r=request(1280,800);r.destination_y=UINT_MAX;bad(r,EINVAL);
 r=request(1280,800);r.source_x=1;bad(r,EINVAL);
 r=request(1280,800);r.request_base++;bad(r,EINVAL);
 r=request(1280,800);r.source.gpu_address++;bad(r,EINVAL);
 r=request(1280,800);r.source.gpu_address=0x100000000ULL;bad(r,EINVAL);
 r=request(1280,800);r.source.bytes=UINT64_MAX;bad(r,EINVAL);
 r=request(1280,800);r.source.bytes--;bad(r,ENOSPC);
 r=request(1280,800);r.source.allocation_id=0;bad(r,ENOTSUP);
 r=request(1280,800);r.source.format=2;bad(r,ENOTSUP);
 r=request(1280,800);r.source.stride--;bad(r,ENOTSUP);
 r=request(1280,800);r.source.stride=32768;bad(r,ENOTSUP);
 r=request(1280,800);r.destination.gpu_address=r.source.gpu_address;bad(r,EINVAL);
 r=request(1280,800);r.destination.allocation_id=1;bad(r,EINVAL);
 r=request(1280,800);r.reserved=1;bad(r,EINVAL);
 r=request(1280,800);CHECK(remesa_twod_copy_prepare(NULL,NULL)==EINVAL);
 CHECK(remesa_twod_copy_prepare(&r,(struct remesa_twod_copy_plan*)&r)==EINVAL);
 printf("PASS %u checks, %u original-kernel packet comparisons; CPU encoding only\n",checks,cases);return 0;
}
