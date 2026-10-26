/* SPDX-License-Identifier: MIT */
#include "remesa_ta_scene.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;assert(x);}while(0)
static struct remesa_ta_fragment_binding_request valid(void)
{
 struct remesa_ta_fragment_binding_request r={0};
 r.address_window_base=UINT64_C(0x20000000);
 r.primary=(struct remesa_ta_fragment_pds_reference){UINT64_C(0x20000000),4096,0x160,56,12,0,1};
 r.secondary=(struct remesa_ta_fragment_pds_reference){UINT64_C(0x20000000),4096,0x220,4,0,0,0};
 return r;
}
static void reject(struct remesa_ta_fragment_binding_request r,int expected)
{
 uint8_t out[18],before[18];memset(out,0xa5,sizeof(out));memcpy(before,out,sizeof(out));
 CHECK(remesa_ta_fragment_binding_encode(&r,out+1)==expected);
 CHECK(!memcmp(before,out,sizeof(out)));
}
int main(int argc,char **argv)
{
 struct remesa_ta_fragment_binding_request r=valid(),bad;
 uint8_t out[18];unsigned i;
 if(argc==2 && !strcmp(argv[1],"--dump")){
  CHECK(!remesa_ta_fragment_binding_encode(&r,out));
  for(i=0;i<16;i++)printf("%02x",out[i]);
  puts("");return 0;
 }
 CHECK(argc==1);memset(out,0xa5,sizeof(out));
 CHECK(!remesa_ta_fragment_binding_encode(&r,out+1));
 {const uint8_t expected[16]={0x40,0,0,0,0x22,0,0,0,0,0,3,0,0x16,0,0,0x0c};CHECK(!memcmp(out+1,expected,16));}
 CHECK(out[0]==0xa5 && out[17]==0xa5);
 /* Independent source-position variations: distinct references, unchanged
  * scheduler and DATA counts. These are arithmetic tests, not GPU evidence. */
 for(i=0;i<3;i++){
  r.primary.byte_offset=0x160+i*0x20;r.secondary.byte_offset=0x220+i*0x20;
  CHECK(!remesa_ta_fragment_binding_encode(&r,out));
  CHECK(out[4]==(uint8_t)(0x22+i*2));CHECK(out[12]==(uint8_t)(0x16+i*2));
  CHECK(out[8]==0 && out[9]==0 && out[10]==3 && out[11]==0);
 }
 r=valid();
 bad=r;bad.primary.field10=1;reject(bad,ENOTSUP);
 bad=r;bad.primary.data_dwords=16;reject(bad,ENOTSUP);
 bad=r;bad.primary.field14=0;reject(bad,ENOTSUP);
 bad=r;bad.secondary.data_dwords=12;reject(bad,ENOTSUP);
 bad=r;bad.secondary.field10=1;reject(bad,ENOTSUP);
 bad=r;bad.secondary.field14=1;reject(bad,ENOTSUP);
 bad=r;bad.primary.object_bytes=60;reject(bad,ENOTSUP);
 bad=r;bad.secondary.object_bytes=8;reject(bad,ENOTSUP);
 bad=r;bad.primary.bo_bytes=1;reject(bad,EINVAL);
 bad=r;bad.primary.byte_offset=UINT64_MAX;reject(bad,EINVAL);
 bad=r;bad.primary.bo_gpu_va=UINT64_MAX;reject(bad,EINVAL);
 bad=r;bad.primary.bo_bytes=UINT64_MAX;reject(bad,EINVAL);
 bad=r;bad.primary.byte_offset++;reject(bad,EINVAL);
 bad=r;bad.secondary.byte_offset=r.primary.byte_offset+32;reject(bad,EINVAL);
 bad=r;bad.secondary.bo_gpu_va=UINT64_C(0x30000000);reject(bad,EINVAL);
 bad=r;bad.address_window_base++;reject(bad,EINVAL);
 bad=r;bad.address_window_base=UINT64_C(0x100000000);reject(bad,EINVAL);
 bad=r;bad.primary.bo_gpu_va=0;reject(bad,EINVAL);
 CHECK(remesa_ta_fragment_binding_encode((const void *)(UINTPTR_MAX-sizeof(r)+1),out)==EINVAL);
 CHECK(remesa_ta_fragment_binding_encode(&r,(void *)(UINTPTR_MAX-14))==EINVAL);
 CHECK(remesa_ta_fragment_binding_encode(NULL,out)==EINVAL);
 CHECK(remesa_ta_fragment_binding_encode(&r,NULL)==EINVAL);
 /* Snapshot must precede writes, including byte-unaligned output aliases. */
 {struct remesa_ta_fragment_binding_request alias=r;uint8_t expected[16];
 CHECK(!remesa_ta_fragment_binding_encode(&r,expected));
 CHECK(!remesa_ta_fragment_binding_encode(&alias,((uint8_t*)&alias)+1));
 CHECK(!memcmp(((uint8_t*)&alias)+1,expected,16));}
 {struct remesa_ta_fragment_binding_request alias=r,before;alias.primary.field10=1;before=alias;
 CHECK(remesa_ta_fragment_binding_encode(&alias,((uint8_t*)&alias)+1)==ENOTSUP);
 CHECK(!memcmp(&alias,&before,sizeof(alias)));}
 printf("PASS %u fragment binding checks; CPU only\n",checks);return 0;
}
