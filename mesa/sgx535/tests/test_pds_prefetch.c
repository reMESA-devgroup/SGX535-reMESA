/* SPDX-License-Identifier: MIT */
#include "remesa_pds_prefetch.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t word(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void fill(struct remesa_pds_prefetch_request *r,unsigned n,unsigned pattern)
{
 memset(r,0,sizeof(*r));r->task[0]=0x101234;r->task[1]=0x08000000;r->task[2]=0x20;
 r->entry_count=n;r->sample_count=16;
 for(unsigned i=0;i<48;i++)r->samples[i/3][i%3]=0x40001000u+i*0x101;
 for(unsigned i=0;i<n;i++){
  unsigned sampled=pattern==0||(pattern==2&&(i&1))||(pattern==3&&!(i&1))||(pattern==5&&i%3==2);
  r->entries[i].control=sampled?0x0c00f100u|(i&15):0x0cc0000fu;
  if(pattern==5&&!sampled)r->entries[i].control=(i%3==0)?0x0cc0000fu:0x0cc00001u;
  if(pattern==4)r->entries[i].control=(i&1)?0x0cc00001u:0x0cc0000fu;
  r->entries[i].sample=sampled?(i*7)%16:REMESA_PDS_PREFETCH_NO_SAMPLE;
 }
}
int main(int argc,char **argv)
{
 int corpus=argc==2&&!strcmp(argv[1],"--corpus");
 CHECK(argc==1||corpus);
 for(unsigned n=0;n<=16;n++)for(unsigned pattern=0;pattern<6;pattern++)for(unsigned align=0;align<4;align++){
  struct remesa_pds_prefetch_request r;fill(&r,n,pattern);
  uint8_t storage[REMESA_PDS_PREFETCH_MAX_BYTES+32],masks[sizeof(storage)];
  memset(storage,0xa5,sizeof(storage));memset(masks,0xa5,sizeof(masks));
  uint8_t *image=storage+8+align,*mask=masks+8+align;
  struct remesa_pds_prefetch_layout l;
  CHECK(!remesa_pds_prefetch_encode(&r,image,mask,REMESA_PDS_PREFETCH_MAX_BYTES,&l));
  CHECK(l.data_bytes%16==0&&l.total_bytes==l.data_bytes+l.code_bytes);
  CHECK(l.total_bytes<=REMESA_PDS_PREFETCH_MAX_BYTES);
  for(size_t i=0;i<8+align;i++)CHECK(storage[i]==0xa5&&masks[i]==0xa5);
  for(size_t i=8+align+l.total_bytes;i<sizeof(storage);i++)CHECK(storage[i]==0xa5&&masks[i]==0xa5);
  for(size_t i=0;i<l.total_bytes;i++){CHECK(mask[i]==0||mask[i]==0xff);if(!mask[i])CHECK(image[i]==0);}
  if(corpus){
   printf("{\"n\":%u,\"pattern\":%u,\"alignment_words\":%u,\"data_bytes\":%zu,\"image_bytes\":%zu,\"words\":[",n,pattern,align,l.data_bytes,l.total_bytes);
   for(size_t i=0;i<l.total_bytes;i+=4)printf("%s%u",i?",":"",word(image+i));
   printf("],\"masks\":[");for(size_t i=0;i<l.total_bytes;i+=4)printf("%s%u",i?",":"",word(mask+i));puts("]}");
  }
 }
 struct remesa_pds_prefetch_request r;fill(&r,1,0);
 uint8_t bytes[REMESA_PDS_PREFETCH_MAX_BYTES],mask[sizeof(bytes)];
 struct remesa_pds_prefetch_layout l;
 for(unsigned test=0;test<7;test++){
  fill(&r,1,0);memset(bytes,0xa5,sizeof(bytes));memset(mask,0xa5,sizeof(mask));memset(&l,0xa5,sizeof(l));
  int expected;
  if(test==0){r.entry_count=17;expected=E2BIG;}
  else if(test==1){r.sample_count=17;expected=E2BIG;}
  else if(test==2){r.sample_count=0;expected=ERANGE;}
  else if(test==3){r.reserved=1;expected=EINVAL;}
  else if(test==4){r.entries[0].sample=16;expected=ERANGE;}
  else if(test==5)expected=ENOSPC;
  else expected=EINVAL;
  int e=remesa_pds_prefetch_encode(&r,bytes,test==6?bytes:mask,test==5?1:sizeof(bytes),&l);
  CHECK(e==expected);
  for(unsigned i=0;i<sizeof(bytes);i++)CHECK(bytes[i]==0xa5&&mask[i]==0xa5);
  const uint8_t *p=(const void *)&l;for(unsigned i=0;i<sizeof(l);i++)CHECK(p[i]==0xa5);
 }
 if(!corpus)printf("PASS %u checks\n",checks);
 return 0;
}
