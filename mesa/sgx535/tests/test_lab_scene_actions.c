/* SPDX-License-Identifier: MIT */
#include "../../../tools/remesa/lab_scene_actions.h"
#include "../../../tools/psb-dri-re/frozen_fixed_io.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;assert(x);}while(0)
struct monitor {const struct sgx535_frozen_reg_action *oracle;size_t pos,count;unsigned reads,writes;int timeout,fail_write;};
static int write_io(void *p,sgx535_u32 reg,sgx535_u32 value)
{struct monitor *m=p;CHECK(m->pos<m->count);const struct sgx535_frozen_reg_action *a=&m->oracle[m->pos];CHECK(a->kind==SGX535_REG_WRITE&&a->offset==reg&&a->value==value);m->writes++;if(m->fail_write)return 1;m->pos++;return 0;}
static int read_io(void *p,sgx535_u32 reg,sgx535_u32 *value)
{struct monitor *m=p;CHECK(m->pos<m->count);const struct sgx535_frozen_reg_action *a=&m->oracle[m->pos];CHECK((a->kind==SGX535_REG_POLL_SET||a->kind==SGX535_REG_POLL_CLEAR)&&a->offset==reg);m->reads++;if(m->timeout){*value=~a->value;return 0;}*value=a->value;m->pos++;return 0;}
static int barrier_io(void *p){(void)p;assert(0);return 1;}
int main(void)
{
 unsigned width,height,base,bytes,engine,cases=0;
 const struct sgx535_fixed_io io={write_io,read_io,barrier_io};
 while(scanf("%u %u %u %u %u",&width,&height,&base,&bytes,&engine)==5){
  size_t count=engine?20:31,n=999;struct sgx535_frozen_reg_action oracle[31],out[32];
  for(size_t i=0;i<count;i++)CHECK(scanf("%u %u %u %u",&oracle[i].kind,&oracle[i].offset,&oracle[i].value,&oracle[i].mask)==4);
  memset(out,0xa5,sizeof(out));
  struct remesa_lab_scene_fire_request r={width,height,base,bytes};
  CHECK(!remesa_lab_scene_fire_encode(&r,engine,out,32,&n));
  CHECK(n==count&&!memcmp(out,oracle,n*sizeof(*out)));
  for(size_t i=n*sizeof(*out);i<sizeof(out);i++)CHECK(((unsigned char *)out)[i]==0xa5);
  struct monitor m={.oracle=oracle,.count=count};
  CHECK(!sgx535_fixed_run_actions(&io,&m,out,count,3));CHECK(m.pos==count&&m.reads>0&&m.writes>0);
  m=(struct monitor){.oracle=oracle,.count=count,.timeout=1};
  CHECK(sgx535_fixed_run_actions(&io,&m,out,count,3)!=0);CHECK(m.reads==3&&m.pos<count);
  m=(struct monitor){.oracle=oracle,.count=count,.fail_write=1};
  CHECK(sgx535_fixed_run_actions(&io,&m,out,count,3)!=0);CHECK(m.pos==0&&m.writes==1&&m.reads==0);
  cases++;
 }
 CHECK(cases==10);
 for(unsigned mode=0;mode<12;mode++){
  struct remesa_lab_scene_fire_request r={1280,800,0x407d1000,0x1e000};
  struct sgx535_frozen_reg_action out[31],saved[31];size_t n=999,capacity=31;unsigned e=0;int expected=EINVAL;
  memset(out,0xa5,sizeof(out));memcpy(saved,out,sizeof(out));
  switch(mode){
  case 0:r.width=0;break;
  case 1:r.height=2048;expected=ENOTSUP;break;
  case 2:r.region_address++;break;
  case 3:r.region_bytes--;break;
  case 4:r.region_bytes-=4096;expected=ENOSPC;break;
  case 5:r.region_address=0xfffff000;expected=EOVERFLOW;break;
  case 6:r.region_address=0x30000000;break;
  case 7:capacity=30;expected=ENOSPC;break;
  case 8:capacity=SIZE_MAX;expected=EOVERFLOW;break;
  case 9:e=2;expected=ENOTSUP;break;
  case 10:r.region_bytes=0;break;
  case 11:r.region_address=UINT64_C(0x100000000);break;
  }
  CHECK(remesa_lab_scene_fire_encode(&r,e,out,capacity,&n)==expected);
  CHECK(n==999&&!memcmp(out,saved,sizeof(out)));
 }
 struct remesa_lab_scene_fire_request r={1280,800,0x407d1000,0x1e000};
 struct sgx535_frozen_reg_action out[31];size_t n=999;
 CHECK(remesa_lab_scene_fire_encode(&r,0,(void *)&r,31,&n)==EINVAL);
 CHECK(remesa_lab_scene_fire_encode(&r,0,out,31,(void *)out)==EINVAL);
 CHECK(remesa_lab_scene_fire_encode(NULL,0,out,31,&n)==EINVAL);
 printf("PASS10originalfireoracles variable32..1280x800/address and actual action consumer simulated IO, bounded poll/write failures, transactional rejects; NO GPU (%u checks)\n",checks);
 return 0;
}
