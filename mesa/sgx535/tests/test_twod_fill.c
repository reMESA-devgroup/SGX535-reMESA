/* SPDX-License-Identifier: MIT. Packet checks only, no GPU execution. */
#include "remesa_twod.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned word(const unsigned char *p)
{return (unsigned)p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
static void reject(struct remesa_twod_fill r,int error)
{
 struct remesa_twod_copy_plan p,old;memset(&p,0xa5,sizeof p);old=p;
 assert(remesa_twod_fill_prepare(&r,&p)==error&&!memcmp(&p,&old,sizeof p));
}
int main(void)
{
 const unsigned sizes[][2]={{1280,800},{640,640},{641,479},{1,1},{4095,4095}};
 const unsigned colors[]={0,0xffffffffu,0x7f123456u,0x01020304u};
 for(unsigned n=0;n<5;n++)for(unsigned c=0;c<4;c++){
  unsigned w=sizes[n][0],h=sizes[n][1],pitch=(w+1)*4;
  struct remesa_twod_fill r={.destination={2,0x41000000,(uint64_t)pitch*(h+1),w+1,h+1,pitch,1},
   .request_base=0x40000000,.destination_x=1,.destination_y=1,.width=w,.height=h,.color=colors[c]};
  struct {unsigned pre;struct remesa_twod_copy_plan p;unsigned post;} g={.pre=123,.post=456};
  struct remesa_twod_fill saved=r;assert(!remesa_twod_fill_prepare(&r,&g.p));
  assert(!memcmp(&r,&saved,sizeof r)&&g.pre==123&&g.post==456);
  assert(g.p.dwords==7&&!g.p.source_id&&!g.p.source_first&&!g.p.source_end&&g.p.destination_id==2);
  assert(word(g.p.command)==(0xa0060000u|pitch)&&word(g.p.command+4)==0x1000000u);
  assert(word(g.p.command+8)==0x8000f0f0u&&word(g.p.command+12)==colors[c]);
  assert(word(g.p.command+16)==0x1001u&&word(g.p.command+20)==((w<<12)|h)&&word(g.p.command+24)==0xf0000000u);
  for(unsigned i=28;i<40;i++)assert(!g.p.command[i]);
  assert(g.p.destination_first==0x41000000u+pitch+4&&g.p.destination_end==g.p.destination_first+(uint64_t)(h-1)*pitch+w*4);
  r.reserved=1;reject(r,EINVAL);r=saved;r.width=0;reject(r,EINVAL);
  r=saved;r.width=8;reject(r,EOPNOTSUPP);r=saved;r.height=8;reject(r,EOPNOTSUPP);
  r=saved;r.destination_x=r.destination.width;reject(r,EINVAL);
  r=saved;r.destination.bytes--;reject(r,ENOSPC);
  r=saved;r.destination.gpu_address=0x3ffffffc;reject(r,EINVAL);
  r=saved;r.request_base++;reject(r,EINVAL);
  r=saved;r.destination.stride--;reject(r,EOPNOTSUPP);
  r=saved;r.destination.format=0;reject(r,EOPNOTSUPP);
  assert(remesa_twod_fill_prepare(&r,(void*)&r)==EINVAL);
 }
 struct remesa_twod_copy_plan p;assert(remesa_twod_fill_prepare(NULL,&p)==EINVAL);
 puts("PASS native fill packets: 20 dimension/color pairs, exact seven-word size, zero source, guards, bounds, addresses, format and transactional refusal; GPU0");return 0;
}
