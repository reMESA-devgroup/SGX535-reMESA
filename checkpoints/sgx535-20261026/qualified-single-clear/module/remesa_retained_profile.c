/* SPDX-License-Identifier: GPL-2.0-only */
/* Exact qualified profile admission, not reconstruction of instruction meaning.
 * Build pins the already admitted FIRE3 tables. No historical sources loaded.
 */
#ifndef REMESA_MEMORY_TEST
#include <linux/errno.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include "frozen_kernel_contract.h"
#endif
#include "remesa_retained_profile.h"
int remesa_retained_triangle_images(void *context,const struct remesa_memory_view *v,
 unsigned n,const void *commands,unsigned bytes)
{
 struct sgx535_frozen_bo bos[SGX535_BO_COUNT];
 struct sgx535_frozen_cpu_view expected[SGX535_BO_COUNT]={0};
 struct sgx535_frozen_command_pairs pairs;
 const sgx535_u32 registers[2]={4,3};
 unsigned i;int e=-EINVAL;
 if(!context || !v || n!=SGX535_BO_COUNT || !commands || bytes!=sizeof(pairs))return -EINVAL;
 for(i=0;i<n;i++){
  struct sgx535_frozen_requirement required;
  if(sgx535_frozen_get_requirement(i,&required) || !v[i].cpu ||
     v[i].binding.bytes!=required.size)goto done;
  bos[i]=(struct sgx535_frozen_bo){.role=i,.domain=required.domain,.size=required.size,
   .gpu_va=required.domain==SGX535_DOMAIN_LOCAL?0:v[i].binding.va,.owner_token=v[i].binding.id};
  if(i<=SGX535_BO_COLOR){expected[i].bytes=vzalloc(required.size);
   if(!expected[i].bytes){e=-ENOMEM;goto done;}expected[i].length=required.size;}
 }
 if(sgx535_frozen_initialize_user_images(expected) ||
    sgx535_frozen_apply_relocations(bos,n,*(u64*)context,expected,registers) ||
    sgx535_frozen_extract_commands(&expected[SGX535_BO_CONTROL],&pairs) ||
    memcmp(&pairs,commands,sizeof(pairs)))goto done;
 for(i=0;i<n;i++) {
  if(i<=SGX535_BO_COLOR){if(memcmp(expected[i].bytes,v[i].cpu,expected[i].length))goto done;}
  else {unsigned j;const unsigned char *p=v[i].cpu;
   /* Fresh auxiliary backing is zero-filled before qualified executor setup. */
   for(j=0;j<v[i].binding.bytes;j++)if(p[j])goto done;
  }
 }
 e=0;
done:for(i=0;i<=SGX535_BO_COLOR;i++)if(expected[i].bytes)vfree(expected[i].bytes);
 return e;
}

/* Separate, default-disabled differential profile. The original full-image
 * validator remains authoritative for every byte outside this 8-byte field.
 * Numerical field composition follows the independently authored userspace
 * constant codec. It does not assert new hardware ISA semantics.
 */
static sgx535_u32 remesa_green_read32(const unsigned char *p)
{
 return (sgx535_u32)p[0] | ((sgx535_u32)p[1]<<8) |
        ((sgx535_u32)p[2]<<16) | ((sgx535_u32)p[3]<<24);
}
static void remesa_green_write32(unsigned char *p,sgx535_u32 value)
{
 unsigned i;
 for(i=0;i<4;i++)p[i]=(unsigned char)(value>>(i*8));
}
static sgx535_u32 remesa_green_constant_control(void)
{
 return (31u<<27) | (4u<<24) | (1u<<21) | (1u<<23) | (1u<<18) | 1u;
}
int remesa_retained_green_fragment_images(void *context,
 const struct remesa_memory_view *v,unsigned n,const void *commands,unsigned bytes)
{
 struct remesa_memory_view normalized[SGX535_BO_COUNT];
 const sgx535_u32 low_mask=(1u<<21)-1;
 const sgx535_u32 variable_mask=(31u<<4)|(63u<<12);
 const sgx535_u32 green=0xff00ff00u,magenta=0xffff00ffu;
 const unsigned char *program;
 unsigned char *copy;
 sgx535_u32 first,second,payload;
 int result;
 if(!context || !v || n!=SGX535_BO_COUNT || !commands ||
    bytes!=sizeof(struct sgx535_frozen_command_pairs) ||
    !v[SGX535_BO_USE].cpu || v[SGX535_BO_USE].binding.bytes!=0x8000)
  return -EINVAL;
 program=v[SGX535_BO_USE].cpu;
 first=remesa_green_read32(program);second=remesa_green_read32(program+4);
 if((first&~low_mask) ||
    (second&~variable_mask)!=remesa_green_constant_control())return -EINVAL;
 payload=first|(((second>>4)&31u)<<21)|(((second>>12)&63u)<<26);
 if(payload!=green)return -EINVAL;
 copy=vzalloc(0x8000);
 if(!copy)return -ENOMEM;
 memcpy(copy,program,0x8000);
 /* Normalize only a private comparison copy; submitted backing is untouched. */
 remesa_green_write32(copy,magenta&low_mask);
 remesa_green_write32(copy+4,remesa_green_constant_control()|
  (((magenta>>21)&31u)<<4)|(((magenta>>26)&63u)<<12));
 memcpy(normalized,v,sizeof(normalized));
 normalized[SGX535_BO_USE].cpu=copy;
 result=remesa_retained_triangle_images(context,normalized,n,commands,bytes);
 vfree(copy);
 return result;
}
