/* SPDX-License-Identifier: MIT */
/* CPU field/range checks only. No physical texture sampling or API admission. */
#include "remesa_texture_state.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef NDEBUG
#error Assertions must remain enabled
#endif
static unsigned checks;
static void fail(struct remesa_texture_cpu_strided_state s,size_t cap,int error)
{
   uint8_t out[32],before[32]; memset(out,0xa5,sizeof(out));
   memcpy(before,out,sizeof(out));size_t written=37;
   assert(remesa_texture_cpu_strided_state_encode(&s,out+4,cap,&written)==error);
   assert(written==37 && !memcmp(out,before,sizeof(out))); checks++;
}
static void stream_cases(void)
{
   struct remesa_texture_cpu_stream_sampler_state s={.image_address=0x40001000,
      .width=1280,.height=800,.stride_bytes=5120,.format_id=20};
   uint8_t out[24],before[24];size_t written;
   for(unsigned min=0;min<2;min++)for(unsigned mag=0;mag<2;mag++) {
      s.min_filter_linear=min;s.mag_filter_linear=mag;
      memset(out,0xa5,sizeof(out));written=37;
      assert(!remesa_texture_cpu_stream_sampler_encode(&s,out+1,20,&written));
      uint32_t word0=0;for(unsigned i=0;i<4;i++)word0|=(uint32_t)out[i+1]<<(8*i);
      assert(word0==(UINT32_C(0x83ea00ff)|(min<<10)|(mag<<12)));
      assert(written==12&&out[0]==0xa5);
      for(unsigned i=13;i<24;i++)assert(out[i]==0xa5);
      checks++;
   }
   for(unsigned i=0;i<15;i++) {
      s.min_filter_linear=s.mag_filter_linear=0;s.image_address=0x40001000;
      size_t cap=i<12?i:12;int error=ENOSPC;
      if(i==12){s.min_filter_linear=2;error=ENOTSUP;}
      if(i==13){s.mag_filter_linear=2;error=ENOTSUP;}
      if(i==14){s.image_address=UINT64_C(0x100000000);error=ERANGE;}
      memset(out,0xa5,sizeof(out));memcpy(before,out,sizeof(out));written=37;
      assert(remesa_texture_cpu_stream_sampler_encode(&s,out,cap,&written)==error);
      assert(written==37&&!memcmp(out,before,sizeof(out)));checks++;
   }
   assert(remesa_texture_cpu_stream_sampler_encode(NULL,out,24,&written)==EINVAL);
   assert(remesa_texture_cpu_stream_sampler_encode(&s,NULL,24,&written)==EINVAL);
   assert(remesa_texture_cpu_stream_sampler_encode(&s,out,24,NULL)==EINVAL);
   checks+=3;
}
int main(void)
{
   struct remesa_texture_cpu_strided_state s={.image_address=0x40001000,
      .width=1280,.height=800,.stride_bytes=5120,.format_id=20,.filter_id=0};
   const uint8_t expected[]={0xff,0x00,0x08,0x80,0x1f,0xf3,0x4f,0x6c,
      0x00,0x10,0x00,0x40};
   uint8_t out[32];memset(out,0xa5,sizeof(out));size_t written=0;
   assert(!remesa_texture_cpu_strided_state_encode(&s,out+4,24,&written));
   assert(written==12 && !memcmp(out+4,expected,12));
   for (unsigned i=0;i<32;i++) if(i<4 || i>=16)assert(out[i]==0xa5);
   checks++;
   for(unsigned n=0;n<12;n++)fail(s,n,ENOSPC);
   struct remesa_texture_cpu_strided_state v=s;
   v.width=0;fail(v,12,EINVAL);v=s;v.height=0;fail(v,12,EINVAL);
   v=s;v.width=4097;fail(v,12,ERANGE);v=s;v.height=4097;fail(v,12,ERANGE);
   v=s;v.stride_bytes=16388;fail(v,12,ERANGE);
   v=s;v.stride_bytes=5116;fail(v,12,EINVAL);
   v=s;v.stride_bytes=5121;fail(v,12,EINVAL);
   v=s;v.image_address=0;fail(v,12,EINVAL);
   v=s;v.image_address++;fail(v,12,EINVAL);
   v=s;v.image_address=UINT64_C(0x100000000);fail(v,12,ERANGE);
   v=s;v.image_address=UINT64_C(0xfffffffc);fail(v,12,ERANGE);
   v=s;v.format_id=0;fail(v,12,ENOTSUP);v=s;v.filter_id=3;fail(v,12,ENOTSUP);
   v=(struct remesa_texture_cpu_strided_state){.image_address=0xfffffffc,
      .width=1,.height=1,.stride_bytes=4,.format_id=21,.filter_id=2};
   assert(!remesa_texture_cpu_strided_state_encode(&v,out,12,&written));checks++;
   v.height=2;fail(v,12,ERANGE);
   v=(struct remesa_texture_cpu_strided_state){.image_address=4,
      .width=4096,.height=4096,.stride_bytes=16384,.format_id=20,.filter_id=0};
   assert(!remesa_texture_cpu_strided_state_encode(&v,out,12,&written));checks++;
   memset(out,0xa5,sizeof(out));written=37;
   assert(remesa_texture_cpu_strided_state_encode(NULL,out,32,&written)==EINVAL);
   assert(remesa_texture_cpu_strided_state_encode(&s,NULL,32,&written)==EINVAL);
   assert(remesa_texture_cpu_strided_state_encode(&s,out,32,NULL)==EINVAL);
   for(unsigned i=0;i<32;i++)assert(out[i]==0xa5);
   assert(written==37);checks+=3;
   stream_cases();
   printf("texture state CPU checks=%u GPU operations=0\n",checks);
   return 0;
}
