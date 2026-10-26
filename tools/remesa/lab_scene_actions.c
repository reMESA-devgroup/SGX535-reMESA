/* SPDX-License-Identifier: MIT */
#include "lab_scene_actions.h"
#include "remesa/scene_layout.h"
#include "remesa/scene_dpm.h"
#ifdef __KERNEL__
#include <linux/kernel.h>
#else
#include <stdint.h>
#include <errno.h>
#include <string.h>
#endif
static int span(const void *p,size_t n)
{return p&&n&&n-1<=(~(uintptr_t)0)-(uintptr_t)p;}
static int overlap(const void *a,size_t an,const void *b,size_t bn)
{uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;}
static void write_action(struct sgx535_frozen_reg_action *a,size_t *n,
 sgx535_u32 reg,sgx535_u32 value)
{a[(*n)++]=(struct sgx535_frozen_reg_action){SGX535_REG_WRITE,reg,value,0};}
static void wait_clear(struct sgx535_frozen_reg_action *a,size_t *n,
 sgx535_u32 status,sgx535_u32 mask,sgx535_u32 clear)
{
 a[(*n)++]=(struct sgx535_frozen_reg_action){SGX535_REG_POLL_SET,status,mask,mask};
 write_action(a,n,clear,mask);
 a[(*n)++]=(struct sgx535_frozen_reg_action){SGX535_REG_POLL_CLEAR,status,0,mask};
}
static void closed_prefix(struct sgx535_frozen_reg_action *a,size_t *n)
{
 /* Original4f10/5040 use the same three kick/wait/clear calls. The actual
  *4e10 helper executes each kick+clear store;4ca0 observes their status.
  * The bounded oracle cuts only4ca0, not these stores. */
 write_action(a,n,0xad4,1);wait_clear(a,n,0x138,0x44,0x140);
 write_action(a,n,0xae0,1);wait_clear(a,n,0x138,1,0x140);
 /* Selected original context[3]=ffff; closed helper ORs10000000. */
 write_action(a,n,0x804,0x1000ffffU);
 wait_clear(a,n,0x12c,0x04000000U,0x134);
}
int remesa_lab_scene_fire_encode(const struct remesa_lab_scene_fire_request *request,
 enum remesa_lab_scene_engine engine,struct sgx535_frozen_reg_action *out,
 size_t capacity,size_t *written)
{
 size_t bytes,required,n=0;
 struct remesa_lab_scene_fire_request r;
 struct remesa_scene_layout layout;
 struct sgx535_frozen_reg_action actions[REMESA_LAB_TA_FIRE_ACTIONS];
 sgx535_u32 base;
 int e;
 if(capacity>(~(size_t)0)/sizeof(*out))return EOVERFLOW;
 bytes=capacity*sizeof(*out);
 if(!span(request,sizeof(*request))||!span(out,bytes)||!span(written,sizeof(*written))||
    (uintptr_t)request%_Alignof(struct remesa_lab_scene_fire_request)||
    (uintptr_t)out%_Alignof(struct sgx535_frozen_reg_action)||
    (uintptr_t)written%_Alignof(size_t)||
    overlap(request,sizeof(*request),out,bytes)||
    overlap(request,sizeof(*request),written,sizeof(*written))||
    overlap(out,bytes,written,sizeof(*written)))return EINVAL;
 if(engine!=REMESA_LAB_SCENE_TA&&engine!=REMESA_LAB_SCENE_RASTER)return EOPNOTSUPP;
 r=*request;
 e=remesa_scene_layout_build(r.width,r.height,0,&layout);
 if(e)return e;
 if(r.region_address<0x40000000ULL||r.region_address>0xffffffffU||
    (r.region_address&4095)||!r.region_bytes||(r.region_bytes&4095))return EINVAL;
 if(r.region_bytes>0x100000000ULL-r.region_address)return EOVERFLOW;
 if(r.region_bytes<(sgx535_u64)layout.allocation_pages*4096)return ENOSPC;
 required=engine==REMESA_LAB_SCENE_TA?REMESA_LAB_TA_FIRE_ACTIONS:REMESA_LAB_RASTER_FIRE_ACTIONS;
 if(capacity<required)return ENOSPC;
 base=(sgx535_u32)r.region_address;
 if(engine==REMESA_LAB_SCENE_TA){
  /* Original4550 normal TA/context0/setup4 branch. All addresses and extents
   * derive from original3a40 layout, including large-target cookie7=10000.
   * No use of unwritten cookie15 or an external XHW descriptor. */
  write_action(actions,&n,0x220,base+layout.cookie[8]);
  write_action(actions,&n,0x65c,0);write_action(actions,&n,0x694,2);
  write_action(actions,&n,0x698,2);write_action(actions,&n,0x24c,1);
  write_action(actions,&n,0x224,0x80000000U);
  write_action(actions,&n,0x208,layout.cookie[2]);
  write_action(actions,&n,0x20c,layout.cookie[3]);
  write_action(actions,&n,0x214,layout.cookie[4]);
  write_action(actions,&n,0x210,layout.cookie[5]);
  write_action(actions,&n,0x248,layout.cookie[6]);
  write_action(actions,&n,0x21c,base+layout.cookie[7]);
  write_action(actions,&n,0x274,0);
  wait_clear(actions,&n,0x12c,0x00100a40U,0x134);
  write_action(actions,&n,0xc90,0x30000000U);
  closed_prefix(actions,&n);write_action(actions,&n,0xa08,1);write_action(actions,&n,0x200,1);
 }else{
  /* Original4550 raster/context1/flags15 ->4030 fresh cookie14=0 ->5040. */
  write_action(actions,&n,0x630,3);write_action(actions,&n,0x65c,0);
  write_action(actions,&n,0x63c,3);write_action(actions,&n,0x658,0);
  write_action(actions,&n,0x408,base+layout.cookie[7]);
  closed_prefix(actions,&n);write_action(actions,&n,0x43c,1);
  write_action(actions,&n,0xa08,1);write_action(actions,&n,0x428,1);
 }
 if(n!=required)return EPROTO;
 memcpy(out,actions,n*sizeof(*out));*written=n;return 0;
}

int remesa_lab_dpm_actions_encode(const struct remesa_lab_dpm_load_request *request,
 struct sgx535_frozen_reg_action *out,size_t capacity,size_t *written)
{
 struct remesa_lab_dpm_control plan;
 struct sgx535_frozen_reg_action actions[REMESA_LAB_DPM_LOAD_ACTIONS];
 struct remesa_lab_dpm_load_request r;
 size_t bytes,n=0;unsigned i;int e;
 if(capacity>(~(size_t)0)/sizeof(*out))return EOVERFLOW;
 bytes=capacity*sizeof(*out);
 if(!span(request,sizeof(*request))||!span(out,bytes)||!span(written,sizeof(*written))||
    (uintptr_t)request%_Alignof(struct remesa_lab_dpm_load_request)||
    (uintptr_t)out%_Alignof(struct sgx535_frozen_reg_action)||
    (uintptr_t)written%_Alignof(size_t)||
    overlap(request,sizeof(*request),out,bytes)||
    overlap(request,sizeof(*request),written,sizeof(*written))||
    overlap(out,bytes,written,sizeof(*written)))return EINVAL;
 r=*request;
 e=remesa_scene_dpm_build(r.table_address,r.table_bytes,r.parameter_address,r.parameter_bytes,&plan);
 if(e)return e;
 if(capacity<REMESA_LAB_DPM_LOAD_ACTIONS)return ENOSPC;
 for(i=0;i<22;i++)write_action(actions,&n,plan.load_pairs[i*2],plan.load_pairs[i*2+1]);
 wait_clear(actions,&n,0x118,plan.retained_completion_mask,0x114);
 write_action(actions,&n,0x6a8,1);wait_clear(actions,&n,0x12c,0x400000,0x134);
 if(n!=REMESA_LAB_DPM_LOAD_ACTIONS)return EPROTO;
 memcpy(out,actions,sizeof(actions));*written=n;return 0;
}
