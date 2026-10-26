/* SPDX-License-Identifier: MIT */
#include "remesa_ta_scalar.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>
static int span(const void *p,size_t n)
{return p&&n&&n-1<=UINTPTR_MAX-(uintptr_t)p;}
static void store(uint8_t *p,uint32_t v)
{for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
int remesa_ta_scalar_groups_encode(enum remesa_ta_scalar_profile profile,
 uint8_t *bytes,size_t capacity,size_t *written)
{
 if(!span(bytes,capacity)||!span(written,sizeof(*written))||
    (uintptr_t)written%_Alignof(size_t))return EINVAL;
 uintptr_t a=(uintptr_t)bytes,b=(uintptr_t)written;
 if(a<=b?b-a<capacity:a-b<sizeof(*written))return EINVAL;
 if(profile!=REMESA_TA_SCALAR_POSITION4&&
    profile!=REMESA_TA_SCALAR_POSITION4_PRIMARY_COLOR4)return ENOTSUP;
 if(capacity<REMESA_TA_SCALAR_BYTES)return ENOSPC;
 const uint32_t primary=profile==REMESA_TA_SCALAR_POSITION4_PRIMARY_COLOR4;
 /* Original2e26a..2e2bc: position32bit x4, plus the primary-color record
  * when fragment InputsRead bit1 is present.2e47d..2e48f packs record DWORD
  * count in bits24..31 and ORs layout flags. Default2b4c3 group200 is zero. */
 const uint32_t record_bytes=16u*(1u+primary);
 const uint32_t layout=(UINT32_C(1)<<12)|(primary<<11);
 uint8_t value[REMESA_TA_SCALAR_BYTES];
 store(value,UINT32_C(0x200)|UINT32_C(0x400)|UINT32_C(0x800));store(value+4,0);
 store(value+8,((record_bytes>>2)<<24)|layout);
 /*2b4b9 defaultctx14198→270ec render488→28bac group800. */
 store(value+12,UINT32_C(0x358637bd));
 memcpy(bytes,value,sizeof(value));*written=sizeof(value);return 0;
}
