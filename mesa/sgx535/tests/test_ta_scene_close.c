/* SPDX-License-Identifier: MIT */
#include "remesa_ta_index.h"
#include "remesa_render_target.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do {assert(x);checks++;}while(0)
static uint32_t rd(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static struct remesa_ta_scene_close_request base(void){return (struct remesa_ta_scene_close_request){.pds_bo_gpu_va=0x20000000,.pds_bo_bytes=0x10000,.pds_byte_offset=0x4460,.pds_object_bytes=60};}
static void reject(struct remesa_ta_scene_close_request r,int e){uint8_t o[12],old[12];memset(o,0xa5,12);memcpy(old,o,12);CHECK(remesa_ta_scene_close_encode(&r,o)==e);CHECK(!memcmp(old,o,12));}
int main(int argc,char **argv){
 const uint32_t dims[][2]={{1,1},{16,16},{17,33},{32,32},{64,64},{1280,800},{2048,2048}};
 uint8_t o[14];
 for(unsigned i=0;i<7;i++){
  memset(o,0xa5,sizeof(o));CHECK(!remesa_ta_scene_close_data_encode(dims[i][0],dims[i][1],o+1));CHECK(o[0]==0xa5&&o[9]==0xa5);CHECK(rd(o+1)==0x2000);
  struct remesa_render_target_request rt={.width=dims[i][0],.height=dims[i][1],.pitch_pixels=(dims[i][0]+31)&~31u,.gpu_va=0x40000000,.allocation_bytes=2048*2048*4};struct remesa_render_target_state s;
  CHECK(!remesa_render_target_encode_linear_bgra8(&rt,&s));CHECK(rd(o+5)==s.termination_extent);
  if(argc==2&&!strcmp(argv[1],"--dump")){printf("data %u %u ",dims[i][0],dims[i][1]);for(unsigned j=1;j<9;j++)printf("%02x",o[j]);puts("");}
 }
 for(unsigned i=0;i<2;i++){
  struct remesa_ta_scene_close_request r=base();if(!i)r.pds_byte_offset=0x160;
  memset(o,0xa5,14);CHECK(!remesa_ta_scene_close_encode(&r,o+1));CHECK(o[0]==0xa5&&o[13]==0xa5);
  if(argc==2&&!strcmp(argv[1],"--dump")){printf("close %u ",(unsigned)(r.pds_bo_gpu_va+r.pds_byte_offset));for(unsigned j=1;j<13;j++)printf("%02x",o[j]);puts("");}
 }
 struct remesa_ta_scene_close_request r=base();r.pds_object_bytes=59;reject(r,ERANGE);r=base();r.pds_bo_bytes=0x449b;reject(r,ERANGE);r=base();r.pds_bo_gpu_va=0xffffffff;reject(r,ERANGE);r=base();r.pds_byte_offset=UINT64_MAX;reject(r,ERANGE);r=base();r.pds_byte_offset++;reject(r,EINVAL);
 CHECK(remesa_ta_scene_close_encode(NULL,o)==EINVAL);r=base();CHECK(remesa_ta_scene_close_encode(&r,NULL)==EINVAL);CHECK(remesa_ta_scene_close_encode((const void *)UINTPTR_MAX,o)==EINVAL);CHECK(remesa_ta_scene_close_encode(&r,(void *)UINTPTR_MAX)==EINVAL);
 memset(o,0xa5,14);CHECK(remesa_ta_scene_close_data_encode(0,32,o)==EINVAL);CHECK(o[0]==0xa5);CHECK(remesa_ta_scene_close_data_encode(32,0,o)==EINVAL);CHECK(remesa_ta_scene_close_data_encode(2049,32,o)==ENOTSUP);CHECK(remesa_ta_scene_close_data_encode(32,UINT32_MAX,o)==ENOTSUP);CHECK(remesa_ta_scene_close_data_encode(32,32,NULL)==EINVAL);CHECK(remesa_ta_scene_close_data_encode(32,32,(void *)UINTPTR_MAX)==EINVAL);
 union {struct remesa_ta_scene_close_request r;uint8_t b[sizeof(struct remesa_ta_scene_close_request)];}a;a.r=base();uint8_t expected[12];CHECK(!remesa_ta_scene_close_encode(&a.r,expected));CHECK(!remesa_ta_scene_close_encode(&a.r,a.b));CHECK(!memcmp(expected,a.b,12));
 r=base();r.pds_bo_bytes=0x449c;CHECK(!remesa_ta_scene_close_encode(&r,o));
 if(argc==1)printf("PASS %u focused close checks\n",checks);
 return 0;
}
