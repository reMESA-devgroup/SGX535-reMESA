/* SPDX-License-Identifier: MIT */
/* Literal/texture CPU upload image, never GPU execution evidence. */
#include "remesa_texture_state.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef NDEBUG
#error Assertions must remain enabled
#endif
static unsigned checks;
static void reject(const struct remesa_texture_cpu_sa_uniform *u,size_t nu,
 const struct remesa_texture_cpu_sa_texture *t,size_t nt,size_t cap,int error)
{
   unsigned char out[64],before[64];memset(out,0xa5,sizeof(out));
   memcpy(before,out,sizeof(out));size_t n=37;
   assert(remesa_texture_cpu_sa_image_encode(u,nu,t,nt,out,cap,&n)==error);
   assert(n==37&&!memcmp(out,before,sizeof(out)));checks++;
}
int main(void)
{
   struct remesa_texture_cpu_sa_uniform u[2]={{0,0x3f800000},{4,0xdeadbeef}};
   struct remesa_texture_cpu_sa_texture t[2]={{.destination_dword=1,
      .sampler={.image_address=0x40001000,.width=1280,.height=800,
         .stride_bytes=5120,.format_id=20}}};
   unsigned char out[64];memset(out,0xa5,sizeof(out));size_t n=37;
   assert(!remesa_texture_cpu_sa_image_encode(u,2,t,1,out,64,&n)&&n==20);
   const unsigned char expected[]={0,0,0x80,0x3f,0xff,0,0xea,0x83,
      0x1f,0xf3,0x4f,0x6c,0,0x10,0,0x40,0xef,0xbe,0xad,0xde};
   assert(!memcmp(out,expected,sizeof(expected)));
   for(unsigned i=20;i<64;i++)assert(out[i]==0xa5);
   checks++;
   for(unsigned i=0;i<20;i++)reject(u,2,t,1,i,ENOSPC);
   u[1].destination_dword=0;reject(u,2,t,1,64,EINVAL);
   u[1].destination_dword=2;reject(u,2,t,1,64,EINVAL);
   u[1].destination_dword=5;reject(u,2,t,1,64,EINVAL);
   u[1].destination_dword=4;t[0].destination_dword=3;reject(u,2,t,1,64,EINVAL);
   t[0].destination_dword=1;t[0].sampler.format_id=0;reject(u,2,t,1,64,ENOTSUP);
   t[0].sampler.format_id=20;t[0].sampler.image_address++;reject(u,2,t,1,64,EINVAL);
   t[0].sampler.image_address=0x40001000;
   reject(u,SIZE_MAX,t,1,64,E2BIG);reject(u,2,t,SIZE_MAX,64,E2BIG);
   reject(NULL,2,t,1,64,EINVAL);reject(u,2,NULL,1,64,EINVAL);
   reject(NULL,0,NULL,0,64,ENODATA);
   u[0].destination_dword=7;u[1].destination_dword=0;
   t[1]=t[0];t[1].destination_dword=3;reject(u,2,t,2,64,EINVAL);
   t[1].destination_dword=4;t[1].sampler.image_address+=0x1000;
   assert(!remesa_texture_cpu_sa_image_encode(u,2,t,2,out,64,&n)&&n==32);checks++;
   struct remesa_texture_cpu_sa_uniform all[1024];
   unsigned char large[4097];for(unsigned i=0;i<1024;i++)all[i]=(struct remesa_texture_cpu_sa_uniform){i,i};
   memset(large,0xa5,sizeof(large));
   assert(!remesa_texture_cpu_sa_image_encode(all,1024,NULL,0,large,4097,&n));
   assert(n==4096&&large[4096]==0xa5);checks++;
   unsigned char saved[sizeof(u)];memcpy(saved,u,sizeof(u));n=37;
   assert(remesa_texture_cpu_sa_image_encode(u,2,t,1,u,sizeof(u),&n)==EINVAL);
   assert(n==37&&!memcmp(u,saved,sizeof(u)));checks++;
   assert(remesa_texture_cpu_sa_image_encode(u,2,t,1,out,64,NULL)==EINVAL);checks++;
   printf("texture SA image CPU checks=%u GPU operations=0\n",checks);
   return 0;
}
