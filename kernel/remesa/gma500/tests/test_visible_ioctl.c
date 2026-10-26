/* SPDX-License-Identifier: MIT */
/* Original native GEM implementation, injected kernel APIs. NO hardware. */
#define main legacy_visible_readback_test
#include "test_visible_readback.c"
#undef main
#include "../remesa_gem_visible_ioctl.inc"
#include <stddef.h>
static unsigned checks;
#define CHECK(x) do{checks++;assert(x);}while(0)
static struct remesa_visible_readback_request request(struct remesa_memory_ticket t,u32 id)
{
 return (struct remesa_visible_readback_request){.version=1,.id=id,.context=t.context,
  .operation=t.operation,.slot=t.slot,.generation=t.generation,.length=4096};
}
static void reject(struct drm_device *d,struct drm_file *f,
 struct remesa_visible_readback_request *r,int error)
{
 struct remesa_visible_readback_request saved=*r;
 CHECK(remesa_gem_visible_readback_ioctl(d,r,f)==error);
 CHECK(!memcmp(r,&saved,sizeof(*r)));
}
int main(int argc,char **argv)
{
 CHECK(argc==2);mode=(unsigned)atoi(argv[1]);CHECK(mode<=3);
 CHECK(sizeof(struct remesa_visible_readback_request)==4144);
 CHECK(offsetof(struct remesa_visible_readback_request,data)==48);
 struct test_mmu mmu={true,{0}};struct drm_psb_private priv={&mmu,(void *)1,{0x28000000,32,0x80000000}};
 struct drm_device dev={&priv},other_dev={&priv};struct drm_file other_file={2};
 struct remesa_gem_memory *s=NULL;CHECK(!remesa_gem_memory_open(&dev,&owner,77,&s));
 struct remesa_memory_request alloc={.version=1,.context=77,.action=REMESA_MEM_CREATE,.bytes=4096000};
 CHECK(!remesa_gem_memory_request(s,&owner,&alloc));u32 id=alloc.id;
 CHECK(!action(s,REMESA_MEM_PIN,id));alloc.bytes=0;alloc.action=REMESA_MEM_MAP;alloc.va=0x40000000;
 CHECK(!remesa_gem_memory_request(s,&owner,&alloc));
 struct backing *bo=&s->bo[id&31];
 for(size_t i=0;i<4096000;i++)bo->gem.bytes[i]=(unsigned char)((i>>12)^i);
 struct remesa_memory_binding binding={id,0x40000000,4096000};struct remesa_memory_ticket t;
 unsigned char commands[8]={0};CHECK(!remesa_gem_job_prepare(s,&binding,1,commands,8,&t));
 struct remesa_visible_readback_request r=request(t,id);
 endpoint=s;admin=false;reject(&dev,&owner,&r,-EPERM);admin=true;
 reject(&dev,&owner,&r,-EOPNOTSUPP);memory_enabled=true;
 reject(&dev,&other_file,&r,-EACCES);reject(&other_dev,&owner,&r,-EACCES);
 endpoint=NULL;reject(&dev,&owner,&r,-ENODEV);endpoint=s;
 r.context++;reject(&dev,&owner,&r,-EACCES);r=request(t,id);
 for(unsigned fault=0;fault<10;fault++) {
  r=request(t,id);
  if(fault==0)r.version=2;
  if(fault==1)r.id=0;
  if(fault==2)r.operation=0;
  if(fault==3)r.slot=8;
  if(fault==4)r.generation=0;
  if(fault==5)r.length=0;
  if(fault==6)r.length=4097;
  if(fault==7)r.reserved=1;
  if(fault==8)r.data[4095]=1;
  if(fault==9)r.context=0;
  reject(&dev,&owner,&r,-EINVAL);
 }
 r=request(t,id);reject(&dev,&owner,&r,-EBUSY);
 s->executor=&executor;CHECK(!remesa_gem_job_issue(s,t));reject(&dev,&owner,&r,-EBUSY);
 CHECK(!remesa_gem_job_wait(s,t,1));reject(&dev,&owner,&r,-EBUSY);
 int e=remesa_gem_job_publish(s,t);
 if(mode==1){CHECK(e==-EIO);reject(&dev,&owner,&r,-EIO);goto held;}
 CHECK(!e);
 memory_enabled=false;reject(&dev,&owner,&r,-EOPNOTSUPP);memory_enabled=true;
 r.slot^=1;reject(&dev,&owner,&r,-ENOENT);r=request(t,id);
 r.operation++;reject(&dev,&owner,&r,-ENOENT);r=request(t,id);
 r.generation++;reject(&dev,&owner,&r,-ENOENT);r=request(t,id);
 r.id++;reject(&dev,&owner,&r,-ENOENT);r=request(t,id);
 r.offset=4096000;reject(&dev,&owner,&r,-EINVAL);r.offset=~(u64)0;reject(&dev,&owner,&r,-EINVAL);r=request(t,id);
 if(mode==2){test_pt[999].pfn++;reject(&dev,&owner,&r,-EIO);goto held;}
 if(mode==3){fail_vmap=1;reject(&dev,&owner,&r,-ENOMEM);fail_vmap=0;CHECK(bo->holds==1);}
 for(unsigned page=0;page<1000;page++) {
  r=request(t,id);r.offset=(u64)page*4096;
  CHECK(!remesa_gem_visible_readback_ioctl(&dev,&r,&owner));
  CHECK(!memcmp(r.data,bo->gem.bytes+(size_t)page*4096,4096)&&bo->holds==1);
 }
 r=request(t,id);r.offset=4095999;r.length=1;
 CHECK(!remesa_gem_visible_readback_ioctl(&dev,&r,&owner));CHECK(r.data[0]==bo->gem.bytes[4095999]);
 const unsigned char zero[4096]={0};CHECK(!memcmp(r.data+1,zero,4095));
 CHECK(starts==1&&polls==1&&publishes==1&&!acks);
 CHECK(action(s,REMESA_MEM_LEASE,id)==-EBUSY);
 CHECK(!remesa_gem_job_acknowledge(s,t));r=request(t,id);
 CHECK(!remesa_gem_visible_readback_ioctl(&dev,&r,&owner));
 CHECK(!remesa_gem_job_retire(s,t));r=request(t,id);reject(&dev,&owner,&r,-ENOENT);
 endpoint=NULL;CHECK(!remesa_gem_memory_close(s));
 CHECK(!gems&&!pins&&!ptes&&!power&&!module_refs&&!device_refs);
 printf("PASS CPU_ONLY visible envelope mode%u:1000pages1280x800, file/context/ticket/gate/state checks, no job transitions or writes (%u checks)\n",mode,checks);return 0;
held:
 CHECK(bo->holds==1&&remesa_gem_job_retire(s,t)==-EBUSY&&remesa_gem_memory_close(s)==-EBUSY);
 CHECK(starts==1&&polls==1&&publishes==1&&!acks);
 printf("PASS CPU_ONLY failed visible envelope mode%u: untouched output, quarantine/holds retained (%u checks)\n",mode,checks);return 0;
}
