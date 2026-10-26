/* SPDX-License-Identifier: MIT */
#include "remesa_ta_scene.h"
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;assert(x);}while(0)
static const struct remesa_ta_window_viewport_request cases[]={
 {32,32,0,0,32,32,24,0,1,REMESA_TA_VIEWPORT_MATRIX_WINDOW},{32,32,0,0,32,32,24,.25f,.75f,REMESA_TA_VIEWPORT_MATRIX_WINDOW},
 {32,32,4,6,16,8,16,0,1,REMESA_TA_VIEWPORT_MATRIX_WINDOW},{64,64,0,0,64,64,24,1,0,REMESA_TA_VIEWPORT_MATRIX_WINDOW},
 {64,64,7,9,33,31,16,.125f,.875f,REMESA_TA_VIEWPORT_MATRIX_WINDOW},{64,64,0,0,31,17,24,.75f,.25f,REMESA_TA_VIEWPORT_MATRIX_WINDOW}};
static void reject(struct remesa_ta_window_viewport_request r,int expected)
{uint8_t out[30],before[30];memset(out,0xa5,sizeof(out));memcpy(before,out,sizeof(out));
 CHECK(remesa_ta_window_viewport_encode(&r,out+1)==expected);CHECK(!memcmp(before,out,sizeof(out)));}
int main(int argc,char **argv)
{
 uint8_t out[30];struct remesa_ta_window_viewport_request r=cases[0];unsigned i;
 if(argc==2 && !strcmp(argv[1],"--dump")){
  for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
   CHECK(!remesa_ta_window_viewport_encode(&cases[i],out));
   for(unsigned j=0;j<28;j++)printf("%02x",out[j]);
   puts("");}return 0;
 }
 CHECK(argc==1);memset(out,0xa5,sizeof(out));CHECK(!remesa_ta_window_viewport_encode(&r,out+1));
 CHECK(out[0]==0xa5 && out[29]==0xa5);
 {const uint8_t h[4]={0,1,0,0};CHECK(!memcmp(out+1,h,4));}
 for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++)CHECK(!remesa_ta_window_viewport_encode(&cases[i],out));
 r=cases[0];r.width=0;reject(r,EINVAL);
 r=cases[0];r.framebuffer_height=0;reject(r,EINVAL);
 r=cases[0];r.framebuffer_height=2049;reject(r,EINVAL);
 r=cases[0];r.x=UINT32_MAX;reject(r,EINVAL);
 r=cases[0];r.y=1;reject(r,EINVAL);
 r=cases[0];r.width=UINT32_MAX;reject(r,EINVAL);
 r=cases[0];r.depth_bits=32;reject(r,ENOTSUP);
 r=cases[0];r.near_z=NAN;reject(r,EINVAL);
 r=cases[0];r.far_z=INFINITY;reject(r,EINVAL);
 r=cases[0];r.near_z=-.1f;reject(r,EINVAL);
 r=cases[0];r.far_z=2;reject(r,EINVAL);
 r=cases[0];r.near_z=.1f;reject(r,ENOTSUP);
 r=cases[0];CHECK(remesa_ta_window_viewport_encode(NULL,out)==EINVAL);
 CHECK(remesa_ta_window_viewport_encode(&r,NULL)==EINVAL);
 CHECK(remesa_ta_window_viewport_encode((void *)(UINTPTR_MAX-sizeof(r)+1),out)==EINVAL);
 CHECK(remesa_ta_window_viewport_encode(&r,(void *)(UINTPTR_MAX-26))==EINVAL);
 {struct remesa_ta_window_viewport_request a=r;uint8_t expected[28];
 CHECK(!remesa_ta_window_viewport_encode(&r,expected));
 CHECK(!remesa_ta_window_viewport_encode(&a,((uint8_t*)&a)+1));
 CHECK(!memcmp(((uint8_t*)&a)+1,expected,28));}
 {struct remesa_ta_window_viewport_request a=r,before;a.depth_bits=32;before=a;
 CHECK(remesa_ta_window_viewport_encode(&a,((uint8_t*)&a)+1)==ENOTSUP);
 CHECK(!memcmp(&a,&before,sizeof(a)));}
 printf("PASS %u window viewport checks; CPU only\n",checks);return 0;
}
