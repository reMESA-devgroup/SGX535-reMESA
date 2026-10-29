/* SPDX-License-Identifier: MIT */
/* Compare independently encoded output moves to the external finalizer oracle. */
#include "remesa_usse.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
int main(void)
{
 for(unsigned end=0;end<2;end++){
  struct remesa_usse_cpu_move move={
   .source={2,0},.destination={1,0},.control18=end,
   .control21=1,.control23=1
  };
  unsigned char guarded[24];size_t written=0;
  memset(guarded,0xa5,sizeof(guarded));
  if(remesa_usse_cpu_move_encode(&move,guarded+8,8,&written)||written!=8)return 1;
  for(unsigned i=0;i<8;i++)if(guarded[i]!=0xa5||guarded[16+i]!=0xa5)return 2;
  if(fwrite(guarded+8,1,8,stdout)!=8)return 3;
 }
 return 0;
}
