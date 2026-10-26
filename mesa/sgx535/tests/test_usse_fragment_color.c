/* SPDX-License-Identifier: MIT */
#include "remesa_usse_fragment_color.h"
#include "remesa_usse.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;assert(x);}while(0)
int main(void)
{
 unsigned input,logical,allocated,count=0;char hex[17];
 while(scanf("%u %u %u %16s",&input,&logical,&allocated,hex)==4){
  unsigned char expected[8],out[32],saved[32];
  CHECK(strlen(hex)==16);for(unsigned i=0;i<8;i++){unsigned v;CHECK(sscanf(hex+2*i,"%2x",&v)==1);expected[i]=v;}
  struct remesa_usse_fragment_color_request r={input,0};
  struct remesa_usse_fragment_color_extent extent={99,98,97,96,95,94,93};
  memset(out,0xa5,sizeof(out));memcpy(saved,out,sizeof(out));
  CHECK(!remesa_usse_fragment_color_encode(&r,out,sizeof(out),&extent));
  CHECK(extent.logical_bytes==logical&&logical==8);
  CHECK(extent.allocation_bytes==allocated&&allocated==16&&extent.alignment_bytes==32);
  CHECK(!memcmp(out,expected,8));CHECK(!memcmp(out+8,saved+8,24));
  struct remesa_usse_cpu_move decoded;
  CHECK(!remesa_usse_cpu_move_decode(out,8,&decoded));
  CHECK(decoded.control18==1&&decoded.control12==1&&decoded.control23==1);
  CHECK(decoded.source.class_id==2&&decoded.source.number==0);
  CHECK(decoded.destination.class_id==1&&decoded.destination.number==0);
  CHECK(extent.r_id==input+9&&extent.r_components==4&&extent.r_mode==3&&!extent.r_flag);
  for(unsigned bad=0;bad<6;bad++){
   struct remesa_usse_fragment_color_request q=r;
   struct remesa_usse_fragment_color_extent unchanged={99,98,97,96,95,94,93};extent=unchanged;
   memcpy(out,saved,sizeof(out));int error;
   if(bad<3){q.input_slot=bad==0?0:bad==1?3:0xffffffffu;error=ENOTSUP;}
   else if(bad==3){q.link_key=1;error=ENOTSUP;}
   else if(bad==4){q.link_key=4;error=ENOTSUP;}
   else error=ENOSPC;
   CHECK(remesa_usse_fragment_color_encode(&q,out,bad==5?7:32,&extent)==error);
   CHECK(!memcmp(out,saved,sizeof(out)));CHECK(!memcmp(&extent,&unchanged,sizeof(extent)));
  }
  CHECK(remesa_usse_fragment_color_encode(NULL,out,32,&extent)==EINVAL);
  CHECK(remesa_usse_fragment_color_encode(&r,NULL,32,&extent)==EINVAL);
  CHECK(remesa_usse_fragment_color_encode(&r,out,32,NULL)==EINVAL);
  /* Aliased input/output and output/metadata fail before writes. */
  memcpy(out,saved,sizeof(out));
  CHECK(remesa_usse_fragment_color_encode(&r,&r,sizeof(r),&extent)==EINVAL);
  CHECK(remesa_usse_fragment_color_encode(&r,out,sizeof(out),
    (struct remesa_usse_fragment_color_extent *)(void *)out)==EINVAL);
  CHECK(!memcmp(out,saved,sizeof(out)));
  count++;
 }
 CHECK(count==2);printf("PASS whole original COLOR0/COLOR1 FS USE (%u checks); logical8 allocation16 align32, CPU final-record bit18, transactional rejects; CPU ONLY\n",checks);
 return 0;
}
