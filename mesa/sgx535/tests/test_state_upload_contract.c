/* SPDX-License-Identifier: MIT. CPU codecs and preserved carrier bytes only. */
#include "remesa/state_upload.h"
#include "remesa_pds_structure.h"
#include "remesa_usse.h"
#include <assert.h>
#include <stdio.h>
static void auxiliary(unsigned count,unsigned char *bytes)
{
 const struct remesa_usse_cpu_move move={.source={2,0},.destination={1,0},.control12=count-1,.control21=1,.control23=1};
 const struct remesa_usse_cpu_three_source tail={.source={{0,0},{2,count},{2,0}},
  .source_extension={0,1,1},.word0_control21=1,.word1_control18=1};size_t n;
 assert(!remesa_usse_cpu_move_encode(&move,bytes,8,&n)&&n==8);
 assert(!remesa_usse_cpu_three_source_encode(&tail,bytes+8,8,&n)&&n==8);
}
int main(void)
{
 unsigned char pds[64],mask[64],aux[32],saved[64];size_t n;
 struct remesa_state_upload out,initial={7,8,9};
 const unsigned fixups[]={0,4,0x804,0x7ffff};
 for(unsigned words=1;words<=16;words++)for(unsigned f=0;f<4;f++){
  struct remesa_pds_state_image_request r={0x20004000,4096,256,words,fixups[f],REMESA_PDS_STATE_TASK_ISP};
  memset(pds,0xa5,sizeof pds);memset(aux,0xa5,sizeof aux);
  assert(!remesa_pds_structure_encode_state_image(&r,pds,mask,sizeof pds,&n)&&n==60);
  assert(!remesa_state_upload_pds(pds,60,words,&out)&&out.source_address==0x20004100&&out.source_bytes==4*words&&out.use_lower19==fixups[f]);
  for(unsigned i=12;i<48;i++)pds[i]=(unsigned char)(i*17);
  assert(!remesa_state_upload_pds(pds,64,words,&out));
  auxiliary(words,aux);assert(!remesa_state_upload_auxiliary(aux,16,words)&&!remesa_state_upload_auxiliary(aux,32,words)&&aux[16]==0xa5);
  memcpy(saved,pds,64);
  const unsigned offsets[]={0,4,11,48,52,56};
  for(unsigned k=0;k<6;k++){
   memcpy(pds,saved,64);pds[offsets[k]]^=k==0?1:0x80;out=initial;
   assert(remesa_state_upload_pds(pds,60,words,&out)&&!memcmp(&out,&initial,sizeof out));
  }
  memcpy(pds,saved,64);
  for(unsigned k=0;k<16;k++){
   aux[k]^=1;assert(remesa_state_upload_auxiliary(aux,16,words)==EINVAL);aux[k]^=1;
  }
 }
 unsigned char oracle[180];assert(fread(oracle,1,sizeof oracle,stdin)==sizeof oracle&&fgetc(stdin)==EOF);
 const unsigned counts[]={11,14,2},offsets[]={0x350,0x3bc,0x43c},low[]={0x804,0xa04,0xc04};
 for(unsigned k=0;k<3;k++){
  assert(!remesa_state_upload_pds(oracle+k*60,60,counts[k],&out));
  assert(out.source_address==0x20004000+offsets[k]&&out.source_bytes==counts[k]*4&&out.use_lower19==low[k]);
 }
 out=initial;
 assert(remesa_state_upload_pds(pds,59,16,&out)==EINVAL&&!memcmp(&out,&initial,sizeof out));
 assert(remesa_state_upload_pds(pds,60,0,&out)==EINVAL&&remesa_state_upload_pds(pds,60,17,&out)==EINVAL);
 assert(remesa_state_upload_pds(pds,60,16,(void *)pds)==EINVAL);
 assert(remesa_state_upload_auxiliary(aux,15,16)==EINVAL);
 assert(remesa_state_upload_auxiliary((void *)(UINTPTR_MAX-1),16,16)==EINVAL);
 puts("PASS state upload: original PDS/USE encoders1..16 and4 lower19 values,3 independent carriers, unknown DATA preserved, committed60/16 vs capacity, every auxiliary byte/known field tamper, aliases and atomic rejection; GPU0");return 0;
}
