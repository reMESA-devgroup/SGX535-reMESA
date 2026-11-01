/* SPDX-License-Identifier: MIT. Syscall model, never creates GPU pixels. */
#define __wrap_ioctl previous_scene_model_ioctl
#define REMESA_NATIVE_MODEL_PROFILE_MIN 7u
#define REMESA_NATIVE_MODEL_PROFILE_MAX 10u
#include "test_scene_native_client_model.c"
#undef __wrap_ioctl
static bool returned_native,teardown_started;
static unsigned reads_after_retirement;
int __wrap_ioctl(int fd,unsigned long command,...)
{
 va_list ap;va_start(ap,command);void *arg=va_arg(ap,void *);va_end(ap);
 if(command==DRM_IOCTL_REMESA_SCENE_PROBE){
  const struct remesa_scene_probe_request *request=arg;
  assert((request->profile>=9)==(getenv("REMESA_GALLIUM_MODEL_MATRIX")!=NULL));
  int result=previous_scene_model_ioctl(fd,command,arg);
  assert(!result);returned_native=true;
  if(getenv("REMESA_GALLIUM_MODEL_BAD_RECEIPT"))((struct remesa_scene_probe_request *)arg)->phase=8;
  return result;
 }
 assert(fd==99&&command==DRM_IOCTL_REMESA_MEMORY);
 struct remesa_memory_request *r=arg;
 if(r->action==REMESA_MEM_INFO){
  struct remesa_memory_info info={0};
  for(unsigned k=0;k<count;k++)if(bo[k].bytes){
   info.objects++;info.allocated_bytes+=bo[k].size;info.pinned_objects+=bo[k].pin;
   info.mapped_pages+=bo[k].va?bo[k].size/4096:0;info.leases+=bo[k].lease;
  }
  if(r->id){
   unsigned k=r->id-32;assert(k<count&&bo[k].bytes);
   info.object_bytes=bo[k].size;info.gpu_va=bo[k].va;info.object_pinned=bo[k].pin;
   info.verified_pages=info.object_mapped_pages=bo[k].va?bo[k].size/4096:0;
   info.object_lease=bo[k].lease;
  }
  memcpy(r->data,&info,sizeof info);return 0;
 }
 if(r->action==REMESA_MEM_READ&&returned_native){assert(!teardown_started);reads_after_retirement++;}
 if(r->action==REMESA_MEM_UNMAP||r->action==REMESA_MEM_UNPIN||r->action==REMESA_MEM_DESTROY){
  unsigned k=r->id-32;assert(k<count&&bo[k].bytes&&!bo[k].lease);
  if(returned_native){assert(reads_after_retirement);teardown_started=true;}
  if(r->action==REMESA_MEM_UNMAP){assert(bo[k].va);bo[k].va=0;}
  if(r->action==REMESA_MEM_UNPIN){assert(bo[k].pin&&!bo[k].va);bo[k].pin=false;}
  if(r->action==REMESA_MEM_DESTROY){assert(!bo[k].pin&&!bo[k].va);free(bo[k].bytes);bo[k].bytes=NULL;}
  return 0;
 }
 return previous_scene_model_ioctl(fd,command,arg);
}
