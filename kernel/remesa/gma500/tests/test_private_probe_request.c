/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include "../remesa_private_probe_request.h"
_Static_assert(sizeof(struct remesa_private_probe_request)==112,"wire size");
_Static_assert(offsetof(struct remesa_private_probe_request,operation)==64,"response offset");
_Static_assert(offsetof(struct remesa_private_probe_request,reserved)==108,"reserved offset");
int main(void)
{
 struct remesa_private_probe_request good={.version=1,.context=1,.budget_ms=1000};
 unsigned checks=0;for(unsigned i=0;i<11;i++)good.ids[i]=32+i;
#define CHECK(x) do{checks++;assert(x);}while(0)
 CHECK(remesa_private_probe_request_valid(&good));CHECK(!remesa_private_probe_request_valid(NULL));
 /* Every response/reserved word and every bit of flags must be zero. */
 for(unsigned at=64;at<112;at+=4)for(unsigned bit=0;bit<32;bit++){
  struct remesa_private_probe_request bad=good;
  uint32_t value=1u<<bit;memcpy((unsigned char*)&bad+at,&value,4);
  CHECK(!remesa_private_probe_request_valid(&bad));
 }
 for(unsigned bit=0;bit<32;bit++){
  struct remesa_private_probe_request bad=good;bad.flags=1u<<bit;
  CHECK(!remesa_private_probe_request_valid(&bad));
 }
 for(unsigned i=0;i<11;i++){
  struct remesa_private_probe_request bad=good;bad.ids[i]=0;CHECK(!remesa_private_probe_request_valid(&bad));
  for(unsigned j=0;j<11;j++)if(i!=j){bad=good;bad.ids[i]=bad.ids[j];CHECK(!remesa_private_probe_request_valid(&bad));}
 }
 for(unsigned v=0;v<4;v++){struct remesa_private_probe_request bad=good;bad.version=v;CHECK(remesa_private_probe_request_valid(&bad)==(v==1));}
 good.context=0;CHECK(!remesa_private_probe_request_valid(&good));good.context=1;
 for(unsigned budget=0;budget<=10001;budget++){
  good.budget_ms=budget;CHECK(remesa_private_probe_request_valid(&good)==(budget>=1&&budget<=10000));
 }
 printf("PASS %u private probe wire checks; no allocation/device/GPU operation\n",checks);
 return 0;
}
