/* SPDX-License-Identifier: MIT
 * Original CPU operand construction using existing USSE codecs.
 * --state uses the original state writer287ff count relationship; --manual-vs8
 * selects the separately demonstrated software vertex producer40355.
 * --background uses39c5c ->30e22(builder,1,2) ->30fb7.
 * This utility emits bytes only. It opens no GPU/DRM resource and authorizes
 * no submission. Numeric classes do not identify physical register banks.
 */
#include "remesa_usse.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv)
{
 if(argc==2&&!strcmp(argv[1],"--parameters")) {
  const struct remesa_usse_cpu_control_ee request={.record_last=1};
  unsigned char bytes[8];size_t written=0;
  int error=remesa_usse_cpu_control_ee_encode(&request,bytes,sizeof(bytes),&written);
  if(error)return error;
  return written==sizeof(bytes)&&fwrite(bytes,1,written,stdout)==written&&
   !fflush(stdout)?0:EIO;
 }
 unsigned count;int manual,background=0;
 if(argc==2&&!strcmp(argv[1],"--background")) {
  background=1;manual=0;count=2;
 } else if(argc==2&&!strcmp(argv[1],"--manual-vs8")) {
  manual=1;count=8;
 } else if(argc==3&&!strcmp(argv[1],"--state")) {
  if(!argv[2][0]||argv[2][0]<'0'||argv[2][0]>'9')return EINVAL;
  char *end;errno=0;unsigned long n=strtoul(argv[2],&end,10);
  if(errno||*end||n<1u||n>16u)return EINVAL;
  manual=0;count=(unsigned)n;
 } else return EINVAL;
 struct remesa_usse_cpu_move move={.source={2,0},.destination={1,0},
  .control12=count-1u,.control21=background?0u:1u,.control23=1,
  .control18=background?1u:0u};
 /* State producer28d8b..28dc7 supplies (n<<4)|8 as source1 to30a09.
  * Manual vertex producer404c2 supplies tag8 with number0 instead. */
 struct remesa_usse_cpu_three_source tail={
  .source={{0,0},{2,manual?0u:count},{2,0}},
  .source_extension={0,1,1},.word0_control21=1,
  .word1_control12_13=manual?1u:0u,.word1_control18=1};
 unsigned char bytes[REMESA_USSE_CPU_MOVE_BYTES+REMESA_USSE_CPU_THREE_SOURCE_BYTES];
 size_t written=0;
 int e=remesa_usse_cpu_move_encode(&move,bytes,REMESA_USSE_CPU_MOVE_BYTES,&written);
 if(e)return e;
 if(written!=REMESA_USSE_CPU_MOVE_BYTES)return EPROTO;
 if(background)
  return fwrite(bytes,1,written,stdout)==written&&!fflush(stdout)?0:EIO;
 e=remesa_usse_cpu_three_source_encode(&tail,bytes+written,
  REMESA_USSE_CPU_THREE_SOURCE_BYTES,&written);
 if(e)return e;
 if(written!=REMESA_USSE_CPU_THREE_SOURCE_BYTES)return EPROTO;
 return fwrite(bytes,1,sizeof(bytes),stdout)==sizeof(bytes)&&!fflush(stdout)?0:EIO;
}
