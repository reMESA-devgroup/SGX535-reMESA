/* SPDX-License-Identifier: MIT. Existing CPU producers and preserved CODE. */
#include "remesa/event_fetch.h"
#include "remesa/scene_auxiliary.h"
#include "remesa_pds_structure.h"
#include "remesa_render_target.h"
#include "remesa_usse.h"
#include <assert.h>
#include <stdio.h>
static void put(unsigned char *p,uint32_t v)
{for(unsigned i=0;i<4;i++)p[i]=v>>(8*i);}
int main(void)
{
 unsigned char oracle[84],image[400],mask[400],saved[400],aux[40];size_t n;
 assert(fread(oracle,1,84,stdin)==84&&fgetc(stdin)==EOF);
 const uint32_t fixups[]={0,3,0x804,0x7ffff};struct remesa_event_fetch decoded,initial={1,2,3};
 for(unsigned e=0;e<4;e++)for(unsigned t=0;t<4;t++)for(unsigned offset=0;offset<=64;offset+=16){
  const struct remesa_pds_psb_event_data_request r={0x200e0000,4096,offset,fixups[e],fixups[t]};
  memset(image,0xa5,sizeof image);memset(mask,0x5a,sizeof mask);
  assert(!remesa_pds_structure_encode_psb_event_profile(&r,image,mask,400,&n)&&n==148&&!memcmp(image+64,oracle,84));
  assert(image[148]==0xa5&&mask[148]==0x5a);
  for(unsigned i=0;i<148;i++)if(!mask[i])image[i]=(unsigned char)(17*i);
  assert(!remesa_event_fetch_read(image,148,&decoded)&&decoded.source_address==r.source_allocation_address+offset&&
   decoded.end_lower19==fixups[e]&&decoded.transfer_lower19==fixups[t]);
  assert(!remesa_event_fetch_read(image,400,&decoded));memcpy(saved,image,400);
  for(unsigned i=4;i<148;i++)if(mask[i]&&!(i>=8&&i<12)&&!(i>=16&&i<20)){
   image[i]^=1;decoded=initial;assert(remesa_event_fetch_read(image,148,&decoded)==EINVAL&&!memcmp(&decoded,&initial,sizeof decoded));memcpy(image,saved,400);
  }
  put(image,0x80000);assert(remesa_event_fetch_read(image,148,&decoded)==EINVAL);memcpy(image,saved,400);
  put(image+16,0x280000);assert(remesa_event_fetch_read(image,148,&decoded)==EINVAL);memcpy(image,saved,400);
  put(image+8,0xfffffff4);assert(remesa_event_fetch_read(image,148,&decoded)==ERANGE);memcpy(image,saved,400);
  decoded=initial;assert(remesa_event_fetch_read(image,147,&decoded)==EINVAL&&!memcmp(&decoded,&initial,sizeof decoded));
 }
 const uint32_t dims[][2]={{1280,800},{640,640},{641,479},{1600,768}};
 for(unsigned d=0;d<4;d++){
  uint32_t pitch=(dims[d][0]+31)&~31u;
  const struct remesa_render_target_request r={dims[d][0],dims[d][1],pitch,0x44000000,(uint64_t)pitch*dims[d][1]*4};
  struct remesa_render_target_state target;assert(!remesa_render_target_encode_linear_bgra8(&r,&target));
  assert(!remesa_event_target_check(target.target_data,16,pitch,r.height,r.gpu_va));
  for(unsigned i=0;i<16;i++){target.target_data[i]^=1;assert(remesa_event_target_check(target.target_data,16,pitch,r.height,r.gpu_va)==EINVAL);target.target_data[i]^=1;}
  assert(remesa_event_target_check(target.target_data,15,pitch,r.height,r.gpu_va)==EINVAL);
 }
 const struct remesa_usse_cpu_move move={.source={2,0},.destination={1,0},.control12=1,.control18=1,.control23=1};
 memset(aux,0xa5,40);assert(!remesa_usse_cpu_move_encode(&move,aux,40,&n)&&n==8&&aux[8]==0xa5&& !remesa_scene_background_aux(aux,8));
 for(unsigned i=0;i<8;i++){aux[i]^=1;assert(remesa_scene_background_aux(aux,8)==EINVAL);aux[i]^=1;}
 assert(remesa_scene_background_aux(aux,7)==EINVAL);
 memset(aux,0xa5,40);assert(!remesa_usse_psb_event_transfer_encode(0,aux,40,&n)&&n==16&&aux[16]==0xa5&&!remesa_scene_event_transfer_aux(aux,16));
 for(unsigned i=0;i<16;i++){aux[i]^=1;assert(remesa_scene_event_transfer_aux(aux,16)==EINVAL);aux[i]^=1;}
 assert(remesa_scene_event_transfer_aux(aux,15)==EINVAL);
 for(unsigned mode=1;mode<=3;mode+=2){assert(!remesa_usse_psb_event_transfer_encode(mode,aux,40,&n));assert(remesa_scene_event_transfer_aux(aux,16)==EINVAL);}
 memset(aux,0xa5,40);assert(!remesa_usse_psb_event_end_encode(aux,40,&n)&&n==8&&aux[8]==0xa5&&!remesa_scene_event_end_aux(aux,8));
 for(unsigned i=0;i<8;i++){aux[i]^=1;assert(remesa_scene_event_end_aux(aux,8)==EINVAL);aux[i]^=1;}
 assert(remesa_scene_event_end_aux(aux,7)==EINVAL);
 assert(remesa_event_fetch_read(image,148,(void *)image)==EINVAL);
 puts("PASS event contracts: unchanged DATA64/CODE84 producer and historical CODE oracle,80 relocation combinations, target pitch/four dimensions, full background/transfer/end auxiliary bytes, masks/capacity/atomic errors; GPU0");return 0;
}
