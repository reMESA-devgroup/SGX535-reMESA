/* SPDX-License-Identifier: MIT */
#include "remesa_ta_scalar.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
 unsigned char oracle[4][16],b[20],old[20];
 assert(fread(oracle,1,sizeof(oracle),stdin)==sizeof(oracle)&&fgetc(stdin)==EOF);
 for(unsigned i=0;i<4;i++) {
  memset(b,0xa5,sizeof(b));size_t n=99;
  enum remesa_ta_scalar_profile profile=i==3?REMESA_TA_SCALAR_POSITION4_PRIMARY_COLOR4:REMESA_TA_SCALAR_POSITION4;
  assert(!remesa_ta_scalar_groups_encode(profile,b,sizeof(b),&n));
  assert(n==16&&!memcmp(b,oracle[i],16));
  for(unsigned j=16;j<20;j++)assert(b[j]==0xa5);
 }
 memset(old,0xa5,sizeof(old));memcpy(b,old,sizeof(b));size_t n=99;
 assert(remesa_ta_scalar_groups_encode((enum remesa_ta_scalar_profile)2,b,sizeof(b),&n)==ENOTSUP);
 assert(n==99&&!memcmp(b,old,sizeof(b)));
 assert(remesa_ta_scalar_groups_encode((enum remesa_ta_scalar_profile)-1,b,sizeof(b),&n)==ENOTSUP);
 assert(n==99&&!memcmp(b,old,sizeof(b)));
 assert(remesa_ta_scalar_groups_encode(REMESA_TA_SCALAR_POSITION4,b,15,&n)==ENOSPC);
 assert(n==99&&!memcmp(b,old,sizeof(b)));
 assert(remesa_ta_scalar_groups_encode(REMESA_TA_SCALAR_POSITION4,NULL,16,&n)==EINVAL);
 assert(remesa_ta_scalar_groups_encode(REMESA_TA_SCALAR_POSITION4,b,16,NULL)==EINVAL);
 assert(remesa_ta_scalar_groups_encode(REMESA_TA_SCALAR_POSITION4,(void *)&n,16,&n)==EINVAL);
 assert(remesa_ta_scalar_groups_encode(REMESA_TA_SCALAR_POSITION4,b,sizeof(b),(void *)(b+1))==EINVAL);
 assert(remesa_ta_scalar_groups_encode(REMESA_TA_SCALAR_POSITION4,(void *)UINTPTR_MAX,16,&n)==EINVAL);
 assert(n==99&&!memcmp(b,old,sizeof(b)));
 puts("PASS:4 original compiler/default-init/constructor/layout/setter/serializer oracles;2 scalar profiles; negative/transaction tests; CPU ONLY");
 return 0;
}
