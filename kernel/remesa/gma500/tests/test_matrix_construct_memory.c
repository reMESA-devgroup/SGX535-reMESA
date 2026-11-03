/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual retained memory act()/PTE/lease/bounds checks. MMU/pages modeled;
 * ready-owner metadata is a fixture, never a source or GPU certificate. */
#define main inherited_twod_memory_tests
#include "test_gem_twod.c"
#undef main
int main(void)
{
 struct test_mmu mmu={true,{0}};struct drm_psb_private priv={&mmu,(void*)1,{0x28000000,32,0x80000000}};
 struct drm_device dev={&priv};struct remesa_gem_memory *s=NULL;unsigned owner=0;
 CHECK(!remesa_gem_memory_open(&dev,&file,77,&s));
 u32 color=create(s,32,32,0x40001000);
 s->private_session=&owner;s->private_matrix_color=color;s->private_matrix_constructing=true;
 struct remesa_memory_request r={.version=1,.action=REMESA_MEM_CREATE,.context=77,.bytes=4096};
 CHECK(!remesa_gem_memory_request(s,&file,&r));u32 buffer=r.id;
 r=(struct remesa_memory_request){.version=1,.context=77,.id=buffer,.action=REMESA_MEM_PIN};
 CHECK(!remesa_gem_memory_request(s,&file,&r));r.action=REMESA_MEM_MAP;r.va=0x41000000;
 CHECK(!remesa_gem_memory_request(s,&file,&r));
 r=(struct remesa_memory_request){.version=1,.context=77,.id=buffer^32,.action=REMESA_MEM_LEASE,.flags=1};
 CHECK(remesa_gem_memory_request(s,&file,&r)==-ENOENT);
 r.id=buffer;CHECK(!remesa_gem_memory_request(s,&file,&r));
 r=(struct remesa_memory_request){.version=1,.context=77,.id=buffer,.action=REMESA_MEM_WRITE,.length=16};
 memset(r.data,0x5a,16);r.offset=4096;CHECK(remesa_gem_memory_request(s,&file,&r)==-EINVAL);
 r.offset=0;CHECK(!remesa_gem_memory_request(s,&file,&r));
 r=(struct remesa_memory_request){.version=1,.context=77,.id=buffer,.action=REMESA_MEM_UNLEASE};
 CHECK(!remesa_gem_memory_request(s,&file,&r));
 r=(struct remesa_memory_request){.version=1,.context=77,.id=color,.action=REMESA_MEM_LEASE,.flags=1};
 CHECK(remesa_gem_memory_request(s,&file,&r)==-EPERM);
 for(unsigned action=REMESA_MEM_UNMAP;action<=REMESA_MEM_DESTROY;action++){
  r=(struct remesa_memory_request){.version=1,.context=77,.id=color,.action=action};
  CHECK(remesa_gem_memory_request(s,&file,&r)==-EPERM);
 }
 struct backing *b=&s->bo[buffer&31];struct remesa_memory_binding use={buffer,b->va.start,b->gem.size};
 struct remesa_memory_ticket ticket;unsigned command=0;
 CHECK(!remesa_gem_job_prepare(s,&use,1,&command,sizeof command,&ticket));
 r=(struct remesa_memory_request){.version=1,.context=77,.id=buffer,.action=REMESA_MEM_LEASE};
 CHECK(remesa_gem_memory_request(s,&file,&r)==-EBUSY);CHECK(!remesa_gem_job_cancel(s,ticket));
 CHECK(!remesa_gem_memory_request(s,&file,&r));
 r=(struct remesa_memory_request){.version=1,.context=77,.id=buffer,.action=REMESA_MEM_READ,.length=16};
 CHECK(!remesa_gem_memory_request(s,&file,&r));
 for(unsigned i=0;i<16;i++)CHECK(r.data[i]==0x5a);
 r=(struct remesa_memory_request){.version=1,.context=77,.id=buffer,.action=REMESA_MEM_UNLEASE};
 CHECK(!remesa_gem_memory_request(s,&file,&r));
 s->private_matrix_constructing=false;
 r=(struct remesa_memory_request){.version=1,.context=77,.action=REMESA_MEM_CREATE,.bytes=4096};
 CHECK(remesa_gem_memory_request(s,&file,&r)==-EBUSY);
 CHECK(remesa_gem_memory_close(s)==-EBUSY);
 /* Fixture metadata teardown only; real owner release is tested separately. */
 s->private_session=NULL;s->private_matrix_color=0;
 CHECK(!remesa_gem_memory_close(s));CHECK(!gems&&!pins&&!ptes&&!claimed&&!power&&!device_refs&&!module_refs);
 puts("PASS actual normal CPU construction: allocate/pin/map/write/read, COLOR protection, generation/bounds/holds/freeze; GPU0");
 return 0;
}
