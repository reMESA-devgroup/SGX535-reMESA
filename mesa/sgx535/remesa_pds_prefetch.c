/* SPDX-License-Identifier: MIT */
/* Independent field construction from original EMGD primary CPU writer17cd0.
 * No original executable template or physical PDS semantic decoder is used. */
#include "remesa_pds_prefetch.h"
#include <errno.h>
#include <string.h>
static int range(const void *p,size_t n)
{return p&&(uintptr_t)p<=UINTPTR_MAX-n;}
static int apart(const void *a,size_t an,const void *b,size_t bn)
{return (uintptr_t)a+an<=(uintptr_t)b||(uintptr_t)b+bn<=(uintptr_t)a;}
static void put(uint8_t *p,uint32_t v)
{for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t get(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static unsigned alloc(unsigned count[2])
{
 unsigned bank=(count[1]/2<count[0]/2),n=count[bank]++;
 return (n/8)*16+n%8+bank*8;
}
/* Original helper17790 selects pair lanes from cursor mod4. The low
 * selector is5(task),2(control),4(texture triple), not an opaque CODE input.
 * Cursor is a CPU allocation ordinal, not an authenticated hardware register. */
static uint32_t transfer(unsigned cursor,unsigned selector)
{
 unsigned q=cursor/4;
 uint32_t word=(7u<<24)|selector|(q<<13);
 if(cursor&2u)return word|((cursor&1u)?0x18a0u:0x1620u)|((q+1u)<<18);
 return word|((cursor&1u)?0xde0u:0x360u)|(q<<18);
}
int remesa_pds_prefetch_encode(const struct remesa_pds_prefetch_request *request,
 uint8_t *bytes,uint8_t *known_mask,size_t capacity,
 struct remesa_pds_prefetch_layout *layout)
{
 if(!range(request,sizeof(*request))||!range(layout,sizeof(*layout))||
    !range(bytes,capacity)||!range(known_mask,capacity)||
    (uintptr_t)request%_Alignof(struct remesa_pds_prefetch_request)||
    (uintptr_t)layout%_Alignof(struct remesa_pds_prefetch_layout))return EINVAL;
 if(!apart(request,sizeof(*request),layout,sizeof(*layout))||
    !apart(request,sizeof(*request),bytes,capacity)||
    !apart(request,sizeof(*request),known_mask,capacity)||
    !apart(layout,sizeof(*layout),bytes,capacity)||
    !apart(layout,sizeof(*layout),known_mask,capacity)||
    !apart(bytes,capacity,known_mask,capacity))return EINVAL;
 if(request->reserved)return EINVAL;
 if(request->entry_count>REMESA_PDS_PREFETCH_ENTRIES||
    request->sample_count>REMESA_PDS_PREFETCH_SAMPLES)return E2BIG;
 for(unsigned i=0;i<request->entry_count;i++)
  if(request->entries[i].sample!=REMESA_PDS_PREFETCH_NO_SAMPLE&&
     request->entries[i].sample>=request->sample_count)return ERANGE;
 uint8_t image[REMESA_PDS_PREFETCH_MAX_BYTES]={0};
 uint8_t mask[REMESA_PDS_PREFETCH_MAX_BYTES]={0};
 unsigned count[2]={0},merge=UINT32_MAX,merged[REMESA_PDS_PREFETCH_ENTRIES]={0};
 unsigned high=0;
 #define STORE(value) do {unsigned c=alloc(count);put(image+c*4,(value)); \
  memset(mask+c*4,0xff,4);if(c+1>high)high=c+1;}while(0)
 for(unsigned i=0;i<3;i++)STORE(request->task[i]);
 for(unsigned i=0;i<request->entry_count;i++){
  const struct remesa_pds_prefetch_entry *e=&request->entries[i];
  if(e->sample==REMESA_PDS_PREFETCH_NO_SAMPLE){
   unsigned c=alloc(count);put(image+c*4,e->control);memset(mask+c*4,0xff,4);
   if(c+1>high)high=c+1;
   if((e->control&15u)==15u)merge=c;
  }else{
   if(merge==UINT32_MAX){STORE(e->control);}else{
    put(image+merge*4,(get(image+merge*4)&~UINT32_C(0xbff))|(e->control&0xbffu));
    merged[i]=1;
   }
   for(unsigned j=0;j<3;j++)STORE(request->samples[e->sample][j]);
   merge=UINT32_MAX;
  }
 }
 #undef STORE
 size_t data=((size_t)high*4+15u)&~(size_t)15u;
 unsigned cursor=3,ncode=0;
 put(image+data+4*ncode++,transfer(0,5));
 for(unsigned i=0;i<request->entry_count;i++){
  if(request->entries[i].sample==REMESA_PDS_PREFETCH_NO_SAMPLE){
   put(image+data+4*ncode++,transfer(cursor++,2));
  }else{
   if(!merged[i])put(image+data+4*ncode++,transfer(cursor++,2));
   put(image+data+4*ncode++,transfer(cursor,4));cursor+=3;
  }
 }
 /* Existing original CPU trailer class AF, not inserted padding. */
 put(image+data+4*ncode++,(0xafu<<24));
 struct remesa_pds_prefetch_layout l={data,ncode*4,data+ncode*4};
 if(l.total_bytes>sizeof(image))return EOVERFLOW;
 if(capacity<l.total_bytes)return ENOSPC;
 memset(mask+data,0xff,l.code_bytes);
 memcpy(bytes,image,l.total_bytes);memcpy(known_mask,mask,l.total_bytes);*layout=l;
 return 0;
}
