#define REMESA_MEMORY_TEST
#include "../module/remesa_gem_memory.c"
#include <stdio.h>
static struct drm_file owner={1};
static int request(struct remesa_gem_memory*s,unsigned action,u32 id){struct remesa_memory_request r={.version=1,.context=77,.action=action,.id=id};return remesa_gem_memory_request(s,&owner,&r);}
int main(void){
 struct test_mmu mmu={true,{0}};struct drm_psb_private priv={&mmu,(void*)1,{0x28000000,32,0x80000000}};struct drm_device dev={&priv};struct remesa_gem_memory*s=NULL;
 assert(!remesa_gem_memory_open(&dev,&owner,77,&s));struct remesa_memory_binding uses[2];struct remesa_memory_ticket ticket;struct remesa_memory_view views[2];unsigned char commands[8]={1};
 for(unsigned i=0;i<2;i++){
  struct remesa_memory_request r={.version=1,.context=77,.action=REMESA_MEM_CREATE,.bytes=8192};assert(!remesa_gem_memory_request(s,&owner,&r));uses[i]=(struct remesa_memory_binding){r.id,0x40000000+i*8192,8192};assert(!request(s,REMESA_MEM_PIN,r.id));r=(struct remesa_memory_request){.version=1,.context=77,.action=REMESA_MEM_MAP,.id=r.id,.va=uses[i].va};assert(!remesa_gem_memory_request(s,&owner,&r));
 }
 assert(!remesa_gem_job_prepare(s,uses,2,commands,8,&ticket));fail_vmap_call=2;
 assert(remesa_gem_job_views(s,ticket,views,2)==-ENOMEM);assert(vmap_calls==2&&vunmap_calls==1);assert(!s->bo[uses[0].id&31].cached_job_view&&!s->bo[uses[1].id&31].cached_job_view);
 fail_vmap_call=0;assert(remesa_gem_job_views(s,ticket,views,2)==2);assert(vmap_calls==4);void*first=views[0].cpu;memset(first,0x93,8192);
 assert(request(s,REMESA_MEM_UNMAP,uses[0].id)==-EBUSY);assert(request(s,REMESA_MEM_UNPIN,uses[0].id)==-EBUSY);assert(!remesa_gem_job_cancel(s,ticket));assert(vunmap_calls==1);
 assert(!remesa_gem_job_prepare(s,uses,2,commands,8,&ticket));assert(remesa_gem_job_views(s,ticket,views,2)==2&&vmap_calls==4&&views[0].cpu==first);for(unsigned i=0;i<8192;i++)assert(((unsigned char*)first)[i]==0x93);
 assert(!remesa_gem_job_cancel(s,ticket));for(unsigned i=0;i<2;i++){assert(!request(s,REMESA_MEM_UNMAP,uses[i].id));assert(!request(s,REMESA_MEM_UNPIN,uses[i].id));assert(!s->bo[uses[i].id&31].cached_job_view);assert(!request(s,REMESA_MEM_DESTROY,uses[i].id));}
 assert(vunmap_calls==3);assert(!remesa_gem_memory_close(s));assert(!gems&&!pins&&!ptes&&!power&&!claimed&&!device_refs&&!module_refs);
 puts("PASS: injected second vmap failure rolls back first; retry allocates2; second job reuses2 mappings and preserved bytes without vmap/vunmap; busy while held; final unpin releases exactly2 cached mappings. CPU fake APIs, GPU0.");return 0;
}
