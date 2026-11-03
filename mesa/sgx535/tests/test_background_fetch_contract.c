/* SPDX-License-Identifier: MIT. Actual encoder comparison; CPU only. */
#include "remesa/background_fetch.h"
#include "remesa_ta_scene.h"
#include "remesa_pds_structure.h"
#include <assert.h>
#include <stdio.h>
static void put(unsigned char *p,uint32_t v)
{for(unsigned i=0;i<4;i++)p[i]=v>>(8*i);}
int main(void)
{
 const uint32_t dims[][2]={{1280,800},{640,640},{641,479},{1600,768},{1,1},{2048,2048}};
 const uint32_t windows[]={0,0x20000000,0xf0000000},fixups[]={0,3,0x804,0x7ffff};
 unsigned char object[80],image[96],mask[96],saved[96];size_t n;
 struct remesa_background_binding binding,initial_binding={1,2};struct remesa_background_task task,initial_task={3,4};
 for(unsigned d=0;d<6;d++)for(unsigned b=0;b<3;b++)for(unsigned f=0;f<4;f++){
  uint32_t width=dims[d][0],height=dims[d][1];uint64_t base=(uint64_t)windows[b]+0x1000;
  struct remesa_background_object_request r={.address_window_base=windows[b],.primary={base,4096,32,64,12,1,0},
   .secondary={base,4096,96,4,0,0,0},.width=width,.height=height,.depth=1};
  memset(object,0xa5,sizeof object);assert(!remesa_background_object_encode(&r,object)&&object[68]==0xa5);
  assert(!remesa_background_binding_read(object,68,width,height,windows[b],&binding)&&binding.primary==base+32&&binding.secondary==base+96);
  assert(!remesa_background_binding_read(object,80,width,height,windows[b],&binding));
  for(unsigned i=12;i<68;i++){
   object[i]^=1;binding=initial_binding;assert(remesa_background_binding_read(object,68,width,height,windows[b],&binding)==EINVAL&&!memcmp(&binding,&initial_binding,sizeof binding));object[i]^=1;
  }
  const struct remesa_pds_background_request primary={0x44000000,fixups[f],4,width,height};
  memset(image,0xa5,sizeof image);memset(mask,0,sizeof mask);
  assert(!remesa_pds_structure_encode_background(&primary,image,mask,sizeof image,&n)&&n==64&&image[64]==0xa5);
  assert(!remesa_background_primary_image(image,sizeof image,width,height,&task));
  for(unsigned i=0;i<64;i++)if(!mask[i])for(unsigned bit=0;bit<8;bit++){
   image[i]^=1u<<bit;task=initial_task;
   assert(remesa_background_primary_image(image,sizeof image,width,height,&task)==EINVAL&&!memcmp(&task,&initial_task,sizeof task));
   assert(!remesa_background_primary(image,64,width,height,&task));image[i]^=1u<<bit;
  }
  for(unsigned i=0;i<64;i++)if(!mask[i])image[i]=(unsigned char)(17*i);
  assert(!remesa_background_primary(image,64,width,height,&task)&&task.color_address==0x44000000&&task.use_lower19==fixups[f]);
  assert(!remesa_background_primary(image,96,width,height,&task));memcpy(saved,image,sizeof image);
  for(unsigned i=4;i<64;i++)if(mask[i]&&(i<40||i>=44)){
   image[i]^=1;task=initial_task;assert(remesa_background_primary(image,64,width,height,&task)==EINVAL&&!memcmp(&task,&initial_task,sizeof task));memcpy(image,saved,sizeof image);
  }
  task=initial_task;assert(remesa_background_primary(image,63,width,height,&task)==EINVAL&&!memcmp(&task,&initial_task,sizeof task));
  put(image+40,0x44000001);assert(remesa_background_primary(image,64,width,height,&task)==ERANGE);
  object[0]^=1;binding=initial_binding;assert(remesa_background_binding_read(object,68,width,height,windows[b],&binding)==ERANGE&&!memcmp(&binding,&initial_binding,sizeof binding));object[0]^=1;
  object[4]^=2;assert(remesa_background_binding_read(object,68,width,height,windows[b],&binding)==EINVAL);object[4]^=2;
  put(object,(uint32_t)((base+32)>>4)&0xffffff);assert(remesa_background_binding_read(object,68,width,height,windows[b],&binding)==EINVAL);
 }
 assert(remesa_background_binding_read(object,67,640,640,0x20000000,&binding)==EINVAL);
 assert(remesa_background_binding_read(object,68,640,640,0x20000000,(void *)object)==EINVAL);
 assert(remesa_background_primary(image,64,0,640,&task)==EINVAL);
 assert(remesa_background_primary(image,64,2049,640,&task)==EINVAL);
 puts("PASS background contracts: unchanged object68/primary64 encoders, six dimensions/three windows/four fixups, full geometry and known fields/CODE, unknown masks/capacity/alignment/overlap/atomic errors; GPU0");return 0;
}
