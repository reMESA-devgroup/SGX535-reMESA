/* SPDX-License-Identifier: MIT */
/* Trusted metadata fixtures only; no real mappings, bank leases or GPU work. */
#include "remesa_pds.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef NDEBUG
#error Assertions must remain enabled
#endif
static unsigned checks;
static struct remesa_pds_constant_loader_request request(void)
{
 return (struct remesa_pds_constant_loader_request){.operation=12,.generation=13,.owner_cookie=7,
  .source={11,7,9,0x20000000,0,20,4096},.auxiliary={12,7,9,0x20100000,128,8,4096},
  .pds={13,7,9,0x20200000,0,64,4096},.constant_dwords=5,.compiler_attribute_dwords=33,.use_bank=1};
}
static void reject(struct remesa_pds_constant_loader_request r,int error)
{
 struct remesa_pds_constant_loader_plan p,before;memset(&p,0xa5,sizeof p);before=p;
 assert(remesa_pds_constant_loader_prepare(&r,&p)==error);
 assert(!memcmp(&p,&before,sizeof p));checks++;
}
int main(void)
{
 struct remesa_pds_constant_loader_request r=request(),v=r;
 struct remesa_pds_constant_loader_plan p;
 assert(!remesa_pds_constant_loader_prepare(&r,&p));
 assert(p.program_bytes==60&&p.request.pds.bytes==64&&p.request.source.bytes==20);
 assert(p.attribute_dwords==33&&p.attribute_units==2&&p.attribute_mask==0x01fc8000&&p.attribute_bits==0x88000);
 const unsigned char end[]={0,0,0,0,0x40,1,4,0xf8};assert(!memcmp(p.auxiliary_code,end,8));
 assert(p.execution_error==ENOTSUP&&!remesa_pds_constant_loader_revalidate(&p,&r));checks++;
 for(unsigned i=60;i<64;i++)assert(!p.pds_image[i]&&!p.pds_known_mask[i]);
 v.source.mapping_generation++;assert(remesa_pds_constant_loader_revalidate(&p,&v)==ESTALE);checks++;
 p.execution_error=0;assert(remesa_pds_constant_loader_revalidate(&p,&r)==EPROTO);checks++;
 p.execution_error=ENOTSUP;p.auxiliary_code[0]=1;
 assert(remesa_pds_constant_loader_revalidate(&p,&r)==EPROTO);checks++;
 v=r;v.constant_dwords=0;reject(v,EINVAL);v.constant_dwords=129;reject(v,ENOTSUP);
 v=r;v.owner_cookie=0;reject(v,EINVAL);v=r;v.source.owner_cookie=8;reject(v,EINVAL);
 v=r;v.source.bytes=24;reject(v,EINVAL);v=r;v.auxiliary.bytes=16;reject(v,EINVAL);
 v=r;v.auxiliary.offset=4;reject(v,EINVAL);v=r;v.pds.offset=4;reject(v,EINVAL);
 v=r;v.pds.bytes=56;reject(v,ENOSPC);v=r;v.compiler_attribute_dwords=4065;reject(v,ERANGE);
 v=r;v.source.address=UINT64_C(0x100000000);reject(v,ERANGE);
 v=r;v.auxiliary.mapping_generation=0;reject(v,EINVAL);
 v=r;v.pds.address=v.auxiliary.address;reject(v,EINVAL);
 v=r;v.pds=v.auxiliary;v.pds.offset=0;v.pds.bytes=64;
 assert(!remesa_pds_constant_loader_prepare(&v,&p));checks++;
 v.pds.mapping_generation++;reject(v,ESTALE);v.pds.mapping_generation--;
 v.pds.offset=128;reject(v,EINVAL);
 v=r;v.compiler_attribute_dwords=0;
 assert(!remesa_pds_constant_loader_prepare(&v,&p)&&p.attribute_dwords==5&&p.attribute_units==1);checks++;
 v.constant_dwords=17;v.source.bytes=68;
 assert(!remesa_pds_constant_loader_prepare(&v,&p)&&p.program_bytes==64&&p.attribute_dwords==17);checks++;
 v.pds.bytes=60;reject(v,ENOSPC);
 assert(remesa_pds_constant_loader_prepare(NULL,&p)==EINVAL);checks++;
 assert(remesa_pds_constant_loader_prepare(&r,NULL)==EINVAL);checks++;
 puts("PDS constant loader metadata/auxiliary checks passed; GPU operations=0");
 printf("checks=%u\n",checks);return 0;
}
