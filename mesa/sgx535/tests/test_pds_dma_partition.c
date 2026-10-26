/* SPDX-License-Identifier: MIT; independent original-x86 oracle carrier required. */
#include "remesa_pds_structure.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned get32(const unsigned char*p){return p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24);}
int main(int argc,char**argv){
 assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f);unsigned char raw[24];unsigned cases=0;
 while(fread(raw,1,24,f)==24){
  unsigned n=get32(raw);struct remesa_pds_dma_partition p;
  memset(&p,0xa5,sizeof(p));assert(!remesa_pds_structure_dma_partition(n,&p));
  assert(p.count==get32(raw+4));for(unsigned i=0;i<2;i++){assert(p.source_word_offset[i]==get32(raw+8+4*i));assert(p.control[i]==get32(raw+16+4*i));}
  if(n<=16){uint32_t legacy;assert(!remesa_pds_structure_dma_control(n,&legacy)&&legacy==p.control[0]);}
  else {uint32_t legacy=0xa5a5a5a5;assert(remesa_pds_structure_dma_control(n,&legacy)==ENOTSUP&&legacy==0xa5a5a5a5);}
  ++cases;
 }
 assert(feof(f)&&!ferror(f)&&cases==16);fclose(f);
 struct remesa_pds_dma_partition p,bak;memset(&p,0xa5,sizeof(p));bak=p;
 assert(remesa_pds_structure_dma_partition(0,&p)==EINVAL&&!memcmp(&p,&bak,sizeof(p)));
 assert(remesa_pds_structure_dma_partition(129,&p)==ENOTSUP&&!memcmp(&p,&bak,sizeof(p)));
 assert(remesa_pds_structure_dma_partition(UINT32_MAX,&p)==ENOTSUP&&!memcmp(&p,&bak,sizeof(p)));
 assert(remesa_pds_structure_dma_partition(37,NULL)==EINVAL);
 _Alignas(struct remesa_pds_dma_partition) unsigned char unaligned[sizeof(p)+8];memset(unaligned,0xa5,sizeof(unaligned));
 assert(remesa_pds_structure_dma_partition(37,(void*)(unaligned+1))==EINVAL);for(unsigned i=0;i<sizeof(unaligned);i++)assert(unaligned[i]==0xa5);
 puts("PASS 16 original x86 partition oracles; 37/41-word two descriptors; legacy unchanged; negative outputs transactional; no GPU");return 0;
}
