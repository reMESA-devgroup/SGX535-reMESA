/* SPDX-License-Identifier: MIT */
#ifndef REMESA_SCENE_DPM_H
#define REMESA_SCENE_DPM_H
#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/string.h>
typedef u32 remesa_dpm_u32;
typedef u64 remesa_dpm_u64;
#else
#include <stdint.h>
#include <errno.h>
#include <string.h>
typedef uint32_t remesa_dpm_u32;
typedef uint64_t remesa_dpm_u64;
#endif
/* Shared original CPU info/load arithmetic. Fixed32MiB LAB allocation profile;
 * this is not a hardware capacity declaration or memory-ownership validator.
 * Waitmask7 is original CPU behavior;15 retains the qualified four-completion
 * policy. Never use the former to weaken the native executor. No MMIO. */
struct remesa_lab_dpm_control {
 remesa_dpm_u32 cookie[13],info_bytes;
 remesa_dpm_u32 load_pairs[44];
 remesa_dpm_u32 original_wait_mask,retained_completion_mask;
};

static inline int remesa_scene_dpm_build(remesa_dpm_u64 table,
 remesa_dpm_u64 table_bytes,remesa_dpm_u64 parameter,remesa_dpm_u64 parameter_bytes,
 struct remesa_lab_dpm_control *out)
{
 struct remesa_lab_dpm_control plan={0};
 remesa_dpm_u32 pages=8192,first,last,range,end,i;
 if(!out)return EINVAL;
 if(table_bytes!=0x2000000ULL||parameter_bytes!=0x2000000ULL)return EOPNOTSUPP;
 if(table<0x40000000ULL||table>0x100000000ULL-table_bytes||(table&4095)||
    parameter<0x30000000ULL||parameter>0x40000000ULL-parameter_bytes||
    (parameter&0xfffff))return EOPNOTSUPP;
 first=(remesa_dpm_u32)((parameter&0xfffffff)/4096);last=first+pages-1;
 if(last>65535)return ERANGE;
 plan.info_bytes=0x620000;plan.cookie[0]=(remesa_dpm_u32)table;
 plan.cookie[1]=(remesa_dpm_u32)(parameter&0xfffffff);plan.cookie[2]=pages;
 for(i=3;i<=5;i++)plan.cookie[i]=pages-0x600;
 plan.cookie[6]=pages-0x640;plan.cookie[10]=first;
 plan.cookie[11]=last;plan.cookie[12]=last-1;
 range=(last<<16)|first;end=(last-1)<<16;
 {
  const remesa_dpm_u32 pairs[44]={
   0x618,plan.cookie[0],0x61c,range,0x648,end,0x638,0,
   0x620,plan.cookie[4],0x624,plan.cookie[5],0x628,plan.cookie[3],0x668,plan.cookie[6],0x684,1,
   0x600,plan.cookie[0],0x604,range,0x64c,end,0x660,0,0x680,1,
   0x610,plan.cookie[0],0x614,range,0x654,end,0x688,1,
   0x608,plan.cookie[0],0x60c,range,0x650,end,0x690,1};
  memcpy(plan.load_pairs,pairs,sizeof(pairs));
 }
 plan.original_wait_mask=7;plan.retained_completion_mask=15;
 memcpy(out,&plan,sizeof(plan));return 0;
}
#endif
