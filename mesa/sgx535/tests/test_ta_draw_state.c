/* SPDX-License-Identifier: MIT */
#include "remesa_ta_scene.h"
#include "remesa_fragment_body.h"
#include "remesa_shader_program.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;assert(x);}while(0)
static uint32_t word(const uint8_t *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static struct remesa_ta_constant_draw_state_request valid(void)
{
 struct remesa_ta_constant_draw_state_request r={0};
 r.isp=(struct remesa_isp_opaque_triangle_request){true,true,true,REMESA_DEPTH_LESS,15};
 r.fragment.address_window_base=UINT64_C(0x20000000);
 r.fragment.primary=(struct remesa_ta_fragment_pds_reference){UINT64_C(0x20000000),4096,0x160,56,12,0,1};
 r.fragment.secondary=(struct remesa_ta_fragment_pds_reference){UINT64_C(0x20000000),4096,0x220,4,0,0,0};
 r.fragment.profile=REMESA_TA_FRAGMENT_AUTHORED_CONSTANT;
 r.viewport=(struct remesa_ta_window_viewport_request){32,32,0,0,32,32,24,0,1,REMESA_TA_VIEWPORT_INITIAL_ZERO};
 r.scalar_profile=REMESA_TA_SCALAR_POSITION4;return r;
}
static void reject(struct remesa_ta_constant_draw_state_request r,int expected)
{uint8_t b[58],saved[58];memset(b,0xa5,sizeof(b));memcpy(saved,b,sizeof(b));
 CHECK(remesa_ta_constant_draw_state_encode(&r,b+1)==expected);CHECK(!memcmp(b,saved,sizeof(b)));}
static void cached_checks(void)
{
 struct remesa_ta_constant_draw_state_request r=valid(),bad;
 uint8_t b[58],saved[58],full[56],expected[52];size_t written=99;
 CHECK(!remesa_ta_constant_draw_state_encode(&r,full));
 memcpy(expected,full,48);expected[0]=0x41;expected[1]=0x0b;memcpy(expected+48,full+52,4);
 memset(b,0xa5,sizeof(b));
 CHECK(!remesa_ta_constant_draw_after_zclear_encode(&r,b+1,52,&written));
 CHECK(written==52 && b[0]==0xa5 && b[53]==0xa5);
 CHECK(!memcmp(b+1,expected,52));CHECK(word(b+1)==0xb41);
 CHECK(word(b+49)==0x358637bd);
 memset(b,0xa5,sizeof(b));memcpy(saved,b,sizeof(b));written=99;
 CHECK(remesa_ta_constant_draw_after_zclear_encode(&r,b,51,&written)==ENOSPC);
 CHECK(written==99 && !memcmp(b,saved,sizeof(b)));
 bad=r;bad.scalar_profile=REMESA_TA_SCALAR_POSITION4_PRIMARY_COLOR4;
 CHECK(remesa_ta_constant_draw_after_zclear_encode(&bad,b,52,&written)==ENOTSUP);
 CHECK(written==99 && !memcmp(b,saved,sizeof(b)));
 bad=r;bad.fragment.primary.byte_offset++;
 CHECK(remesa_ta_constant_draw_after_zclear_encode(&bad,b,52,&written)==EINVAL);
 CHECK(written==99 && !memcmp(b,saved,sizeof(b)));
 CHECK(remesa_ta_constant_draw_after_zclear_encode(NULL,b,52,&written)==EINVAL);
 CHECK(remesa_ta_constant_draw_after_zclear_encode(&r,NULL,52,&written)==EINVAL);
 CHECK(remesa_ta_constant_draw_after_zclear_encode(&r,b,52,NULL)==EINVAL);
 CHECK(remesa_ta_constant_draw_after_zclear_encode(&r,b,0,&written)==EINVAL);
 CHECK(remesa_ta_constant_draw_after_zclear_encode(&r,(void*)(UINTPTR_MAX-50),52,&written)==EINVAL);
 CHECK(remesa_ta_constant_draw_after_zclear_encode(&r,b,52,(void*)(UINTPTR_MAX-2))==EINVAL);
 {union {size_t alignment;uint8_t bytes[64];} u;memset(&u,0xa5,sizeof(u));
  CHECK(remesa_ta_constant_draw_after_zclear_encode(&r,u.bytes,52,(size_t*)u.bytes)==EINVAL);
  CHECK(u.bytes[0]==0xa5);}
 {struct remesa_ta_constant_draw_state_request a=r;
  CHECK(!remesa_ta_constant_draw_after_zclear_encode(&a,((uint8_t*)&a)+1,52,&written));
  CHECK(!memcmp(((uint8_t*)&a)+1,expected,52));}
 {struct remesa_ta_constant_draw_state_request a=bad,saved_request=a;
  CHECK(remesa_ta_constant_draw_after_zclear_encode(&a,((uint8_t*)&a)+1,52,&written)==EINVAL);
  CHECK(!memcmp(&a,&saved_request,sizeof(a)));}
}
int main(int argc,char **argv)
{
 struct remesa_ta_constant_draw_state_request r=valid(),bad;
 uint8_t b[58],expected[56],v[28];
 if(argc==2 && !strcmp(argv[1],"--body-dump")){
  const uint32_t colors[]={UINT32_C(0xff00ff00),UINT32_C(0xffff00ff),UINT32_C(0xffffff00)};
  for(unsigned k=0;k<3;k++){
   struct remesa_shader_program program;struct remesa_fragment_constant body;
   CHECK(!remesa_shader_program_prepare_color(colors[k],&program));
   CHECK(!remesa_fragment_constant_prepare(&program.usse,&body));
   for(unsigned j=0;j<8;j++)printf("%02x",body.record[j]);
   puts("");}return 0;
 }
 if(argc==2 && (!strcmp(argv[1],"--cached-dump") || !strcmp(argv[1],"--ordered-dump"))){
  int ordered=!strcmp(argv[1],"--ordered-dump");size_t written;
  for(unsigned k=0;k<3;k++){
   r=valid();r.fragment.primary.byte_offset+=32*k;r.fragment.secondary.byte_offset+=32*k;
   CHECK(!remesa_ta_constant_draw_after_zclear_encode(&r,b,sizeof(b),&written));
   for(unsigned j=0;j<written;j++)printf("%02x",b[j]);
   puts("");
   if(ordered)for(unsigned step=1;step<3;step++){
    r.fragment.primary.byte_offset+=32;r.fragment.secondary.byte_offset+=32;
    CHECK(!remesa_ta_fragment_binding_encode(&r.fragment,b));
    for(unsigned j=0;j<16;j++)printf("%02x",b[j]);
    puts("");
   }
  }return 0;
 }
 if(argc==2 && (!strcmp(argv[1],"--dump") || !strcmp(argv[1],"--clear-dump"))){
  int clear=!strcmp(argv[1],"--clear-dump");
  for(unsigned k=0;k<3;k++){
   r.fragment.primary.byte_offset=0x160+32*k;r.fragment.secondary.byte_offset=0x220+32*k;
   if(clear){r.fragment.profile=REMESA_TA_FRAGMENT_SCENE_ZCLEAR;r.fragment.primary.field14=0;CHECK(!remesa_ta_scene_zclear_state_encode(&r.fragment,b));}
   else CHECK(!remesa_ta_constant_draw_state_encode(&r,b));
   for(unsigned j=0;j<(clear?44u:56u);j++)printf("%02x",b[j]);
   puts("");}return 0;
 }
 cached_checks();
 CHECK(argc==1);memset(b,0xa5,sizeof(b));CHECK(!remesa_ta_constant_draw_state_encode(&r,b+1));
 CHECK(b[0]==0xa5 && b[57]==0xa5);CHECK(word(b+1)==0xf41);CHECK(word(b+5)==0x400000);
 CHECK(word(b+9)==0x22 && word(b+13)==0x30000 && word(b+17)==0xc000016);
 for(unsigned i=20;i<44;i++)CHECK(b[i+1]==0);
 CHECK(word(b+45)==0 && word(b+49)==0x4001000 && word(b+53)==0x358637bd);
 CHECK(!remesa_ta_window_viewport_encode(&r.viewport,v));CHECK(word(v)==0x100);
 for(unsigned i=4;i<28;i++)CHECK(v[i]==0);
 bad=r;bad.viewport.path=(enum remesa_ta_viewport_cpu_path)99;reject(bad,ENOTSUP);
 bad=r;bad.viewport.path=REMESA_TA_VIEWPORT_MATRIX_WINDOW;reject(bad,ENOTSUP);
 bad=r;bad.scalar_profile=REMESA_TA_SCALAR_POSITION4_PRIMARY_COLOR4;reject(bad,ENOTSUP);
 bad=r;bad.fragment.profile=(enum remesa_ta_fragment_cpu_profile)99;reject(bad,ENOTSUP);
 bad=r;bad.fragment.primary.field14=0;reject(bad,ENOTSUP);
 bad=r;bad.fragment.primary.byte_offset++;reject(bad,EINVAL);
 bad=r;bad.viewport.width=0;reject(bad,EINVAL);
 bad=r;bad.isp.rgba_mask=1;reject(bad,ENOTSUP);
 bad=r;bad.isp.compare=REMESA_DEPTH_EQUAL;reject(bad,ENOTSUP);
 CHECK(remesa_ta_constant_draw_state_encode(NULL,b)==EINVAL);
 CHECK(remesa_ta_constant_draw_state_encode(&r,NULL)==EINVAL);
 CHECK(remesa_ta_constant_draw_state_encode((void*)(UINTPTR_MAX-sizeof(r)+1),b)==EINVAL);
 CHECK(remesa_ta_constant_draw_state_encode(&r,(void*)(UINTPTR_MAX-54))==EINVAL);
 CHECK(!remesa_ta_constant_draw_state_encode(&r,expected));
 {struct remesa_ta_constant_draw_state_request a=r;CHECK(!remesa_ta_constant_draw_state_encode(&a,((uint8_t*)&a)+1));CHECK(!memcmp(((uint8_t*)&a)+1,expected,56));}
 {struct remesa_ta_constant_draw_state_request a=r,saved;a.fragment.primary.field14=0;saved=a;CHECK(remesa_ta_constant_draw_state_encode(&a,((uint8_t*)&a)+1)==ENOTSUP);CHECK(!memcmp(&a,&saved,sizeof(a)));}
 /* The clear contract is separate, not a generic opaque color-mask rule. */
 memset(b,0xa5,sizeof(b));CHECK(remesa_ta_scene_zclear_state_encode(&r.fragment,b+1)==ENOTSUP);CHECK(b[1]==0xa5);
 r.fragment.profile=REMESA_TA_FRAGMENT_SCENE_ZCLEAR;r.fragment.primary.field14=0;
 CHECK(!remesa_ta_scene_zclear_state_encode(&r.fragment,b+1));CHECK(b[0]==0xa5 && b[45]==0xa5);
 CHECK(word(b+1)==0x54c5 && word(b+5)==0x1e00100 && word(b+9)==0xe000000);
 CHECK(word(b+13)==0x22 && word(b+17)==0x30000 && word(b+21)==0xc000016);
 CHECK(word(b+25)==0 && word(b+29)==0 && word(b+33)==0x4001000 && word(b+37)==0x10000 && word(b+41)==0);
 CHECK(remesa_ta_constant_draw_state_encode(&r,b)==ENOTSUP);
 CHECK(remesa_ta_scene_zclear_state_encode(NULL,b)==EINVAL);
 CHECK(remesa_ta_scene_zclear_state_encode(&r.fragment,NULL)==EINVAL);
 CHECK(remesa_ta_scene_zclear_state_encode((void*)(UINTPTR_MAX-sizeof(r.fragment)+1),b)==EINVAL);
 CHECK(remesa_ta_scene_zclear_state_encode(&r.fragment,(void*)(UINTPTR_MAX-42))==EINVAL);
 {struct remesa_ta_fragment_binding_request a=r.fragment;uint8_t e[44];CHECK(!remesa_ta_scene_zclear_state_encode(&a,e));CHECK(!remesa_ta_scene_zclear_state_encode(&a,((uint8_t*)&a)+1));CHECK(!memcmp(((uint8_t*)&a)+1,e,44));}
 {struct remesa_ta_fragment_binding_request a=r.fragment;uint8_t bytes[46],saved[46];
  a.primary.byte_offset++;memset(bytes,0xa5,sizeof(bytes));memcpy(saved,bytes,sizeof(bytes));
  CHECK(remesa_ta_scene_zclear_state_encode(&a,bytes+1)==EINVAL);CHECK(!memcmp(bytes,saved,sizeof(bytes)));}
 printf("PASS %u draw/scene-Z-clear DATA checks; CPU only\n",checks);return 0;
}
