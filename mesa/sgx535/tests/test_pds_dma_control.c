/* SPDX-License-Identifier: MIT */
#include "remesa_pds_structure.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
static uint32_t le(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int main(void)
{
 unsigned char oracle[64];assert(fread(oracle,1,64,stdin)==64);assert(fgetc(stdin)==EOF);
 for(unsigned n=1;n<=16;n++){uint32_t word=73;assert(!remesa_pds_structure_dma_control(n,&word));assert(word==le(oracle+4*(n-1)));}
 uint32_t word=73;assert(remesa_pds_structure_dma_control(0,&word)==EINVAL&&word==73);
 assert(remesa_pds_structure_dma_control(17,&word)==ENOTSUP&&word==73);
 assert(remesa_pds_structure_dma_control(UINT32_MAX,&word)==ENOTSUP&&word==73);
 assert(remesa_pds_structure_dma_control(1,NULL)==EINVAL);
 assert(remesa_pds_structure_dma_control(1,(uint32_t *)(UINTPTR_MAX-1))==EINVAL);
 puts("PASS DMA CPU control:16 independent original descriptor comparisons;5 negative cases");return 0;
}
