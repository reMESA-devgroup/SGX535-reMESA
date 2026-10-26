/* SPDX-License-Identifier: MIT */
#include "../../../tools/remesa/lab_scene_actions.h"
#include "remesa/scene_dpm.h"
#include "../../../tools/psb-dri-re/frozen_fixed_io.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;assert(x);}while(0)
struct monitor {const struct sgx535_frozen_reg_action *a;unsigned pos,reads,missing_bit3;};
static int wr(void *p,sgx535_u32 reg,sgx535_u32 value)
{struct monitor *m=p;const struct sgx535_frozen_reg_action *a=&m->a[m->pos++];CHECK(a->kind==SGX535_REG_WRITE&&reg==a->offset&&value==a->value);return 0;}
static int rd(void *p,sgx535_u32 reg,sgx535_u32 *value)
{struct monitor *m=p;const struct sgx535_frozen_reg_action *a=&m->a[m->pos];CHECK(reg==a->offset&&(a->kind==SGX535_REG_POLL_SET||a->kind==SGX535_REG_POLL_CLEAR));m->reads++;if(m->missing_bit3&&reg==0x118){*value=7;return 0;}*value=a->value;m->pos++;return 0;}
static int barrier(void *p){(void)p;assert(0);return 1;}
int main(void)
{
 unsigned table,parameter,cookie[13],pairs[44],cases=0;
 const struct sgx535_fixed_io io={wr,rd,barrier};
 while(scanf("%u %u",&table,&parameter)==2){
  for(unsigned i=0;i<13;i++)CHECK(scanf("%u",cookie+i)==1);
  for(unsigned i=0;i<44;i++)CHECK(scanf("%u",pairs+i)==1);
  struct remesa_lab_dpm_load_request r={table,0x2000000,parameter,0x2000000};
  struct remesa_lab_dpm_control plan;
  CHECK(!remesa_scene_dpm_build(r.table_address,r.table_bytes,r.parameter_address,r.parameter_bytes,&plan));
  CHECK(!memcmp(plan.cookie,cookie,sizeof(cookie))&&!memcmp(plan.load_pairs,pairs,sizeof(pairs)));
  CHECK(plan.original_wait_mask==7&&plan.retained_completion_mask==15);
  struct sgx535_frozen_reg_action out[30];memset(out,0xa5,sizeof(out));size_t n=999;
  CHECK(!remesa_lab_dpm_actions_encode(&r,out,30,&n));CHECK(n==29);
  for(unsigned i=0;i<22;i++)CHECK(out[i].kind==SGX535_REG_WRITE&&out[i].offset==pairs[i*2]&&out[i].value==pairs[i*2+1]&&!out[i].mask);
  /* Independent retained policy from the physically qualified load contract,
   * not the original helper's weaker7. Never start INITEND with bit3 missing. */
  const struct sgx535_frozen_reg_action tail[]={
   {SGX535_REG_POLL_SET,0x118,15,15},{SGX535_REG_WRITE,0x114,15,0},
   {SGX535_REG_POLL_CLEAR,0x118,0,15},{SGX535_REG_WRITE,0x6a8,1,0},
   {SGX535_REG_POLL_SET,0x12c,0x400000,0x400000},{SGX535_REG_WRITE,0x134,0x400000,0},
   {SGX535_REG_POLL_CLEAR,0x12c,0,0x400000}};
  CHECK(!memcmp(out+22,tail,sizeof(tail)));
  for(unsigned i=29*sizeof(*out);i<sizeof(out);i++)CHECK(((unsigned char *)out)[i]==0xa5);
  struct monitor m={out,0,0,0};CHECK(!sgx535_fixed_run_actions(&io,&m,out,n,3));CHECK(m.pos==29&&m.reads==4);
  m=(struct monitor){out,0,0,1};CHECK(sgx535_fixed_run_actions(&io,&m,out,n,3));CHECK(m.pos==22&&m.reads==3);
  cases++;
 }
 CHECK(cases==2);
 for(unsigned bad=0;bad<10;bad++){
  struct remesa_lab_dpm_load_request r={0x407ef000,0x2000000,0x30100000,0x2000000};
  struct sgx535_frozen_reg_action out[29],saved[29];memset(out,0xa5,sizeof(out));memcpy(saved,out,sizeof(out));size_t n=999,cap=29;int expected=EOPNOTSUPP;
  if(bad==0)r.table_address++;
  if(bad==1)r.parameter_address+=4096;
  if(bad==2)r.table_bytes-=4096;
  if(bad==3)r.parameter_bytes=4096;
  if(bad==4)r.table_address=0xffffffff;
  if(bad==5)r.parameter_address=UINT64_MAX;
  if(bad==6)r.table_address=0x30000000;
  if(bad==7){cap=28;expected=ENOSPC;}
  if(bad==8){cap=SIZE_MAX;expected=EOVERFLOW;}
  if(bad==9)r.parameter_address=0x40000000;
  CHECK(remesa_lab_dpm_actions_encode(&r,out,cap,&n)==expected);CHECK(n==999&&!memcmp(out,saved,sizeof(out)));
 }
 puts("PASS2 originalDPMoracles,29nativeactions, strictfourcompletion+noINITENDonmissingbit3, transactional negatives; NO GPU");
 printf("checks=%u\n",checks);return 0;
}
