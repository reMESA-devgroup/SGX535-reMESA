/* SPDX-License-Identifier: MIT */
#include "remesa_usse_sample.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;assert(x);}while(0)
static void rejected(struct remesa_usse_sample_2d_body s,size_t capacity,int error)
{
 uint8_t out[32],old[32];size_t written=777;
 memset(out,0xa5,sizeof(out));memcpy(old,out,sizeof(out));
 CHECK(remesa_usse_sample_2d_body_encode(&s,out,capacity,&written)==error);
 CHECK(written==777&&!memcmp(out,old,sizeof(out)));
}
int main(int argc,char **argv)
{
 const int dump=argc==2&&!strcmp(argv[1],"--corpus");
 struct remesa_usse_sample_2d_body s={7,11,19,0,0},bad;
 uint8_t out[32],old[32];size_t written=777;
 for(size_t cap=0;cap<16;cap++)rejected(s,cap,ENOSPC);
 for(unsigned field=0;field<4;field++){
  bad=s;
  if(field==0)bad.destination=128;
  if(field==1)bad.coordinates=128;
  if(field==2)bad.sampler=128;
  if(field==3)bad.drc=4;
  rejected(bad,sizeof(out),ERANGE);
 }
 bad=s;bad.reserved=1;rejected(bad,sizeof(out),EINVAL);
 memset(out,0xa5,sizeof(out));memcpy(old,out,sizeof(out));
 CHECK(remesa_usse_sample_2d_body_encode(NULL,out,sizeof(out),&written)==EINVAL);
 CHECK(remesa_usse_sample_2d_body_encode(&s,NULL,sizeof(out),&written)==EINVAL);
 CHECK(remesa_usse_sample_2d_body_encode(&s,out,sizeof(out),NULL)==EINVAL);
 CHECK(!memcmp(out,old,sizeof(out))&&written==777);
 struct remesa_usse_sample_2d_body saved=s;
 CHECK(remesa_usse_sample_2d_body_encode(&s,&s,sizeof(s),&written)==EINVAL);
 CHECK(!memcmp(&s,&saved,sizeof(s))&&written==777);
 CHECK(remesa_usse_sample_2d_body_encode(&s,out,sizeof(out),(size_t *)(void *)&s)==EINVAL);
 CHECK(!memcmp(&s,&saved,sizeof(s))&&!memcmp(out,old,sizeof(out)));
 CHECK(remesa_usse_sample_2d_body_encode(&s,out,sizeof(out),(size_t *)(void *)out)==EINVAL);
 CHECK(!memcmp(out,old,sizeof(out)));
 CHECK(remesa_usse_sample_2d_body_encode(&s,out,SIZE_MAX,&written)==EINVAL);
 CHECK(remesa_usse_sample_2d_body_encode(&s,(void *)(UINTPTR_MAX-7),16,&written)==EINVAL);
 CHECK(remesa_usse_sample_2d_body_encode((void *)(UINTPTR_MAX-7),out,16,&written)==EINVAL);
 CHECK(remesa_usse_sample_2d_body_encode(&s,out,16,(void *)(UINTPTR_MAX-1))==EINVAL);
 CHECK(!memcmp(out,old,sizeof(out))&&written==777);
 const unsigned indices[]={0,1,7,11,19,63,64,126,127};unsigned tuples=0;
 for(unsigned d=0;d<9;d++)for(unsigned c=0;c<9;c++)for(unsigned t=0;t<9;t++)
 for(unsigned drc=0;drc<4;drc++){
  s=(struct remesa_usse_sample_2d_body){indices[d],indices[c],indices[t],drc,0};
  memset(out,0xa5,sizeof(out));written=777;
  CHECK(!remesa_usse_sample_2d_body_encode(&s,out,sizeof(out),&written));
  CHECK(written==16);
  for(unsigned i=16;i<sizeof(out);i++)CHECK(out[i]==0xa5);
  if(dump){
   printf("%u %u %u %u ",s.destination,s.coordinates,s.sampler,s.drc);
   for(unsigned i=0;i<16;i++)printf("%02x",out[i]);
   putchar('\n');
  }
  tuples++;
 }
 if(!dump)printf("PASS %u sample-body checks; %u tuples; CPU construction only\n",checks,tuples);
 return 0;
}
